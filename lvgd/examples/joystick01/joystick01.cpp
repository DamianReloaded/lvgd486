
#include "../../vgd/Display/DisplayMode13h.hpp"
#include "../../vgd/Input/Keyboard.hpp"
#include "../../vgd/Input/Joystick.hpp"
#include "../../vgd/Font/BmpFont.hpp"
#include "../../vgd/Font/FontArcade01.hpp"

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

vgd::FontArcade01 FontFps;

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

static int GetAxisMovement(int value)
{
    const int deadZone = 6000;

    if (value > -deadZone && value < deadZone)
        return 0;

    if (value < 0)
        value += deadZone;
    else
        value -= deadZone;

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
        if (joystick.IsButtonPressed(i))
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

    // Direction indicators.
    if (joystick.IsDirectionPressed(Joystick::DirectionUp))
        display.DrawRectangle(150, 8, 8, 8, 12);

    if (joystick.IsDirectionPressed(Joystick::DirectionDown))
        display.DrawRectangle(150, 24, 8, 8, 12);

    if (joystick.IsDirectionPressed(Joystick::DirectionLeft))
        display.DrawRectangle(140, 16, 8, 8, 12);

    if (joystick.IsDirectionPressed(Joystick::DirectionRight))
        display.DrawRectangle(160, 16, 8, 8, 12);

    // Show the last button press as a small indicator.
    for (int i = 0; i < joystick.GetButtonCount() &&
                    i < Joystick::MaxButtons; ++i)
    {
        if (joystick.WasButtonPressed(i))
        {
            display.DrawRectangle(8 + i * 6, 24, 4, 4, 14);
        }
    }
}

static int RunGame(DisplayMode13h& display)
{
    Keyboard keyboard;
    Joystick joystick;

    bool displayInitialized = false;
    bool keyboardInitialized = false;
    bool joystickInitialized = false;
    int result = 0;

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
    }
    else
    {
        joystickInitialized = true;
        joystick.SetDeadZone(6000);
    }

    gettimeofday(&previousTime, NULL);

    while (!StopRequested)
    {
        keyboard.Update();

        if (joystickInitialized)
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

        if (elapsedMilliseconds < 0)
            elapsedMilliseconds = 0;
        else if (elapsedMilliseconds > 50)
            elapsedMilliseconds = 50;

        int axisX = 0;
        int axisY = 0;

        if (joystickInitialized)
        {
            axisX = GetAxisMovement(joystick.GetAxisX());
            axisY = GetAxisMovement(joystick.GetAxisY());
        }

        // Smooth analog movement.
        scrollX16 +=
            static_cast<int64_t>(axisX) * 180 *
            elapsedMilliseconds * 65536 / 1000000;

        scrollY16 +=
            static_cast<int64_t>(axisY) * 180 *
            elapsedMilliseconds * 65536 / 1000000;

        // Keyboard directional movement.
        if (keyboard.IsDown(Keyboard::KeyLeft))
            scrollX16 -=
                static_cast<int64_t>(180) *
                elapsedMilliseconds * 65536 / 1000;

        if (keyboard.IsDown(Keyboard::KeyRight))
            scrollX16 +=
                static_cast<int64_t>(180) *
                elapsedMilliseconds * 65536 / 1000;

        if (keyboard.IsDown(Keyboard::KeyUp))
            scrollY16 -=
                static_cast<int64_t>(180) *
                elapsedMilliseconds * 65536 / 1000;

        if (keyboard.IsDown(Keyboard::KeyDown))
            scrollY16 +=
                static_cast<int64_t>(180) *
                elapsedMilliseconds * 65536 / 1000;

        // D-pad and analog directional state.
        if (joystickInitialized)
        {
            if (joystick.IsDirectionPressed(Joystick::DirectionUp))
            {
                // Direction state is already represented by the analog
                // stick for analog movement. D-pad input can be used here
                // independently if desired.
            }
        }

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
