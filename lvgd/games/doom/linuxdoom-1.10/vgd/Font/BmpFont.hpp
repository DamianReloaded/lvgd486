#pragma once
#include <stdint.h>

namespace vgd
{
    struct BMPFont
    {
        int Width;
        int Height;
        int FirstDigit;
        int DigitCount;
        int FirstUpper;
        int UpperCount;
        const uint8_t *Space;
        const uint8_t *Digits;
        const uint8_t *Upper;
    };

    inline const uint8_t *BMPFontGetGlyph(const BMPFont &font, unsigned char character)
    {
        if (character == ' ')
            return font.Space;

        if (character >= 'a' && character <= 'z')
            character = (unsigned char)(character - 'a' + 'A');

        if (character >= font.FirstDigit && character < font.FirstDigit + font.DigitCount)
            return font.Digits + (character - font.FirstDigit) * font.Height;

        if (character >= font.FirstUpper && character < font.FirstUpper + font.UpperCount)
            return font.Upper + (character - font.FirstUpper) * font.Height;

        return font.Space;
    }

    template <typename PixelWriter>
    inline void BMPFontDrawGlyph(const BMPFont &font, unsigned char character, int x, int y, int width, int height, uint8_t color, int framebufferWidth, int framebufferHeight, PixelWriter putPixel)
    {
        if (!font.Space || !font.Digits || !font.Upper || font.Width <= 0 || font.Height <= 0 || width <= 0 || height <= 0)
            return;

        const uint8_t *glyph = BMPFontGetGlyph(font, character);

        for (int dy = 0; dy < height; ++dy)
        {
            int py = y + dy;
            if (py < 0 || py >= framebufferHeight)
                continue;

            int sy = dy * font.Height / height;
            uint8_t bits = glyph[sy];

            for (int dx = 0; dx < width; ++dx)
            {
                int px = x + dx;
                if (px < 0 || px >= framebufferWidth)
                    continue;

                int sx = dx * font.Width / width;
                if (bits & (0x80 >> sx))
                    putPixel(px, py, color);
            }
        }
    }

    template <typename PixelWriter>
    inline void BMPFontDrawText(const BMPFont &font, const char *text, int x, int y, int width, int height, int spacing, uint8_t color, int framebufferWidth, int framebufferHeight, PixelWriter putPixel)
    {
        if (!text || width <= 0 || height <= 0)
            return;

        while (*text)
        {
            BMPFontDrawGlyph(font, (unsigned char)*text, x, y, width, height, color, framebufferWidth, framebufferHeight, putPixel);
            x += width + spacing;
            ++text;
        }
    }
}