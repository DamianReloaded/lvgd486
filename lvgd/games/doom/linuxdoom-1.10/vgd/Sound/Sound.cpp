
#include "Sound.hpp"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

Sound::Configuration::Configuration()
    : SampleRate(22050),
      Channels(1),
      BufferFrames(256)
{
}

Sound::Sound()
    : m_Device(-1),
      m_Mixer(-1),
      m_MixerControl(-1),
      m_Initialized(false),
      m_ThreadStarted(false),
      m_StopWorker(false),
      m_WorkerFailed(false),
      m_StopRequested(false),
      m_SampleRate(0),
      m_Channels(0),
      m_BufferFrames(0),
      m_SampleFormat(FormatUnsigned8),
      m_Phase(0),
      m_PhaseIncrement(0),
      m_Frequency(0),
      m_PcmCallback(NULL),
      m_PcmContext(NULL),
      m_MutexInitialized(false),
      m_NextBuffer(0),
      m_DevicePath()
{
}

Sound::~Sound()
{
    Shutdown();
}

bool Sound::Initialize(const Configuration& configuration)
{
    if (m_Initialized)
        return true;

    if (configuration.SampleRate < 4000 ||
        configuration.SampleRate > 192000)
    {
        fprintf(stderr, "Sound: invalid sample rate.\n");
        return false;
    }

    if (configuration.Channels != 1 &&
        configuration.Channels != 2)
    {
        fprintf(stderr, "Sound: channels must be 1 or 2.\n");
        return false;
    }

    if (configuration.BufferFrames < 1 ||
        configuration.BufferFrames > MaximumBufferFrames)
    {
        fprintf(stderr, "Sound: invalid buffer size %d.\n",
                configuration.BufferFrames);
        return false;
    }

    if (pthread_mutex_init(&m_Mutex, NULL) != 0)
    {
        fprintf(stderr, "Sound: cannot initialize mutex.\n");
        return false;
    }

    m_MutexInitialized = true;
    m_StopRequested = false;

    static const char* DevicePaths[] =
    {
        "/dev/dsp",
        "/dev/sound/dspW",
        "/dev/sound/dsp"
    };

    const size_t deviceCount =
        sizeof(DevicePaths) / sizeof(DevicePaths[0]);

    for (size_t i = 0; i < deviceCount; ++i)
    {
        if (m_StopRequested)
        {
            DestroyMutex();
            return false;
        }

        const char* path = DevicePaths[i];
        int device = open(path, O_WRONLY);

        if (device < 0)
        {
            fprintf(stderr, "Sound: cannot open %s: %s\n",
                    path, strerror(errno));
            continue;
        }

        int actualRate = 0;
        int actualChannels = 0;
        int actualFormat = 0;

        if (!ConfigureDevice(device, path, configuration,
                             actualRate, actualChannels, actualFormat))
        {
            close(device);
            continue;
        }

        m_Device = device;
        m_DevicePath.Assign(path);

        m_SampleRate = actualRate;
        m_Channels = actualChannels;
        m_SampleFormat = actualFormat == AFMT_S16_LE
            ? FormatSigned16
            : FormatUnsigned8;

        m_BufferFrames = configuration.BufferFrames;
        m_Phase = 0;
        m_PhaseIncrement = 0;
        m_Frequency = 0;
        m_PcmCallback = NULL;
        m_PcmContext = NULL;
        m_StopWorker = false;
        m_WorkerFailed = false;
        m_NextBuffer = 0;

        for (int b = 0; b < DoubleBufferCount; ++b)
        {
            m_Buffers[b].ByteCount = 0;
            m_Buffers[b].Ready = false;
        }

        m_Initialized = true;

        printf("Sound: OSS initialized.\n");
        printf("Sound: device %s.\n", m_DevicePath.CStr());
        printf("Sound: sample rate %d Hz.\n", m_SampleRate);
        printf("Sound: channels %d.\n", m_Channels);
        printf("Sound: format %s.\n",
               m_SampleFormat == FormatSigned16 ? "S16_LE" : "U8");
        printf("Sound: generation block %d frames.\n", m_BufferFrames);

        InitializeMixer();
		SetVolume(100);
        return true;
    }

    fprintf(stderr, "Sound: no usable PCM playback device found.\n");

    DestroyMutex();
    return false;
}

void Sound::InitializeMixer()
{
    static const char* MixerPaths[] =
    {
        "/dev/mixer",
        "/dev/sound/mixer"
    };

    for (size_t i = 0; i < sizeof(MixerPaths) / sizeof(MixerPaths[0]); ++i)
    {
        m_Mixer = open(MixerPaths[i], O_RDWR);

        if (m_Mixer >= 0)
        {
            printf("Sound: mixer device %s.\n", MixerPaths[i]);
            break;
        }
    }

    if (m_Mixer < 0)
    {
        fprintf(stderr, "Sound: mixer unavailable: %s\n",
                strerror(errno));
        return;
    }

    int deviceMask = 0;

    if (ioctl(m_Mixer, SOUND_MIXER_READ_DEVMASK, &deviceMask) < 0)
    {
        fprintf(stderr, "Sound: cannot read mixer device mask: %s\n",
                strerror(errno));
        close(m_Mixer);
        m_Mixer = -1;
        return;
    }

    if (deviceMask & (1 << SOUND_MIXER_PCM))
    {
        m_MixerControl = SOUND_MIXER_PCM;
        printf("Sound: using PCM mixer control.\n");
    }
    else if (deviceMask & (1 << SOUND_MIXER_VOLUME))
    {
        m_MixerControl = SOUND_MIXER_VOLUME;
        printf("Sound: using master volume control.\n");
    }
    else
    {
        printf("Sound: no usable PCM or master mixer control.\n");
        close(m_Mixer);
        m_Mixer = -1;
    }
}

void Sound::Stop()
{
    m_StopRequested = true;
}

bool Sound::StartWorker()
{
    if (!m_Initialized)
        return false;

    if (m_ThreadStarted)
        return true;

    pthread_mutex_lock(&m_Mutex);
    m_StopWorker = false;
    m_WorkerFailed = false;
    pthread_mutex_unlock(&m_Mutex);

    const int error = pthread_create(
        &m_Thread, NULL, Sound::WorkerEntry, this);

    if (error != 0)
    {
        fprintf(stderr, "Sound: pthread_create failed: %s\n",
                strerror(error));
        return false;
    }

    m_ThreadStarted = true;
    return true;
}

void Sound::Shutdown()
{
    if (m_ThreadStarted)
    {
        pthread_mutex_lock(&m_Mutex);
        m_StopWorker = true;
        pthread_mutex_unlock(&m_Mutex);

        pthread_join(m_Thread, NULL);
        m_ThreadStarted = false;
    }

    if (m_Device >= 0)
    {
        if (m_Initialized && !m_StopRequested)
        {
            if (ioctl(m_Device, SNDCTL_DSP_SYNC, 0) < 0 &&
                errno != EINTR)
            {
                perror("Sound: SNDCTL_DSP_SYNC");
            }
        }

        close(m_Device);
    }

    if (m_Mixer >= 0)
        close(m_Mixer);

    m_Device = -1;
    m_Mixer = -1;
    m_MixerControl = -1;
    m_Initialized = false;
    m_StopWorker = false;
    m_WorkerFailed = false;
    m_SampleRate = 0;
    m_Channels = 0;
    m_BufferFrames = 0;
    m_Phase = 0;
    m_PhaseIncrement = 0;
    m_Frequency = 0;
    m_PcmCallback = NULL;
    m_PcmContext = NULL;
    m_NextBuffer = 0;
    m_DevicePath.Clear();

    for (int b = 0; b < DoubleBufferCount; ++b)
    {
        m_Buffers[b].ByteCount = 0;
        m_Buffers[b].Ready = false;
    }

    DestroyMutex();
}

bool Sound::SetVolume(int volume)
{
    return SetVolume(volume, volume);
}

bool Sound::SetVolume(int left, int right)
{
    if (m_Mixer < 0 || m_MixerControl < 0)
        return false;

    if (left < 0)
        left = 0;
    else if (left > 100)
        left = 100;

    if (right < 0)
        right = 0;
    else if (right > 100)
        right = 100;

    int value = (right << 8) | left;

    if (ioctl(m_Mixer, MIXER_WRITE(m_MixerControl), &value) < 0)
    {
        fprintf(stderr, "Sound: cannot set mixer volume: %s\n",
                strerror(errno));
        return false;
    }

    return true;
}

bool Sound::GetVolume(int& left, int& right)
{
    if (m_Mixer < 0 || m_MixerControl < 0)
        return false;

    int value = 0;

    if (ioctl(m_Mixer, MIXER_READ(m_MixerControl), &value) < 0)
    {
        fprintf(stderr, "Sound: cannot read mixer volume: %s\n",
                strerror(errno));
        return false;
    }

    left = value & 0xFF;
    right = (value >> 8) & 0xFF;

    return true;
}

bool Sound::IsInitialized() const
{
    return m_Initialized;
}

bool Sound::WorkerFailed()
{
    if (!m_MutexInitialized)
        return true;

    pthread_mutex_lock(&m_Mutex);
    const bool failed = m_WorkerFailed;
    pthread_mutex_unlock(&m_Mutex);

    return failed;
}

int Sound::GetSampleRate() const
{
    return m_SampleRate;
}

int Sound::GetChannels() const
{
    return m_Channels;
}

int Sound::GetBufferFrames() const
{
    return m_BufferFrames;
}

Sound::SampleFormat Sound::GetSampleFormat() const
{
    return m_SampleFormat;
}

int Sound::GetFrequency()
{
    if (!m_MutexInitialized)
        return 0;

    pthread_mutex_lock(&m_Mutex);
    const int frequency = m_Frequency;
    pthread_mutex_unlock(&m_Mutex);

    return frequency;
}

const char* Sound::GetDevicePath() const
{
    return m_DevicePath.CStr();
}

void Sound::PlayNote(int frequency)
{
    if (frequency <= 0)
    {
        StopNote();
        return;
    }

    if (!m_Initialized || !m_MutexInitialized)
        return;

    if (frequency > m_SampleRate / 2)
        frequency = m_SampleRate / 2;

    const uint32_t increment = static_cast<uint32_t>(
        (static_cast<uint64_t>(frequency) << 32) /
        static_cast<unsigned int>(m_SampleRate));

    pthread_mutex_lock(&m_Mutex);
    m_Frequency = frequency;
    m_PhaseIncrement = increment;
    pthread_mutex_unlock(&m_Mutex);
}

void Sound::StopNote()
{
    if (!m_MutexInitialized)
        return;

    pthread_mutex_lock(&m_Mutex);
    m_Frequency = 0;
    m_PhaseIncrement = 0;
    pthread_mutex_unlock(&m_Mutex);
}

bool Sound::SetPcmCallback(PcmCallback callback, void* context)
{
    if (!m_Initialized || !m_MutexInitialized)
        return false;

    pthread_mutex_lock(&m_Mutex);

    if (m_ThreadStarted)
    {
        pthread_mutex_unlock(&m_Mutex);
        fprintf(stderr, "Sound: PCM callback must be set before "
                        "starting the worker.\n");
        return false;
    }

    m_PcmCallback = callback;
    m_PcmContext = context;

    pthread_mutex_unlock(&m_Mutex);
    return true;
}

void* Sound::WorkerEntry(void* argument)
{
    Sound* sound = static_cast<Sound*>(argument);
    sound->WorkerLoop();
    return NULL;
}

bool Sound::ShouldStopWorker()
{
    bool stop = false;

    if (m_MutexInitialized)
    {
        pthread_mutex_lock(&m_Mutex);
        stop = m_StopWorker;
        pthread_mutex_unlock(&m_Mutex);
    }
    else
    {
        stop = true;
    }

    return stop || m_StopRequested;
}

bool Sound::GenerateBuffer(AudioBuffer& buffer)
{
    int16_t samples[MaximumBufferFrames];

    uint32_t phaseIncrement = 0;
    PcmCallback pcmCallback = NULL;
    void* pcmContext = NULL;

    pthread_mutex_lock(&m_Mutex);
    phaseIncrement = m_PhaseIncrement;
    pcmCallback = m_PcmCallback;
    pcmContext = m_PcmContext;
    pthread_mutex_unlock(&m_Mutex);

    if (pcmCallback != NULL)
    {
        pcmCallback(pcmContext, samples, m_BufferFrames);
    }
    else
    {
        for (int frame = 0; frame < m_BufferFrames; ++frame)
        {
            int16_t sample = 0;

            if (phaseIncrement != 0)
            {
                sample = (m_Phase & 0x80000000U)
                    ? static_cast<int16_t>(-10000)
                    : static_cast<int16_t>(10000);

                m_Phase += phaseIncrement;
            }

            samples[frame] = sample;
        }
    }

    if (!ConvertPcmToOutput(buffer, samples))
        return false;

    buffer.Ready = true;
    return true;
}

bool Sound::ConvertPcmToOutput(AudioBuffer& buffer,
                               const int16_t* samples)
{
    const size_t bytesPerSample =
        m_SampleFormat == FormatSigned16 ? 2 : 1;

    const size_t byteCount =
        static_cast<size_t>(m_BufferFrames) *
        static_cast<size_t>(m_Channels) *
        bytesPerSample;

    if (byteCount > sizeof(buffer.Bytes))
    {
        fprintf(stderr, "Sound: internal audio buffer overflow prevented.\n");
        return false;
    }

    if (m_SampleFormat == FormatSigned16)
    {
        int16_t* output =
            reinterpret_cast<int16_t*>(buffer.Bytes);

        for (int frame = 0; frame < m_BufferFrames; ++frame)
        {
            for (int channel = 0; channel < m_Channels; ++channel)
                *output++ = samples[frame];
        }
    }
    else
    {
        uint8_t* output = buffer.Bytes;

        for (int frame = 0; frame < m_BufferFrames; ++frame)
        {
            int value = static_cast<int>(samples[frame]) / 256 + 128;

            if (value < 0)
                value = 0;
            else if (value > 255)
                value = 255;

            const uint8_t sample = static_cast<uint8_t>(value);

            for (int channel = 0; channel < m_Channels; ++channel)
                *output++ = sample;
        }
    }

    buffer.ByteCount = byteCount;
    return true;
}

bool Sound::WriteBuffer(const AudioBuffer& buffer)
{
    const uint8_t* data = buffer.Bytes;
    size_t remaining = buffer.ByteCount;

    while (remaining > 0 && !ShouldStopWorker())
    {
        const ssize_t written = write(m_Device, data, remaining);

        if (written < 0)
        {
            if (errno == EINTR)
                continue;

            if (!m_StopRequested)
            {
                fprintf(stderr, "Sound: write to %s failed: %s\n",
                        m_DevicePath.CStr(), strerror(errno));
            }

            SetWorkerFailed();
            return false;
        }

        if (written == 0)
        {
            if (!m_StopRequested)
                fprintf(stderr, "Sound: zero-length write.\n");

            SetWorkerFailed();
            return false;
        }

        data += written;
        remaining -= static_cast<size_t>(written);
    }

    return remaining == 0;
}

void Sound::WorkerLoop()
{
    m_NextBuffer = 0;

    while (!ShouldStopWorker())
    {
        AudioBuffer& output = m_Buffers[m_NextBuffer];

        if (!GenerateBuffer(output))
        {
            SetWorkerFailed();
            return;
        }

        if (!WriteBuffer(output))
        {
            if (ShouldStopWorker())
                break;

            return;
        }

        output.Ready = false;
        output.ByteCount = 0;
        m_NextBuffer ^= 1;
    }
}

void Sound::SetWorkerFailed()
{
    if (!m_MutexInitialized)
        return;

    pthread_mutex_lock(&m_Mutex);
    m_WorkerFailed = true;
    m_StopWorker = true;
    pthread_mutex_unlock(&m_Mutex);
}

void Sound::DestroyMutex()
{
    if (m_MutexInitialized)
    {
        pthread_mutex_destroy(&m_Mutex);
        m_MutexInitialized = false;
    }
}

bool Sound::ConfigureDevice(int device,
                            const char* path,
                            const Configuration& configuration,
                            int& actualRate,
                            int& actualChannels,
                            int& actualFormat)
{
    /*
     * Low-latency OSS fragment configuration.
     * Adjust these two values to tune buffering.
     */
    const int FragmentCount = 2;
    const int FragmentFrames = 256;

    /*
     * Request fragment configuration before configuring
     * the remaining DSP parameters.
     */
    int format = AFMT_S16_LE;

    /*
     * Select format first to calculate the correct fragment size.
     */
    if (ioctl(device, SNDCTL_DSP_SETFMT, &format) >= 0 &&
        format == AFMT_S16_LE)
    {
        actualFormat = AFMT_S16_LE;
    }
    else
    {
        format = AFMT_U8;

        if (ioctl(device, SNDCTL_DSP_SETFMT, &format) < 0 ||
            format != AFMT_U8)
        {
            fprintf(stderr, "Sound: %s supports neither S16_LE "
                            "nor U8 PCM.\n", path);
            return false;
        }

        actualFormat = AFMT_U8;
    }

    int channels = configuration.Channels;

    if (ioctl(device, SNDCTL_DSP_CHANNELS, &channels) < 0)
    {
        fprintf(stderr, "Sound: %s channel configuration failed: %s\n",
                path, strerror(errno));
        return false;
    }

    if (channels != 1 && channels != 2)
    {
        fprintf(stderr, "Sound: %s returned unsupported channel count %d.\n",
                path, channels);
        return false;
    }

    int sampleRate = configuration.SampleRate;

    if (ioctl(device, SNDCTL_DSP_SPEED, &sampleRate) < 0)
    {
        fprintf(stderr, "Sound: %s sample-rate configuration failed: %s\n",
                path, strerror(errno));
        return false;
    }

    if (sampleRate < 4000 || sampleRate > 192000)
    {
        fprintf(stderr, "Sound: %s returned invalid sample rate %d.\n",
                path, sampleRate);
        return false;
    }

    actualRate = sampleRate;
    actualChannels = channels;

    /*
     * Calculate fragment size in bytes.
     * OSS fragment sizes must be powers of two.
     */
    const int bytesPerSample = actualFormat == AFMT_S16_LE ? 2 : 1;
    const int bytesPerFrame = channels * bytesPerSample;
    const int requestedBytes = FragmentFrames * bytesPerFrame;

    int fragmentExponent = 0;
    int fragmentBytes = 1;

    while (fragmentBytes < requestedBytes)
    {
        fragmentBytes <<= 1;
        ++fragmentExponent;
    }

    if (fragmentExponent < 4)
        fragmentExponent = 4;

    if (fragmentExponent > 15)
        fragmentExponent = 15;

    fragmentBytes = 1 << fragmentExponent;

    int fragments = (FragmentCount << 16) | fragmentExponent;

    printf("Sound: requesting OSS fragments:\n");
    printf("Sound: fragment count %d.\n", FragmentCount);
    printf("Sound: fragment size %d bytes.\n", fragmentBytes);

    if (ioctl(device, SNDCTL_DSP_SETFRAGMENT, &fragments) < 0)
    {
        fprintf(stderr, "Sound: %s SETFRAGMENT failed: %s\n",
                path, strerror(errno));
        fprintf(stderr, "Sound: using driver default buffering.\n");
    }
    else
    {
        const int acceptedCount = (fragments >> 16) & 0xFFFF;
        const int acceptedExponent = fragments & 0xFFFF;

        printf("Sound: OSS accepted fragment configuration:\n");
        printf("Sound: fragment count %d.\n", acceptedCount);

        if (acceptedExponent >= 0 && acceptedExponent <= 15)
        {
            printf("Sound: fragment size %d bytes.\n",
                   1 << acceptedExponent);
        }
    }

    /*
     * Report actual OSS playback buffer configuration.
     */
    audio_buf_info bufferInfo;
    memset(&bufferInfo, 0, sizeof(bufferInfo));

    if (ioctl(device, SNDCTL_DSP_GETOSPACE, &bufferInfo) == 0)
    {
        printf("Sound: OSS output buffer diagnostics:\n");
        printf("Sound: fragment size %d bytes.\n", bufferInfo.fragsize);
        printf("Sound: total fragments %d.\n", bufferInfo.fragstotal);
        printf("Sound: available fragments %d.\n", bufferInfo.fragments);
        printf("Sound: available bytes %d.\n", bufferInfo.bytes);

        if (bufferInfo.fragsize > 0 && bufferInfo.fragstotal > 0)
        {
            const int64_t totalBytes =
                static_cast<int64_t>(bufferInfo.fragsize) *
                bufferInfo.fragstotal;

            const int64_t totalFrames = totalBytes / bytesPerFrame;
            const int64_t capacityMs = totalFrames * 1000 / actualRate;

            printf("Sound: total output capacity approximately %lld ms.\n",
                   static_cast<long long>(capacityMs));
        }
    }
    else
    {
        fprintf(stderr, "Sound: %s GETOSPACE failed: %s\n",
                path, strerror(errno));
    }

    int blockSize = 0;

    if (ioctl(device, SNDCTL_DSP_GETBLKSIZE, &blockSize) == 0)
    {
        printf("Sound: OSS reported block size %d bytes.\n", blockSize);
    }

#ifdef SNDCTL_DSP_GETODELAY
    int queuedBytes = 0;

    if (ioctl(device, SNDCTL_DSP_GETODELAY, &queuedBytes) == 0)
    {
        const int queuedMs =
            queuedBytes * 1000 / (actualRate * bytesPerFrame);

        printf("Sound: currently queued audio approximately %d ms.\n",
               queuedMs);
    }
#endif

    return true;
}
