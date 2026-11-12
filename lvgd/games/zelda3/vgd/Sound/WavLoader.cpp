#include "WavLoader.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

static uint16_t ReadLE16(const uint8_t* data)
{
    return static_cast<uint16_t>(
        static_cast<uint16_t>(data[0]) |
        (static_cast<uint16_t>(data[1]) << 8));
}

static uint32_t ReadLE32(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

WavLoader::WavLoader()
    : m_Samples(NULL),
      m_SampleFrames(0),
      m_SampleRate(0),
      m_Error(NULL)
{
}

WavLoader::~WavLoader()
{
    Unload();
}

bool WavLoader::SetError(const char* message)
{
    m_Error = message;
    return false;
}

bool WavLoader::Load(const char* filePath)
{
    Unload();

    if (!filePath || !filePath[0])
        return SetError("Invalid file path");

    FILE* file = fopen(filePath, "rb");

    if (!file)
        return SetError("Could not open WAV file");

    bool success = false;
    uint8_t* rawData = NULL;
    int16_t* output = NULL;

    uint32_t fileSize = 0;
    uint32_t dataOffset = 0;
    uint32_t dataSize = 0;
    uint32_t sampleRate = 0;
    uint16_t audioFormat = 0;
    uint16_t channels = 0;
    uint16_t bitsPerSample = 0;
    uint16_t blockAlign = 0;
    bool haveFormat = false;
    bool haveData = false;

    // Determine file size without assuming the file is seekable.
    if (fseek(file, 0, SEEK_END) != 0)
    {
        SetError("Could not seek WAV file");
        goto cleanup;
    }

    {
        const long length = ftell(file);

        if (length < 12 || static_cast<unsigned long>(length) > UINT_MAX)
        {
            SetError("Invalid WAV file size");
            goto cleanup;
        }

        fileSize = static_cast<uint32_t>(length);
    }

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        SetError("Could not rewind WAV file");
        goto cleanup;
    }

    {
        uint8_t header[12];

        if (fread(header, 1, sizeof(header), file) != sizeof(header))
        {
            SetError("Could not read WAV header");
            goto cleanup;
        }

        if (memcmp(header, "RIFF", 4) != 0 ||
            memcmp(header + 8, "WAVE", 4) != 0)
        {
            SetError("Not a RIFF/WAVE file");
            goto cleanup;
        }

        const uint32_t riffSize = ReadLE32(header + 4);

        if (riffSize < 4 ||
            static_cast<uint64_t>(riffSize) + 8 > fileSize)
        {
            SetError("Invalid RIFF size");
            goto cleanup;
        }

        fileSize = riffSize + 8;
    }

    // Locate the format and data chunks. Either may occur first.
    {
        uint32_t offset = 12;

        while (offset <= fileSize - 8)
        {
            uint8_t chunkHeader[8];

            if (fseek(file, offset, SEEK_SET) != 0 ||
                fread(chunkHeader, 1, sizeof(chunkHeader), file) !=
                    sizeof(chunkHeader))
            {
                SetError("Could not read WAV chunk");
                goto cleanup;
            }

            const uint32_t chunkSize = ReadLE32(chunkHeader + 4);
            const uint64_t chunkEnd =
                static_cast<uint64_t>(offset) + 8 + chunkSize;

            const uint64_t nextOffset = chunkEnd + (chunkSize & 1U);

            if (chunkEnd > fileSize || nextOffset > fileSize)
            {
                SetError("WAV chunk exceeds file bounds");
                goto cleanup;
            }

            if (memcmp(chunkHeader, "fmt ", 4) == 0)
            {
                if (haveFormat || chunkSize < 16)
                {
                    SetError("Invalid WAV format chunk");
                    goto cleanup;
                }

                uint8_t format[16];

                if (fread(format, 1, sizeof(format), file) !=
                    sizeof(format))
                {
                    SetError("Could not read WAV format");
                    goto cleanup;
                }

                audioFormat = ReadLE16(format);
                channels = ReadLE16(format + 2);
                sampleRate = ReadLE32(format + 4);
                blockAlign = ReadLE16(format + 12);
                bitsPerSample = ReadLE16(format + 14);
                haveFormat = true;
            }
            else if (memcmp(chunkHeader, "data", 4) == 0)
            {
                if (!haveData)
                {
                    dataOffset = offset + 8;
                    dataSize = chunkSize;
                    haveData = true;
                }
            }

            offset = static_cast<uint32_t>(nextOffset);
        }
    }

    if (!haveFormat || !haveData)
    {
        SetError("WAV format or data chunk missing");
        goto cleanup;
    }

    if (audioFormat != 1)
    {
        SetError("Only uncompressed PCM WAV is supported");
        goto cleanup;
    }

    if (channels != 1 && channels != 2)
    {
        SetError("Only mono and stereo WAV files are supported");
        goto cleanup;
    }

    if (bitsPerSample != 8 && bitsPerSample != 16)
    {
        SetError("Only 8-bit and 16-bit PCM are supported");
        goto cleanup;
    }

    if (sampleRate < 1 || sampleRate > 192000)
    {
        SetError("Invalid WAV sample rate");
        goto cleanup;
    }

    {
        const uint16_t expectedAlign =
            static_cast<uint16_t>(channels * (bitsPerSample / 8));

        if (blockAlign != expectedAlign ||
            dataSize % blockAlign != 0)
        {
            SetError("Invalid WAV sample alignment");
            goto cleanup;
        }
    }

    {
        const uint64_t frames = dataSize / blockAlign;

        if (frames == 0 ||
            frames > static_cast<uint64_t>(SIZE_MAX) / sizeof(int16_t) ||
            frames > UINT64_MAX / sizeof(int16_t))
        {
            SetError("WAV sample buffer is too large or empty");
            goto cleanup;
        }

        rawData = static_cast<uint8_t*>(malloc(dataSize));
        output = static_cast<int16_t*>(
            malloc(static_cast<size_t>(frames) * sizeof(int16_t)));

        if (!rawData || !output)
        {
            SetError("Out of memory loading WAV file");
            goto cleanup;
        }

        if (fseek(file, dataOffset, SEEK_SET) != 0)
        {
            fprintf(stderr,
                "WAV: seek failed: offset=%lu\n",
                (unsigned long)dataOffset);
            SetError("Could not seek to WAV sample data");
            goto cleanup;
        }

        {
            const size_t bytesRead = fread(rawData, 1, dataSize, file);

            if (bytesRead != dataSize)
            {
                fprintf(stderr,
                    "WAV: short read: offset=%lu expected=%lu got=%lu "
                    "ferror=%d feof=%d\n",
                    (unsigned long)dataOffset,
                    (unsigned long)dataSize,
                    (unsigned long)bytesRead,
                    ferror(file),
                    feof(file));

                SetError("Could not read WAV sample data");
                goto cleanup;
            }
        }

        for (uint64_t i = 0; i < frames; ++i)
        {
            int32_t left;
            int32_t right;

            if (bitsPerSample == 8)
            {
                left = (static_cast<int32_t>(
                    rawData[i * blockAlign]) - 128) * 256;

                right = left;

                if (channels == 2)
                {
                    right = (static_cast<int32_t>(
                        rawData[i * blockAlign + 1]) - 128) * 256;
                }
            }
            else
            {
                const uint8_t* sample = rawData + i * blockAlign;

                left = static_cast<int16_t>(ReadLE16(sample));
                right = left;

                if (channels == 2)
                    right = static_cast<int16_t>(ReadLE16(sample + 2));
            }

            // Average stereo channels using a wide intermediate.
            output[i] = static_cast<int16_t>((left + right) / 2);
        }

        m_Samples = output;
        output = NULL;
        m_SampleFrames = frames;
        m_SampleRate = static_cast<int>(sampleRate);
        m_Error = NULL;
        success = true;
    }

cleanup:
    free(rawData);
    free(output);
    fclose(file);

    return success;
}

void WavLoader::Unload()
{
    free(m_Samples);

    m_Samples = NULL;
    m_SampleFrames = 0;
    m_SampleRate = 0;
    m_Error = NULL;
}

bool WavLoader::IsLoaded() const
{
    return m_Samples != NULL;
}

const int16_t* WavLoader::GetSamples() const
{
    return m_Samples;
}

uint64_t WavLoader::GetSampleFrames() const
{
    return m_SampleFrames;
}

int WavLoader::GetSampleRate() const
{
    return m_SampleRate;
}

const char* WavLoader::GetError() const
{
    return m_Error ? m_Error : "";
}
