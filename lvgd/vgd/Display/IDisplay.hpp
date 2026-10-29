#pragma once
#include <cstdint>

namespace vgd
{
    class BMPFont;

    class IDisplay
    {
    public:
        virtual ~IDisplay() {}
        virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;
        virtual bool Present() = 0;
        virtual int GetWidth() const = 0;
        virtual int GetHeight() const = 0;
        virtual void Clear(uint8_t color) = 0;
        virtual void PutPixel(int x, int y, uint8_t color) = 0;
        virtual void DrawRectangle(int x, int y, int width, int height, uint8_t color) = 0;
        virtual void DrawText(const BMPFont& font, const char *text, int x, int y, uint8_t color) = 0;
    };
}