#ifndef VGD_SYNTHESIZER_HPP
#define VGD_SYNTHESIZER_HPP

#include <stdint.h>
#include "../Util/Vector.hpp"

namespace vgd
{

class Synthesizer
{
public:
    enum
    {
        InstrumentCount = 128,
        DefaultSampleRate = 44100,
        DefaultDuration = 2000,
        LoopFrameCount = 2048,
        LoopCycles = 8
    };

    enum Waveform
    {
        WaveSine,
        WaveTriangle,
        WaveSaw,
        WaveSquare,
        WavePulse,
        WaveNoise,
        WavePlucked
    };

    struct Settings
    {
        int SampleRate;
        int DurationMs;
        int AttackMs;
        int DecayMs;
        int ReleaseMs;
        int MasterGain;
        int Brightness;
        int NoiseLevel;

        Settings();
    };

    struct Instrument
    {
        Waveform Type;
        float Harmonics[16];
        float Attack;
        float Decay;
        float Sustain;
        float Release;
        float Brightness;
        float Noise;
        float Detune;
    };

    struct Sample
    {
        Vector<int16_t> Data;
        int SampleRate;
        int LoopStart;
        int LoopEnd;

        Sample();
        void Clear();
        int GetFrameCount() const;
    };

    Synthesizer();

    void SetSettings(const Settings& settings);
    const Settings& GetSettings() const;

    bool SetInstrument(int program, const Instrument& instrument);
    const Instrument* GetInstrument(int program) const;

    bool Generate(int program, int note, int velocity, Sample& sample) const;
    bool GenerateLoopSample(int program, int note, Sample& sample) const;
    bool GenerateDrum(int note, int velocity, Sample& sample) const;

    static float NoteToFrequency(int note);

private:
    Settings m_Settings;
    Instrument m_Instruments[InstrumentCount];

    void InitializeInstruments();
    void InitializeInstrument(int program, Instrument& instrument);

    float GenerateWaveform(const Instrument& instrument, float phase, float frequency, float time, int note, int velocity) const;
    float GenerateHarmonics(const Instrument& instrument, float phase, float frequency, float time, int note, int velocity) const;
    float GenerateNoise(unsigned int& seed) const;
    float GenerateEnvelope(const Instrument& instrument, float time, float duration, int attackMs, int decayMs, int releaseMs) const;

    bool GenerateInternal(int program, int note, int velocity, Sample& sample, bool loop) const;

    static float Clamp(float value, float minimum, float maximum);
};

}

#endif