
#include "config.h"
#include "types.h"
#include "config_keyboard.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zelda_features.h"
#include "util.h"

namespace
{

struct KeyNameId
{
    const char *name;
    uint16 id;
    uint16 size;
};

struct KeyMapHashEnt
{
    uint16 key;
    uint16 cmd;
    uint16 next;
};

struct GamepadMapEnt
{
    uint32 modifiers;
    uint16 cmd;
    uint16 next;
};

struct VgdKeyName
{
    const char *name;
    vgd::Keyboard::Key key;
};

static const uint16 kDefaultKbdControls[kKeys_Total] =
{
    0,

    // Controls
    vgd::Keyboard::KeyUp,
    vgd::Keyboard::KeyDown,
    vgd::Keyboard::KeyLeft,
    vgd::Keyboard::KeyRight,
    vgd::Keyboard::KeyRightShift,
    vgd::Keyboard::KeyEnter,
    vgd::Keyboard::KeyX,
    vgd::Keyboard::KeyZ,
    vgd::Keyboard::KeyS,
    vgd::Keyboard::KeyA,
    vgd::Keyboard::KeyC,
    vgd::Keyboard::KeyV,

    // Load State
    vgd::Keyboard::KeyF1,
    vgd::Keyboard::KeyF2,
    vgd::Keyboard::KeyF3,
    vgd::Keyboard::KeyF4,
    vgd::Keyboard::KeyF5,
    vgd::Keyboard::KeyF6,
    vgd::Keyboard::KeyF7,
    vgd::Keyboard::KeyF8,
    vgd::Keyboard::KeyF9,
    vgd::Keyboard::KeyF10,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    // Save State
    vgd::Keyboard::KeyF1 | KeyMod_Shift,
    vgd::Keyboard::KeyF2 | KeyMod_Shift,
    vgd::Keyboard::KeyF3 | KeyMod_Shift,
    vgd::Keyboard::KeyF4 | KeyMod_Shift,
    vgd::Keyboard::KeyF5 | KeyMod_Shift,
    vgd::Keyboard::KeyF6 | KeyMod_Shift,
    vgd::Keyboard::KeyF7 | KeyMod_Shift,
    vgd::Keyboard::KeyF8 | KeyMod_Shift,
    vgd::Keyboard::KeyF9 | KeyMod_Shift,
    vgd::Keyboard::KeyF10 | KeyMod_Shift,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    // Replay State
    vgd::Keyboard::KeyF1 | KeyMod_Ctrl,
    vgd::Keyboard::KeyF2 | KeyMod_Ctrl,
    vgd::Keyboard::KeyF3 | KeyMod_Ctrl,
    vgd::Keyboard::KeyF4 | KeyMod_Ctrl,
    vgd::Keyboard::KeyF5 | KeyMod_Ctrl,
    vgd::Keyboard::KeyF6 | KeyMod_Ctrl,
    vgd::Keyboard::KeyF7 | KeyMod_Ctrl,
    vgd::Keyboard::KeyF8 | KeyMod_Ctrl,
    vgd::Keyboard::KeyF9 | KeyMod_Ctrl,
    vgd::Keyboard::KeyF10 | KeyMod_Ctrl,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    // Load Reference State
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    // Replay Reference State
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    // CheatLife, CheatKeys, CheatEquipment, CheatWalkThroughWalls
    vgd::Keyboard::KeyW,
    vgd::Keyboard::KeyO,
    vgd::Keyboard::KeyW | KeyMod_Shift,
    vgd::Keyboard::KeyE | KeyMod_Ctrl,

    // ClearKeyLog, StopReplay, Fullscreen, Reset, Pause, PauseDimmed,
    // Turbo, ReplayTurbo, WindowBigger, WindowSmaller, DisplayPerf, ToggleRenderer
    vgd::Keyboard::KeyK,
    vgd::Keyboard::KeyL,
    vgd::Keyboard::KeyEnter | KeyMod_Alt,
    vgd::Keyboard::KeyR | KeyMod_Ctrl,
    vgd::Keyboard::KeyP | KeyMod_Shift,
    vgd::Keyboard::KeyP,
    vgd::Keyboard::KeyTab,
    vgd::Keyboard::KeyT,
    0,
    0,
    vgd::Keyboard::KeyF,
    vgd::Keyboard::KeyR
};

#define M(n) {#n, kKeys_##n, kKeys_##n##_Last - kKeys_##n + 1}
#define S(n) {#n, kKeys_##n, 1}

static const KeyNameId kKeyNameId[] =
{
    {"Null", kKeys_Null, 65535},
    M(Controls), M(Load), M(Save), M(Replay), M(LoadRef), M(ReplayRef),
    S(CheatLife), S(CheatKeys), S(CheatEquipment), S(CheatWalkThroughWalls),
    S(ClearKeyLog), S(StopReplay), S(Fullscreen), S(Reset),
    S(Pause), S(PauseDimmed), S(Turbo), S(ReplayTurbo),
    S(WindowBigger), S(WindowSmaller), S(VolumeUp), S(VolumeDown),
    S(DisplayPerf), S(ToggleRenderer)
};

#undef M
#undef S

static const VgdKeyName kVgdKeyNames[] =
{
    {"Escape", vgd::Keyboard::KeyEscape},

    {"1", vgd::Keyboard::Key1},
    {"2", vgd::Keyboard::Key2},
    {"3", vgd::Keyboard::Key3},
    {"4", vgd::Keyboard::Key4},
    {"5", vgd::Keyboard::Key5},
    {"6", vgd::Keyboard::Key6},
    {"7", vgd::Keyboard::Key7},
    {"8", vgd::Keyboard::Key8},
    {"9", vgd::Keyboard::Key9},
    {"0", vgd::Keyboard::Key0},

    {"Minus", vgd::Keyboard::KeyMinus},
    {"Equals", vgd::Keyboard::KeyEquals},
    {"Backspace", vgd::Keyboard::KeyBackspace},
    {"Tab", vgd::Keyboard::KeyTab},

    {"Q", vgd::Keyboard::KeyQ},
    {"W", vgd::Keyboard::KeyW},
    {"E", vgd::Keyboard::KeyE},
    {"R", vgd::Keyboard::KeyR},
    {"T", vgd::Keyboard::KeyT},
    {"Y", vgd::Keyboard::KeyY},
    {"U", vgd::Keyboard::KeyU},
    {"I", vgd::Keyboard::KeyI},
    {"O", vgd::Keyboard::KeyO},
    {"P", vgd::Keyboard::KeyP},

    {"LeftBracket", vgd::Keyboard::KeyLeftBracket},
    {"RightBracket", vgd::Keyboard::KeyRightBracket},
    {"Enter", vgd::Keyboard::KeyEnter},
    {"Return", vgd::Keyboard::KeyEnter},
    {"LeftCtrl", vgd::Keyboard::KeyLeftCtrl},
    {"RightCtrl", vgd::Keyboard::KeyRightCtrl},

    {"A", vgd::Keyboard::KeyA},
    {"S", vgd::Keyboard::KeyS},
    {"D", vgd::Keyboard::KeyD},
    {"F", vgd::Keyboard::KeyF},
    {"G", vgd::Keyboard::KeyG},
    {"H", vgd::Keyboard::KeyH},
    {"J", vgd::Keyboard::KeyJ},
    {"K", vgd::Keyboard::KeyK},
    {"L", vgd::Keyboard::KeyL},

    {"Semicolon", vgd::Keyboard::KeySemicolon},
    {"Apostrophe", vgd::Keyboard::KeyApostrophe},
    {"Grave", vgd::Keyboard::KeyGrave},
    {"LeftShift", vgd::Keyboard::KeyLeftShift},
    {"RightShift", vgd::Keyboard::KeyRightShift},
    {"Backslash", vgd::Keyboard::KeyBackslash},

    {"Z", vgd::Keyboard::KeyZ},
    {"X", vgd::Keyboard::KeyX},
    {"C", vgd::Keyboard::KeyC},
    {"V", vgd::Keyboard::KeyV},
    {"B", vgd::Keyboard::KeyB},
    {"N", vgd::Keyboard::KeyN},
    {"M", vgd::Keyboard::KeyM},

    {"Comma", vgd::Keyboard::KeyComma},
    {"Period", vgd::Keyboard::KeyPeriod},
    {"Slash", vgd::Keyboard::KeySlash},
    {"KeypadMultiply", vgd::Keyboard::KeyKeypadMultiply},
    {"LeftAlt", vgd::Keyboard::KeyLeftAlt},
    {"RightAlt", vgd::Keyboard::KeyRightAlt},
    {"Space", vgd::Keyboard::KeySpace},
    {"CapsLock", vgd::Keyboard::KeyCapsLock},

    {"F1", vgd::Keyboard::KeyF1},
    {"F2", vgd::Keyboard::KeyF2},
    {"F3", vgd::Keyboard::KeyF3},
    {"F4", vgd::Keyboard::KeyF4},
    {"F5", vgd::Keyboard::KeyF5},
    {"F6", vgd::Keyboard::KeyF6},
    {"F7", vgd::Keyboard::KeyF7},
    {"F8", vgd::Keyboard::KeyF8},
    {"F9", vgd::Keyboard::KeyF9},
    {"F10", vgd::Keyboard::KeyF10},
    {"F11", vgd::Keyboard::KeyF11},
    {"F12", vgd::Keyboard::KeyF12},

    {"NumLock", vgd::Keyboard::KeyNumLock},
    {"ScrollLock", vgd::Keyboard::KeyScrollLock},

    {"Keypad7", vgd::Keyboard::KeyKeypad7},
    {"Keypad8", vgd::Keyboard::KeyKeypad8},
    {"Keypad9", vgd::Keyboard::KeyKeypad9},
    {"KeypadMinus", vgd::Keyboard::KeyKeypadMinus},
    {"Keypad4", vgd::Keyboard::KeyKeypad4},
    {"Keypad5", vgd::Keyboard::KeyKeypad5},
    {"Keypad6", vgd::Keyboard::KeyKeypad6},
    {"KeypadPlus", vgd::Keyboard::KeyKeypadPlus},
    {"Keypad1", vgd::Keyboard::KeyKeypad1},
    {"Keypad2", vgd::Keyboard::KeyKeypad2},
    {"Keypad3", vgd::Keyboard::KeyKeypad3},
    {"Keypad0", vgd::Keyboard::KeyKeypad0},
    {"KeypadPeriod", vgd::Keyboard::KeyKeypadPeriod},

    {"Insert", vgd::Keyboard::KeyInsert},
    {"Delete", vgd::Keyboard::KeyDelete},
    {"Home", vgd::Keyboard::KeyHome},
    {"End", vgd::Keyboard::KeyEnd},
    {"PageUp", vgd::Keyboard::KeyPageUp},
    {"PageDown", vgd::Keyboard::KeyPageDown},
    {"Up", vgd::Keyboard::KeyUp},
    {"Down", vgd::Keyboard::KeyDown},
    {"Left", vgd::Keyboard::KeyLeft},
    {"Right", vgd::Keyboard::KeyRight}
};

static uint16 keymap_hash_first[255];
static KeyMapHashEnt *keymap_hash = NULL;
static int keymap_hash_size = 0;
static bool has_keynameid[countof(kKeyNameId)];

static uint16 joymap_first[kGamepadBtn_Count];
static GamepadMapEnt *joymap_ents = NULL;
static int joymap_size = 0;
static bool has_joypad_controls = false;

static bool KeyMapHash_Add(uint16 key, uint16 cmd)
{
    if ((keymap_hash_size & 0xff) == 0)
    {
        if (keymap_hash_size > 10000)
            Die("Too many keys");

        KeyMapHashEnt *newEntries = static_cast<KeyMapHashEnt *>(realloc(keymap_hash, sizeof(KeyMapHashEnt) * (keymap_hash_size + 256)));
        if (!newEntries)
            Die("realloc failure");

        keymap_hash = newEntries;
    }

    int i = keymap_hash_size++;
    KeyMapHashEnt *ent = &keymap_hash[i];

    ent->key = key;
    ent->cmd = cmd;
    ent->next = 0;

    uint16 *cur = &keymap_hash_first[(uint32)key % 255];

    while (*cur)
    {
        KeyMapHashEnt *existing = &keymap_hash[*cur - 1];

        if (existing->key == key)
            return false;

        cur = &existing->next;
    }

    *cur = i + 1;
    return true;
}

static int KeyMapHash_Find(uint16 key)
{
    int i = keymap_hash_first[key % 255];

    while (i)
    {
        KeyMapHashEnt *ent = &keymap_hash[i - 1];

        if (ent->key == key)
            return ent->cmd;

        i = ent->next;
    }

    return 0;
}

static vgd::Keyboard::Key ParseVgdKeyName(const char *name)
{
    for (size_t i = 0; i < countof(kVgdKeyNames); i++)
    {
        if (StringEqualsNoCase(name, kVgdKeyNames[i].name))
            return kVgdKeyNames[i].key;
    }

    return vgd::Keyboard::KeyUnknown;
}

static void ParseKeyArray(char *value, int cmd, int size)
{
    char *s;
    int i = 0;

    for (; i < size && (s = NextDelim(&value, ',')) != NULL; i++, cmd += (cmd != 0))
    {
        if (*s == 0)
            continue;

        uint32 keyWithMod = 0;

        for (;;)
        {
            if (StringStartsWithNoCase(s, "Shift+"))
            {
                keyWithMod |= KeyMod_Shift;
                s += 6;
            }
            else if (StringStartsWithNoCase(s, "Ctrl+"))
            {
                keyWithMod |= KeyMod_Ctrl;
                s += 5;
            }
            else if (StringStartsWithNoCase(s, "Alt+"))
            {
                keyWithMod |= KeyMod_Alt;
                s += 4;
            }
            else
            {
                break;
            }
        }

        vgd::Keyboard::Key key = ParseVgdKeyName(s);

        if (key == vgd::Keyboard::KeyUnknown)
        {
            fprintf(stderr, "Unknown key: '%s'\n", s);
            continue;
        }

        uint16 mappedKey = static_cast<uint16>(keyWithMod | key);

        if (!KeyMapHash_Add(mappedKey, cmd))
            fprintf(stderr, "Duplicate key: '%s'\n", s);
    }
}

static int CountBits32(uint32 n)
{
    int count = 0;

    for (; n != 0; count++)
        n &= (n - 1);

    return count;
}

static void GamepadMap_Add(int button, uint32 modifiers, uint16 cmd)
{
    if (button < 0 || button >= kGamepadBtn_Count)
        return;

    if ((joymap_size & 0xff) == 0)
    {
        if (joymap_size > 1000)
            Die("Too many joypad keys");

        GamepadMapEnt *newEntries = static_cast<GamepadMapEnt *>(realloc(joymap_ents, sizeof(GamepadMapEnt) * (joymap_size + 64)));

        if (!newEntries)
            Die("realloc failure");

        joymap_ents = newEntries;
    }

    uint16 *p = &joymap_first[button];
    int cb = CountBits32(modifiers);

    while (*p && cb < CountBits32(joymap_ents[*p - 1].modifiers))
        p = &joymap_ents[*p - 1].next;

    int i = joymap_size++;
    GamepadMapEnt *ent = &joymap_ents[i];

    ent->modifiers = modifiers;
    ent->cmd = cmd;
    ent->next = *p;
    *p = i + 1;
}

static int ParseGamepadButtonName(const char **value)
{
    const char *s = *value;

    static const char *const kGamepadKeyNames[] =
    {
        "Back", "Guide", "Start", "L3", "R3",
        "L1", "R1", "DpadUp", "DpadDown", "DpadLeft", "DpadRight",
        "L2", "R2", "Lb", "Rb", "A", "B", "X", "Y"
    };

    static const uint8 kGamepadKeyIds[] =
    {
        kGamepadBtn_Back, kGamepadBtn_Guide, kGamepadBtn_Start,
        kGamepadBtn_L3, kGamepadBtn_R3, kGamepadBtn_L1, kGamepadBtn_R1,
        kGamepadBtn_DpadUp, kGamepadBtn_DpadDown,
        kGamepadBtn_DpadLeft, kGamepadBtn_DpadRight,
        kGamepadBtn_L2, kGamepadBtn_R2,
        kGamepadBtn_L1, kGamepadBtn_R1,
        kGamepadBtn_A, kGamepadBtn_B, kGamepadBtn_X, kGamepadBtn_Y
    };

    for (size_t i = 0; i != countof(kGamepadKeyNames); i++)
    {
        const char *r = StringStartsWithNoCase(s, kGamepadKeyNames[i]);

        if (r)
        {
            *value = r;
            return kGamepadKeyIds[i];
        }
    }

    return kGamepadBtn_Invalid;
}

static const uint8 kDefaultGamepadCmds[] =
{
    kGamepadBtn_DpadUp, kGamepadBtn_DpadDown,
    kGamepadBtn_DpadLeft, kGamepadBtn_DpadRight,
    kGamepadBtn_Back, kGamepadBtn_Start,
    kGamepadBtn_B, kGamepadBtn_A,
    kGamepadBtn_Y, kGamepadBtn_X,
    kGamepadBtn_L1, kGamepadBtn_R1
};

static void ParseGamepadArray(char *value, int cmd, int size)
{
    char *s;
    int i = 0;

    for (; i < size && (s = NextDelim(&value, ',')) != NULL; i++, cmd += (cmd != 0))
    {
        if (*s == 0)
            continue;

        uint32 modifiers = 0;
        const char *ss = s;

        for (;;)
        {
            int button = ParseGamepadButtonName(&ss);

            if (button == kGamepadBtn_Invalid)
            {
                fprintf(stderr, "Unknown gamepad button: '%s'\n", s);
                break;
            }

            while (*ss == ' ' || *ss == '\t')
                ss++;

            if (*ss == '+')
            {
                ss++;
                modifiers |= 1 << button;
            }
            else if (*ss == 0)
            {
                GamepadMap_Add(button, modifiers, cmd);
                break;
            }
            else
            {
                fprintf(stderr, "Unknown gamepad button: '%s'\n", s);
                break;
            }
        }
    }
}

static void RegisterDefaultKeys()
{
    for (int i = 1; i < countof(kKeyNameId); i++)
    {
        if (!has_keynameid[i])
        {
            int size = kKeyNameId[i].size;
            int k = kKeyNameId[i].id;

            for (int j = 0; j < size; j++, k++)
                KeyMapHash_Add(kDefaultKbdControls[k], k);
        }
    }

    if (!has_joypad_controls)
    {
        for (int i = 0; i < countof(kDefaultGamepadCmds); i++)
            GamepadMap_Add(kDefaultGamepadCmds[i], 0, kKeys_Controls + i);
    }
}

static int GetIniSection(const char *s)
{
    if (StringEqualsNoCase(s, "[KeyMap]")) return 0;
    if (StringEqualsNoCase(s, "[Graphics]")) return 1;
    if (StringEqualsNoCase(s, "[Sound]")) return 2;
    if (StringEqualsNoCase(s, "[General]")) return 3;
    if (StringEqualsNoCase(s, "[Features]")) return 4;
    if (StringEqualsNoCase(s, "[GamepadMap]")) return 5;

    return -1;
}

static bool ParseBool(const char *value, bool *result)
{
    bool rv = false;

    switch (*value++ | 32)
    {
        case '0':
            if (*value == 0) break;
            return false;

        case 'f':
            if (StringEqualsNoCase(value, "alse")) break;
            return false;

        case 'n':
            if (StringEqualsNoCase(value, "o")) break;
            return false;

        case 'o':
            rv = (*value | 32) == 'n';
            if (StringEqualsNoCase(value, rv ? "n" : "ff")) break;
            return false;

        case '1':
            rv = true;
            if (*value == 0) break;
            return false;

        case 'y':
            rv = true;
            if (StringEqualsNoCase(value, "es")) break;
            return false;

        case 't':
            rv = true;
            if (StringEqualsNoCase(value, "rue")) break;
            return false;

        default:
            return false;
    }

    if (result)
    {
        *result = rv;
        return true;
    }

    return rv;
}

static bool ParseBoolBit(const char *value, uint32 *data, uint32 mask)
{
    bool tmp;

    if (!ParseBool(value, &tmp))
        return false;

    *data = (*data & ~mask) | (tmp ? mask : 0);
    return true;
}

static bool HandleIniConfig(int section, const char *key, char *value)
{
    //fprintf(stderr, "%d:%s:%s\n", section, key, value);
    if (section == 0)
    {
        for (int i = 0; i < countof(kKeyNameId); i++)
        {
            if (StringEqualsNoCase(key, kKeyNameId[i].name))
            {
                has_keynameid[i] = true;
                ParseKeyArray(value, kKeyNameId[i].id, kKeyNameId[i].size);
                return true;
            }
        }
    }
    else if (section == 5)
    {
        for (int i = 0; i < countof(kKeyNameId); i++)
        {
            if (StringEqualsNoCase(key, kKeyNameId[i].name))
            {
                if (i == 1)
                    has_joypad_controls = true;

                ParseGamepadArray(value, kKeyNameId[i].id, kKeyNameId[i].size);
                return true;
            }
        }
    }
    else if (section == 1)
    {
        if (StringEqualsNoCase(key, "WindowSize"))
        {
            char *s;

            if (StringEqualsNoCase(value, "Auto"))
            {
                g_config.window_width = 0;
                g_config.window_height = 0;
                return true;
            }

            while ((s = NextDelim(&value, 'x')) != NULL)
            {
                if (g_config.window_width == 0)
                    g_config.window_width = atoi(s);
                else
                {
                    g_config.window_height = atoi(s);
                    return true;
                }
            }
        }
        else if (StringEqualsNoCase(key, "EnhancedMode7"))
            return ParseBool(value, &g_config.enhanced_mode7);
        else if (StringEqualsNoCase(key, "NewRenderer"))
            return ParseBool(value, &g_config.new_renderer);
        else if (StringEqualsNoCase(key, "IgnoreAspectRatio"))
            return ParseBool(value, &g_config.ignore_aspect_ratio);
        else if (StringEqualsNoCase(key, "Fullscreen"))
        {
            g_config.fullscreen = (uint8)strtol(value, NULL, 10);
            return true;
        }
        else if (StringEqualsNoCase(key, "WindowScale"))
        {
            g_config.window_scale = (uint8)strtol(value, NULL, 10);
            return true;
        }
        else if (StringEqualsNoCase(key, "OutputMethod"))
        {
            if (StringEqualsNoCase(value, "VGD") || StringEqualsNoCase(value, "Auto"))
                g_config.output_method = 0;
            else
            {
                char *end = NULL;
                long method = strtol(value, &end, 10);

                if (end == value || *end != 0 || method < 0 || method > 255)
                    return false;

                g_config.output_method = (uint8)method;
            }

            return true;
        }
        else if (StringEqualsNoCase(key, "LinearFiltering"))
            return ParseBool(value, &g_config.linear_filtering);
        else if (StringEqualsNoCase(key, "NoSpriteLimits"))
            return ParseBool(value, &g_config.no_sprite_limits);
        else if (StringEqualsNoCase(key, "LinkGraphics"))
        {
            g_config.link_graphics = value;
            return true;
        }
        else if (StringEqualsNoCase(key, "Shader"))
        {
            g_config.shader = *value ? value : NULL;
            return true;
        }
        else if (StringEqualsNoCase(key, "DimFlashes"))
            return ParseBoolBit(value, &g_config.features0, kFeatures0_DimFlashes);
    }
    else if (section == 2)
    {
        if (StringEqualsNoCase(key, "EnableAudio"))
            return ParseBool(value, &g_config.enable_audio);
        else if (StringEqualsNoCase(key, "AudioFreq"))
        {
            g_config.audio_freq = (uint16)strtol(value, NULL, 10);
            return true;
        }
        else if (StringEqualsNoCase(key, "AudioChannels"))
        {
            g_config.audio_channels = (uint8)strtol(value, NULL, 10);
            return true;
        }
        else if (StringEqualsNoCase(key, "AudioSamples"))
        {
            g_config.audio_samples = (uint16)strtol(value, NULL, 10);
            return true;
        }
        else if (StringEqualsNoCase(key, "EnableMSU"))
        {
            if (StringEqualsNoCase(value, "opuz"))
                g_config.enable_msu = kMsuEnabled_Opuz;
            else if (StringEqualsNoCase(value, "deluxe"))
                g_config.enable_msu = kMsuEnabled_MsuDeluxe;
            else if (StringEqualsNoCase(value, "deluxe-opuz"))
                g_config.enable_msu = kMsuEnabled_MsuDeluxe | kMsuEnabled_Opuz;
            else
                return ParseBool(value, (bool *)&g_config.enable_msu);

            return true;
        }
        else if (StringEqualsNoCase(key, "MSUPath"))
        {
            g_config.msu_path = value;
            return true;
        }
        else if (StringEqualsNoCase(key, "MSUVolume"))
        {
            g_config.msuvolume = atoi(value);
            return true;
        }
        else if (StringEqualsNoCase(key, "ResumeMSU"))
            return ParseBool(value, &g_config.resume_msu);
    }
    else if (section == 3)
    {
        if (StringEqualsNoCase(key, "Autosave"))
        {
            g_config.autosave = (bool)strtol(value, NULL, 10);
            return true;
        }
        else if (StringEqualsNoCase(key, "ExtendedAspectRatio"))
        {
            const char *s;
            int h = 224;
            bool nospr = false;
            bool novis = false;

            while ((s = NextDelim(&value, ',')) != NULL)
            {
                if (strcmp(s, "extend_y") == 0)
                {
                    h = 240;
                    g_config.extend_y = true;
                }
                else if (strcmp(s, "16:9") == 0)
                    g_config.extended_aspect_ratio = (h * 16 / 9 - 256) / 2;
                else if (strcmp(s, "16:10") == 0)
                    g_config.extended_aspect_ratio = (h * 16 / 10 - 256) / 2;
                else if (strcmp(s, "18:9") == 0)
                    g_config.extended_aspect_ratio = (h * 18 / 9 - 256) / 2;
                else if (strcmp(s, "4:3") == 0)
                    g_config.extended_aspect_ratio = 0;
                else if (strcmp(s, "unchanged_sprites") == 0)
                    nospr = true;
                else if (strcmp(s, "no_visual_fixes") == 0)
                    novis = true;
                else
                    return false;
            }

            if (g_config.extended_aspect_ratio && !nospr)
                g_config.features0 |= kFeatures0_ExtendScreen64;

            if (g_config.extended_aspect_ratio && !novis)
                g_config.features0 |= kFeatures0_WidescreenVisualFixes;

            return true;
        }
        else if (StringEqualsNoCase(key, "DisplayPerfInTitle"))
            return ParseBool(value, &g_config.display_perf_title);
        else if (StringEqualsNoCase(key, "DisableFrameDelay"))
            return ParseBool(value, &g_config.disable_frame_delay);
        else if (StringEqualsNoCase(key, "Language"))
        {
            g_config.language = value;
            return true;
        }
    }
    else if (section == 4)
    {
        if (StringEqualsNoCase(key, "ItemSwitchLR"))
            return ParseBoolBit(value, &g_config.features0, kFeatures0_SwitchLR);
        else if (StringEqualsNoCase(key, "ItemSwitchLRLimit"))
            return ParseBoolBit(value, &g_config.features0, kFeatures0_SwitchLRLimit);
        else if (StringEqualsNoCase(key, "TurnWhileDashing"))
            return ParseBoolBit(value, &g_config.features0, kFeatures0_TurnWhileDashing);
        else if (StringEqualsNoCase(key, "MirrorToDarkworld"))
            return ParseBoolBit(value, &g_config.features0, kFeatures0_MirrorToDarkworld);
        else if (StringEqualsNoCase(key, "CollectItemsWithSword"))
            return ParseBoolBit(value, &g_config.features0, kFeatures0_CollectItemsWithSword);
        else if (StringEqualsNoCase(key, "BreakPotsWithSword"))
            return ParseBoolBit(value, &g_config.features0, kFeatures0_BreakPotsWithSword);
        else if (StringEqualsNoCase(key, "DisableLowHealthBeep"))
            return ParseBoolBit(value, &g_config.features0, kFeatures0_DisableLowHealthBeep);
        else if (StringEqualsNoCase(key, "SkipIntroOnKeypress"))
            return ParseBoolBit(value, &g_config.features0, kFeatures0_SkipIntroOnKeypress);
        else if (StringEqualsNoCase(key, "ShowMaxItemsInYellow"))
            return ParseBoolBit(value, &g_config.features0, kFeatures0_ShowMaxItemsInYellow);
        else if (StringEqualsNoCase(key, "MoreActiveBombs"))
            return ParseBoolBit(value, &g_config.features0, kFeatures0_MoreActiveBombs);
        else if (StringEqualsNoCase(key, "CarryMoreRupees"))
            return ParseBoolBit(value, &g_config.features0, kFeatures0_CarryMoreRupees);
        else if (StringEqualsNoCase(key, "MiscBugFixes"))
            return ParseBoolBit(value, &g_config.features0, kFeatures0_MiscBugFixes);
        else if (StringEqualsNoCase(key, "GameChangingBugFixes"))
            return ParseBoolBit(value, &g_config.features0, kFeatures0_GameChangingBugFixes);
        else if (StringEqualsNoCase(key, "CancelBirdTravel"))
            return ParseBoolBit(value, &g_config.features0, kFeatures0_CancelBirdTravel);
    }

    return false;
}

static bool ParseOneConfigFile(const char *filename, int depth)
{
    char *filedata = (char *)ReadWholeFile(filename, NULL);
    char *p;

    if (!filedata)
        return false;

    int section = -2;
    g_config.memory_buffer = filedata;

    for (int lineno = 1; (p = NextLineStripComments(&filedata)) != NULL; lineno++)
    {
        if (*p == 0)
            continue;

        if (*p == '[')
        {
            section = GetIniSection(p);

            if (section < 0)
                fprintf(stderr, "%s:%d: Invalid .ini section %s\n", filename, lineno, p);
        }
        else if (*p == '!' && SkipPrefix(p + 1, "include "))
        {
            char *tt = p + 8;
            char *new_filename = ReplaceFilenameWithNewPath(filename, NextPossiblyQuotedString(&tt));

            if (depth > 10 || !ParseOneConfigFile(new_filename, depth + 1))
                fprintf(stderr, "Warning: Unable to read %s\n", new_filename);

            free(new_filename);
        }
        else if (section == -2)
        {
            fprintf(stderr, "%s:%d: Expecting [section]\n", filename, lineno);
        }
        else
        {
            char *v = SplitKeyValue(p);

            if (v == NULL)
            {
                fprintf(stderr, "%s:%d: Expecting 'key=value'\n", filename, lineno);
                continue;
            }

            if (section >= 0 && !HandleIniConfig(section, p, v))
                fprintf(stderr, "%s:%d: Can't parse '%s'\n", filename, lineno, p);
        }
    }

    return true;
}

} // namespace

Config g_config;

int FindCmdForVgdKey(vgd::Keyboard::Key key, uint32 modifiers)
{
    if (key <= vgd::Keyboard::KeyUnknown || key >= vgd::Keyboard::KeyCount)
        return 0;

    if (key != vgd::Keyboard::KeyLeftAlt && key != vgd::Keyboard::KeyRightAlt)
        modifiers &= ~KeyMod_Alt;

    if (key != vgd::Keyboard::KeyLeftCtrl && key != vgd::Keyboard::KeyRightCtrl)
        modifiers &= ~KeyMod_Ctrl;

    if (key != vgd::Keyboard::KeyLeftShift && key != vgd::Keyboard::KeyRightShift)
        modifiers &= ~KeyMod_Shift;

    uint16 mappedKey = static_cast<uint16>(key) | static_cast<uint16>(modifiers);

    return KeyMapHash_Find(mappedKey);
}

int FindCmdForGamepadButton(int button, uint32 modifiers)
{
    if (button < 0 || button >= kGamepadBtn_Count)
        return 0;

    GamepadMapEnt *ent;

    for (int e = joymap_first[button]; e != 0; e = ent->next)
    {
        ent = &joymap_ents[e - 1];

        if ((modifiers & ent->modifiers) == ent->modifiers)
            return ent->cmd;
    }

    return 0;
}

void ParseConfigFile(const char *filename)
{
    g_config.msuvolume = 100;

    if (filename != NULL || !ParseOneConfigFile("zelda3.user.ini", 0))
    {
        if (filename == NULL)
            filename = "zelda3.ini";

        if (!ParseOneConfigFile(filename, 0))
            fprintf(stderr, "Warning: Unable to read config file %s\n", filename);
    }

    RegisterDefaultKeys();
}
