#pragma once

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "Image.hpp"

namespace vgd
{

class BmpLoader
{
private:
    struct Header
    {
        unsigned int PixelOffset;
        unsigned int DibSize;
        unsigned int ColorsUsed;
        int Width;
        int Height;
        unsigned short Planes;
        unsigned short BitsPerPixel;
        unsigned int Compression;
    };

    static unsigned short ReadU16(const unsigned char *data)
    {
        return static_cast<unsigned short>(
            data[0] | (static_cast<unsigned short>(data[1]) << 8));
    }

    static unsigned int ReadU32(const unsigned char *data)
    {
        return static_cast<unsigned int>(data[0]) |
               (static_cast<unsigned int>(data[1]) << 8) |
               (static_cast<unsigned int>(data[2]) << 16) |
               (static_cast<unsigned int>(data[3]) << 24);
    }

    static int ReadHeader(FILE *file, Header &header)
    {
        unsigned char fileHeader[14];
        unsigned char dibHeader[40];

        if (fread(fileHeader, 1, sizeof(fileHeader), file) != sizeof(fileHeader))
            return false;

        if (fileHeader[0] != 'B' || fileHeader[1] != 'M')
            return false;

        header.PixelOffset = ReadU32(fileHeader + 10);

        if (fread(dibHeader, 1, sizeof(dibHeader), file) != sizeof(dibHeader))
            return false;

        header.DibSize = ReadU32(dibHeader);

        if (header.DibSize < 40)
            return false;

        unsigned int width = ReadU32(dibHeader + 4);
        unsigned int height = ReadU32(dibHeader + 8);

        header.Width = static_cast<int>(width);
        header.Height = static_cast<int>(height);

        header.Planes = ReadU16(dibHeader + 12);
        header.BitsPerPixel = ReadU16(dibHeader + 14);
        header.Compression = ReadU32(dibHeader + 16);
        header.ColorsUsed = ReadU32(dibHeader + 32);

        if (header.Width <= 0 || header.Height == 0)
            return false;

        if (header.Height == INT_MIN)
            return false;

        if (header.Planes != 1 ||
            header.BitsPerPixel != 8 ||
            header.Compression != 0)
        {
            return false;
        }

        if (header.DibSize > 40)
        {
            if (fseek(file, header.DibSize - 40, SEEK_CUR) != 0)
                return false;
        }

        return true;
    }

    static bool ReadPalette(FILE *file, const Header &header,
                            unsigned char *palette)
    {
        unsigned int colorCount = header.ColorsUsed;

        if (colorCount == 0)
            colorCount = 256;

        if (colorCount == 0 || colorCount > 256)
            return false;

        unsigned char entry[4];

        for (unsigned int i = 0; i < colorCount; ++i)
        {
            if (fread(entry, 1, 4, file) != 4)
                return false;

            if (palette != NULL)
            {
                palette[i * 3 + 0] = entry[2];
                palette[i * 3 + 1] = entry[1];
                palette[i * 3 + 2] = entry[0];
            }
        }

        return true;
    }

public:
    // Loads the embedded BMP palette as 256 RGB entries.
    // Missing entries are initialized to black.

    static bool LoadPalette(const char *path, unsigned char *palette)
    {
        if (path == NULL || palette == NULL)
            return false;

        FILE *file = fopen(path, "rb");

        if (file == NULL)
            return false;

        Header header;
        bool result = ReadHeader(file, header);

        if (result)
        {
            memset(palette, 0, 256 * 3);
            result = ReadPalette(file, header, palette);
        }

        fclose(file);

        return result;
    }

    // Loads an indexed BMP into an Image.
    // The embedded palette is skipped because the renderer
    // uses one shared palette for all images.

    static bool Load(const char *path, Image &image)
    {
        if (path == NULL)
            return false;

        FILE *file = fopen(path, "rb");

        if (file == NULL)
            return false;

        Header header;

        if (!ReadHeader(file, header))
        {
            fclose(file);
            return false;
        }

        unsigned int colorCount = header.ColorsUsed;

        if (colorCount == 0)
            colorCount = 256;

        if (colorCount == 0 || colorCount > 256)
        {
            fclose(file);
            return false;
        }

        long paletteEnd = 14L + header.DibSize + colorCount * 4L;

        if (header.PixelOffset < static_cast<unsigned int>(paletteEnd))
        {
            fclose(file);
            return false;
        }

        if (fseek(file, header.PixelOffset, SEEK_SET) != 0)
        {
            fclose(file);
            return false;
        }

        int width = header.Width;
        int height = header.Height < 0 ? -header.Height : header.Height;

        if (width > INT_MAX - 3)
        {
            fclose(file);
            return false;
        }

        int rowSize = (width + 3) & ~3;

        if (rowSize > INT_MAX / height)
        {
            fclose(file);
            return false;
        }

        Image loaded;

        if (!loaded.Create(width, height))
        {
            fclose(file);
            return false;
        }

        unsigned char *row = new unsigned char[rowSize];

        if (row == NULL)
        {
            fclose(file);
            return false;
        }

        bool success = true;
        bool topDown = header.Height < 0;

        for (int y = 0; y < height; ++y)
        {
            if (fread(row, 1, rowSize, file) != static_cast<size_t>(rowSize))
            {
                success = false;
                break;
            }

            int destY = topDown ? y : height - 1 - y;

            memcpy(loaded.GetRow(destY), row, width);
        }

        delete[] row;
        fclose(file);

        if (!success)
            return false;

        image.Swap(loaded);

        return true;
    }
};

}