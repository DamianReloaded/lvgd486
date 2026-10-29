#include "../../vgd/Display/DisplayMode13h.hpp"
#include "../../vgd/Input/Keyboard.hpp"
#include "../../vgd/Input/Mouse.hpp"
#include "../../vgd/Font/BmpFont.hpp"
#include "../../vgd/Font/BmpFont01.hpp"

#include <linux/input.h>
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

static void DrawBackground(DisplayMode13h& display,
                           unsigned long frameNumber)
{
    uint8_t* backBuffer = display.GetBackBuffer();
    const int pitch = display.GetBackBufferPitch();
    const int width = display.GetWidth();
    const int height = display.GetHeight();

    const int scrollX = static_cast<int>(frameNumber % 64);
    const int scrollY = static_cast<int>((frameNumber * 2) % 64);

    for (int y = 0; y < height; ++y)
    {
        uint8_t* row = backBuffer + y * pitch;
        const int patternY = (y + scrollY) / 8;

        for (int x = 0; x < width; ++x)
        {
            const int patternX = (x + scrollX) / 8;

            row[x] = static_cast<uint8_t>(
                ((patternX + patternY) & 15) + 1);
        }
    }
}

static void DrawFrame(DisplayMode13h& display,
                      unsigned long frameNumber,
                      unsigned int fps,
                      int textX,
                      int textY)
{
    DrawBackground(display, frameNumber);

    display.DrawRectangle(24, 32, 272, 136, 15);
    display.DrawRectangle(28, 36, 264, 128, 4);

    display.DrawText(FontFps, "MODE 13H", textX, textY, 15);

    char text[16];
    FormatFPS(text, fps);

    display.DrawText(FontFps, text, 112, 64, 14);
    display.DrawText(FontFps, "PRESS ANY KEY", 96, 144, 15);
}

static int RunGame(DisplayMode13h& display)
{
    Keyboard keyboard;
    Mouse mouse;

    bool displayInitialized = false;
    bool keyboardInitialized = false;
    bool mouseInitialized = false;
    int result = 0;

    unsigned long frameNumber = 0;
    unsigned long fpsFrames = 0;
    unsigned int fps = 0;

    int textX = 112;
    int textY = 48;

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

    if (!mouse.Initialize())
    {
        fprintf(stderr, "Game: mouse initialization failed.\n");
        result = 1;
        goto cleanup;
    }

    mouseInitialized = true;

    gettimeofday(&previousTime, NULL);

    while (!StopRequested)
    {
        keyboard.Update();
        mouse.Update();

        if (!keyboard.IsConsoleActive())
            break;

        const bool ctrlDown =
            keyboard.IsDown(Keyboard::KeyLeftCtrl) ||
            keyboard.IsDown(Keyboard::KeyRightCtrl);

        const bool cDown = keyboard.IsDown(Keyboard::KeyC);
        const bool escapeDown = keyboard.IsDown(Keyboard::KeyEscape);

        if ((ctrlDown && cDown) || escapeDown)
            break;

        bool keyPressed = false;

        for (int key = 1; key < Keyboard::KeyCount; ++key)
        {
            if (keyboard.WasPressed(
                    static_cast<Keyboard::Key>(key)))
            {
                keyPressed = true;
                break;
            }
        }

        if (keyPressed)
            break;

        textX += mouse.GetDeltaX();
        textY += mouse.GetDeltaY();

        const int textWidth = FontFps.Width * 8;
        const int textHeight = FontFps.Height;

        const int maxX =
            display.GetWidth() > textWidth
                ? display.GetWidth() - textWidth
                : 0;

        const int maxY =
            display.GetHeight() > textHeight
                ? display.GetHeight() - textHeight
                : 0;

        if (textX < 0)
            textX = 0;
        else if (textX > maxX)
            textX = maxX;

        if (textY < 0)
            textY = 0;
        else if (textY > maxY)
            textY = maxY;

        DrawFrame(display, frameNumber, fps, textX, textY);

        if (!display.Present())
        {
            fprintf(stderr, "Game: display presentation failed.\n");
            result = 1;
            break;
        }

        ++frameNumber;
        ++fpsFrames;

        struct timeval currentTime;
        gettimeofday(&currentTime, NULL);

        const long elapsedMilliseconds =
            (currentTime.tv_sec - previousTime.tv_sec) * 1000L +
            (currentTime.tv_usec - previousTime.tv_usec) / 1000L;

        if (elapsedMilliseconds >= 1000)
        {
            fps = static_cast<unsigned int>(
                fpsFrames * 1000UL /
                static_cast<unsigned long>(elapsedMilliseconds));

            fpsFrames = 0;
            previousTime = currentTime;
        }
    }

cleanup:
    if (mouseInitialized)
        mouse.Shutdown();

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