
#pragma once

#include <stdint.h>
#include <pthread.h>
#include "../Util/String.hpp"

class Sound
{
public:
    enum SampleFormat
    {
        FormatUnsigned8,
        FormatSigned16
    };

    struct Configuration
    {
        int SampleRate;
        int Channels;
        int BufferFrames;

        Configuration();
    };

    typedef void (*PcmCallback)(void* context,
                                int16_t* samples,
                                int frames);

    Sound();
    ~Sound();

    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;

    bool Initialize(const Configuration& configuration);
    bool StartWorker();
    void Shutdown();

    bool IsInitialized() const;
    bool WorkerFailed();

    int GetSampleRate() const;
    int GetChannels() const;
    int GetBufferFrames() const;
    SampleFormat GetSampleFormat() const;
    int GetFrequency();
    const char* GetDevicePath() const;

    void PlayNote(int frequency);
    void StopNote();

    bool SetPcmCallback(PcmCallback callback, void* context);

    void Stop();

private:
    enum
    {
        MaximumBufferFrames = 8820*10,
        MaximumChannels = 2,
        MaximumBytesPerSample = 2,
        DoubleBufferCount = 2
    };

    struct AudioBuffer
    {
        union
        {
            uint32_t Alignment;
            uint8_t Bytes[
                MaximumBufferFrames *
                MaximumChannels *
                MaximumBytesPerSample
            ];
        };

        size_t ByteCount;
        bool Ready;

        AudioBuffer()
            : ByteCount(0),
              Ready(false)
        {
        }
    };

    static void* WorkerEntry(void* argument);

    bool ShouldStopWorker();
    void WorkerLoop();
    void SetWorkerFailed();
    void DestroyMutex();

    bool GenerateBuffer(AudioBuffer& buffer);
    bool ConvertPcmToOutput(AudioBuffer& buffer,
                            const int16_t* samples);
    bool WriteBuffer(const AudioBuffer& buffer);

    bool ConfigureDevice(int device,
                         const char* path,
                         const Configuration& configuration,
                         int& actualRate,
                         int& actualChannels,
                         int& actualFormat);

    int m_Device;
    bool m_Initialized;

    pthread_t m_Thread;
    bool m_ThreadStarted;
    bool m_StopWorker;
    bool m_WorkerFailed;
    bool m_StopRequested;

    int m_SampleRate;
    int m_Channels;
    int m_BufferFrames;
    SampleFormat m_SampleFormat;

    uint32_t m_Phase;
    uint32_t m_PhaseIncrement;
    int m_Frequency;

    PcmCallback m_PcmCallback;
    void* m_PcmContext;

    pthread_mutex_t m_Mutex;
    bool m_MutexInitialized;

    AudioBuffer m_Buffers[DoubleBufferCount];
    int m_NextBuffer;

    String<64> m_DevicePath;
};
