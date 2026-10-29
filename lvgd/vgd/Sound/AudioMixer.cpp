
#include "AudioMixer.hpp"

#include <string.h>

namespace vgd
{

static void MemoryBarrier()
{
    __asm__ __volatile__("" ::: "memory");
}

static int VolumeToGain(int volume)
{
    int gain = (volume + 64) >> 7;

    if (gain > 256)
        gain = 256;

    return gain;
}

static int ClampSample(int sample)
{
    if (sample > 32767)
        return 32767;

    if (sample < -32768)
        return -32768;

    return sample;
}

AudioMixer::AudioMixer()
    : m_SampleRate(0),
      m_OutputChannels(0),
      m_Initialized(false),
      m_Playing(0),
      m_EventHead(0),
      m_EventTail(0),
      m_LastScheduledFrame(0),
      m_MixedFramePosition(0),
      m_FrameSequence(0),
      m_PublishedFrameLow(0),
      m_PublishedFrameHigh(0)
{
    memset(m_Voices, 0, sizeof(m_Voices));
    memset(m_Events, 0, sizeof(m_Events));
}

AudioMixer::~AudioMixer()
{
    // Stop the Sound worker before destroying the mixer.
}

bool AudioMixer::Initialize(int sampleRate, int outputChannels)
{
    if (m_Initialized ||
        sampleRate <= 0 ||
        (outputChannels != 1 && outputChannels != 2))
    {
        return false;
    }

    m_SampleRate = sampleRate;
    m_OutputChannels = outputChannels;
    m_Playing = 0;

    m_EventHead = 0;
    m_EventTail = 0;
    m_LastScheduledFrame = 0;

    m_MixedFramePosition = 0;
    m_FrameSequence = 0;
    m_PublishedFrameLow = 0;
    m_PublishedFrameHigh = 0;

    memset(m_Voices, 0, sizeof(m_Voices));
    memset(m_Events, 0, sizeof(m_Events));

    for (int i = 0; i < VoiceCount; ++i)
    {
        m_Voices[i].Type = VoiceSilent;

        m_Voices[i].NoiseState =
            0x13579BDFU ^
            (static_cast<uint32_t>(i) * 0x9E3779B9U);
    }

    m_Initialized = true;
    return true;
}

bool AudioMixer::Attach(Sound& sound)
{
    if (!m_Initialized ||
        sound.GetSampleRate() != m_SampleRate)
    {
        return false;
    }

    return sound.SetPcmCallback(MixCallback, this);
}

void AudioMixer::Play()
{
    if (m_Initialized)
        m_Playing = 1;
}

void AudioMixer::Pause()
{
    m_Playing = 0;
}

void AudioMixer::TogglePlayback()
{
    if (m_Initialized)
        m_Playing = m_Playing ? 0 : 1;
}

bool AudioMixer::IsPlaying() const
{
    return m_Initialized && m_Playing != 0;
}

void AudioMixer::PublishFramePosition(uint64_t frame)
{
    ++m_FrameSequence;
    MemoryBarrier();

    m_PublishedFrameLow = static_cast<uint32_t>(frame);
    m_PublishedFrameHigh = static_cast<uint32_t>(frame >> 32);

    MemoryBarrier();
    ++m_FrameSequence;
}

uint64_t AudioMixer::GetMixedFramePosition() const
{
    uint32_t before;
    uint32_t after = 0;
    uint32_t low = 0;
    uint32_t high = 0;

    do
    {
        before = m_FrameSequence;

        if (before & 1U)
            continue;

        MemoryBarrier();

        low = m_PublishedFrameLow;
        high = m_PublishedFrameHigh;

        MemoryBarrier();

        after = m_FrameSequence;
    }
    while (before != after || (after & 1U));

    return (static_cast<uint64_t>(high) << 32) | low;
}

bool AudioMixer::QueueEvent(const Event& event)
{
    const uint32_t head = m_EventHead;

    const uint32_t next =
        (head + 1U) & (EventQueueCapacity - 1U);

    const uint32_t tail = m_EventTail;

    if (next == tail)
        return false;

    if (head != tail && event.Frame < m_LastScheduledFrame)
        return false;

    m_Events[head] = event;
    m_LastScheduledFrame = event.Frame;

    MemoryBarrier();
    m_EventHead = next;

    return true;
}

bool AudioMixer::SchedulePlay(
    uint64_t outputFrame,
    int voice,
    const int16_t* samples,
    uint64_t sampleFrames,
    int sourceRate,
    int leftVolume,
    int rightVolume,
    bool loop)
{
    if (!m_Initialized ||
        voice < 0 || voice >= VoiceCount ||
        samples == NULL || sampleFrames == 0 ||
        sampleFrames > 0xFFFFFFFFULL ||
        sourceRate <= 0 ||
        leftVolume < 0 || leftVolume > 32767 ||
        rightVolume < 0 || rightVolume > 32767)
    {
        return false;
    }

    Event event;
    memset(&event, 0, sizeof(event));

    event.Frame = outputFrame;
    event.Type = EventPlay;
    event.VoiceIndex = voice;
    event.Samples = samples;
    event.SampleFrames = sampleFrames;
    event.SourceRate = sourceRate;
    event.LeftVolume = leftVolume;
    event.RightVolume = rightVolume;
    event.Loop = loop;

    return QueueEvent(event);
}

bool AudioMixer::ScheduleStop(uint64_t outputFrame, int voice)
{
    if (!m_Initialized ||
        voice < 0 || voice >= VoiceCount)
    {
        return false;
    }

    Event event;
    memset(&event, 0, sizeof(event));

    event.Frame = outputFrame;
    event.Type = EventStop;
    event.VoiceIndex = voice;

    return QueueEvent(event);
}

bool AudioMixer::ScheduleOscillator(
    uint64_t outputFrame,
    int voice,
    int frequency,
    int volume)
{
    if (!m_Initialized ||
        voice < 0 || voice >= VoiceCount ||
        frequency < 0 || frequency > m_SampleRate / 2 ||
        volume < 0 || volume > 32767)
    {
        return false;
    }

    Event event;
    memset(&event, 0, sizeof(event));

    event.Frame = outputFrame;
    event.Type = EventOscillator;
    event.VoiceIndex = voice;
    event.Frequency = frequency;
    event.Volume = volume;

    return QueueEvent(event);
}

bool AudioMixer::ScheduleNoiseHit(
    uint64_t outputFrame,
    int voice,
    uint64_t durationFrames,
    int volume)
{
    if (!m_Initialized ||
        voice < 0 || voice >= VoiceCount ||
        durationFrames == 0 ||
        durationFrames > 0xFFFFFFFFULL ||
        volume < 0 || volume > 32767)
    {
        return false;
    }

    Event event;
    memset(&event, 0, sizeof(event));

    event.Frame = outputFrame;
    event.Type = EventNoiseHit;
    event.VoiceIndex = voice;
    event.DurationFrames = durationFrames;
    event.Volume = volume;

    return QueueEvent(event);
}

bool AudioMixer::PlaySample(
    int voice,
    const int16_t* samples,
    uint64_t sampleFrames,
    int sourceRate,
    int leftVolume,
    int rightVolume,
    bool loop)
{
    return SchedulePlay(
        GetMixedFramePosition(),
        voice,
        samples,
        sampleFrames,
        sourceRate,
        leftVolume,
        rightVolume,
        loop);
}

bool AudioMixer::StopVoice(int voice)
{
    return ScheduleStop(GetMixedFramePosition(), voice);
}

void AudioMixer::ApplyEvent(const Event& event)
{
    Voice& voice = m_Voices[event.VoiceIndex];

    if (event.Type == EventStop)
    {
        voice.Type = VoiceSilent;
        voice.Samples = NULL;
        voice.SampleFrames = 0;
        voice.PositionFrame = 0;
        voice.PositionFraction = 0;
        voice.IncrementFrame = 0;
        voice.IncrementFraction = 0;
        voice.LeftGain = 0;
        voice.RightGain = 0;
        voice.Loop = false;

        voice.PhaseIncrement = 0;
        voice.OscillatorVolume = 0;

        voice.NoiseElapsed = 0;
        voice.NoiseDuration = 0;
        voice.NoiseVolume = 0;
        voice.NoiseReduction = 0;
        voice.NoiseReductionStep = 0;
        voice.NoiseRemainder = 0;
        voice.NoiseError = 0;

        return;
    }

    if (event.Type == EventPlay)
    {
        voice.Type = VoiceSample;
        voice.Samples = event.Samples;
        voice.SampleFrames =
            static_cast<uint32_t>(event.SampleFrames);

        voice.PositionFrame = 0;
        voice.PositionFraction = 0;

        const uint64_t increment =
            (static_cast<uint64_t>(event.SourceRate) << 32) /
            static_cast<uint32_t>(m_SampleRate);

        voice.IncrementFrame =
            static_cast<uint32_t>(increment >> 32);

        voice.IncrementFraction =
            static_cast<uint32_t>(increment);

        if (voice.IncrementFrame == 0 &&
            voice.IncrementFraction == 0)
        {
            voice.IncrementFraction = 1;
        }

        voice.LeftGain = VolumeToGain(event.LeftVolume);
        voice.RightGain = VolumeToGain(event.RightVolume);
        voice.Loop = event.Loop;

        return;
    }

    if (event.Type == EventOscillator)
    {
        if (voice.Type != VoiceOscillator)
            voice.Phase = 0;

        voice.Type = VoiceOscillator;
        voice.Samples = NULL;
        voice.SampleFrames = 0;
        voice.PositionFrame = 0;
        voice.PositionFraction = 0;
        voice.IncrementFrame = 0;
        voice.IncrementFraction = 0;
        voice.LeftGain = 0;
        voice.RightGain = 0;
        voice.Loop = false;

        if (event.Frequency == 0)
        {
            voice.PhaseIncrement = 0;
        }
        else
        {
            voice.PhaseIncrement = static_cast<uint32_t>(
                (static_cast<uint64_t>(event.Frequency) << 32) /
                static_cast<uint32_t>(m_SampleRate));
        }

        voice.OscillatorVolume = event.Volume;

        return;
    }

    if (event.Type == EventNoiseHit)
    {
        voice.Type = VoiceNoise;
        voice.Samples = NULL;
        voice.SampleFrames = 0;
        voice.PositionFrame = 0;
        voice.PositionFraction = 0;
        voice.IncrementFrame = 0;
        voice.IncrementFraction = 0;
        voice.LeftGain = 0;
        voice.RightGain = 0;
        voice.Loop = false;

        voice.NoiseElapsed = 0;
        voice.NoiseDuration =
            static_cast<uint32_t>(event.DurationFrames);

        voice.NoiseVolume = event.Volume;

        const uint32_t duration = voice.NoiseDuration;
        const uint32_t targetReduction =
            static_cast<uint32_t>((event.Volume * 5) / 6);

        voice.NoiseReduction = 0;
        voice.NoiseReductionStep =
            static_cast<int>(targetReduction / duration);

        voice.NoiseRemainder = targetReduction % duration;
        voice.NoiseError = 0;

        if (voice.NoiseState == 0)
            voice.NoiseState = 0x13579BDFU;

        return;
    }
}

void AudioMixer::ProcessEvents(uint64_t frame)
{
    for (;;)
    {
        const uint32_t tail = m_EventTail;

        MemoryBarrier();

        if (tail == m_EventHead)
            break;

        if (m_Events[tail].Frame > frame)
            break;

        const Event event = m_Events[tail];

        ApplyEvent(event);

        MemoryBarrier();

        m_EventTail =
            (tail + 1U) & (EventQueueCapacity - 1U);
    }
}

// Called only for voices classified as VoiceSample by Mix().
//
// Do not add a VoiceType check here: Mix() builds the sample-voice
// list once per span. A voice may finish within that span, so this
// function still checks the sample pointer and frame count.
int AudioMixer::GetSampleActive(Voice& voice)
{
    const int16_t* const samples = voice.Samples;
    const uint32_t sampleFrames = voice.SampleFrames;

    if (samples == NULL || sampleFrames == 0)
        return 0;

    uint32_t position = voice.PositionFrame;

    if (position >= sampleFrames)
    {
        if (!voice.Loop)
        {
            voice.Type = VoiceSilent;
            voice.Samples = NULL;
            voice.SampleFrames = 0;
            voice.PositionFrame = 0;
            voice.PositionFraction = 0;
            voice.IncrementFrame = 0;
            voice.IncrementFraction = 0;
            voice.LeftGain = 0;
            voice.RightGain = 0;
            return 0;
        }

        position %= sampleFrames;
        voice.PositionFrame = position;
    }

    const int sampleA = samples[position];
    const uint32_t incrementFraction = voice.IncrementFraction;
    const uint32_t incrementFrame = voice.IncrementFrame;

    // Exact integer source/output rate ratios have no fractional
    // increment. They need neither interpolation nor fractional
    // position updates. For example, 22050 -> 11025 advances by 2.
    if (incrementFraction == 0)
    {
        const uint32_t remaining = sampleFrames - position;

        if (incrementFrame < remaining)
        {
            voice.PositionFrame = position + incrementFrame;
        }
        else if (!voice.Loop)
        {
            voice.PositionFrame = sampleFrames;
        }
        else
        {
            const uint32_t excess = incrementFrame - remaining;

            voice.PositionFrame =
                (excess < sampleFrames)
                    ? excess
                    : excess % sampleFrames;
        }

        return sampleA;
    }

    // Fractional playback uses linear interpolation.
    const uint32_t fraction = voice.PositionFraction;
    int interpolated = sampleA;

    if (fraction != 0)
    {
        uint32_t nextFrame = position + 1U;

        if (nextFrame >= sampleFrames)
            nextFrame = voice.Loop ? 0U : position;

        const int sampleB = samples[nextFrame];
        const int fraction15 = static_cast<int>(fraction >> 17);

        if (fraction15 != 0)
        {
            const int difference = sampleB - sampleA;
            interpolated += (difference * fraction15) >> 15;
        }
    }

    const uint32_t newFraction = fraction + incrementFraction;
    const uint32_t carry = (newFraction < fraction) ? 1U : 0U;

    voice.PositionFraction = newFraction;

    const uint32_t advance = incrementFrame + carry;
    const uint32_t remaining = sampleFrames - position;

    if (advance < remaining)
    {
        voice.PositionFrame = position + advance;
    }
    else if (!voice.Loop)
    {
        voice.PositionFrame = sampleFrames;
    }
    else
    {
        const uint32_t excess = advance - remaining;

        voice.PositionFrame =
            (excess < sampleFrames)
                ? excess
                : excess % sampleFrames;
    }

    return interpolated;
}

int AudioMixer::GetOscillatorSample(Voice& voice)
{
    if (voice.PhaseIncrement == 0 ||
        voice.OscillatorVolume == 0)
    {
        return 0;
    }

    const int sample =
        (voice.Phase & 0x80000000U)
        ? -voice.OscillatorVolume
        : voice.OscillatorVolume;

    voice.Phase += voice.PhaseIncrement;

    return sample;
}

int AudioMixer::GetNoiseSample(Voice& voice)
{
    if (voice.NoiseDuration == 0 ||
        voice.NoiseElapsed >= voice.NoiseDuration)
    {
        voice.Type = VoiceSilent;
        return 0;
    }

    voice.NoiseState =
        voice.NoiseState * 1664525U + 1013904223U;

    const int volume = voice.NoiseVolume - voice.NoiseReduction;

    const int sample =
        (voice.NoiseState & 0x80000000U)
        ? -volume
        : volume;

    voice.NoiseReduction += voice.NoiseReductionStep;

    if (voice.NoiseRemainder != 0)
    {
        const uint32_t threshold =
            voice.NoiseDuration - voice.NoiseRemainder;

        if (voice.NoiseError >= threshold)
        {
            voice.NoiseError -= threshold;
            ++voice.NoiseReduction;
        }
        else
        {
            voice.NoiseError += voice.NoiseRemainder;
        }
    }

    ++voice.NoiseElapsed;

    return sample;
}

void AudioMixer::MixCallback(
    void* context,
    int16_t* output,
    int frames)
{
    AudioMixer* mixer = static_cast<AudioMixer*>(context);

    if (mixer != NULL)
        mixer->Mix(output, frames);
}

void AudioMixer::Mix(int16_t* output, int frames)
{
    if (output == NULL || frames <= 0)
        return;

    uint64_t framePosition = m_MixedFramePosition;

    const bool playing = (m_Playing != 0);
    const int outputChannels = m_OutputChannels;

    int framesMixed = 0;

    while (framesMixed < frames)
    {
        ProcessEvents(framePosition);

        int spanFrames = frames - framesMixed;

        const uint32_t tail = m_EventTail;

        MemoryBarrier();

        if (tail != m_EventHead)
        {
            const uint64_t nextEventFrame = m_Events[tail].Frame;

            if (nextEventFrame <= framePosition)
                continue;

            const uint64_t distance = nextEventFrame - framePosition;

            if (distance < static_cast<uint64_t>(spanFrames))
                spanFrames = static_cast<int>(distance);
        }

        if (spanFrames <= 0)
            continue;

        Voice* sampleVoices[VoiceCount];
        Voice* effectVoices[VoiceCount];

        int sampleCount = 0;
        int effectCount = 0;

        if (playing)
        {
            for (int i = 0; i < VoiceCount; ++i)
            {
                Voice& voice = m_Voices[i];

                if (voice.Type == VoiceSample)
                {
                    sampleVoices[sampleCount++] = &voice;
                }
                else if (voice.Type == VoiceOscillator ||
                         voice.Type == VoiceNoise)
                {
                    effectVoices[effectCount++] = &voice;
                }
            }
        }

        const int spanEnd = framesMixed + spanFrames;

        if (outputChannels == 1)
        {
            while (framesMixed < spanEnd)
            {
                int left = 0;
                int right = 0;

                if (playing)
                {
                    for (int v = 0; v < sampleCount; ++v)
                    {
                        Voice& voice = *sampleVoices[v];
                        const int sample = GetSampleActive(voice);

                        left += (sample * voice.LeftGain) >> 8;
                        right += (sample * voice.RightGain) >> 8;
                    }

                    for (int v = 0; v < effectCount; ++v)
                    {
                        Voice& voice = *effectVoices[v];
                        int sample = 0;

                        if (voice.Type == VoiceOscillator)
                            sample = GetOscillatorSample(voice);
                        else if (voice.Type == VoiceNoise)
                            sample = GetNoiseSample(voice);

                        left += sample;
                        right += sample;
                    }
                }

                left = ClampSample(left);
                right = ClampSample(right);

                output[framesMixed] =
                    static_cast<int16_t>((left + right) >> 1);

                ++framesMixed;
                ++framePosition;
            }
        }
        else
        {
            while (framesMixed < spanEnd)
            {
                int left = 0;
                int right = 0;

                if (playing)
                {
                    for (int v = 0; v < sampleCount; ++v)
                    {
                        Voice& voice = *sampleVoices[v];
                        const int sample = GetSampleActive(voice);

                        left += (sample * voice.LeftGain) >> 8;
                        right += (sample * voice.RightGain) >> 8;
                    }

                    for (int v = 0; v < effectCount; ++v)
                    {
                        Voice& voice = *effectVoices[v];
                        int sample = 0;

                        if (voice.Type == VoiceOscillator)
                            sample = GetOscillatorSample(voice);
                        else if (voice.Type == VoiceNoise)
                            sample = GetNoiseSample(voice);

                        left += sample;
                        right += sample;
                    }
                }

                const int outputIndex = framesMixed * 2;

                output[outputIndex] =
                    static_cast<int16_t>(ClampSample(left));

                output[outputIndex + 1] =
                    static_cast<int16_t>(ClampSample(right));

                ++framesMixed;
                ++framePosition;
            }
        }
    }

    m_MixedFramePosition = framePosition;
    PublishFramePosition(framePosition);
}

} // namespace vgd
