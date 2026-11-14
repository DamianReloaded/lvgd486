// main.cpp

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>

#include "../vgd/Display/DisplayMode13h.hpp"
#include "../vgd/Input/Keyboard.hpp"
#include "../vgd/Util/Clock.hpp"
#include "../vgd/Font/BmpFont.hpp"
#include "../vgd/Font/FontArcade01.hpp"

extern "C"
{
#include "types.h"
#include "variables.h"
#include "ppu.h"
#include "zelda_rtl.h"
#include "zelda_cpu_infra.h"
#include "config.h"
#include "assets.h"
#include "load_gfx.h"
#include "util.h"
#include "audio.h"
}

#include "config_keyboard.hpp"

static bool g_run_without_emu = false;
static int g_input1_state;
static int g_ppu_render_flags = 0;

// ------------------------------------------------------------
// Frame skipping
// ------------------------------------------------------------

static int g_frameskip = 3;
static int g_frameskipCounter = 0;

static bool ShouldRenderFrame()
{
    if (g_frameskipCounter >= g_frameskip)
    {
        g_frameskipCounter = 0;
        return true;
    }

    g_frameskipCounter++;
    return false;
}

// ------------------------------------------------------------
// Input
// ------------------------------------------------------------

static bool g_keyDown[vgd::Keyboard::KeyCount];
static bool g_turbo = false;
static bool g_turbo_all_the_time = false;

static const uint8 kKbdRemap[] =
{
    0, 4, 5, 6, 7, 2, 3, 8, 0, 9, 1, 10, 11
};

// ------------------------------------------------------------
// Forward declarations
// ------------------------------------------------------------

static bool LoadRom(const char* filename);
static void LoadLinkGraphics();
static bool ParseLinkGraphics(uint8* file, size_t length);
static void LoadAssets();
static void SwitchDirectory();
static void UpdateVgaPalette();
static void DrawPpuFrame();
static void DrawFpsOverlay();

static vgd::DisplayMode13h Display;
static vgd::Keyboard KeyboardInput;
static vgd::FontArcade01 g_fpsFont;

static bool g_displayInitialized = false;
static bool g_keyboardInitialized = false;

static uint8 g_replay_turbo = true;


static uint32 g_minPaletteUpdateIntervalMs = 2000;
static uint64_t g_lastPaletteUpdateTime = 0;
static bool g_paletteMustUpdate = true;

static uint32 GetKeyboardModifiers()
{
    uint32 modifiers = 0;

    if (g_keyDown[vgd::Keyboard::KeyLeftAlt] ||
        g_keyDown[vgd::Keyboard::KeyRightAlt])
        modifiers |= KeyMod_Alt;

    if (g_keyDown[vgd::Keyboard::KeyLeftShift] ||
        g_keyDown[vgd::Keyboard::KeyRightShift])
        modifiers |= KeyMod_Shift;

    if (g_keyDown[vgd::Keyboard::KeyLeftCtrl] ||
        g_keyDown[vgd::Keyboard::KeyRightCtrl])
        modifiers |= KeyMod_Ctrl;

    return modifiers;
}

static void HandleCommand(uint32 command, bool pressed)
{
    // Controller buttons.
    if (command >= kKeys_Controls &&
        command <= kKeys_Controls_Last)
    {
        uint32 index = command;

        if (index < sizeof(kKbdRemap) / sizeof(kKbdRemap[0]))
        {
            uint32 mask = 1u << kKbdRemap[index];

            if (pressed)
                g_input1_state |= mask;
            else
                g_input1_state &= ~mask;
        }

        return;
    }

    // Turbo is a held key.
    if (command == kKeys_Turbo)
    {   
        g_turbo = pressed;
        return;
    }

    // The remaining commands are triggered on key-down.
    if (!pressed)
        return;

    // Load state.
    if (command >= kKeys_Load &&
        command <= kKeys_Load_Last)
    {
        SaveLoadSlot(kSaveLoad_Load, command - kKeys_Load);
        return;
    }

    // Save state.
    if (command >= kKeys_Save &&
        command <= kKeys_Save_Last)
    {
        SaveLoadSlot(kSaveLoad_Save, command - kKeys_Save);
        return;
    }
}

static void HandleKeyboardEvent(const vgd::Keyboard::Event& event)
{
    if (event.key <= vgd::Keyboard::KeyUnknown ||
        event.key >= vgd::Keyboard::KeyCount)
        return;

    bool pressed = event.type == vgd::Keyboard::EventKeyDown;

    // Update our event-local key state before resolving modifiers.
    // This avoids using the final keyboard state for earlier events
    // in the same update batch.
    g_keyDown[event.key] = pressed;

    uint32 modifiers = GetKeyboardModifiers();
    int command = FindCmdForVgdKey(event.key, modifiers);

    if (command != 0)
        HandleCommand((uint32)command, pressed);
}

static void UpdateInput(bool& running)
{
    KeyboardInput.Update();

    if (!KeyboardInput.IsConsoleActive())
    {
        // Avoid stuck buttons if this virtual terminal loses focus.
        memset(g_keyDown, 0, sizeof(g_keyDown));
        g_input1_state = 0;
        g_turbo = false;
        return;
    }

    while (KeyboardInput.HasEvent())
    {
        vgd::Keyboard::Event event = KeyboardInput.ReadEvent();

        if (event.type != vgd::Keyboard::EventKeyDown &&
            event.type != vgd::Keyboard::EventKeyUp)
            continue;

        if (event.type == vgd::Keyboard::EventKeyDown &&
            event.key == vgd::Keyboard::KeyEscape)
        {
            running = false;
            break;
        }

        HandleKeyboardEvent(event);
    }
}


// ------------------------------------------------------------
// FPS counter
// ------------------------------------------------------------

static uint64_t g_fpsLastUpdate = 0;
static uint32 g_fpsFrameCount = 0;
static uint32 g_fps = 60;

struct FpsPixelWriter
{
    uint8* Framebuffer;
    int Pitch;
    int Width;
    int Height;

    void operator()(int x, int y, uint8 color) const
    {
        if (x >= 0 && x < Width && y >= 0 && y < Height)
            Framebuffer[y * Pitch + x] = color;
    }
};

static void DrawFpsOverlay()
{
    uint64_t now = vgd::Clock::GetTimeNanoseconds();

    if (g_fpsLastUpdate == 0)
        g_fpsLastUpdate = now;

    g_fpsFrameCount++;

    uint64_t elapsed = now - g_fpsLastUpdate;

    if (elapsed >= 1000000000ULL)
    {
        g_fps = (uint32)((uint64_t)g_fpsFrameCount *
                         1000000000ULL / elapsed);

        g_fpsFrameCount = 0;
        g_fpsLastUpdate = now;
    }

    char text[16];
    snprintf(text, sizeof(text), "FPS %u", g_fps);

    FpsPixelWriter writer =
    {
        Display.GetBackBuffer(),
        Display.GetBackBufferPitch(),
        Display.GetWidth(),
        Display.GetHeight()
    };

    vgd::BMPFontDrawText(
        g_fpsFont,
        text,
        4, 4,
        8, 8,
        1,
        255,
        writer.Width,
        writer.Height,
        writer);
}

// ------------------------------------------------------------
// Profiling
// ------------------------------------------------------------

enum ProfileSection
{
    ProfileKeyboard,
    ProfileGame,
    ProfilePalette,
    ProfilePpu,
    ProfileConversion,
    ProfilePresent,
    ProfileWait,
    ProfileSectionCount
};

struct ProfileTimer
{
    uint64_t Total;
    uint64_t Minimum;
    uint64_t Maximum;
    uint32 Count;
};

static ProfileTimer g_profileTimers[ProfileSectionCount];
static FILE* g_profileFile = NULL;
static uint32 g_profileFrameCount = 0;

static const char* g_profileNames[ProfileSectionCount] =
{
    "Keyboard",
    "ZeldaRunFrame",
    "UpdateVgaPalette",
    "ZeldaDrawPpuFrame",
    "FramebufferConversion",
    "DisplayPresent",
    "WaitForNextFrame"
};

static uint64_t ProfileBegin()
{
    return vgd::Clock::GetTimeNanoseconds();
}

static void ProfileEnd(ProfileSection section, uint64_t startTime)
{
    uint64_t elapsed = vgd::Clock::GetTimeNanoseconds() - startTime;
    ProfileTimer* timer = &g_profileTimers[section];

    timer->Total += elapsed;

    if (timer->Count == 0 || elapsed < timer->Minimum)
        timer->Minimum = elapsed;

    if (elapsed > timer->Maximum)
        timer->Maximum = elapsed;

    timer->Count++;
}

static void ProfileWrite()
{
    if (!g_profileFile)
        return;

    fprintf(g_profileFile, "\n--- Profile: %u displayed frames ---\n", g_profileFrameCount);
    fprintf(g_profileFile, "Frameskip: %d\n", g_frameskip);

    for (int i = 0; i < ProfileSectionCount; i++)
    {
        ProfileTimer* timer = &g_profileTimers[i];

        if (timer->Count == 0)
            continue;

        double average = (double)timer->Total / timer->Count / 1000000.0;
        double minimum = (double)timer->Minimum / 1000000.0;
        double maximum = (double)timer->Maximum / 1000000.0;

        fprintf(g_profileFile,
            "%-24s calls=%u avg=%8.3f ms min=%8.3f ms max=%8.3f ms\n",
            g_profileNames[i],
            timer->Count,
            average,
            minimum,
            maximum);
    }

    fflush(g_profileFile);

    memset(g_profileTimers, 0, sizeof(g_profileTimers));
    g_profileFrameCount = 0;
}

static void ProfileInitialize()
{
    g_profileFile = fopen("log.txt", "w");

    if (!g_profileFile)
    {
        fprintf(stderr, "Warning: Unable to create log.txt\n");
        return;
    }

    fprintf(g_profileFile, "LVGD486 performance profiling\n");
    fprintf(g_profileFile, "Times are milliseconds; statistics are accumulated over 120 displayed frames.\n");
    fflush(g_profileFile);
}

static void ProfileShutdown()
{
    if (!g_profileFile)
        return;

    ProfileWrite();
    fclose(g_profileFile);
    g_profileFile = NULL;
}

// ------------------------------------------------------------
// Zelda callbacks
// ------------------------------------------------------------

extern "C"
{
    const uint8* g_asset_ptrs[kNumberOfAssets];
    uint32 g_asset_sizes[kNumberOfAssets];

    void ZeldaApuLock()
    {
    }

    void ZeldaApuUnlock()
    {
    }

    MemBlk FindInAssetArray(int asset, int idx)
    {
        MemBlk block = { g_asset_ptrs[asset], g_asset_sizes[asset] };
        return FindIndexInMemblk(block, idx);
    }
}

// ------------------------------------------------------------
// Frame timing
// ------------------------------------------------------------

static void WaitForNextFrame(uint64_t frameStartTime)
{
    const uint64_t frameDuration = 1000000000ULL / 60;

    uint64_t now = vgd::Clock::GetTimeNanoseconds();
    uint64_t elapsed = now - frameStartTime;

    if (elapsed >= frameDuration)
        return;

    uint64_t remaining = frameDuration - elapsed;

    struct timeval timeout;
    timeout.tv_sec = remaining / 1000000000ULL;
    timeout.tv_usec = (remaining % 1000000000ULL) / 1000ULL;

    select(0, NULL, NULL, NULL, &timeout);
}

// ------------------------------------------------------------
// VGA palette
// ------------------------------------------------------------

static void UpdateVgaPalette()
{
    if (PpuUpdatePalette(g_zenv.ppu))
    {
        if (!Display.SetPalette((uint8*)PpuGetVgaPalette()))
            Die("Failed to update VGA palette");
    }
}

// ------------------------------------------------------------
// PPU rendering and display conversion
// ------------------------------------------------------------

static void DrawPpuFrame()
{
    uint8* destination = Display.GetBackBuffer();

    if (!destination)
        Die("Failed to obtain VGA back buffer");

    int displayWidth = Display.GetWidth();
    int displayHeight = Display.GetHeight();
    int displayPitch = Display.GetBackBufferPitch();

    // Direct PPU rendering into the VGA framebuffer.
    uint64_t startTime = ProfileBegin();

    ZeldaDrawPpuFrame(
        destination,
        displayPitch,
        g_ppu_render_flags
    );

    ProfileEnd(ProfilePpu, startTime);
}

// ------------------------------------------------------------
// ROM loading
// ------------------------------------------------------------

static bool LoadRom(const char* filename)
{
    size_t length = 0;
    uint8* file = ReadWholeFile(filename, &length);

    if (!file)
        Die("Failed to read ROM file");

    bool result = EmuInitialize(file, length);
    free(file);

    return result;
}

// ------------------------------------------------------------
// Link graphics
// ------------------------------------------------------------

static bool ParseLinkGraphics(uint8* file, size_t length)
{
    if (length < 27 || memcmp(file, "ZSPR", 4) != 0)
        return false;

    uint32 pixelOffset = DWORD(file[9]);
    uint32 pixelLength = WORD(file[13]);
    uint32 paletteOffset = DWORD(file[15]);
    uint32 paletteLength = WORD(file[19]);

    if ((uint64)pixelOffset + pixelLength > length ||
        (uint64)paletteOffset + paletteLength > length ||
        pixelLength != 0x7000)
        return false;

    if (kPalette_ArmorAndGloves_SIZE != 150 || kLinkGraphics_SIZE != 0x7000)
        Die("ParseLinkGraphics: Invalid asset sizes");

    memcpy(kLinkGraphics, file + pixelOffset, 0x7000);

    if (paletteLength >= 120)
        memcpy(kPalette_ArmorAndGloves, file + paletteOffset, 120);

    if (paletteLength >= 124)
        memcpy(kGlovesColor, file + paletteOffset + 120, 4);

    return true;
}

static void LoadLinkGraphics()
{
    if (!g_config.link_graphics)
        return;

    fprintf(stderr, "Loading Link Graphics: %s\n", g_config.link_graphics);

    size_t length = 0;
    uint8* file = ReadWholeFile(g_config.link_graphics, &length);

    if (!file || !ParseLinkGraphics(file, length))
        Die("Unable to load Link Graphics file");

    free(file);
}

// ------------------------------------------------------------
// Assets
// ------------------------------------------------------------

static void LoadAssets()
{
    size_t length = 0;
    uint8* data = ReadWholeFile("zelda3_assets.dat", &length);

    if (!data)
    {
        size_t bpsLength = 0;
        size_t bpsSourceLength = 0;

        uint8* bps = ReadWholeFile("zelda3_assets.bps", &bpsLength);

        if (!bps)
            Die("Failed to read zelda3_assets.dat. Please see the README for information about how you get this file.");

        uint8* bpsSource = ReadWholeFile("zelda3.sfc", &bpsSourceLength);

        if (!bpsSource)
            Die("Missing file: zelda3.sfc");

        data = ApplyBps(bpsSource, bpsSourceLength, bps, bpsLength, &length);

        if (!data)
            Die("Unable to apply zelda3_assets.bps. Please make sure you got the right version of zelda3.sfc");
    }

    static const unsigned char kAssetsSig[] = { kAssets_Sig };

    if (length < 16 + 32 + 32 + 8 + kNumberOfAssets * 4 ||
        memcmp(data, kAssetsSig, 48) != 0 ||
        *(uint32*)(data + 80) != kNumberOfAssets)
        Die("Invalid assets file");

    uint32 offset = 88 + kNumberOfAssets * 4 + *(uint32*)(data + 84);

    for (size_t i = 0; i < kNumberOfAssets; i++)
    {
        uint32 size = *(uint32*)(data + 88 + i * 4);
        offset = (offset + 3) & ~3;

        if ((uint64)offset + size > length)
            Die("Assets file corruption");

        g_asset_sizes[i] = size;
        g_asset_ptrs[i] = data + offset;
        offset += size;
    }

    if (g_config.features0 & kFeatures0_DimFlashes)
    {
        kPalette_DungBgMain[0x484] = 0x70;
        kPalette_DungBgMain[0x485] = 0x95;
        kPalette_DungBgMain[0x486] = 0x57;
    }
}

// ------------------------------------------------------------
// Working directory
// ------------------------------------------------------------

static void SwitchDirectory()
{
    char buffer[4096];

    if (!getcwd(buffer, sizeof(buffer) - 32))
        return;

    size_t position = strlen(buffer);

    for (int step = 0; position != 0 && step < 3; step++)
    {
        memcpy(buffer + position, "/zelda3.ini", 12);

        FILE* file = fopen(buffer, "rb");

        if (file)
        {
            fclose(file);
            buffer[position] = 0;

            if (step != 0)
            {
                printf("Found zelda3.ini in %s\n", buffer);
                int error = chdir(buffer);
                (void)error;
            }

            return;
        }

        position--;

        while (position != 0 && buffer[position] != '/' && buffer[position] != '\\')
            position--;
    }
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main(int argc, char** argv)
{
    argc--;
    argv++;

    const char* configFile = NULL;

    if (argc >= 2 && strcmp(argv[0], "--config") == 0)
    {
        configFile = argv[1];
        argc -= 2;
        argv += 2;
    }
    else
    {
        SwitchDirectory();
    }

    ParseConfigFile(configFile);
    LoadAssets();
    LoadLinkGraphics();

    ZeldaInitialize();

    // Restore the original PPU configuration.
    g_zenv.ppu->extraLeftRight =
        UintMin(g_config.extended_aspect_ratio, kPpuExtraLeftRight);

    g_ppu_render_flags =
        g_config.new_renderer * kPpuRenderFlags_NewRenderer |
        g_config.enhanced_mode7 * kPpuRenderFlags_4x4Mode7 |
        g_config.extend_y * kPpuRenderFlags_Height240 |
        g_config.no_sprite_limits * kPpuRenderFlags_NoSpriteLimits;

    ZeldaEnableMsu(g_config.enable_msu);
    ZeldaSetLanguage(g_config.language);

    if (!Display.Initialize())
        Die("Failed to initialize VGA Mode 13h");

    g_displayInitialized = true;

    if (!KeyboardInput.Initialize())
        Die("Failed to initialize keyboard input");

    g_keyboardInitialized = true;

    if (argc >= 1 && !g_run_without_emu)
        LoadRom(argv[0]);

    mkdir("saves", 0755);

    ZeldaReadSram();

    if (g_config.autosave)
        SaveLoadSlot(kSaveLoad_Load, 0);

    ProfileInitialize();

    fprintf(stderr, "Frameskip: %d (render every %d frames)\n",
        g_frameskip, g_frameskip + 1);

    bool running = true;
    uint32 frameCounter = 0;

    while (running)
    {
        uint64_t frameStartTime = vgd::Clock::GetTimeNanoseconds();

        // ----------------------------------------------------
        // Input
        // ----------------------------------------------------

        uint64_t startTime = ProfileBegin();

        UpdateInput(running);

        ProfileEnd(ProfileKeyboard, startTime);

        if (!running)
            break;

        // ----------------------------------------------------
        // Game simulation
        // ----------------------------------------------------

        startTime = ProfileBegin();

        bool isReplay = ZeldaRunFrame(g_input1_state);

        ProfileEnd(ProfileGame, startTime);

        frameCounter++;

        // ----------------------------------------------------
        // Replay turbo
        // ----------------------------------------------------

        if (isReplay && g_replay_turbo &&
            (frameCounter & 0x7f) != 0)
        {
            continue;
        }

        // ----------------------------------------------------
        // Frame skipping and rendering
        // ----------------------------------------------------

        if (ShouldRenderFrame())
        {
            startTime = ProfileBegin();
            UpdateVgaPalette();
            ProfileEnd(ProfilePalette, startTime);

            DrawPpuFrame();
        }

        // Draw FPS overlay after conversion so the game cannot overwrite it.
        DrawFpsOverlay();

        // ----------------------------------------------------
        // VGA presentation
        // ----------------------------------------------------

        startTime = ProfileBegin();

        if (!Display.Present())
            Die("VGA frame presentation failed");

        ProfileEnd(ProfilePresent, startTime);

        g_profileFrameCount++;

        if (g_profileFrameCount >= 120)
            ProfileWrite();

        // ----------------------------------------------------
        // Frame timing
        // ----------------------------------------------------
        g_turbo_all_the_time = g_fps < 30 ;
        if (!g_turbo && !g_turbo_all_the_time)
        {
            startTime = ProfileBegin();
            WaitForNextFrame(frameStartTime);
            ProfileEnd(ProfileWait, startTime);
        }
    }

    // --------------------------------------------------------
    // Shutdown
    // --------------------------------------------------------

    if (g_config.autosave)
        SaveLoadSlot(kSaveLoad_Save, 0);

    ZeldaWriteSram();

    ProfileShutdown();

    if (g_keyboardInitialized)
    {
        KeyboardInput.Shutdown();
        g_keyboardInitialized = false;
    }

    if (g_displayInitialized)
    {
        Display.Shutdown();
        g_displayInitialized = false;
    }

    return 0;
}