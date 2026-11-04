#pragma once

#include <stdlib.h>
#include <string.h>
#include <limits.h>

namespace vgd
{

class Image
{
public:
    Image()
        : m_Pixels(NULL),
          m_Width(0),
          m_Height(0)
    {
    }

    ~Image()
    {
        Destroy();
    }

    Image(const Image &) = delete;
    Image &operator=(const Image &) = delete;

    bool Create(int width, int height)
    {
        if (width <= 0 || height <= 0)
            return false;

        if (width > INT_MAX / height)
            return false;

        unsigned char *pixels = new unsigned char[width * height];

        delete[] m_Pixels;

        m_Pixels = pixels;
        m_Width = width;
        m_Height = height;

        memset(m_Pixels, 0, width * height);

        return true;
    }

    void Destroy()
    {
        delete[] m_Pixels;

        m_Pixels = NULL;
        m_Width = 0;
        m_Height = 0;
    }

    void Swap(Image &other)
    {
        unsigned char *pixels = m_Pixels;
        int width = m_Width;
        int height = m_Height;

        m_Pixels = other.m_Pixels;
        m_Width = other.m_Width;
        m_Height = other.m_Height;

        other.m_Pixels = pixels;
        other.m_Width = width;
        other.m_Height = height;
    }

    bool IsValid() const
    {
        return m_Pixels != NULL;
    }

    int GetWidth() const
    {
        return m_Width;
    }

    int GetHeight() const
    {
        return m_Height;
    }

    unsigned char *GetPixels()
    {
        return m_Pixels;
    }

    const unsigned char *GetPixels() const
    {
        return m_Pixels;
    }

    unsigned char *GetRow(int y)
    {
        if (y < 0 || y >= m_Height)
            return NULL;

        return m_Pixels + y * m_Width;
    }

    const unsigned char *GetRow(int y) const
    {
        if (y < 0 || y >= m_Height)
            return NULL;

        return m_Pixels + y * m_Width;
    }

    unsigned char GetPixel(int x, int y) const
    {
        if (x < 0 || x >= m_Width ||
            y < 0 || y >= m_Height)
        {
            return 0;
        }

        return m_Pixels[y * m_Width + x];
    }

    void SetPixel(int x, int y, unsigned char color)
    {
        if (x < 0 || x >= m_Width ||
            y < 0 || y >= m_Height)
        {
            return;
        }

        m_Pixels[y * m_Width + x] = color;
    }

    // Opaque blit to a 320x200 framebuffer.

    void Blit(unsigned char *destination, int x, int y) const
    {
        Blit(destination, 320, 200, 320, x, y);
    }

    // Opaque blit with arbitrary destination dimensions and pitch.

    void Blit(unsigned char *destination, int destWidth, int destHeight,
              int destPitch, int x, int y) const
    {
        if (m_Pixels == NULL || destination == NULL)
            return;

        if (destWidth <= 0 || destHeight <= 0 ||
            destPitch < destWidth)
        {
            return;
        }

        int srcX = 0;
        int srcY = 0;
        int copyWidth = m_Width;
        int copyHeight = m_Height;

        if (x < 0)
        {
            srcX = -x;
            copyWidth -= srcX;
            x = 0;
        }

        if (y < 0)
        {
            srcY = -y;
            copyHeight -= srcY;
            y = 0;
        }

        if (x + copyWidth > destWidth)
            copyWidth = destWidth - x;

        if (y + copyHeight > destHeight)
            copyHeight = destHeight - y;

        if (copyWidth <= 0 || copyHeight <= 0)
            return;

        for (int row = 0; row < copyHeight; ++row)
        {
            const unsigned char *src =
                m_Pixels + (srcY + row) * m_Width + srcX;

            unsigned char *dst =
                destination + (y + row) * destPitch + x;

            memcpy(dst, src, copyWidth);
        }
    }

    // Transparent blit, useful for sprites.
    // Pixels matching transparentColor are skipped.

    void BlitTransparent(unsigned char *destination, int x, int y,
                         unsigned char transparentColor) const
    {
        BlitTransparent(destination, 320, 200, 320,
                        x, y, transparentColor);
    }

    void BlitTransparent(unsigned char *destination,
                         int destWidth, int destHeight, int destPitch,
                         int x, int y,
                         unsigned char transparentColor) const
    {
        if (m_Pixels == NULL || destination == NULL)
            return;

        if (destWidth <= 0 || destHeight <= 0 ||
            destPitch < destWidth)
        {
            return;
        }

        int srcX = 0;
        int srcY = 0;
        int copyWidth = m_Width;
        int copyHeight = m_Height;

        if (x < 0)
        {
            srcX = -x;
            copyWidth -= srcX;
            x = 0;
        }

        if (y < 0)
        {
            srcY = -y;
            copyHeight -= srcY;
            y = 0;
        }

        if (x + copyWidth > destWidth)
            copyWidth = destWidth - x;

        if (y + copyHeight > destHeight)
            copyHeight = destHeight - y;

        if (copyWidth <= 0 || copyHeight <= 0)
            return;

        for (int row = 0; row < copyHeight; ++row)
        {
            const unsigned char *src =
                m_Pixels + (srcY + row) * m_Width + srcX;

            unsigned char *dst =
                destination + (y + row) * destPitch + x;

            for (int col = 0; col < copyWidth; ++col)
            {
                if (src[col] != transparentColor)
                    dst[col] = src[col];
            }
        }
    }

private:
    unsigned char *m_Pixels;
    int m_Width;
    int m_Height;
};

}