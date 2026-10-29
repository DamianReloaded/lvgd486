#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/io.h>
#include <linux/kd.h>

#include "../../vgd/Font/BmpFont.hpp"
#include "../../vgd/Font/BmpFont01.hpp"

using namespace vgd;

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200
#define FRAMEBUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)
#define VGA_ADDRESS 0xA0000UL
#define VGA_WINDOW_SIZE 65536
#define MAX_FONT_CHARS 512
#define FONT_BYTES_PER_CHAR 32

static volatile sig_atomic_t StopRequested = 0;
static uint8_t BackBuffer[FRAMEBUFFER_SIZE];

static void HandleSignal(int signalNumber)
{
    (void)signalNumber;
    StopRequested = 1;
}

static inline void OutByte(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t InByte(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

struct ConsoleFont
{
    unsigned int Width;
    unsigned int Height;
    unsigned int CharCount;
    uint8_t Data[MAX_FONT_CHARS * FONT_BYTES_PER_CHAR];
};

struct Mode13hVideo
{
    int ConsoleFd;
    int MemoryFd;
    uint8_t *Framebuffer;

    int IoPermissionEnabled;
    int TerminalStateSaved;
    int ConsoleModeSaved;
    int FontSaved;
    int GraphicsModeEnabled;
    int VgaModeSet;
    int Initialized;

    int OriginalConsoleMode;
    struct termios OriginalTerminal;
    struct ConsoleFont OriginalFont;
};

static void SetMode13h(void);
static void SetTextMode(void);
static void WaitForVerticalRetrace(void);
static void RestoreConsoleFont(struct Mode13hVideo *video);

static void VideoInitState(struct Mode13hVideo *video)
{
    memset(video, 0, sizeof(*video));
    video->ConsoleFd = -1;
    video->MemoryFd = -1;
    video->OriginalConsoleMode = KD_TEXT;
}

static int SaveConsoleFont(struct Mode13hVideo *video)
{
    struct console_font_op operation;

    memset(&operation, 0, sizeof(operation));
    operation.op = KD_FONT_OP_GET;
    operation.flags = 0;
    operation.width = 32;
    operation.height = 32;
    operation.charcount = MAX_FONT_CHARS;
    operation.data = video->OriginalFont.Data;

    if (ioctl(video->ConsoleFd, KDFONTOP, &operation) < 0)
    {
        perror("KDFONTOP GET");
        return 0;
    }

    if (operation.charcount == 0 ||
        operation.charcount > MAX_FONT_CHARS ||
        operation.width == 0 ||
        operation.height == 0)
    {
        fprintf(stderr, "Invalid console font information.\n");
        return 0;
    }

    /*
     * The kernel console font interface stores glyph data in
     * 32 bytes per character. The fixed buffer above therefore
     * accommodates at most MAX_FONT_CHARS glyphs.
     */
    video->OriginalFont.Width = operation.width;
    video->OriginalFont.Height = operation.height;
    video->OriginalFont.CharCount = operation.charcount;
    video->FontSaved = 1;

    return 1;
}

static void RestoreConsoleFont(struct Mode13hVideo *video)
{
    struct console_font_op operation;

    if (!video->FontSaved || video->ConsoleFd < 0)
        return;

    memset(&operation, 0, sizeof(operation));
    operation.op = KD_FONT_OP_SET;
    operation.flags = 0;
    operation.width = video->OriginalFont.Width;
    operation.height = video->OriginalFont.Height;
    operation.charcount = video->OriginalFont.CharCount;
    operation.data = video->OriginalFont.Data;

    if (ioctl(video->ConsoleFd, KDFONTOP, &operation) < 0)
        perror("KDFONTOP SET");

    video->FontSaved = 0;
}

static int VideoInitialize(struct Mode13hVideo *video)
{
    void *mapping;
    struct termios rawTerminal;

    if (video->Initialized)
        return 1;

    video->ConsoleFd = open("/dev/console", O_RDWR);

    if (video->ConsoleFd < 0)
    {
        perror("open /dev/console");
        return 0;
    }

    if (ioctl(video->ConsoleFd, KDGETMODE,
              &video->OriginalConsoleMode) < 0)
    {
        perror("KDGETMODE");
        return 0;
    }

    video->ConsoleModeSaved = 1;

    if (video->OriginalConsoleMode != KD_TEXT)
    {
        fprintf(stderr,
                "Run from a text-mode Linux virtual console.\n");
        return 0;
    }

    if (tcgetattr(video->ConsoleFd, &video->OriginalTerminal) < 0)
    {
        perror("tcgetattr");
        return 0;
    }

    video->TerminalStateSaved = 1;

    if (!SaveConsoleFont(video))
        return 0;

    if (ioperm(0x3C0, 0x20, 1) < 0)
    {
        perror("ioperm");
        return 0;
    }

    video->IoPermissionEnabled = 1;

    video->MemoryFd = open("/dev/mem", O_RDWR | O_SYNC);

    if (video->MemoryFd < 0)
    {
        perror("open /dev/mem");
        return 0;
    }

    mapping = mmap(NULL, VGA_WINDOW_SIZE,
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED, video->MemoryFd, VGA_ADDRESS);

    if (mapping == MAP_FAILED)
    {
        perror("mmap VGA framebuffer");
        return 0;
    }

    video->Framebuffer = (uint8_t *)mapping;

    rawTerminal = video->OriginalTerminal;
    rawTerminal.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    rawTerminal.c_cc[VMIN] = 0;
    rawTerminal.c_cc[VTIME] = 0;

    if (tcsetattr(video->ConsoleFd, TCSANOW, &rawTerminal) < 0)
    {
        perror("tcsetattr raw");
        return 0;
    }

    if (ioctl(video->ConsoleFd, KDSETMODE, KD_GRAPHICS) < 0)
    {
        perror("KDSETMODE KD_GRAPHICS");
        return 0;
    }

    video->GraphicsModeEnabled = 1;

    SetMode13h();

    video->VgaModeSet = 1;
    video->Initialized = 1;

    return 1;
}

static int VideoPresent(struct Mode13hVideo *video,
                        const uint8_t *backBuffer)
{
    if (!video->Initialized ||
        !video->Framebuffer ||
        !backBuffer)
        return 0;

    WaitForVerticalRetrace();
    memcpy(video->Framebuffer, backBuffer, FRAMEBUFFER_SIZE);

    return 1;
}

static void VideoShutdown(struct Mode13hVideo *video)
{
    if (video->VgaModeSet && video->IoPermissionEnabled)
    {
        SetTextMode();
        video->VgaModeSet = 0;
    }

    if (video->ConsoleFd >= 0 &&
        video->ConsoleModeSaved &&
        video->GraphicsModeEnabled)
    {
        if (ioctl(video->ConsoleFd, KDSETMODE,
                  video->OriginalConsoleMode) < 0)
            perror("KDSETMODE restore");

        video->GraphicsModeEnabled = 0;
    }

    RestoreConsoleFont(video);

    if (video->ConsoleFd >= 0 && video->TerminalStateSaved)
    {
        if (tcsetattr(video->ConsoleFd, TCSANOW,
                      &video->OriginalTerminal) < 0)
            perror("tcsetattr restore");

        video->TerminalStateSaved = 0;
    }

    if (video->Framebuffer)
    {
        munmap(video->Framebuffer, VGA_WINDOW_SIZE);
        video->Framebuffer = NULL;
    }

    if (video->MemoryFd >= 0)
    {
        close(video->MemoryFd);
        video->MemoryFd = -1;
    }

    if (video->IoPermissionEnabled)
    {
        if (ioperm(0x3C0, 0x20, 0) < 0)
            perror("ioperm revoke");

        video->IoPermissionEnabled = 0;
    }

    if (video->ConsoleFd >= 0)
    {
        close(video->ConsoleFd);
        video->ConsoleFd = -1;
    }

    video->Initialized = 0;
}

static void SetMode13h(void)
{
    static const uint8_t Sequencer[5] =
        { 0x03, 0x01, 0x0F, 0x00, 0x0E };

    static const uint8_t CRTC[25] =
    {
        0x5F, 0x4F, 0x50, 0x82, 0x54,
        0x80, 0xBF, 0x1F, 0x00, 0x41,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x9C, 0x0E, 0x8F, 0x28,
        0x40, 0x96, 0xB9, 0xA3, 0xFF
    };

    static const uint8_t Graphics[9] =
        { 0x00, 0x00, 0x00, 0x00, 0x00,
          0x40, 0x05, 0x0F, 0xFF };

    static const uint8_t Attribute[21] =
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
        0x0F, 0x00, 0x00
    };

    int i;

    OutByte(0x3C2, 0x63);
    OutByte(0x3C4, 0x00);
    OutByte(0x3C5, 0x01);

    for (i = 1; i < 5; ++i)
    {
        OutByte(0x3C4, (uint8_t)i);
        OutByte(0x3C5, Sequencer[i]);
    }

    OutByte(0x3C4, 0x00);
    OutByte(0x3C5, Sequencer[0]);

    OutByte(0x3D4, 0x11);
    OutByte(0x3D5, (uint8_t)(CRTC[0x11] & 0x7F));

    for (i = 0; i < 25; ++i)
    {
        if (i == 0x11)
            continue;

        OutByte(0x3D4, (uint8_t)i);
        OutByte(0x3D5, CRTC[i]);
    }

    OutByte(0x3D4, 0x11);
    OutByte(0x3D5, CRTC[0x11]);

    for (i = 0; i < 9; ++i)
    {
        OutByte(0x3CE, (uint8_t)i);
        OutByte(0x3CF, Graphics[i]);
    }

    for (i = 0; i < 21; ++i)
    {
        (void)InByte(0x3DA);
        OutByte(0x3C0, (uint8_t)i);
        OutByte(0x3C0, Attribute[i]);
    }

    (void)InByte(0x3DA);
    OutByte(0x3C0, 0x20);
    OutByte(0x3C6, 0xFF);
    OutByte(0x3C8, 0);

    for (i = 0; i < 256; ++i)
    {
        uint8_t red = 0;
        uint8_t green = 0;
        uint8_t blue = 0;

        if (i < 16)
        {
            red = (i & 4) ? 42 : 0;
            green = (i & 2) ? 42 : 0;
            blue = (i & 1) ? 42 : 0;

            if (i & 8)
            {
                if (red) red = 63;
                if (green) green = 63;
                if (blue) blue = 63;
            }
        }
        else
        {
            red = (uint8_t)((i >> 5) * 9);
            green = (uint8_t)(((i >> 2) & 7) * 9);
            blue = (uint8_t)((i & 3) * 21);
        }

        OutByte(0x3C9, red);
        OutByte(0x3C9, green);
        OutByte(0x3C9, blue);
    }

    (void)InByte(0x3DA);
    OutByte(0x3C0, 0x20);
}

static void SetTextMode(void)
{
    static const uint8_t Sequencer[5] =
        { 0x03, 0x00, 0x03, 0x00, 0x02 };

    static const uint8_t CRTC[25] =
    {
        0x5F, 0x4F, 0x50, 0x82, 0x55,
        0x81, 0xBF, 0x1F, 0x00, 0x4F,
        0x0D, 0x0E, 0x00, 0x00, 0x00,
        0x50, 0x9C, 0x0E, 0x8F, 0x28,
        0x1F, 0x96, 0xB9, 0xA3, 0xFF
    };

    static const uint8_t Graphics[9] =
        { 0x00, 0x00, 0x00, 0x00, 0x00,
          0x10, 0x0E, 0x00, 0xFF };

    static const uint8_t Attribute[21] =
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x0C, 0x00,
        0x0F, 0x08, 0x00
    };

    int i;

    OutByte(0x3C2, 0x67);
    OutByte(0x3C4, 0x00);
    OutByte(0x3C5, 0x01);

    for (i = 1; i < 5; ++i)
    {
        OutByte(0x3C4, (uint8_t)i);
        OutByte(0x3C5, Sequencer[i]);
    }

    OutByte(0x3C4, 0x00);
    OutByte(0x3C5, Sequencer[0]);

    OutByte(0x3D4, 0x11);
    OutByte(0x3D5, (uint8_t)(CRTC[0x11] & 0x7F));

    for (i = 0; i < 25; ++i)
    {
        if (i == 0x11)
            continue;

        OutByte(0x3D4, (uint8_t)i);
        OutByte(0x3D5, CRTC[i]);
    }

    OutByte(0x3D4, 0x11);
    OutByte(0x3D5, CRTC[0x11]);

    for (i = 0; i < 9; ++i)
    {
        OutByte(0x3CE, (uint8_t)i);
        OutByte(0x3CF, Graphics[i]);
    }

    for (i = 0; i < 21; ++i)
    {
        (void)InByte(0x3DA);
        OutByte(0x3C0, (uint8_t)i);
        OutByte(0x3C0, Attribute[i]);
    }

    (void)InByte(0x3DA);
    OutByte(0x3C0, 0x20);
}

static void WaitForVerticalRetrace(void)
{
    unsigned int count = 100000;

    while ((InByte(0x3DA) & 0x08) && --count)
        ;

    count = 100000;

    while (!(InByte(0x3DA) & 0x08) && --count)
        ;
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

static void PutPixel(uint8_t *framebuffer,
                     int x, int y, uint8_t color)
{
    if (x >= 0 && x < SCREEN_WIDTH &&
        y >= 0 && y < SCREEN_HEIGHT)
        framebuffer[y * SCREEN_WIDTH + x] = color;
}

/*
 * BMPFontDrawText is templated on PixelWriter and calls it as:
 *     putPixel(x, y, color)
 *
 * Keep this callable object rather than passing a C callback
 * and a separate context pointer.
 */
struct PixelWriter
{
    uint8_t *Framebuffer;

    void operator()(int x, int y, uint8_t color) const
    {
        PutPixel(Framebuffer, x, y, color);
    }
};

static void ClearScreen(uint8_t color)
{
    memset(BackBuffer, color, sizeof(BackBuffer));
}

static void DrawRectangle(int x, int y,
                          int width, int height, uint8_t color)
{
    int px;
    int py;

    for (px = x; px < x + width; ++px)
    {
        PutPixel(BackBuffer, px, y, color);
        PutPixel(BackBuffer, px, y + height - 1, color);
    }

    for (py = y; py < y + height; ++py)
    {
        PutPixel(BackBuffer, x, py, color);
        PutPixel(BackBuffer, x + width - 1, py, color);
    }
}

static void DrawText(const char *text, int x, int y, uint8_t color)
{
    PixelWriter writer = { BackBuffer };

    BMPFontDrawText(
        FontFps, text, x, y, 8, 8, 1, color,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        writer);
}

static char *AppendText(char *p, const char *s)
{
    while (*s)
        *p++ = *s++;

    return p;
}

static char *AppendUnsigned(char *p, unsigned int value)
{
    char digits[10];
    unsigned int count = 0;

    do
    {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    }
    while (value);

    while (count)
        *p++ = digits[--count];

    return p;
}

static void FormatFPS(char *buffer, unsigned int fps)
{
    char *p = buffer;

    p = AppendText(p, "FPS ");
    p = AppendUnsigned(p, fps);
    *p = '\0';
}

static void DrawFrame(unsigned long frameNumber, unsigned int fps)
{
    int x;
    int y;
    int scrollX = (int)(frameNumber % 64);
    int scrollY = (int)((frameNumber * 2) % 64);

    ClearScreen(0);

    for (y = 0; y < SCREEN_HEIGHT; ++y)
    {
        int patternY = (y + scrollY) / 8;

        for (x = 0; x < SCREEN_WIDTH; ++x)
        {
            int patternX = (x + scrollX) / 8;
            int pattern = (patternX + patternY) & 15;

            BackBuffer[y * SCREEN_WIDTH + x] =
                (uint8_t)(pattern + 1);
        }
    }

    DrawRectangle(24, 32, 272, 136, 15);
    DrawRectangle(28, 36, 264, 128, 4);
    DrawText("MODE 13H", 112, 48, 15);

    {
        char text[16];

        FormatFPS(text, fps);
        DrawText(text, 112, 64, 14);
    }

    DrawText("PRESS ANY KEY", 96, 144, 15);
}

static int RunGame(struct Mode13hVideo *video)
{
    struct timeval previousTime;
    unsigned long frameNumber = 0;
    unsigned long fpsFrames = 0;
    unsigned int fps = 0;

    if (!VideoInitialize(video))
    {
        VideoShutdown(video);
        return 1;
    }

    gettimeofday(&previousTime, NULL);

    while (!StopRequested)
    {
        struct timeval currentTime;
        struct timeval timeout;
        fd_set readSet;
        long elapsedMilliseconds;
        int result;

        DrawFrame(frameNumber, fps);

        if (!VideoPresent(video, BackBuffer))
            break;

        ++frameNumber;
        ++fpsFrames;

        gettimeofday(&currentTime, NULL);

        elapsedMilliseconds =
            (currentTime.tv_sec - previousTime.tv_sec) * 1000L +
            (currentTime.tv_usec - previousTime.tv_usec) / 1000L;

        if (elapsedMilliseconds >= 1000)
        {
            fps = (unsigned int)(
                fpsFrames * 1000UL /
                (unsigned long)elapsedMilliseconds);

            fpsFrames = 0;
            previousTime = currentTime;
        }

        FD_ZERO(&readSet);
        FD_SET(STDIN_FILENO, &readSet);

        timeout.tv_sec = 0;
        timeout.tv_usec = 1000;

        result = select(STDIN_FILENO + 1,
                        &readSet, NULL, NULL, &timeout);

        if (result > 0 && FD_ISSET(STDIN_FILENO, &readSet))
        {
            char key;

            if (read(STDIN_FILENO, &key, 1) > 0)
                break;
        }
        else if (result < 0 && errno != EINTR)
        {
            perror("select");
            break;
        }
    }

    VideoShutdown(video);
    return 0;
}

int main(void)
{
    struct sigaction action;
    struct Mode13hVideo video;

    memset(&action, 0, sizeof(action));
    action.sa_handler = HandleSignal;
    sigemptyset(&action.sa_mask);

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);

    VideoInitState(&video);
    return RunGame(&video);
}
