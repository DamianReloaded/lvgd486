
#pragma once

#include "IDisplay.hpp"
#include <cstdint>
#include <cstddef>
#include <termios.h>

namespace vgd
{
    class DisplayMode13h final : public IDisplay
    {
    public:
        DisplayMode13h();
        ~DisplayMode13h();

        DisplayMode13h(const DisplayMode13h&) = delete;
        DisplayMode13h& operator=(const DisplayMode13h&) = delete;

        bool Initialize() override;
        void Shutdown() override;
        bool Present() override;

        // Set the maximum presentation rate.
        // The default is 60 FPS.
        // Pass 0 to disable the frame limiter.
        // Valid rates are 0 through 1000 FPS.
        bool SetFrameRate(unsigned int framesPerSecond);

        int GetWidth() const override;
        int GetHeight() const override;

        void Clear(uint8_t color) override;
        void PutPixel(int x, int y, uint8_t color) override;
        void DrawRectangle(
            int x,
            int y,
            int width,
            int height,
            uint8_t color) override;

        void DrawText(
            const BMPFont& font,
            const char* text,
            int x,
            int y,
            uint8_t color) override;

        uint8_t* GetBackBuffer();
        int GetBackBufferPitch() const;
		bool SetPalette(const uint8_t* palette);
		
    private:
        static constexpr int VGA_ADDRESS = 0xA0000;
        static constexpr int VGA_WINDOW_SIZE = 65536;
        static constexpr int SCREEN_WIDTH = 320;
        static constexpr int SCREEN_HEIGHT = 200;
        static constexpr int FRAMEBUFFER_SIZE =
            SCREEN_WIDTH * SCREEN_HEIGHT;

        static constexpr int MAX_FONT_CHARS = 512;
        static constexpr int FONT_BYTES_PER_CHAR = 32;

        static constexpr unsigned int DEFAULT_FRAME_RATE = 60;
        static constexpr unsigned int MAX_FRAME_RATE = 1000;

        struct ConsoleFont
        {
            unsigned int Width;
            unsigned int Height;
            unsigned int CharCount;

            uint8_t Data[
                MAX_FONT_CHARS * FONT_BYTES_PER_CHAR];
        };

        int m_ConsoleFd;
        int m_MemoryFd;

        uint8_t* m_Framebuffer;
        uint8_t m_BackBuffer[FRAMEBUFFER_SIZE];

        bool m_IoPermissionEnabled;
        bool m_TerminalStateSaved;
        bool m_ConsoleModeSaved;
        bool m_FontSaved;
        bool m_GraphicsModeEnabled;
        bool m_VgaModeSet;
        bool m_Initialized;

        int m_OriginalConsoleMode;
        struct termios m_OriginalTerminal;
        struct ConsoleFont m_OriginalFont;

        // Frame-rate limiter state.
        unsigned int m_FrameRate;
        uint64_t m_FramePeriodUs;
        uint64_t m_NextFrameDeadlineUs;
        uint64_t m_LastFrameTimeUs;

        static uint64_t GetTimeMicroseconds();

        void LimitFrameRate();

        bool SaveConsoleFont();
        void RestoreConsoleFont();

        void SetMode13h();
        void SetTextMode();
        void WaitForVerticalRetrace();
    };
}
