
#ifndef VGD_AUDIOMIXER_HPP
#define VGD_AUDIOMIXER_HPP

#include "Sound.hpp"

#include <stdint.h>

namespace vgd
{

class AudioMixer
{
public:
    enum
    {
        VoiceCount = 8,
        EventQueueCapacity = 256
    };

    AudioMixer();
    ~AudioMixer();

    bool Initialize(int sampleRate, int outputChannels);
    bool Attach(Sound& sound);

    void Play();
    void Pause();
    void TogglePlayback();
    bool IsPlaying() const;

    bool SchedulePlay(
        uint64_t outputFrame,
        int voice,
        const int16_t* samples,
        uint64_t sampleFrames,
        int sourceRate,
        int leftVolume,
        int rightVolume,
        bool loop);

    bool ScheduleStop(uint64_t outputFrame, int voice);

    bool ScheduleOscillator(
        uint64_t outputFrame,
        int voice,
        int frequency,
        int volume);

    bool ScheduleNoiseHit(
        uint64_t outputFrame,
        int voice,
        uint64_t durationFrames,
        int volume);

    bool PlaySample(
        int voice,
        const int16_t* samples,
        uint64_t sampleFrames,
        int sourceRate,
        int leftVolume,
        int rightVolume,
        bool loop);

    bool StopVoice(int voice);

    uint64_t GetMixedFramePosition() const;

private:
    AudioMixer(const AudioMixer&);
    AudioMixer& operator=(const AudioMixer&);

    enum EventType
    {
        EventPlay,
        EventStop,
        EventOscillator,
        EventNoiseHit
    };

    enum VoiceType
    {
        VoiceSilent,
        VoiceSample,
        VoiceOscillator,
        VoiceNoise
    };

    struct Voice
    {
        VoiceType Type;

        const int16_t* Samples;
        uint32_t SampleFrames;

        uint32_t PositionFrame;
        uint32_t PositionFraction;

        // 32.32 source-frame increment.
        uint32_t IncrementFrame;
        uint32_t IncrementFraction;

        // 8-bit fixed-point channel gains.
        // 0 = silent, 128 = half, 256 = unity.
        int LeftGain;
        int RightGain;

        bool Loop;

        uint32_t Phase;
        uint32_t PhaseIncrement;
        int OscillatorVolume;

        uint32_t NoiseState;
        uint32_t NoiseElapsed;
        uint32_t NoiseDuration;
        int NoiseVolume;

        int NoiseReduction;
        int NoiseReductionStep;
        uint32_t NoiseRemainder;
        uint32_t NoiseError;
    };

    struct Event
    {
        uint64_t Frame;
        EventType Type;
        int VoiceIndex;

        const int16_t* Samples;
        uint64_t SampleFrames;
        int SourceRate;
        int LeftVolume;
        int RightVolume;
        bool Loop;

        int Frequency;
        int Volume;
        uint64_t DurationFrames;
    };

    static void MixCallback(void* context, int16_t* output, int frames);

    bool QueueEvent(const Event& event);
    void ProcessEvents(uint64_t frame);
    void ApplyEvent(const Event& event);

    // Called only for voices collected as VoiceSample by Mix().
    // Handles sample completion internally because a voice may
    // finish during a span and remain in the cached pointer list.
    int GetSampleActive(Voice& voice);

    int GetOscillatorSample(Voice& voice);
    int GetNoiseSample(Voice& voice);

    void Mix(int16_t* output, int frames);
    void PublishFramePosition(uint64_t frame);

    int m_SampleRate;
    int m_OutputChannels;
    bool m_Initialized;
    volatile int m_Playing;

    Voice m_Voices[VoiceCount];

    // Single producer: control thread.
    // Single consumer: audio worker.
    Event m_Events[EventQueueCapacity];
    volatile uint32_t m_EventHead;
    volatile uint32_t m_EventTail;
    uint64_t m_LastScheduledFrame;

    uint64_t m_MixedFramePosition;

    volatile uint32_t m_FrameSequence;
    volatile uint32_t m_PublishedFrameLow;
    volatile uint32_t m_PublishedFrameHigh;
};

} // namespace vgd

#endif
