
// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// LVGD486 - LinuxDoom 1.10 VGA video backend
//
// Target: Linux 2.4 / i486
// Video: VGA Mode 13h (320x200, 256 colors)
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#include "DisplayMode13h.hpp"
#include "Keyboard.hpp"
#include "Mouse.hpp"

extern "C"
{
    #include "doomstat.h"
    #include "i_system.h"
    #include "v_video.h"
    #include "m_argv.h"
    #include "d_main.h"
    #include "doomdef.h"
}

typedef vgd::Keyboard KeyboardDevice;
typedef vgd::Mouse MouseDevice;

static vgd::DisplayMode13h Display;
static KeyboardDevice KeyboardInput;
static MouseDevice MouseInput;

static byte* VideoBuffer = NULL;

static boolean GraphicsInitialized = false;
static boolean KeyboardInitialized = false;
static boolean MouseInitialized = false;

static int MouseButtonState = 0;

static int TranslateKey(KeyboardDevice::Key key)
{
    switch (key)
    {
        case KeyboardDevice::KeyEscape:          return KEY_ESCAPE;
        case KeyboardDevice::Key1:               return '1';
        case KeyboardDevice::Key2:               return '2';
        case KeyboardDevice::Key3:               return '3';
        case KeyboardDevice::Key4:               return '4';
        case KeyboardDevice::Key5:               return '5';
        case KeyboardDevice::Key6:               return '6';
        case KeyboardDevice::Key7:               return '7';
        case KeyboardDevice::Key8:               return '8';
        case KeyboardDevice::Key9:               return '9';
        case KeyboardDevice::Key0:               return '0';
        case KeyboardDevice::KeyMinus:           return '-';
        case KeyboardDevice::KeyEquals:          return '=';
        case KeyboardDevice::KeyBackspace:       return KEY_BACKSPACE;
        case KeyboardDevice::KeyTab:             return KEY_TAB;
        case KeyboardDevice::KeyQ:               return 'q';
        case KeyboardDevice::KeyW:               return 'w';
        case KeyboardDevice::KeyE:               return 'e';
        case KeyboardDevice::KeyR:               return 'r';
        case KeyboardDevice::KeyT:               return 't';
        case KeyboardDevice::KeyY:               return 'y';
        case KeyboardDevice::KeyU:               return 'u';
        case KeyboardDevice::KeyI:               return 'i';
        case KeyboardDevice::KeyO:               return 'o';
        case KeyboardDevice::KeyP:               return 'p';
        case KeyboardDevice::KeyLeftBracket:      return '[';
        case KeyboardDevice::KeyRightBracket:     return ']';
        case KeyboardDevice::KeyEnter:            return KEY_ENTER;
        case KeyboardDevice::KeyLeftCtrl:
        case KeyboardDevice::KeyRightCtrl:        return KEY_RCTRL;
        case KeyboardDevice::KeyA:                return 'a';
        case KeyboardDevice::KeyS:                return 's';
        case KeyboardDevice::KeyD:                return 'd';
        case KeyboardDevice::KeyF:                return 'f';
        case KeyboardDevice::KeyG:                return 'g';
        case KeyboardDevice::KeyH:                return 'h';
        case KeyboardDevice::KeyJ:                return 'j';
        case KeyboardDevice::KeyK:                return 'k';
        case KeyboardDevice::KeyL:                return 'l';
        case KeyboardDevice::KeySemicolon:        return ';';
        case KeyboardDevice::KeyApostrophe:       return '\'';
        case KeyboardDevice::KeyGrave:            return '`';
        case KeyboardDevice::KeyLeftShift:
        case KeyboardDevice::KeyRightShift:       return KEY_RSHIFT;
        case KeyboardDevice::KeyBackslash:        return '\\';
        case KeyboardDevice::KeyZ:                return 'z';
        case KeyboardDevice::KeyX:                return 'x';
        case KeyboardDevice::KeyC:                return 'c';
        case KeyboardDevice::KeyV:                return 'v';
        case KeyboardDevice::KeyB:                return 'b';
        case KeyboardDevice::KeyN:                return 'n';
        case KeyboardDevice::KeyM:                return 'm';
        case KeyboardDevice::KeyComma:             return ',';
        case KeyboardDevice::KeyPeriod:            return '.';
        case KeyboardDevice::KeySlash:             return '/';
        case KeyboardDevice::KeyLeftAlt:
        case KeyboardDevice::KeyRightAlt:         return KEY_RALT;
        case KeyboardDevice::KeySpace:             return ' ';
        case KeyboardDevice::KeyF1:                return KEY_F1;
        case KeyboardDevice::KeyF2:                return KEY_F2;
        case KeyboardDevice::KeyF3:                return KEY_F3;
        case KeyboardDevice::KeyF4:                return KEY_F4;
        case KeyboardDevice::KeyF5:                return KEY_F5;
        case KeyboardDevice::KeyF6:                return KEY_F6;
        case KeyboardDevice::KeyF7:                return KEY_F7;
        case KeyboardDevice::KeyF8:                return KEY_F8;
        case KeyboardDevice::KeyF9:                return KEY_F9;
        case KeyboardDevice::KeyF10:               return KEY_F10;
        case KeyboardDevice::KeyF11:               return KEY_F11;
        case KeyboardDevice::KeyF12:               return KEY_F12;
        case KeyboardDevice::KeyKeypad7:           return '7';
        case KeyboardDevice::KeyKeypad8:           return '8';
        case KeyboardDevice::KeyKeypad9:           return '9';
        case KeyboardDevice::KeyKeypadMinus:       return '-';
        case KeyboardDevice::KeyKeypad4:           return '4';
        case KeyboardDevice::KeyKeypad5:           return '5';
        case KeyboardDevice::KeyKeypad6:           return '6';
        case KeyboardDevice::KeyKeypadPlus:        return '+';
        case KeyboardDevice::KeyKeypad1:           return '1';
        case KeyboardDevice::KeyKeypad2:           return '2';
        case KeyboardDevice::KeyKeypad3:           return '3';
        case KeyboardDevice::KeyKeypad0:           return '0';
        case KeyboardDevice::KeyKeypadPeriod:      return '.';
        case KeyboardDevice::KeyKeypadMultiply:    return '*';
        case KeyboardDevice::KeyUp:                return KEY_UPARROW;
        case KeyboardDevice::KeyDown:              return KEY_DOWNARROW;
        case KeyboardDevice::KeyLeft:              return KEY_LEFTARROW;
        case KeyboardDevice::KeyRight:             return KEY_RIGHTARROW;
        case KeyboardDevice::KeyDelete:            return KEY_BACKSPACE;
        default:                                   return 0;
    }
}

static void PostKeyboardEvent(evtype_t type, int key)
{
    event_t event;

    event.type = type;
    event.data1 = key;
    event.data2 = 0;
    event.data3 = 0;

    D_PostEvent(&event);
}

static void ProcessKeyboardEvents(void)
{
    if (!KeyboardInitialized)
        return;

    KeyboardInput.Update();

    if (!KeyboardInput.IsConsoleActive())
        return;

    for (int key = 1; key < KeyboardDevice::KeyCount; ++key)
    {
        KeyboardDevice::Key keyboardKey =
            static_cast<KeyboardDevice::Key>(key);

        int doomKey = TranslateKey(keyboardKey);

        if (!doomKey)
            continue;

        if (KeyboardInput.WasPressed(keyboardKey))
            PostKeyboardEvent(ev_keydown, doomKey);

        if (KeyboardInput.WasReleased(keyboardKey))
            PostKeyboardEvent(ev_keyup, doomKey);
    }
}

static void ProcessMouseEvents(void)
{
    if (!MouseInitialized)
        return;

    MouseInput.Update();

    int deltaX = MouseInput.GetDeltaX();
    int deltaY = MouseInput.GetDeltaY();

    int buttons = 0;

    if (MouseInput.IsDown(MouseDevice::ButtonLeft))
        buttons |= 1;

    if (MouseInput.IsDown(MouseDevice::ButtonRight))
        buttons |= 2;

    if (MouseInput.IsDown(MouseDevice::ButtonMiddle))
        buttons |= 4;

    if (deltaX || deltaY || buttons != MouseButtonState)
    {
        event_t event;

        event.type = ev_mouse;
        event.data1 = buttons;
        event.data2 = deltaX;
        event.data3 = -deltaY;

        D_PostEvent(&event);

        MouseButtonState = buttons;
    }
}

extern "C" void I_ShutdownGraphics(void)
{
    if (MouseInitialized)
    {
        MouseInput.Shutdown();
        MouseInitialized = false;
    }

    if (KeyboardInitialized)
    {
        KeyboardInput.Shutdown();
        KeyboardInitialized = false;
    }

    if (GraphicsInitialized)
    {
        Display.Shutdown();
        GraphicsInitialized = false;
    }

    if (VideoBuffer)
    {
        free(VideoBuffer);
        VideoBuffer = NULL;
    }

    MouseButtonState = 0;
    screens[0] = NULL;
}

extern "C" void I_StartFrame(void)
{
}

extern "C" void I_StartTic(void)
{
    ProcessKeyboardEvents();
    ProcessMouseEvents();
}

extern "C" void I_UpdateNoBlit(void)
{
}

extern "C" void I_FinishUpdate(void)
{
    if (!GraphicsInitialized || !screens[0])
        return;

    unsigned char* destination = Display.GetBackBuffer();
    int pitch = Display.GetBackBufferPitch();

    if (!destination || pitch < SCREENWIDTH)
    {
        I_Error("I_FinishUpdate: Invalid VGA backbuffer");
        return;
    }

    for (int y = 0; y < SCREENHEIGHT; ++y)
    {
        memcpy(destination + y * pitch,
               screens[0] + y * SCREENWIDTH,
               SCREENWIDTH);
    }

    if (!Display.Present())
        I_Error("I_FinishUpdate: VGA presentation failed");
}

extern "C" void I_ReadScreen(byte* scr)
{
    if (screens[0] && scr)
        memcpy(scr, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

extern "C" void I_SetPalette(byte* palette)
{
    byte adjustedPalette[768];

    for (int i = 0; i < 256; ++i)
    {
        adjustedPalette[i * 3 + 0] =
            gammatable[usegamma][palette[i * 3 + 0]];

        adjustedPalette[i * 3 + 1] =
            gammatable[usegamma][palette[i * 3 + 1]];

        adjustedPalette[i * 3 + 2] =
            gammatable[usegamma][palette[i * 3 + 2]];
    }

    Display.SetPalette(adjustedPalette);
}

extern "C" void I_InitGraphics(void)
{
    if (GraphicsInitialized)
        return;

    if (!Display.Initialize())
    {
        I_Error("I_InitGraphics: Unable to initialize VGA Mode 13h");
        return;
    }

    GraphicsInitialized = true;

    if (!KeyboardInput.Initialize())
    {
        Display.Shutdown();
        GraphicsInitialized = false;

        I_Error("I_InitGraphics: Unable to initialize keyboard");
        return;
    }

    KeyboardInitialized = true;

    if (!MouseInput.Initialize())
    {
        KeyboardInput.Shutdown();
        KeyboardInitialized = false;

        Display.Shutdown();
        GraphicsInitialized = false;

        I_Error("I_InitGraphics: Unable to initialize mouse");
        return;
    }

    MouseInitialized = true;

    VideoBuffer = (byte*)malloc(SCREENWIDTH * SCREENHEIGHT);

    if (!VideoBuffer)
    {
        MouseInput.Shutdown();
        MouseInitialized = false;

        KeyboardInput.Shutdown();
        KeyboardInitialized = false;

        Display.Shutdown();
        GraphicsInitialized = false;

        I_Error("I_InitGraphics: Unable to allocate video buffer");
        return;
    }

    memset(VideoBuffer, 0, SCREENWIDTH * SCREENHEIGHT);

    screens[0] = VideoBuffer;
    MouseButtonState = 0;
}
