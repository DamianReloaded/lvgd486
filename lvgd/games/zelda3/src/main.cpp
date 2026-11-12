
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

static int g_ppu_render_flags = 0;
static int g_snes_width, g_snes_height;

static bool LoadRom(const char* filename);
static void LoadLinkGraphics();
static bool ParseLinkGraphics(uint8* file, size_t length);
static void LoadAssets();
static void SwitchDirectory();
static void UpdateVgaPalette();
static void DrawPpuFrame();

static vgd::DisplayMode13h Display;
static vgd::Keyboard KeyboardInput;

static bool g_displayInitialized = false;
static bool g_keyboardInitialized = false;

static uint8 g_replay_turbo = true;

static uint8* g_ppuBuffer = NULL;
static size_t g_ppuBufferSize = 0;

static uint8 g_vgaPalette[256 * 3];
static uint8 g_colorLookup[4096];
static uint16 g_lastCgram[256];
static uint8 g_lastBrightness = 0;
static bool g_paletteInitialized = false;

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

static void WaitForNextFrame(uint64_t& nextFrameTime)
{
    const uint64_t frameDuration = 1000000000ULL / 60;

    nextFrameTime += frameDuration;

    uint64_t now = vgd::Clock::GetTimeNanoseconds();

    if (nextFrameTime > now)
    {
        uint64_t remaining = nextFrameTime - now;

        struct timeval timeout;
        timeout.tv_sec = remaining / 1000000000ULL;
        timeout.tv_usec = (remaining % 1000000000ULL) / 1000ULL;

        select(0, NULL, NULL, NULL, &timeout);
    }
    else
    {
        nextFrameTime = now;
    }
}

static uint8 ExpandColor(uint8 value, uint8 brightness)
{
    return (uint8)((((value << 3) | (value >> 2)) * brightness) / 15);
}

static void UpdateVgaPalette()
{
    Ppu* ppu = g_zenv.ppu;

    if (g_paletteInitialized &&
        ppu->brightness == g_lastBrightness &&
        memcmp(ppu->cgram, g_lastCgram, sizeof(g_lastCgram)) == 0)
    {
        return;
    }

    for (int i = 0; i < 256; i++)
    {
        uint16 color = ppu->cgram[i];

        uint8 r = ExpandColor(color & 0x1f, ppu->brightness);
        uint8 g = ExpandColor((color >> 5) & 0x1f, ppu->brightness);
        uint8 b = ExpandColor((color >> 10) & 0x1f, ppu->brightness);

        g_vgaPalette[i * 3 + 0] = r;
        g_vgaPalette[i * 3 + 1] = g;
        g_vgaPalette[i * 3 + 2] = b;
    }

    if (!Display.SetPalette(g_vgaPalette))
        Die("Failed to update VGA palette");

    // Build a 4:4:4 RGB lookup table.
    // Each entry contains the nearest color in the SNES CGRAM palette.

    for (int rIndex = 0; rIndex < 16; rIndex++)
    {
        int r = (rIndex << 4) | 8;

        for (int gIndex = 0; gIndex < 16; gIndex++)
        {
            int g = (gIndex << 4) | 8;

            for (int bIndex = 0; bIndex < 16; bIndex++)
            {
                int b = (bIndex << 4) | 8;

                uint32 bestDistance = 0xffffffff;
                uint8 bestIndex = 0;

                for (int i = 0; i < 256; i++)
                {
                    int dr = r - g_vgaPalette[i * 3 + 0];
                    int dg = g - g_vgaPalette[i * 3 + 1];
                    int db = b - g_vgaPalette[i * 3 + 2];

                    uint32 distance =
                        (uint32)(dr * dr + dg * dg + db * db);

                    if (distance < bestDistance)
                    {
                        bestDistance = distance;
                        bestIndex = (uint8)i;

                        if (distance == 0)
                            break;
                    }
                }

                int lookupIndex =
                    (rIndex << 8) |
                    (gIndex << 4) |
                    bIndex;

                g_colorLookup[lookupIndex] = bestIndex;
            }
        }
    }

    memcpy(g_lastCgram, ppu->cgram, sizeof(g_lastCgram));
    g_lastBrightness = ppu->brightness;
    g_paletteInitialized = true;
}

static void DrawPpuFrame()
{
    int renderScale = PpuGetCurrentRenderScale(g_zenv.ppu, g_ppu_render_flags);

    int renderWidth = g_snes_width * renderScale;
    int renderHeight = g_snes_height * renderScale;
    int renderPitch = renderWidth * 4;

    size_t requiredSize = (size_t)renderPitch * renderHeight;

    if (requiredSize > g_ppuBufferSize)
    {
        uint8* newBuffer = (uint8*)realloc(g_ppuBuffer, requiredSize);

        if (!newBuffer)
            Die("Failed to allocate PPU render buffer");

        g_ppuBuffer = newBuffer;
        g_ppuBufferSize = requiredSize;
    }

    ZeldaDrawPpuFrame(g_ppuBuffer, renderPitch, g_ppu_render_flags);

    int displayWidth = Display.GetWidth();
    int displayHeight = Display.GetHeight();
    int displayPitch = Display.GetBackBufferPitch();

    uint8* destination = Display.GetBackBuffer();

    if (!destination)
        Die("Failed to obtain VGA back buffer");

    Display.Clear(0);

    int copyWidth = renderWidth < displayWidth ? renderWidth : displayWidth;
    int copyHeight = renderHeight < displayHeight ? renderHeight : displayHeight;

    int sourceX = (renderWidth - copyWidth) / 2;
    int sourceY = (renderHeight - copyHeight) / 2;

    int destinationX = (displayWidth - copyWidth) / 2;
    int destinationY = (displayHeight - copyHeight) / 2;

    for (int y = 0; y < copyHeight; y++)
    {
        const uint8* source =
            g_ppuBuffer +
            (size_t)(sourceY + y) * renderPitch +
            sourceX * 4;

        uint8* output =
            destination +
            (size_t)(destinationY + y) * displayPitch +
            destinationX;

        for (int x = 0; x < copyWidth; x++)
        {
            // PPU output is 32-bit BGRA in memory.
            uint8 b = source[x * 4 + 0];
            uint8 g = source[x * 4 + 1];
            uint8 r = source[x * 4 + 2];

            int lookupIndex =
                ((r >> 4) << 8) |
                ((g >> 4) << 4) |
                (b >> 4);

            output[x] = g_colorLookup[lookupIndex];
        }
    }

    if (!Display.Present())
        Die("VGA frame presentation failed");
}

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

    g_snes_width = g_config.extended_aspect_ratio * 2 + 256;
    g_snes_height = g_config.extend_y ? 240 : 224;

    g_wanted_zelda_features = g_config.features0;

    uint32 g_ppu_render_flags =
        g_config.new_renderer * kPpuRenderFlags_NewRenderer |
        g_config.enhanced_mode7 * kPpuRenderFlags_4x4Mode7 |
        g_config.extend_y * kPpuRenderFlags_Height240 |
        g_config.no_sprite_limits * kPpuRenderFlags_NoSpriteLimits;

    ZeldaEnableMsu(g_config.enable_msu);
    ZeldaSetLanguage(g_config.language);

    if (!Display.Initialize())
        Die("Failed to initialize VGA Mode 13h");

    g_displayInitialized = true;

    if (!Display.SetFrameRate(60))
        fprintf(stderr, "Warning: Unable to set VGA frame rate limiter\n");

    if (!KeyboardInput.Initialize())
        Die("Failed to initialize keyboard input");

    g_keyboardInitialized = true;

    if (argc >= 1 && !g_run_without_emu)
        LoadRom(argv[0]);

    mkdir("saves", 0755);

    ZeldaReadSram();

    if (g_config.autosave)
        SaveLoadSlot(kSaveLoad_Load, 0);

    
    uint64_t nextFrameTime = vgd::Clock::GetTimeNanoseconds();

    bool running = true;
    uint32 frameCounter = 0;

    while (running)
    {
        KeyboardInput.Update();

        if (KeyboardInput.IsConsoleActive() &&
            KeyboardInput.WasPressed(vgd::Keyboard::KeyEscape))
        {
            running = false;
        }

        if (!running)
            break;

        bool isReplay = ZeldaRunFrame(0);

        frameCounter++;

        if (isReplay && g_replay_turbo &&
            (frameCounter & 0x7f) != 0)
        {
            continue;
        }

        UpdateVgaPalette();

        DrawPpuFrame();

        WaitForNextFrame(nextFrameTime);
    }

    if (g_config.autosave)
        SaveLoadSlot(kSaveLoad_Save, 0);

    ZeldaWriteSram();

    free(g_ppuBuffer);
    g_ppuBuffer = NULL;
    g_ppuBufferSize = 0;

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
