
#include "DisplayMode13h.hpp"
#include "../Font/BmpFont.hpp"

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/io.h>
#include <linux/kd.h>

namespace vgd
{
    // -------------------------------------------------------------------------
    // VGA port access
    // -------------------------------------------------------------------------

    static inline void OutByte(uint16_t port, uint8_t value)
    {
        __asm__ volatile(
            "outb %0, %1"
            :
            : "a"(value), "Nd"(port));
    }

    static inline uint8_t InByte(uint16_t port)
    {
        uint8_t value;

        __asm__ volatile(
            "inb %1, %0"
            : "=a"(value)
            : "Nd"(port));

        return value;
    }

    // -------------------------------------------------------------------------
    // Timing
    // -------------------------------------------------------------------------

    uint64_t DisplayMode13h::GetTimeMicroseconds()
    {
        struct timeval tv;

        if (gettimeofday(&tv, NULL) != 0)
            return 0;

        return static_cast<uint64_t>(tv.tv_sec) * 1000000ULL
             + static_cast<uint64_t>(tv.tv_usec);
    }

    void DisplayMode13h::LimitFrameRate()
    {
        if (m_FrameRate == 0 || m_FramePeriodUs == 0)
            return;

        uint64_t currentTime = GetTimeMicroseconds();

        if (currentTime == 0)
            return;

        // Initialize the deadline on the first presentation.
        if (m_NextFrameDeadlineUs == 0)
        {
            m_NextFrameDeadlineUs =
                currentTime + m_FramePeriodUs;

            m_LastFrameTimeUs = currentTime;
            return;
        }

        // gettimeofday() is a wall-clock source, not a monotonic
        // clock. Recover if the system clock moves backwards.
        if (m_LastFrameTimeUs != 0 &&
            currentTime < m_LastFrameTimeUs)
        {
            m_NextFrameDeadlineUs =
                currentTime + m_FramePeriodUs;

            m_LastFrameTimeUs = currentTime;
            return;
        }

        // Sleep only when the frame finishes before its deadline.
        // Recheck the clock after sleeping because nanosleep()
        // may return early or be interrupted by a signal.
        while (currentTime < m_NextFrameDeadlineUs)
        {
            const uint64_t remainingUs =
                m_NextFrameDeadlineUs - currentTime;

            struct timespec sleepTime;

            sleepTime.tv_sec =
                static_cast<time_t>(remainingUs / 1000000ULL);

            sleepTime.tv_nsec =
                static_cast<long>(
                    (remainingUs % 1000000ULL) * 1000ULL);

            while (nanosleep(&sleepTime, &sleepTime) < 0)
            {
                if (errno == EINTR)
                    continue;

                // If sleeping fails, stop trying to sleep for
                // this frame. The next frame will retry normally.
                break;
            }

            const uint64_t updatedTime =
                GetTimeMicroseconds();

            if (updatedTime == 0)
                break;

            // Recover if the wall clock moves backwards while
            // we are waiting for the frame deadline.
            if (updatedTime < currentTime)
            {
                currentTime = updatedTime;

                m_NextFrameDeadlineUs =
                    currentTime + m_FramePeriodUs;

                m_LastFrameTimeUs = currentTime;
                return;
            }

            currentTime = updatedTime;
        }

        m_LastFrameTimeUs = currentTime;

        // Advance the deadline by whole frame periods. If the
        // application missed one or more deadlines, skip those
        // deadlines instead of trying to catch up with rapid
        // consecutive presentations.
        if (currentTime >= m_NextFrameDeadlineUs)
        {
            const uint64_t latenessUs =
                currentTime - m_NextFrameDeadlineUs;

            const uint64_t missedPeriods =
                latenessUs / m_FramePeriodUs;

            m_NextFrameDeadlineUs +=
                (missedPeriods + 1ULL) * m_FramePeriodUs;
        }
        else
        {
            m_NextFrameDeadlineUs += m_FramePeriodUs;
        }
    }

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    DisplayMode13h::DisplayMode13h()
        : m_ConsoleFd(-1),
          m_MemoryFd(-1),
          m_Framebuffer(NULL),
          m_IoPermissionEnabled(false),
          m_TerminalStateSaved(false),
          m_ConsoleModeSaved(false),
          m_FontSaved(false),
          m_GraphicsModeEnabled(false),
          m_VgaModeSet(false),
          m_Initialized(false),
          m_OriginalConsoleMode(KD_TEXT),
          m_FrameRate(DEFAULT_FRAME_RATE),
          m_FramePeriodUs(
              (1000000ULL + DEFAULT_FRAME_RATE / 2) /
              DEFAULT_FRAME_RATE),
          m_NextFrameDeadlineUs(0),
          m_LastFrameTimeUs(0)
    {
        memset(&m_OriginalTerminal, 0, sizeof(m_OriginalTerminal));
        memset(&m_OriginalFont, 0, sizeof(m_OriginalFont));
        memset(m_BackBuffer, 0, sizeof(m_BackBuffer));
    }

    DisplayMode13h::~DisplayMode13h()
    {
        Shutdown();
    }

    // -------------------------------------------------------------------------
    // Frame-rate configuration
    // -------------------------------------------------------------------------

    bool DisplayMode13h::SetFrameRate(unsigned int framesPerSecond)
    {
        if (framesPerSecond > MAX_FRAME_RATE)
            return false;

        m_FrameRate = framesPerSecond;

        if (framesPerSecond == 0)
        {
            m_FramePeriodUs = 0;
        }
        else
        {
            // Round to the nearest whole microsecond.
            m_FramePeriodUs =
                (1000000ULL + framesPerSecond / 2) /
                framesPerSecond;

            if (m_FramePeriodUs == 0)
                m_FramePeriodUs = 1;
        }

        // Start a fresh timing schedule after changing the rate.
        m_NextFrameDeadlineUs = 0;
        m_LastFrameTimeUs = 0;

        return true;
    }

    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    bool DisplayMode13h::Initialize()
    {
        void* mapping;
        struct termios rawTerminal;

        if (m_Initialized)
            return true;

        m_NextFrameDeadlineUs = 0;
        m_LastFrameTimeUs = 0;

        m_ConsoleFd = open("/dev/console", O_RDWR);

        if (m_ConsoleFd < 0)
        {
            perror("open /dev/console");
            return false;
        }

        if (ioctl(
                m_ConsoleFd,
                KDGETMODE,
                &m_OriginalConsoleMode) < 0)
        {
            perror("KDGETMODE");
            return false;
        }

        m_ConsoleModeSaved = true;

        if (m_OriginalConsoleMode != KD_TEXT)
        {
            fprintf(
                stderr,
                "Run from a text-mode Linux virtual console.\n");

            return false;
        }

        if (tcgetattr(
                m_ConsoleFd,
                &m_OriginalTerminal) < 0)
        {
            perror("tcgetattr");
            return false;
        }

        m_TerminalStateSaved = true;

        if (!SaveConsoleFont())
            return false;

        if (ioperm(0x3C0, 0x20, 1) < 0)
        {
            perror("ioperm");
            return false;
        }

        m_IoPermissionEnabled = true;

        m_MemoryFd = open("/dev/mem", O_RDWR | O_SYNC);

        if (m_MemoryFd < 0)
        {
            perror("open /dev/mem");
            return false;
        }

        mapping = mmap(
            NULL,
            VGA_WINDOW_SIZE,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            m_MemoryFd,
            VGA_ADDRESS);

        if (mapping == MAP_FAILED)
        {
            perror("mmap VGA framebuffer");
            return false;
        }

        m_Framebuffer = static_cast<uint8_t*>(mapping);

        rawTerminal = m_OriginalTerminal;

        rawTerminal.c_lflag &=
            static_cast<tcflag_t>(~(ICANON | ECHO));

        rawTerminal.c_cc[VMIN] = 0;
        rawTerminal.c_cc[VTIME] = 0;

        if (tcsetattr(
                m_ConsoleFd,
                TCSANOW,
                &rawTerminal) < 0)
        {
            perror("tcsetattr raw");
            return false;
        }

        if (ioctl(m_ConsoleFd, KDSETMODE, KD_GRAPHICS) < 0)
        {
            perror("KDSETMODE KD_GRAPHICS");
            return false;
        }

        m_GraphicsModeEnabled = true;

        SetMode13h();

        m_VgaModeSet = true;
        m_Initialized = true;

        return true;
    }

    // -------------------------------------------------------------------------
    // Shutdown
    // -------------------------------------------------------------------------

    void DisplayMode13h::Shutdown()
    {
        if (m_VgaModeSet && m_IoPermissionEnabled)
        {
            SetTextMode();
            m_VgaModeSet = false;
        }

        if (m_ConsoleFd >= 0 &&
            m_ConsoleModeSaved &&
            m_GraphicsModeEnabled)
        {
            if (ioctl(
                    m_ConsoleFd,
                    KDSETMODE,
                    m_OriginalConsoleMode) < 0)
            {
                perror("KDSETMODE restore");
            }

            m_GraphicsModeEnabled = false;
        }

        RestoreConsoleFont();

        if (m_ConsoleFd >= 0 && m_TerminalStateSaved)
        {
            if (tcsetattr(
                    m_ConsoleFd,
                    TCSANOW,
                    &m_OriginalTerminal) < 0)
            {
                perror("tcsetattr restore");
            }

            m_TerminalStateSaved = false;
        }

        if (m_Framebuffer)
        {
            if (munmap(
                    m_Framebuffer,
                    VGA_WINDOW_SIZE) < 0)
            {
                perror("munmap VGA framebuffer");
            }

            m_Framebuffer = NULL;
        }

        if (m_MemoryFd >= 0)
        {
            close(m_MemoryFd);
            m_MemoryFd = -1;
        }

        if (m_IoPermissionEnabled)
        {
            if (ioperm(0x3C0, 0x20, 0) < 0)
                perror("ioperm revoke");

            m_IoPermissionEnabled = false;
        }

        if (m_ConsoleFd >= 0)
        {
            close(m_ConsoleFd);
            m_ConsoleFd = -1;
        }

        m_ConsoleModeSaved = false;
        m_Initialized = false;

        m_NextFrameDeadlineUs = 0;
        m_LastFrameTimeUs = 0;
    }

    // -------------------------------------------------------------------------
    // Presentation
    // -------------------------------------------------------------------------

    bool DisplayMode13h::Present()
    {
        if (!m_Initialized || !m_Framebuffer)
            return false;

        // Retain VGA vertical-retrace synchronization where
        // the virtual or physical VGA implementation supports it.
        WaitForVerticalRetrace();

        memcpy(
            m_Framebuffer,
            m_BackBuffer,
            sizeof(m_BackBuffer));

        // Independently limit presentation when vertical-retrace
        // synchronization is ineffective or unavailable.
        //LimitFrameRate();

        return true;
    }

    // -------------------------------------------------------------------------
    // Display information
    // -------------------------------------------------------------------------

    int DisplayMode13h::GetWidth() const
    {
        return SCREEN_WIDTH;
    }

    int DisplayMode13h::GetHeight() const
    {
        return SCREEN_HEIGHT;
    }

    uint8_t* DisplayMode13h::GetBackBuffer()
    {
        return m_BackBuffer;
    }

    int DisplayMode13h::GetBackBufferPitch() const
    {
        return SCREEN_WIDTH;
    }

    // -------------------------------------------------------------------------
    // Drawing
    // -------------------------------------------------------------------------

    void DisplayMode13h::Clear(uint8_t color)
    {
        memset(m_BackBuffer, color, sizeof(m_BackBuffer));
    }

    void DisplayMode13h::PutPixel(
        int x,
        int y,
        uint8_t color)
    {
        if (x >= 0 && x < SCREEN_WIDTH &&
            y >= 0 && y < SCREEN_HEIGHT)
        {
            m_BackBuffer[y * SCREEN_WIDTH + x] = color;
        }
    }

    void DisplayMode13h::DrawRectangle(
        int x,
        int y,
        int width,
        int height,
        uint8_t color)
    {
        if (width <= 0 || height <= 0)
            return;

        const int right = x + width - 1;
        const int bottom = y + height - 1;

        if (y >= 0 && y < SCREEN_HEIGHT)
        {
            const int startX = x < 0 ? 0 : x;
            const int endX =
                right >= SCREEN_WIDTH
                    ? SCREEN_WIDTH - 1
                    : right;

            for (int px = startX; px <= endX; ++px)
            {
                m_BackBuffer[y * SCREEN_WIDTH + px] = color;
            }
        }

        if (bottom != y &&
            bottom >= 0 &&
            bottom < SCREEN_HEIGHT)
        {
            const int startX = x < 0 ? 0 : x;
            const int endX =
                right >= SCREEN_WIDTH
                    ? SCREEN_WIDTH - 1
                    : right;

            for (int px = startX; px <= endX; ++px)
            {
                m_BackBuffer[bottom * SCREEN_WIDTH + px] = color;
            }
        }

        const int startY = y < 0 ? 0 : y;
        const int endY =
            bottom >= SCREEN_HEIGHT
                ? SCREEN_HEIGHT - 1
                : bottom;

        if (x >= 0 && x < SCREEN_WIDTH)
        {
            for (int py = startY; py <= endY; ++py)
            {
                m_BackBuffer[py * SCREEN_WIDTH + x] = color;
            }
        }

        if (right != x &&
            right >= 0 &&
            right < SCREEN_WIDTH)
        {
            for (int py = startY; py <= endY; ++py)
            {
                m_BackBuffer[py * SCREEN_WIDTH + right] = color;
            }
        }
    }

    void DisplayMode13h::DrawText(
        const BMPFont& font,
        const char* text,
        int x,
        int y,
        uint8_t color)
    {
        if (!text)
            return;

        struct PixelWriter
        {
            uint8_t* Buffer;

            void operator()(
                int px,
                int py,
                uint8_t pixelColor) const
            {
                if (px >= 0 && px < SCREEN_WIDTH &&
                    py >= 0 && py < SCREEN_HEIGHT)
                {
                    Buffer[py * SCREEN_WIDTH + px] =
                        pixelColor;
                }
            }
        };

        PixelWriter writer = { m_BackBuffer };

        BMPFontDrawText(
            font,
            text,
            x,
            y,
            8,
            8,
            1,
            color,
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            writer);
    }

    // -------------------------------------------------------------------------
    // Console font preservation
    // -------------------------------------------------------------------------

    bool DisplayMode13h::SaveConsoleFont()
    {
        struct console_font_op operation;

        memset(&operation, 0, sizeof(operation));

        operation.op = KD_FONT_OP_GET;
        operation.flags = 0;
        operation.width = 32;
        operation.height = 32;
        operation.charcount = MAX_FONT_CHARS;

        operation.data =
            reinterpret_cast<unsigned char*>(
                m_OriginalFont.Data);

        if (ioctl(m_ConsoleFd, KDFONTOP, &operation) < 0)
        {
            perror("KDFONTOP GET");
            return false;
        }

        if (operation.charcount == 0 ||
            operation.charcount > MAX_FONT_CHARS ||
            operation.width == 0 ||
            operation.height == 0)
        {
            fprintf(
                stderr,
                "Invalid console font information.\n");

            return false;
        }

        m_OriginalFont.Width = operation.width;
        m_OriginalFont.Height = operation.height;
        m_OriginalFont.CharCount = operation.charcount;

        m_FontSaved = true;

        return true;
    }

    void DisplayMode13h::RestoreConsoleFont()
    {
        struct console_font_op operation;

        if (!m_FontSaved || m_ConsoleFd < 0)
            return;

        memset(&operation, 0, sizeof(operation));

        operation.op = KD_FONT_OP_SET;
        operation.flags = 0;
        operation.width = m_OriginalFont.Width;
        operation.height = m_OriginalFont.Height;
        operation.charcount = m_OriginalFont.CharCount;

        operation.data =
            reinterpret_cast<unsigned char*>(
                m_OriginalFont.Data);

        if (ioctl(m_ConsoleFd, KDFONTOP, &operation) < 0)
            perror("KDFONTOP SET");

        m_FontSaved = false;
    }

    // -------------------------------------------------------------------------
    // VGA Mode 13h
    // -------------------------------------------------------------------------

    void DisplayMode13h::SetMode13h()
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
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF };

        static const uint8_t Attribute[21] =
        {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
            0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
            0x0E, 0x0F, 0x41, 0x00, 0x0F, 0x00, 0x00
        };

        OutByte(0x3C2, 0x63);
        OutByte(0x3C4, 0x00);
        OutByte(0x3C5, 0x01);

        for (int i = 1; i < 5; ++i)
        {
            OutByte(0x3C4, static_cast<uint8_t>(i));
            OutByte(0x3C5, Sequencer[i]);
        }

        OutByte(0x3C4, 0x00);
        OutByte(0x3C5, Sequencer[0]);

        OutByte(0x3D4, 0x11);
        OutByte(
            0x3D5,
            static_cast<uint8_t>(CRTC[0x11] & 0x7F));

        for (int i = 0; i < 25; ++i)
        {
            if (i == 0x11)
                continue;

            OutByte(0x3D4, static_cast<uint8_t>(i));
            OutByte(0x3D5, CRTC[i]);
        }

        OutByte(0x3D4, 0x11);
        OutByte(0x3D5, CRTC[0x11]);

        for (int i = 0; i < 9; ++i)
        {
            OutByte(0x3CE, static_cast<uint8_t>(i));
            OutByte(0x3CF, Graphics[i]);
        }

        for (int i = 0; i < 21; ++i)
        {
            (void)InByte(0x3DA);

            OutByte(0x3C0, static_cast<uint8_t>(i));
            OutByte(0x3C0, Attribute[i]);
        }

        (void)InByte(0x3DA);

        OutByte(0x3C0, 0x20);
        OutByte(0x3C6, 0xFF);
        OutByte(0x3C8, 0);

        for (int i = 0; i < 256; ++i)
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
                    if (red)
                        red = 63;

                    if (green)
                        green = 63;

                    if (blue)
                        blue = 63;
                }
            }
            else
            {
                red = static_cast<uint8_t>((i >> 5) * 9);

                green = static_cast<uint8_t>(
                    ((i >> 2) & 7) * 9);

                blue = static_cast<uint8_t>(
                    (i & 3) * 21);
            }

            OutByte(0x3C9, red);
            OutByte(0x3C9, green);
            OutByte(0x3C9, blue);
        }

        (void)InByte(0x3DA);
        OutByte(0x3C0, 0x20);
    }

    // -------------------------------------------------------------------------
    // VGA text mode restoration
    // -------------------------------------------------------------------------

    void DisplayMode13h::SetTextMode()
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
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF };

        static const uint8_t Attribute[21] =
        {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
            0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
            0x0E, 0x0F, 0x0C, 0x00, 0x0F, 0x08, 0x00
        };

        OutByte(0x3C2, 0x67);
        OutByte(0x3C4, 0x00);
        OutByte(0x3C5, 0x01);

        for (int i = 1; i < 5; ++i)
        {
            OutByte(0x3C4, static_cast<uint8_t>(i));
            OutByte(0x3C5, Sequencer[i]);
        }

        OutByte(0x3C4, 0x00);
        OutByte(0x3C5, Sequencer[0]);

        OutByte(0x3D4, 0x11);
        OutByte(
            0x3D5,
            static_cast<uint8_t>(CRTC[0x11] & 0x7F));

        for (int i = 0; i < 25; ++i)
        {
            if (i == 0x11)
                continue;

            OutByte(0x3D4, static_cast<uint8_t>(i));
            OutByte(0x3D5, CRTC[i]);
        }

        OutByte(0x3D4, 0x11);
        OutByte(0x3D5, CRTC[0x11]);

        for (int i = 0; i < 9; ++i)
        {
            OutByte(0x3CE, static_cast<uint8_t>(i));
            OutByte(0x3CF, Graphics[i]);
        }

        for (int i = 0; i < 21; ++i)
        {
            (void)InByte(0x3DA);

            OutByte(0x3C0, static_cast<uint8_t>(i));
            OutByte(0x3C0, Attribute[i]);
        }

        (void)InByte(0x3DA);
        OutByte(0x3C0, 0x20);
    }

    // -------------------------------------------------------------------------
    // Vertical retrace
    // -------------------------------------------------------------------------

    void DisplayMode13h::WaitForVerticalRetrace()
    {
        unsigned int count = 100000;

        // If currently in retrace, wait for it to end.
        while ((InByte(0x3DA) & 0x08) && --count)
        {
        }

        count = 100000;

        // Wait for the next retrace to begin.
        // The timeout prevents an infinite loop if the VGA
        // status bit is not implemented correctly by a VM.
        while (!(InByte(0x3DA) & 0x08) && --count)
        {
        }
    }
	
    // -------------------------------------------------------------------------
    // Palette
    // -------------------------------------------------------------------------

    bool DisplayMode13h::SetPalette(const uint8_t* palette)
    {
        if (!m_Initialized || !m_IoPermissionEnabled || !palette)
            return false;

        // Select the first VGA DAC palette entry.
        OutByte(0x3C8, 0);

        // Upload all 256 RGB entries.
        for (int i = 0; i < 256; ++i)
        {
            OutByte(0x3C9, palette[0] >> 2);
            OutByte(0x3C9, palette[1] >> 2);
            OutByte(0x3C9, palette[2] >> 2);

            palette += 3;
        }

        return true;
    }
}
