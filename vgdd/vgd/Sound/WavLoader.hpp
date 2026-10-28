#pragma once
#include <stdint.h>

class WavLoader
{
public:
    WavLoader();
    ~WavLoader();

    WavLoader(const WavLoader&) = delete;
    WavLoader& operator=(const WavLoader&) = delete;

    bool Load(const char* filePath);
    void Unload();

    bool IsLoaded() const;
    const int16_t* GetSamples() const;
    uint64_t GetSampleFrames() const;
    int GetSampleRate() const;
    const char* GetError() const;

private:
    bool SetError(const char* message);

    int16_t* m_Samples;
    uint64_t m_SampleFrames;
    int m_SampleRate;
    const char* m_Error;
};
