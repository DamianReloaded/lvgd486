
#pragma once

#include <stdint.h>
#include <termios.h>

namespace vgd
{
    class Keyboard
    {
    public:
        enum Key
        {
            KeyUnknown = 0,
            KeyEscape,

            Key1, Key2, Key3, Key4, Key5,
            Key6, Key7, Key8, Key9, Key0,

            KeyMinus, KeyEquals, KeyBackspace, KeyTab,

            KeyQ, KeyW, KeyE, KeyR, KeyT,
            KeyY, KeyU, KeyI, KeyO, KeyP,

            KeyLeftBracket, KeyRightBracket,
            KeyEnter, KeyLeftCtrl,

            KeyA, KeyS, KeyD, KeyF, KeyG,
            KeyH, KeyJ, KeyK, KeyL,

            KeySemicolon, KeyApostrophe, KeyGrave,
            KeyLeftShift, KeyBackslash,

            KeyZ, KeyX, KeyC, KeyV, KeyB,
            KeyN, KeyM,

            KeyComma, KeyPeriod, KeySlash,
            KeyRightShift, KeyKeypadMultiply,

            KeyLeftAlt, KeySpace, KeyCapsLock,

            KeyF1, KeyF2, KeyF3, KeyF4, KeyF5,
            KeyF6, KeyF7, KeyF8, KeyF9, KeyF10,

            KeyNumLock, KeyScrollLock,

            KeyKeypad7, KeyKeypad8, KeyKeypad9,
            KeyKeypadMinus,
            KeyKeypad4, KeyKeypad5, KeyKeypad6,
            KeyKeypadPlus,
            KeyKeypad1, KeyKeypad2, KeyKeypad3,
            KeyKeypad0, KeyKeypadPeriod,

            KeyF11, KeyF12,
            KeyRightCtrl, KeyRightAlt,

            KeyInsert, KeyDelete,
            KeyHome, KeyEnd,
            KeyPageUp, KeyPageDown,
            KeyUp, KeyDown, KeyLeft, KeyRight,

            KeyCount
        };

        enum EventType
        {
            EventKeyDown,
            EventKeyUp,
            EventCharacter
        };

        struct Event
        {
            EventType type;
            Key key;
            uint16_t scanCode;
            uint8_t character;
        };

        Keyboard();
        ~Keyboard();

        Keyboard(const Keyboard&) = delete;
        Keyboard& operator=(const Keyboard&) = delete;

        bool Initialize();
        void Shutdown();
        void Update();

        bool IsDown(Key key) const;
        bool WasPressed(Key key) const;
        bool WasReleased(Key key) const;

        bool HasEvent() const;
        Event ReadEvent();

        bool HasCharacter() const;
        uint8_t ReadCharacter();

        void ClearState();

        int GetConsoleNumber() const;
        int GetActiveConsoleNumber() const;
        bool IsConsoleActive() const;

    private:
        static const int MaxEvents = 256;

        int m_Fd;
        int m_VtFd;

        int m_ConsoleNumber;
        int m_ActiveConsoleNumber;

        unsigned char m_OriginalKeyboardMode;

        bool m_KeyboardModeSaved;
        bool m_KeyboardModeChanged;
        bool m_TermiosSaved;
        bool m_TermiosChanged;
        bool m_Initialized;
        bool m_ConsoleActive;

        struct termios m_OriginalTermios;
        struct termios m_RawTermios;

        Event m_Events[MaxEvents];
        int m_EventCount;
        int m_EventRead;

        uint8_t m_Characters[MaxEvents];
        int m_CharacterCount;
        int m_CharacterRead;

        bool m_Down[KeyCount];
        bool m_Pressed[KeyCount];
        bool m_Released[KeyCount];

        bool m_CapsLock;
        bool m_NumLock;
        bool m_ScrollLock;

        static bool IsValidKey(Key key);

        bool FindConsole();
        bool RefreshActiveConsole();

        void ResetState();
        void CompactQueues();

        static Key ConsoleCodeToKey(unsigned int code);
        uint8_t CharacterFromConsoleCode(unsigned int code) const;

        void AddEvent(
            EventType type,
            Key key,
            uint16_t scanCode,
            uint8_t character);

        void AddCharacterEvent(Key key, unsigned int code);
        void ProcessRawByte(unsigned char byte);
    };
}
