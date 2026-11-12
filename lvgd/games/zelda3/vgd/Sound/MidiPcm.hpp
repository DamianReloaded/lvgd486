#ifndef VGD_MIDIPCM_HPP
#define VGD_MIDIPCM_HPP

#include "IMidi.hpp"
#include "AudioMixer.hpp"
#include "Synthesizer.hpp"

namespace vgd
{

class MidiPcm : public IMidi
{
public:
    static const int ChannelCount = 16;
    static const int InstrumentCount = Synthesizer::InstrumentCount;
    static const int MaxVoices = AudioMixer::VoiceCount / 2;
    static const int InstrumentSlotCount = AudioMixer::VoiceCount;
    static const int SampleFrameCount = Synthesizer::LoopFrameCount;
    static const int SampleRate = Synthesizer::DefaultSampleRate;

    enum Waveform
    {
        WaveformSine,
        WaveformTriangle,
        WaveformSquare,
        WaveformSawtooth,
        WaveformPulse,
        WaveformNoise,
        WaveformPlucked
    };

    struct InstrumentInfo
    {
        bool Loaded;
        bool Synthesized;
        Waveform Type;
        int RootNote;
        int Attack;
        int Decay;
        int Sustain;
        int Release;
    };

    struct VoiceInfo
    {
        bool Active;
        bool KeyDown;
        bool Sustained;
        int Channel;
        int Note;
        int Velocity;
        int Program;
        unsigned int Age;
    };

    MidiPcm();
    virtual ~MidiPcm();

    void SetMixer(AudioMixer* mixer);

    Synthesizer& GetSynthesizer();
    const Synthesizer& GetSynthesizer() const;

    virtual bool Initialize(const char* path);
    virtual bool IsInitialized() const;
    virtual int GetPortCount() const;

    virtual bool SendByte(unsigned char value);
    virtual bool SendMessage(unsigned char status, unsigned char data1, unsigned char data2);

    virtual bool NoteOn(int channel, int note, int velocity);
    virtual bool NoteOff(int channel, int note);
    virtual bool ProgramChange(int channel, int program);
    virtual bool ControlChange(int channel, int control, int value);

    virtual bool SetMasterVolume(int volume);

    virtual bool SilenceAll();
    virtual void Shutdown();

    bool IsInstrumentLoaded(int program) const;
    bool IsInstrumentSynthesized(int program) const;
    bool GetInstrumentInfo(int program, InstrumentInfo& info) const;

    int GetActiveVoiceCount() const;
    bool GetVoiceInfo(int index, VoiceInfo& info) const;

    bool SetInstrument(int program, int rootNote, const int16_t* samples, int sampleFrames, int sampleRate);

private:
    enum
    {
        PercussionProgram = InstrumentCount,
        PercussionChannel = 9
    };

    struct ChannelState
    {
        int Program;
        int Volume;
        int Expression;
        int Pan;
        int PitchBend;
        int Modulation;
        bool Sustain;
    };

    struct VoiceState
    {
        bool Active;
        bool KeyDown;
        bool Sustained;
        int Channel;
        int Note;
        int Velocity;
        int Program;
        int MixerVoice;
        int InstrumentSlot;
        unsigned int Age;
    };

    struct InstrumentSlot
    {
        int Program;
        int Note;
        unsigned int Age;
        Synthesizer::Sample Sample;
        bool Procedural;
        bool Loop;
    };

    void InitializeChannels();
    void ResetChannel(int channel);

    bool DispatchMessage(unsigned char status, unsigned char data1, unsigned char data2);

    bool AllocateVoice(int channel, int note, int velocity);
    int FindVoice(int channel, int note) const;
    int FindFreeVoice() const;
    void ReleaseVoice(int index);
    void ReleaseChannel(int channel);

    int FindInstrumentSlot(int program, int note) const;
    int AllocateInstrumentSlot(int program, int note);
    bool GenerateInstrument(int program, int note, int slot, int velocity);
    bool GetInstrumentSamples(int program, int note, int velocity, int& slot);

    void StartMixerVoice(int index);
    void UpdateMixerVoice(int index);
    int GetSourceRate(const VoiceState& voice) const;
    void GetStereoVolume(const VoiceState& voice, int& left, int& right) const;

    bool IsValidChannel(int channel) const;
    bool IsValidNote(int note) const;
    bool IsValidValue(int value) const;

    AudioMixer* m_Mixer;
    Synthesizer m_Synthesizer;

    ChannelState m_Channels[ChannelCount];
    VoiceState m_Voices[MaxVoices];
    InstrumentSlot m_InstrumentSlots[InstrumentSlotCount];

    bool m_Initialized;
    bool m_SampleBankLoaded;
    unsigned int m_VoiceAge;
    unsigned int m_SampleAge;

    unsigned char m_RunningStatus;
    unsigned char m_MessageData[2];
    int m_MessageDataCount;
    int m_MessageDataRequired;
    bool m_InSysEx;
    int m_MasterVolume;
};

}

#endif