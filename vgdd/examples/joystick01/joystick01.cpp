
#include "../../vgd/Display/DisplayMode13h.hpp"
#include "../../vgd/Input/Keyboard.hpp"
#include "../../vgd/Font/BmpFont.hpp"
#include "../../vgd/Font/BmpFont01.hpp"

#include <linux/input.h>
#include <linux/joystick.h>
#include <linux/kd.h>
#include <linux/vt.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

using namespace vgd;

static volatile sig_atomic_t StopRequested = 0;

static void HandleSignal(int signalNumber)
{
    (void)signalNumber;
    StopRequested = 1;
}

static const BMPFont FontFps =
{
    Font01::Width,
    Font01::Height,
    Font01::FirstDigit,
    Font01::DigitCount,
    Font01::FirstUpper,
    Font01::UpperCount,
    Font01::GlyphSpace,
    &Font01::GlyphDigits[0][0],
    &Font01::GlyphUpper[0][0]
};

static char* AppendText(char* p, const char* s)
{
    while (*s)
        *p++ = *s++;

    return p;
}

static char* AppendUnsigned(char* p, unsigned int value)
{
    char digits[10];
    unsigned int count = 0;

    do
    {
        digits[count++] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    while (value);

    while (count)
        *p++ = digits[--count];

    return p;
}

static void FormatFPS(char* buffer, unsigned int fps)
{
    char* p = buffer;
    p = AppendText(p, "FPS ");
    p = AppendUnsigned(p, fps);
    *p = '\0';
}

static long GetElapsedMilliseconds(const timeval& previousTime,
                                   const timeval& currentTime)
{
    return (currentTime.tv_sec - previousTime.tv_sec) * 1000L +
           (currentTime.tv_usec - previousTime.tv_usec) / 1000L;
}

class Joystick
{
public:
    static const int MaxButtons = 32;
    static const int MaxDevices = 16;

    Joystick()
        : m_FileDescriptor(-1),
          m_AxisCount(0),
          m_ButtonCount(0),
          m_AxisX(0),
          m_AxisY(0)
    {
        memset(m_Buttons, 0, sizeof(m_Buttons));
        memset(m_DeviceName, 0, sizeof(m_DeviceName));
        memset(m_DevicePath, 0, sizeof(m_DevicePath));
    }

    bool Initialize()
    {
        Shutdown();

        char path[32];

        for (int i = 0; i < MaxDevices; ++i)
        {
            snprintf(path, sizeof(path), "/dev/input/js%d", i);

            if (TryOpenDevice(path))
                return true;

            snprintf(path, sizeof(path), "/dev/js%d", i);

            if (TryOpenDevice(path))
                return true;
        }

        fprintf(stderr, "Joystick: no usable joystick found.\n");
        return false;
    }

    void Shutdown()
    {
        if (m_FileDescriptor >= 0)
        {
            close(m_FileDescriptor);
            m_FileDescriptor = -1;
        }

        m_AxisCount = 0;
        m_ButtonCount = 0;
        m_AxisX = 0;
        m_AxisY = 0;

        memset(m_Buttons, 0, sizeof(m_Buttons));
        memset(m_DeviceName, 0, sizeof(m_DeviceName));
        memset(m_DevicePath, 0, sizeof(m_DevicePath));
    }

    void Update()
    {
        if (m_FileDescriptor < 0)
            return;

        struct js_event event;

        while (true)
        {
            const ssize_t bytesRead =
                read(m_FileDescriptor, &event, sizeof(event));

            if (bytesRead == static_cast<ssize_t>(sizeof(event)))
            {
                const unsigned char type =
                    event.type & ~JS_EVENT_INIT;

                if (type == JS_EVENT_AXIS)
                {
                    if (event.number == 0)
                        m_AxisX = event.value;
                    else if (event.number == 1)
                        m_AxisY = event.value;
                }
                else if (type == JS_EVENT_BUTTON &&
                         event.number < MaxButtons)
                {
                    m_Buttons[event.number] = event.value != 0;
                }

                continue;
            }

            if (bytesRead < 0 && errno == EINTR)
                continue;

            if (bytesRead < 0 &&
                (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                break;
            }

            if (bytesRead == 0)
                fprintf(stderr, "Joystick: device disconnected.\n");
            else if (bytesRead < 0)
                perror("Joystick: read");

            Shutdown();
            break;
        }
    }

    bool IsInitialized() const
    {
        return m_FileDescriptor >= 0;
    }

    int GetAxisX() const
    {
        return m_AxisX;
    }

    int GetAxisY() const
    {
        return m_AxisY;
    }

    int GetButtonCount() const
    {
        return m_ButtonCount;
    }

    bool IsButtonDown(int button) const
    {
        return button >= 0 &&
               button < MaxButtons &&
               button < m_ButtonCount &&
               m_Buttons[button];
    }

    const char* GetDeviceName() const
    {
        return m_DeviceName;
    }

    const char* GetDevicePath() const
    {
        return m_DevicePath;
    }

private:
    int m_FileDescriptor;
    unsigned char m_AxisCount;
    unsigned char m_ButtonCount;
    int16_t m_AxisX;
    int16_t m_AxisY;
    bool m_Buttons[MaxButtons];
    char m_DeviceName[128];
    char m_DevicePath[32];

    bool TryOpenDevice(const char* path)
    {
        const int fd = open(path, O_RDONLY | O_NONBLOCK);

        if (fd < 0)
            return false;

        unsigned char axes = 0;
        unsigned char buttons = 0;

        if (ioctl(fd, JSIOCGAXES, &axes) < 0 ||
            ioctl(fd, JSIOCGBUTTONS, &buttons) < 0 ||
            axes < 2)
        {
            close(fd);
            return false;
        }

        m_FileDescriptor = fd;
        m_AxisCount = axes;
        m_ButtonCount = buttons;

        strncpy(m_DevicePath, path, sizeof(m_DevicePath) - 1);
        m_DevicePath[sizeof(m_DevicePath) - 1] = '\0';

        if (ioctl(fd, JSIOCGNAME(sizeof(m_DeviceName)),
                  m_DeviceName) < 0)
        {
            strncpy(m_DeviceName, "Unknown joystick",
                    sizeof(m_DeviceName) - 1);
            m_DeviceName[sizeof(m_DeviceName) - 1] = '\0';
        }

        fprintf(stderr,
                "Joystick: found %s at %s (%u axes, %u buttons).\n",
                m_DeviceName, m_DevicePath,
                static_cast<unsigned int>(m_AxisCount),
                static_cast<unsigned int>(m_ButtonCount));

        return true;
    }
};

static int GetAxisMovement(int value)
{
    const int deadZone = 6000;

    if (value > -deadZone && value < deadZone)
        return 0;

    if (value < 0)
        value += deadZone;
    else
        value -= deadZone;

    // Return a signed magnitude from approximately -1000 to +1000.
    return value * 1000 / 26767;
}

static uint8_t GetQuadColor(const Joystick& joystick)
{
    static const uint8_t colors[] =
    {
        12, 14, 10, 13, 11, 9, 15, 6
    };

    for (int i = 0; i < joystick.GetButtonCount() &&
                    i < Joystick::MaxButtons; ++i)
    {
        if (joystick.IsButtonDown(i))
            return colors[i % (sizeof(colors) / sizeof(colors[0]))];
    }

    return 15;
}

static void DrawBackground(DisplayMode13h& display,
                           int scrollX,
                           int scrollY)
{
    uint8_t* backBuffer = display.GetBackBuffer();
    const int pitch = display.GetBackBufferPitch();
    const int width = display.GetWidth();
    const int height = display.GetHeight();

    // Power-of-two tile sizes allow shifts and masks instead of division.
    const int worldYStart = scrollY;

    for (int y = 0; y < height; ++y)
    {
        uint8_t* row = backBuffer + y * pitch;
        const int worldY = worldYStart + y;
        const int tileY = worldY >> 4;
        const int localY = worldY & 15;

        for (int x = 0; x < width; ++x)
        {
            const int worldX = scrollX + x;
            const int tileX = worldX >> 4;
            const int localX = worldX & 15;

            // Deterministic terrain pattern, with no per-pixel division.
            uint8_t color;

            if (localX == 0 || localY == 0)
            {
                color = 4;
            }
            else
            {
                const unsigned int pattern =
                    static_cast<unsigned int>(tileX * 13) ^
                    static_cast<unsigned int>(tileY * 7);

                switch (pattern & 7u)
                {
                    case 0:
                    case 1:
                        color = 2;
                        break;

                    case 2:
                    case 3:
                    case 4:
                        color = 3;
                        break;

                    case 5:
                        color = 10;
                        break;

                    default:
                        color = 1;
                        break;
                }
            }

            row[x] = color;
        }
    }
}

static void DrawFrame(DisplayMode13h& display,
                      const Joystick& joystick,
                      int scrollX,
                      int scrollY,
                      unsigned int fps)
{
    DrawBackground(display, scrollX, scrollY);

    const int quadWidth = 20;
    const int quadHeight = 20;
    const int quadX = (display.GetWidth() - quadWidth) / 2;
    const int quadY = (display.GetHeight() - quadHeight) / 2;

    const uint8_t quadColor = GetQuadColor(joystick);

    display.DrawRectangle(
        quadX - 2, quadY - 2,
        quadWidth + 4, quadHeight + 4, 0);

    display.DrawRectangle(
        quadX, quadY,
        quadWidth, quadHeight, quadColor);

    display.DrawRectangle(
        quadX + 4, quadY + 4,
        quadWidth - 8, quadHeight - 8, 15);

    display.DrawRectangle(
        quadX + 6, quadY + 6,
        quadWidth - 12, quadHeight - 12, quadColor);

    char text[16];
    FormatFPS(text, fps);
    display.DrawText(FontFps, text, 8, 8, 15);
}

static int RunGame(DisplayMode13h& display)
{
    Keyboard keyboard;
    Joystick joystick;

    bool displayInitialized = false;
    bool keyboardInitialized = false;
    bool joystickInitialized = false;
    int result = 0;

    // Fixed-point scrolling retains subpixel movement between frames.
    int64_t scrollX16 = 0;
    int64_t scrollY16 = 0;

    unsigned long fpsFrames = 0;
    unsigned int fps = 0;

    struct timeval previousTime;
    memset(&previousTime, 0, sizeof(previousTime));

    if (!display.Initialize())
    {
        fprintf(stderr, "Game: display initialization failed.\n");
        display.Shutdown();
        return 1;
    }

    displayInitialized = true;

    if (!keyboard.Initialize())
    {
        fprintf(stderr, "Game: keyboard initialization failed.\n");
        result = 1;
        goto cleanup;
    }

    keyboardInitialized = true;

    if (!joystick.Initialize())
    {
        fprintf(stderr, "Game: no compatible joystick available.\n");
        result = 1;
        goto cleanup;
    }

    joystickInitialized = true;

    gettimeofday(&previousTime, NULL);

    while (!StopRequested)
    {
        // Always process pending input before doing any rendering.
        keyboard.Update();
        joystick.Update();

        if (!keyboard.IsConsoleActive())
            break;

        const bool ctrlDown =
            keyboard.IsDown(Keyboard::KeyLeftCtrl) ||
            keyboard.IsDown(Keyboard::KeyRightCtrl);

        if ((ctrlDown && keyboard.IsDown(Keyboard::KeyC)) ||
            keyboard.IsDown(Keyboard::KeyEscape))
        {
            break;
        }

        struct timeval currentTime;
        gettimeofday(&currentTime, NULL);

        long elapsedMilliseconds =
            GetElapsedMilliseconds(previousTime, currentTime);

        previousTime = currentTime;

        // Avoid a large jump after scheduling delays or a debugger pause.
        if (elapsedMilliseconds < 0)
            elapsedMilliseconds = 0;
        else if (elapsedMilliseconds > 50)
            elapsedMilliseconds = 50;

        const int axisX = GetAxisMovement(joystick.GetAxisX());
        const int axisY = GetAxisMovement(joystick.GetAxisY());

        // Maximum scrolling speed: 180 pixels per second.
        // Fixed-point arithmetic preserves small movements at low FPS.
        scrollX16 +=
            static_cast<int64_t>(axisX) * 180 *
            elapsedMilliseconds * 65536 / 1000000;

        scrollY16 +=
            static_cast<int64_t>(axisY) * 180 *
            elapsedMilliseconds * 65536 / 1000000;

        const int scrollX = static_cast<int>(scrollX16 >> 16);
        const int scrollY = static_cast<int>(scrollY16 >> 16);

        DrawFrame(display, joystick, scrollX, scrollY, fps);

        if (!display.Present())
        {
            fprintf(stderr, "Game: display presentation failed.\n");
            result = 1;
            break;
        }

        ++fpsFrames;

        gettimeofday(&currentTime, NULL);

        const long fpsElapsedMilliseconds =
            GetElapsedMilliseconds(previousTime, currentTime);

        // Keep the FPS calculation independent of movement timing.
        static struct timeval fpsStartTime;
        static bool fpsTimerStarted = false;

        if (!fpsTimerStarted)
        {
            fpsStartTime = currentTime;
            fpsTimerStarted = true;
        }

        const long totalFpsMilliseconds =
            GetElapsedMilliseconds(fpsStartTime, currentTime);

        if (totalFpsMilliseconds >= 1000)
        {
            fps = static_cast<unsigned int>(
                fpsFrames * 1000UL /
                static_cast<unsigned long>(totalFpsMilliseconds));

            fpsFrames = 0;
            fpsStartTime = currentTime;
        }

        (void)fpsElapsedMilliseconds;
    }

cleanup:
    if (joystickInitialized)
        joystick.Shutdown();

    if (keyboardInitialized)
        keyboard.Shutdown();

    if (displayInitialized)
        display.Shutdown();

    return result;
}

int main()
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));

    action.sa_handler = HandleSignal;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, NULL) != 0)
        perror("sigaction SIGINT");

    if (sigaction(SIGTERM, &action, NULL) != 0)
        perror("sigaction SIGTERM");

    if (sigaction(SIGHUP, &action, NULL) != 0)
        perror("sigaction SIGHUP");

    DisplayMode13h display;
    return RunGame(display);
}
