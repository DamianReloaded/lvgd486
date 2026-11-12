
#include "Keyboard.hpp"

#include <linux/input.h>
#include <linux/kd.h>
#include <linux/vt.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

namespace vgd
{
    // ---------------------------------------------------------------------
    // Construction and destruction
    // ---------------------------------------------------------------------

    Keyboard::Keyboard()
        : m_Fd(-1),
          m_VtFd(-1),
          m_ConsoleNumber(0),
          m_ActiveConsoleNumber(0),
          m_OriginalKeyboardMode(0),
          m_KeyboardModeSaved(false),
          m_KeyboardModeChanged(false),
          m_TermiosSaved(false),
          m_TermiosChanged(false),
          m_Initialized(false),
          m_ConsoleActive(false),
          m_EventCount(0),
          m_EventRead(0),
          m_CharacterCount(0),
          m_CharacterRead(0),
          m_CapsLock(false),
          m_NumLock(false),
          m_ScrollLock(false)
    {
        memset(&m_OriginalTermios, 0, sizeof(m_OriginalTermios));
        memset(&m_RawTermios, 0, sizeof(m_RawTermios));

        ResetState();
    }

    Keyboard::~Keyboard()
    {
        Shutdown();
    }

    // ---------------------------------------------------------------------
    // Initialization
    // ---------------------------------------------------------------------

    bool Keyboard::Initialize()
    {
        if (m_Initialized)
            return true;

        Shutdown();
        ResetState();

        /*
         * FindConsole() follows the SVGAlib approach:
         *
         * 1. Inspect stdin, stdout and stderr for an existing Linux VT.
         * 2. Obtain the active VT from /dev/console.
         * 3. Try the active VT's devfs path, /dev/vc/N.
         * 4. Fall back to the traditional /dev/ttyN path.
         *
         * We deliberately select the active VT rather than an arbitrary
         * free VT returned by VT_OPENQRY.
         */
        if (!FindConsole())
        {
            Shutdown();
            return false;
        }

        if (!RefreshActiveConsole())
        {
            fprintf(stderr,
                    "Keyboard: unable to determine the active virtual "
                    "terminal.\n");

            Shutdown();
            return false;
        }

        fprintf(stderr,
                "Keyboard: selected VT%d; active VT=%d\n",
                m_ConsoleNumber,
                m_ActiveConsoleNumber);

        if (!m_ConsoleActive)
        {
            fprintf(stderr,
                    "Keyboard: selected VT%d is not active.\n",
                    m_ConsoleNumber);

            Shutdown();
            return false;
        }

        // Save the existing keyboard mode.
        if (ioctl(m_Fd, KDGKBMODE, &m_OriginalKeyboardMode) < 0)
        {
            perror("Keyboard: KDGKBMODE");

            Shutdown();
            return false;
        }

        m_KeyboardModeSaved = true;

        // Save the existing terminal settings.
        if (tcgetattr(m_Fd, &m_OriginalTermios) < 0)
        {
            perror("Keyboard: tcgetattr");

            Shutdown();
            return false;
        }

        m_TermiosSaved = true;
        m_RawTermios = m_OriginalTermios;

        /*
         * Follow SVGAlib's terminal configuration for medium-raw input.
         *
         * Do not use cfmakeraw(): the original SVGAlib sequence explicitly
         * clears the flags required for its console input path.
         */
        m_RawTermios.c_lflag &= ~(ICANON | ECHO | ISIG);

        m_RawTermios.c_iflag &=
            ~(ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);

        // A read returns immediately when no input is waiting.
        m_RawTermios.c_cc[VMIN] = 0;
        m_RawTermios.c_cc[VTIME] = 0;

        /*
         * Apply terminal settings before changing the keyboard mode,
         * matching SVGAlib's initialization order.
         */
        if (tcsetattr(m_Fd, TCSAFLUSH, &m_RawTermios) < 0)
        {
            perror("Keyboard: tcsetattr");

            Shutdown();
            return false;
        }

        m_TermiosChanged = true;

        // Enable Linux medium-raw keyboard mode.
        if (ioctl(m_Fd, KDSKBMODE, K_MEDIUMRAW) < 0)
        {
            perror("Keyboard: KDSKBMODE K_MEDIUMRAW");

            Shutdown();
            return false;
        }

        m_KeyboardModeChanged = true;
        m_Initialized = true;

        ResetState();

        fprintf(stderr,
                "Keyboard: initialized on VT%d in medium-raw mode.\n",
                m_ConsoleNumber);

        return true;
    }

    // ---------------------------------------------------------------------
    // Shutdown and restoration
    // ---------------------------------------------------------------------

    void Keyboard::Shutdown()
    {
        /*
         * Restore the keyboard mode and terminal settings independently.
         * This also handles partially completed initialization.
         */
        if (m_Fd >= 0)
        {
            if (m_KeyboardModeChanged && m_KeyboardModeSaved)
            {
                if (ioctl(
                        m_Fd,
                        KDSKBMODE,
                        static_cast<int>(m_OriginalKeyboardMode)) < 0)
                {
                    perror("Keyboard: restore keyboard mode");
                }

                m_KeyboardModeChanged = false;
            }

            if (m_TermiosChanged && m_TermiosSaved)
            {
                if (tcsetattr(
                        m_Fd,
                        TCSANOW,
                        &m_OriginalTermios) < 0)
                {
                    perror("Keyboard: restore terminal settings");
                }

                m_TermiosChanged = false;
            }
        }

        m_KeyboardModeSaved = false;
        m_TermiosSaved = false;

        if (m_Fd >= 0)
        {
            close(m_Fd);
            m_Fd = -1;
        }

        if (m_VtFd >= 0)
        {
            close(m_VtFd);
            m_VtFd = -1;
        }

        m_Initialized = false;
        m_ConsoleActive = false;

        ResetState();
    }

    // ---------------------------------------------------------------------
    // Per-frame update
    // ---------------------------------------------------------------------

    void Keyboard::Update()
    {
        // Pressed/released transitions last for one Update() call.
        for (int i = 0; i < KeyCount; ++i)
        {
            m_Pressed[i] = false;
            m_Released[i] = false;
        }

        CompactQueues();

        if (!m_Initialized || m_Fd < 0)
            return;

        // Do not consume input while another VT is active.
        if (!RefreshActiveConsole() || !m_ConsoleActive)
            return;

        unsigned char buffer[64];

        for (;;)
        {
            const ssize_t count = read(m_Fd, buffer, sizeof(buffer));

            if (count < 0)
            {
                if (errno == EINTR)
                    continue;

                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;

                perror("Keyboard: read");
                break;
            }

            if (count == 0)
                break;

            for (ssize_t i = 0; i < count; ++i)
                ProcessRawByte(buffer[i]);
        }
    }

    // ---------------------------------------------------------------------
    // Key state accessors
    // ---------------------------------------------------------------------

    bool Keyboard::IsDown(Key key) const
    {
        return IsValidKey(key) && m_Down[key];
    }

    bool Keyboard::WasPressed(Key key) const
    {
        return IsValidKey(key) && m_Pressed[key];
    }

    bool Keyboard::WasReleased(Key key) const
    {
        return IsValidKey(key) && m_Released[key];
    }

    // ---------------------------------------------------------------------
    // Event queue
    // ---------------------------------------------------------------------

    bool Keyboard::HasEvent() const
    {
        return m_EventRead < m_EventCount;
    }

    Keyboard::Event Keyboard::ReadEvent()
    {
        Event event;

        event.type = EventKeyDown;
        event.key = KeyUnknown;
        event.scanCode = 0;
        event.character = 0;

        if (HasEvent())
            event = m_Events[m_EventRead++];

        return event;
    }

    // ---------------------------------------------------------------------
    // Character queue
    // ---------------------------------------------------------------------

    bool Keyboard::HasCharacter() const
    {
        return m_CharacterRead < m_CharacterCount;
    }

    uint8_t Keyboard::ReadCharacter()
    {
        if (!HasCharacter())
            return 0;

        return m_Characters[m_CharacterRead++];
    }

    // ---------------------------------------------------------------------
    // Public state management and console information
    // ---------------------------------------------------------------------

    void Keyboard::ClearState()
    {
        ResetState();
    }

    int Keyboard::GetConsoleNumber() const
    {
        return m_ConsoleNumber;
    }

    int Keyboard::GetActiveConsoleNumber() const
    {
        return m_ActiveConsoleNumber;
    }

    bool Keyboard::IsConsoleActive() const
    {
        return m_ConsoleActive;
    }

    // ---------------------------------------------------------------------
    // Key validation
    // ---------------------------------------------------------------------

    bool Keyboard::IsValidKey(Key key)
    {
        return key > KeyUnknown && key < KeyCount;
    }

    // ---------------------------------------------------------------------
    // Find the active Linux virtual console
    // ---------------------------------------------------------------------

    bool Keyboard::FindConsole()
    {
        /*
         * Keep a control descriptor for VT_GETSTATE.
         *
         * The diagnostic established that /dev/console is available and
         * supports VT_GETSTATE on the target Linux 2.4 system.
         */
        m_VtFd = open("/dev/console", O_RDONLY | O_NONBLOCK);

        if (m_VtFd < 0)
        {
            perror("Keyboard: open /dev/console");
        }

        /*
         * First follow SVGAlib's strategy and inspect descriptors 0, 1
         * and 2. A descriptor is accepted only if it identifies a Linux
         * virtual console and accepts VT_GETMODE.
         */
        for (int fd = 0; fd <= 2; ++fd)
        {
            struct vt_mode mode;
            memset(&mode, 0, sizeof(mode));

            if (ioctl(fd, VT_GETMODE, &mode) < 0)
                continue;

            struct stat st;

            if (fstat(fd, &st) < 0)
                continue;

            if (!S_ISCHR(st.st_mode))
                continue;

            const unsigned int deviceMajor =
                static_cast<unsigned int>(major(st.st_rdev));

            const unsigned int deviceMinor =
                static_cast<unsigned int>(minor(st.st_rdev));

            if (deviceMajor != 4 ||
                deviceMinor < 1 ||
                deviceMinor > 63)
            {
                continue;
            }

            const int duplicateFd = dup(fd);

            if (duplicateFd < 0)
            {
                perror("Keyboard: dup console descriptor");
                continue;
            }

            m_Fd = duplicateFd;
            m_ConsoleNumber = static_cast<int>(deviceMinor);

            /*
             * If /dev/console was unavailable, retain a second descriptor
             * to the discovered VT for VT_GETSTATE attempts.
             */
            if (m_VtFd < 0)
            {
                m_VtFd = dup(fd);

                if (m_VtFd < 0)
                    perror("Keyboard: dup VT control descriptor");
            }

            const char* name = ttyname(fd);

            fprintf(stderr,
                    "Keyboard: found virtual console on inherited "
                    "descriptor %d; VT%d%s%s.\n",
                    fd,
                    m_ConsoleNumber,
                    name ? ", device " : "",
                    name ? name : "");

            return true;
        }

        /*
         * No inherited descriptor was a virtual console.
         *
         * Find the active VT using the console control descriptor. Then
         * open that VT through devfs or the traditional device path.
         */
        if (m_VtFd < 0)
        {
            fprintf(stderr,
                    "Keyboard: cannot open /dev/console and no inherited "
                    "descriptor identifies a virtual console.\n");

            return false;
        }

        struct vt_stat state;
        memset(&state, 0, sizeof(state));

        if (ioctl(m_VtFd, VT_GETSTATE, &state) < 0)
        {
            perror("Keyboard: VT_GETSTATE during console discovery");
            return false;
        }

        const int activeVt = static_cast<int>(state.v_active);

        if (activeVt < 1 || activeVt > 63)
        {
            fprintf(stderr,
                    "Keyboard: kernel reported invalid active VT %d.\n",
                    activeVt);

            return false;
        }

        /*
         * /dev/vc/N is the devfs path used by this Linux 2.4 system.
         * /dev/ttyN is the conventional Linux virtual-console path.
         *
         * Try the active VT only. Opening a different VT would not make
         * that inactive console receive the user's keyboard input.
         */
        char paths[2][32];

        snprintf(paths[0], sizeof(paths[0]),
                 "/dev/vc/%d", activeVt);

        snprintf(paths[1], sizeof(paths[1]),
                 "/dev/tty%d", activeVt);

        for (int i = 0; i < 2; ++i)
        {
            const int fd = open(
                paths[i],
                O_RDWR | O_NOCTTY | O_NONBLOCK);

            if (fd < 0)
            {
                fprintf(stderr,
                        "Keyboard: cannot open %s: %s\n",
                        paths[i],
                        strerror(errno));

                continue;
            }

            struct stat st;

            if (fstat(fd, &st) < 0)
            {
                fprintf(stderr,
                        "Keyboard: fstat %s failed: %s\n",
                        paths[i],
                        strerror(errno));

                close(fd);
                continue;
            }

            if (!S_ISCHR(st.st_mode) ||
                major(st.st_rdev) != 4 ||
                minor(st.st_rdev) !=
                    static_cast<unsigned int>(activeVt))
            {
                fprintf(stderr,
                        "Keyboard: %s is not the expected VT device.\n",
                        paths[i]);

                close(fd);
                continue;
            }

            struct vt_mode mode;
            memset(&mode, 0, sizeof(mode));

            if (ioctl(fd, VT_GETMODE, &mode) < 0)
            {
                fprintf(stderr,
                        "Keyboard: VT_GETMODE failed on %s: %s\n",
                        paths[i],
                        strerror(errno));

                close(fd);
                continue;
            }

            m_Fd = fd;
            m_ConsoleNumber = activeVt;

            fprintf(stderr,
                    "Keyboard: opened active VT%d through %s.\n",
                    m_ConsoleNumber,
                    paths[i]);

            return true;
        }

        fprintf(stderr,
                "Keyboard: could not open active VT%d. "
                "Tried /dev/vc/%d and /dev/tty%d.\n",
                activeVt,
                activeVt,
                activeVt);

        return false;
    }

    // ---------------------------------------------------------------------
    // Refresh active virtual console information
    // ---------------------------------------------------------------------

    bool Keyboard::RefreshActiveConsole()
    {
        struct vt_stat state;
        memset(&state, 0, sizeof(state));

        /*
         * Prefer the /dev/console control descriptor. If it is unavailable
         * or rejects VT_GETSTATE, try the selected virtual-console fd.
         */
        int result = -1;

        if (m_VtFd >= 0)
            result = ioctl(m_VtFd, VT_GETSTATE, &state);

        if (result < 0 && m_Fd >= 0)
            result = ioctl(m_Fd, VT_GETSTATE, &state);

        if (result < 0)
        {
            m_ConsoleActive = false;
            return false;
        }

        m_ActiveConsoleNumber = static_cast<int>(state.v_active);

        m_ConsoleActive =
            (m_ActiveConsoleNumber == m_ConsoleNumber);

        return true;
    }

    // ---------------------------------------------------------------------
    // Reset key states and queues
    // ---------------------------------------------------------------------

    void Keyboard::ResetState()
    {
        m_EventCount = 0;
        m_EventRead = 0;

        m_CharacterCount = 0;
        m_CharacterRead = 0;

        for (int i = 0; i < KeyCount; ++i)
        {
            m_Down[i] = false;
            m_Pressed[i] = false;
            m_Released[i] = false;
        }

        m_CapsLock = false;
        m_NumLock = false;
        m_ScrollLock = false;
    }

    // ---------------------------------------------------------------------
    // Compact consumed queue entries
    // ---------------------------------------------------------------------

    void Keyboard::CompactQueues()
    {
        if (m_EventRead == m_EventCount)
        {
            m_EventRead = 0;
            m_EventCount = 0;
        }
        else if (m_EventRead > 0)
        {
            const int remaining = m_EventCount - m_EventRead;

            memmove(
                m_Events,
                &m_Events[m_EventRead],
                remaining * sizeof(Event));

            m_EventCount = remaining;
            m_EventRead = 0;
        }

        if (m_CharacterRead == m_CharacterCount)
        {
            m_CharacterRead = 0;
            m_CharacterCount = 0;
        }
        else if (m_CharacterRead > 0)
        {
            const int remaining =
                m_CharacterCount - m_CharacterRead;

            memmove(
                m_Characters,
                &m_Characters[m_CharacterRead],
                remaining * sizeof(uint8_t));

            m_CharacterCount = remaining;
            m_CharacterRead = 0;
        }
    }

    // ---------------------------------------------------------------------
    // Convert Linux console keycodes to Keyboard::Key
    // ---------------------------------------------------------------------

    Keyboard::Key Keyboard::ConsoleCodeToKey(unsigned int code)
    {
        switch (code)
        {
        case KEY_ESC: return KeyEscape;

        case KEY_1: return Key1;
        case KEY_2: return Key2;
        case KEY_3: return Key3;
        case KEY_4: return Key4;
        case KEY_5: return Key5;
        case KEY_6: return Key6;
        case KEY_7: return Key7;
        case KEY_8: return Key8;
        case KEY_9: return Key9;
        case KEY_0: return Key0;

        case KEY_MINUS: return KeyMinus;
        case KEY_EQUAL: return KeyEquals;
        case KEY_BACKSPACE: return KeyBackspace;
        case KEY_TAB: return KeyTab;

        case KEY_Q: return KeyQ;
        case KEY_W: return KeyW;
        case KEY_E: return KeyE;
        case KEY_R: return KeyR;
        case KEY_T: return KeyT;
        case KEY_Y: return KeyY;
        case KEY_U: return KeyU;
        case KEY_I: return KeyI;
        case KEY_O: return KeyO;
        case KEY_P: return KeyP;

        case KEY_LEFTBRACE: return KeyLeftBracket;
        case KEY_RIGHTBRACE: return KeyRightBracket;
        case KEY_ENTER: return KeyEnter;
        case KEY_LEFTCTRL: return KeyLeftCtrl;

        case KEY_A: return KeyA;
        case KEY_S: return KeyS;
        case KEY_D: return KeyD;
        case KEY_F: return KeyF;
        case KEY_G: return KeyG;
        case KEY_H: return KeyH;
        case KEY_J: return KeyJ;
        case KEY_K: return KeyK;
        case KEY_L: return KeyL;

        case KEY_SEMICOLON: return KeySemicolon;
        case KEY_APOSTROPHE: return KeyApostrophe;
        case KEY_GRAVE: return KeyGrave;
        case KEY_LEFTSHIFT: return KeyLeftShift;
        case KEY_BACKSLASH: return KeyBackslash;

        case KEY_Z: return KeyZ;
        case KEY_X: return KeyX;
        case KEY_C: return KeyC;
        case KEY_V: return KeyV;
        case KEY_B: return KeyB;
        case KEY_N: return KeyN;
        case KEY_M: return KeyM;

        case KEY_COMMA: return KeyComma;
        case KEY_DOT: return KeyPeriod;
        case KEY_SLASH: return KeySlash;
        case KEY_RIGHTSHIFT: return KeyRightShift;
        case KEY_KPASTERISK: return KeyKeypadMultiply;

        case KEY_LEFTALT: return KeyLeftAlt;
        case KEY_SPACE: return KeySpace;
        case KEY_CAPSLOCK: return KeyCapsLock;

        case KEY_F1: return KeyF1;
        case KEY_F2: return KeyF2;
        case KEY_F3: return KeyF3;
        case KEY_F4: return KeyF4;
        case KEY_F5: return KeyF5;
        case KEY_F6: return KeyF6;
        case KEY_F7: return KeyF7;
        case KEY_F8: return KeyF8;
        case KEY_F9: return KeyF9;
        case KEY_F10: return KeyF10;

        case KEY_NUMLOCK: return KeyNumLock;
        case KEY_SCROLLLOCK: return KeyScrollLock;

        case KEY_KP7: return KeyKeypad7;
        case KEY_KP8: return KeyKeypad8;
        case KEY_KP9: return KeyKeypad9;
        case KEY_KPMINUS: return KeyKeypadMinus;
        case KEY_KP4: return KeyKeypad4;
        case KEY_KP5: return KeyKeypad5;
        case KEY_KP6: return KeyKeypad6;
        case KEY_KPPLUS: return KeyKeypadPlus;
        case KEY_KP1: return KeyKeypad1;
        case KEY_KP2: return KeyKeypad2;
        case KEY_KP3: return KeyKeypad3;
        case KEY_KP0: return KeyKeypad0;
        case KEY_KPDOT: return KeyKeypadPeriod;

        case KEY_F11: return KeyF11;
        case KEY_F12: return KeyF12;

        case KEY_RIGHTCTRL: return KeyRightCtrl;
        case KEY_RIGHTALT: return KeyRightAlt;

        case KEY_INSERT: return KeyInsert;
        case KEY_DELETE: return KeyDelete;
        case KEY_HOME: return KeyHome;
        case KEY_END: return KeyEnd;
        case KEY_PAGEUP: return KeyPageUp;
        case KEY_PAGEDOWN: return KeyPageDown;
        case KEY_UP: return KeyUp;
        case KEY_DOWN: return KeyDown;
        case KEY_LEFT: return KeyLeft;
        case KEY_RIGHT: return KeyRight;

        default:
            return KeyUnknown;
        }
    }

    // ---------------------------------------------------------------------
    // Convert Linux console keycodes to characters
    // ---------------------------------------------------------------------

    uint8_t Keyboard::CharacterFromConsoleCode(
        unsigned int code) const
    {
        const bool shift =
            m_Down[KeyLeftShift] || m_Down[KeyRightShift];

        const bool ctrl =
            m_Down[KeyLeftCtrl] || m_Down[KeyRightCtrl];

        const bool upper = shift ^ m_CapsLock;

        uint8_t ch = 0;

        switch (code)
        {
        case KEY_A: ch = 'a'; break;
        case KEY_B: ch = 'b'; break;
        case KEY_C: ch = 'c'; break;
        case KEY_D: ch = 'd'; break;
        case KEY_E: ch = 'e'; break;
        case KEY_F: ch = 'f'; break;
        case KEY_G: ch = 'g'; break;
        case KEY_H: ch = 'h'; break;
        case KEY_I: ch = 'i'; break;
        case KEY_J: ch = 'j'; break;
        case KEY_K: ch = 'k'; break;
        case KEY_L: ch = 'l'; break;
        case KEY_M: ch = 'm'; break;
        case KEY_N: ch = 'n'; break;
        case KEY_O: ch = 'o'; break;
        case KEY_P: ch = 'p'; break;
        case KEY_Q: ch = 'q'; break;
        case KEY_R: ch = 'r'; break;
        case KEY_S: ch = 's'; break;
        case KEY_T: ch = 't'; break;
        case KEY_U: ch = 'u'; break;
        case KEY_V: ch = 'v'; break;
        case KEY_W: ch = 'w'; break;
        case KEY_X: ch = 'x'; break;
        case KEY_Y: ch = 'y'; break;
        case KEY_Z: ch = 'z'; break;

        case KEY_1: ch = shift ? '!' : '1'; break;
        case KEY_2: ch = shift ? '@' : '2'; break;
        case KEY_3: ch = shift ? '#' : '3'; break;
        case KEY_4: ch = shift ? '$' : '4'; break;
        case KEY_5: ch = shift ? '%' : '5'; break;
        case KEY_6: ch = shift ? '^' : '6'; break;
        case KEY_7: ch = shift ? '&' : '7'; break;
        case KEY_8: ch = shift ? '*' : '8'; break;
        case KEY_9: ch = shift ? '(' : '9'; break;
        case KEY_0: ch = shift ? ')' : '0'; break;

        case KEY_MINUS: ch = shift ? '_' : '-'; break;
        case KEY_EQUAL: ch = shift ? '+' : '='; break;

        case KEY_LEFTBRACE:
            ch = shift ? '{' : '[';
            break;

        case KEY_RIGHTBRACE:
            ch = shift ? '}' : ']';
            break;

        case KEY_SEMICOLON:
            ch = shift ? ':' : ';';
            break;

        case KEY_APOSTROPHE:
            ch = shift ? '"' : '\'';
            break;

        case KEY_GRAVE:
            ch = shift ? '~' : '`';
            break;

        case KEY_BACKSLASH:
            ch = shift ? '|' : '\\';
            break;

        case KEY_COMMA:
            ch = shift ? '<' : ',';
            break;

        case KEY_DOT:
            ch = shift ? '>' : '.';
            break;

        case KEY_SLASH:
            ch = shift ? '?' : '/';
            break;

        case KEY_SPACE: ch = ' '; break;
        case KEY_ENTER: ch = '\r'; break;
        case KEY_TAB: ch = '\t'; break;
        case KEY_BACKSPACE: ch = '\b'; break;
        case KEY_ESC: ch = 27; break;

        case KEY_KPASTERISK: ch = '*'; break;
        case KEY_KPMINUS: ch = '-'; break;
        case KEY_KPPLUS: ch = '+'; break;

        case KEY_KP7:
            if (m_NumLock) ch = '7';
            break;

        case KEY_KP8:
            if (m_NumLock) ch = '8';
            break;

        case KEY_KP9:
            if (m_NumLock) ch = '9';
            break;

        case KEY_KP4:
            if (m_NumLock) ch = '4';
            break;

        case KEY_KP5:
            if (m_NumLock) ch = '5';
            break;

        case KEY_KP6:
            if (m_NumLock) ch = '6';
            break;

        case KEY_KP1:
            if (m_NumLock) ch = '1';
            break;

        case KEY_KP2:
            if (m_NumLock) ch = '2';
            break;

        case KEY_KP3:
            if (m_NumLock) ch = '3';
            break;

        case KEY_KP0:
            if (m_NumLock) ch = '0';
            break;

        case KEY_KPDOT:
            if (m_NumLock) ch = '.';
            break;

        default:
            break;
        }

        if (ch >= 'a' && ch <= 'z')
        {
            if (ctrl)
                return static_cast<uint8_t>(ch - 'a' + 1);

            if (upper)
                ch = static_cast<uint8_t>(ch - 'a' + 'A');
        }

        return ch;
    }

    // ---------------------------------------------------------------------
    // Event and character insertion
    // ---------------------------------------------------------------------

    void Keyboard::AddEvent(
        EventType type,
        Key key,
        uint16_t scanCode,
        uint8_t character)
    {
        if (m_EventCount >= MaxEvents)
            return;

        Event& event = m_Events[m_EventCount++];

        event.type = type;
        event.key = key;
        event.scanCode = scanCode;
        event.character = character;
    }

    void Keyboard::AddCharacterEvent(Key key, unsigned int code)
    {
        const uint8_t ch = CharacterFromConsoleCode(code);

        if (ch == 0)
            return;

        if (m_CharacterCount < MaxEvents)
            m_Characters[m_CharacterCount++] = ch;

        AddEvent(
            EventCharacter,
            key,
            static_cast<uint16_t>(code),
            ch);
    }

    // ---------------------------------------------------------------------
    // Process one medium-raw keyboard byte
    // ---------------------------------------------------------------------

    void Keyboard::ProcessRawByte(unsigned char byte)
    {
        /*
         * Medium-raw format:
         *   bit 7 clear: key press
         *   bit 7 set:   key release
         *   bits 0-6:    Linux console keycode
         */
        const unsigned int code = byte & 0x7f;
        const bool down = (byte & 0x80) == 0;

        const Key key = ConsoleCodeToKey(code);

        if (key == KeyUnknown)
            return;

        const bool wasDown = m_Down[key];

        if (down)
        {
            if (wasDown)
            {
                // Repeated key-down: generate another character if possible.
                AddCharacterEvent(key, code);
                return;
            }

            m_Down[key] = true;
            m_Pressed[key] = true;

            if (key == KeyCapsLock)
                m_CapsLock = !m_CapsLock;
            else if (key == KeyNumLock)
                m_NumLock = !m_NumLock;
            else if (key == KeyScrollLock)
                m_ScrollLock = !m_ScrollLock;

            AddEvent(
                EventKeyDown,
                key,
                static_cast<uint16_t>(code),
                0);

            AddCharacterEvent(key, code);
        }
        else
        {
            if (!wasDown)
                return;

            m_Down[key] = false;
            m_Released[key] = true;

            AddEvent(
                EventKeyUp,
                key,
                static_cast<uint16_t>(code),
                0);
        }
    }
}
