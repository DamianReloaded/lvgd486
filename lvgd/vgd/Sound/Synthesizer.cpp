/*
//You can configure it before initializing MIDI:
    vgd::MidiPcm Midi;
    vgd::Synthesizer::Settings Settings;
    Settings.SampleRate = 44100;
    Settings.Brightness = 120;
    Settings.NoiseLevel = 100;
    Midi.GetSynthesizer().SetSettings(Settings);
    Midi.Initialize(0);

You can also customize individual instruments:
    vgd::Synthesizer::Instrument Instrument;
    const vgd::Synthesizer::Instrument* Existing = Midi.GetSynthesizer().GetInstrument(0);
    if (Existing)
    {
        Instrument = *Existing;
        Instrument.Brightness = 1.5f;
        Instrument.Harmonics[5] = 0.25f;

        Midi.GetSynthesizer().SetInstrument(0, Instrument);
    }

*/
#include "Synthesizer.hpp"

#include <math.h>
#include <string.h>

namespace vgd
{

static const float Pi = 3.14159265358979323846f;
static const float TwoPi = 6.28318530717958647692f;

Synthesizer::Settings::Settings()
{
    SampleRate = DefaultSampleRate;
    DurationMs = DefaultDuration;
    AttackMs = 20;
    DecayMs = 150;
    ReleaseMs = 250;
    MasterGain = 100;
    Brightness = 100;
    NoiseLevel = 100;
}

Synthesizer::Sample::Sample()
{
    SampleRate = DefaultSampleRate;
    LoopStart = 0;
    LoopEnd = 0;
}

void Synthesizer::Sample::Clear()
{
    Data.Clear();
    LoopStart = 0;
    LoopEnd = 0;
}

int Synthesizer::Sample::GetFrameCount() const
{
    return Data.Size();
}

Synthesizer::Synthesizer()
{
    InitializeInstruments();
}

void Synthesizer::SetSettings(const Settings& settings)
{
    m_Settings = settings;

    if (m_Settings.SampleRate != 11025 &&
        m_Settings.SampleRate != 22050 &&
        m_Settings.SampleRate != 44100)
        m_Settings.SampleRate = DefaultSampleRate;

    if (m_Settings.DurationMs < 50)
        m_Settings.DurationMs = 50;

    if (m_Settings.MasterGain < 0)
        m_Settings.MasterGain = 0;
    else if (m_Settings.MasterGain > 100)
        m_Settings.MasterGain = 100;

    if (m_Settings.Brightness < 0)
        m_Settings.Brightness = 0;
    else if (m_Settings.Brightness > 200)
        m_Settings.Brightness = 200;

    if (m_Settings.NoiseLevel < 0)
        m_Settings.NoiseLevel = 0;
    else if (m_Settings.NoiseLevel > 200)
        m_Settings.NoiseLevel = 200;
}

const Synthesizer::Settings& Synthesizer::GetSettings() const
{
    return m_Settings;
}

bool Synthesizer::SetInstrument(int program, const Instrument& instrument)
{
    if (program < 0 || program >= InstrumentCount)
        return false;

    m_Instruments[program] = instrument;
    return true;
}

const Synthesizer::Instrument* Synthesizer::GetInstrument(int program) const
{
    if (program < 0 || program >= InstrumentCount)
        return 0;

    return &m_Instruments[program];
}

float Synthesizer::NoteToFrequency(int note)
{
    return 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
}

float Synthesizer::Clamp(float value, float minimum, float maximum)
{
    if (value < minimum)
        return minimum;

    if (value > maximum)
        return maximum;

    return value;
}

void Synthesizer::InitializeInstruments()
{
    for (int i = 0; i < InstrumentCount; ++i)
        InitializeInstrument(i, m_Instruments[i]);
}

void Synthesizer::InitializeInstrument(int program, Instrument& instrument)
{
    memset(&instrument, 0, sizeof(instrument));

    instrument.Type = WaveSine;
    instrument.Attack = 0.02f;
    instrument.Decay = 0.2f;
    instrument.Sustain = 0.7f;
    instrument.Release = 0.25f;
    instrument.Brightness = 1.0f;

    const int family = program / 8;

    switch (family)
    {
        case 0:
            instrument.Type = WavePlucked;
            instrument.Attack = 0.002f;
            instrument.Decay = 0.35f;
            instrument.Sustain = 0.45f;
            instrument.Release = 0.25f;
            instrument.Brightness = 1.3f;
            instrument.Harmonics[0] = 1.0f;
            instrument.Harmonics[1] = 0.55f;
            instrument.Harmonics[2] = 0.30f;
            instrument.Harmonics[3] = 0.18f;
            instrument.Harmonics[4] = 0.08f;
            instrument.Noise = 0.015f;
            break;

        case 1:
            instrument.Type = WaveSine;
            instrument.Attack = 0.005f;
            instrument.Decay = 0.4f;
            instrument.Sustain = 0.35f;
            instrument.Release = 0.3f;
            instrument.Harmonics[0] = 1.0f;
            instrument.Harmonics[1] = 0.35f;
            instrument.Harmonics[2] = 0.12f;
            instrument.Harmonics[3] = 0.05f;
            break;

        case 2:
            instrument.Type = WaveTriangle;
            instrument.Attack = 0.01f;
            instrument.Decay = 0.3f;
            instrument.Sustain = 0.75f;
            instrument.Release = 0.3f;
            instrument.Harmonics[0] = 1.0f;
            instrument.Harmonics[2] = 0.25f;
            instrument.Harmonics[4] = 0.10f;
            break;

        case 3:
            instrument.Type = WaveSquare;
            instrument.Attack = 0.005f;
            instrument.Decay = 0.1f;
            instrument.Sustain = 0.9f;
            instrument.Release = 0.15f;
            instrument.Harmonics[0] = 1.0f;
            instrument.Harmonics[1] = 0.5f;
            instrument.Harmonics[2] = 0.33f;
            instrument.Harmonics[3] = 0.25f;
            instrument.Harmonics[4] = 0.2f;
            break;

        case 4:
            instrument.Type = WaveSaw;
            instrument.Attack = 0.04f;
            instrument.Decay = 0.3f;
            instrument.Sustain = 0.8f;
            instrument.Release = 0.35f;
            instrument.Brightness = 1.2f;
            instrument.Harmonics[0] = 1.0f;
            instrument.Harmonics[1] = 0.5f;
            instrument.Harmonics[2] = 0.33f;
            instrument.Harmonics[3] = 0.25f;
            instrument.Harmonics[4] = 0.2f;
            instrument.Harmonics[5] = 0.16f;
            break;

        case 5:
            instrument.Type = WaveSaw;
            instrument.Attack = 0.03f;
            instrument.Decay = 0.2f;
            instrument.Sustain = 0.85f;
            instrument.Release = 0.25f;
            instrument.Brightness = 1.4f;
            instrument.Detune = 0.003f;
            instrument.Harmonics[0] = 1.0f;
            instrument.Harmonics[1] = 0.7f;
            instrument.Harmonics[2] = 0.45f;
            instrument.Harmonics[3] = 0.3f;
            instrument.Harmonics[4] = 0.2f;
            instrument.Harmonics[5] = 0.12f;
            break;

        case 6:
            instrument.Type = WaveSquare;
            instrument.Attack = 0.015f;
            instrument.Decay = 0.15f;
            instrument.Sustain = 0.8f;
            instrument.Release = 0.2f;
            instrument.Brightness = 1.3f;
            instrument.Harmonics[0] = 1.0f;
            instrument.Harmonics[1] = 0.8f;
            instrument.Harmonics[2] = 0.5f;
            instrument.Harmonics[3] = 0.3f;
            instrument.Noise = 0.02f;
            break;

        case 7:
            instrument.Type = WaveTriangle;
            instrument.Attack = 0.01f;
            instrument.Decay = 0.25f;
            instrument.Sustain = 0.65f;
            instrument.Release = 0.25f;
            instrument.Harmonics[0] = 1.0f;
            instrument.Harmonics[1] = 0.2f;
            instrument.Harmonics[2] = 0.1f;
            break;

        default:
            instrument.Type = WaveSine;
            instrument.Attack = 0.02f;
            instrument.Decay = 0.2f;
            instrument.Sustain = 0.7f;
            instrument.Release = 0.25f;
            instrument.Harmonics[0] = 1.0f;
            instrument.Harmonics[1] = 0.3f;
            instrument.Harmonics[2] = 0.15f;
            break;
    }

    float variation = 1.0f + ((program % 8) - 3.5f) * 0.035f;

    for (int i = 0; i < 16; ++i)
    {
        instrument.Harmonics[i] *= variation;
        variation *= 0.96f;
    }
}

float Synthesizer::GenerateNoise(unsigned int& seed) const
{
    seed = seed * 1664525u + 1013904223u;
    return ((float)((seed >> 8) & 0xFFFF) / 32767.5f) - 1.0f;
}

float Synthesizer::GenerateHarmonics(const Instrument& instrument, float phase, float frequency, float time, int note, int velocity) const
{
    float result = 0.0f;
    float total = 0.0f;

    const float brightness =
        instrument.Brightness * ((float)m_Settings.Brightness / 100.0f);

    for (int i = 0; i < 16; ++i)
    {
        const float harmonic = (float)(i + 1);
        const float weight = instrument.Harmonics[i];

        if (weight <= 0.0f)
            continue;

        const float rolloff =
            1.0f / powf(harmonic, 0.5f + (2.0f - brightness) * 0.7f);

        float amplitude = weight * rolloff;

        if (instrument.Type == WavePlucked)
            amplitude *= expf(-time * harmonic * 0.8f);

        const float detune = instrument.Detune * harmonic;
        const float harmonicPhase = phase * harmonic * (1.0f + detune);

        result += sinf(harmonicPhase) * amplitude;
        total += amplitude;
    }

    return total > 0.0f ? result / total : 0.0f;
}

float Synthesizer::GenerateWaveform(const Instrument& instrument, float phase, float frequency, float time, int note, int velocity) const
{
    float value = 0.0f;

    switch (instrument.Type)
    {
        case WaveSine:
            value = sinf(phase);
            break;

        case WaveTriangle:
            value = asinf(sinf(phase)) * (2.0f / Pi);
            break;

        case WaveSaw:
            value = 0.0f;
            for (int i = 1; i <= 12; ++i)
                value -= sinf(phase * i) / (float)i;
            value *= 0.6366f;
            break;

        case WaveSquare:
            value = sinf(phase) >= 0.0f ? 1.0f : -1.0f;
            break;

        case WavePulse:
            value = fmodf(phase, TwoPi) < TwoPi * 0.25f ? 1.0f : -1.0f;
            break;

        case WaveNoise:
            value = 0.0f;
            break;

        case WavePlucked:
            value = GenerateHarmonics(instrument, phase, frequency, time, note, velocity);
            break;
    }

    if (instrument.Type != WavePlucked)
    {
        const float harmonics =
            GenerateHarmonics(instrument, phase, frequency, time, note, velocity);

        value = (value * 0.35f) + (harmonics * 0.65f);
    }

    return value;
}

float Synthesizer::GenerateEnvelope(const Instrument& instrument, float time, float duration, int attackMs, int decayMs, int releaseMs) const
{
    float attack = instrument.Attack;
    float decay = instrument.Decay;
    const float sustain = Clamp(instrument.Sustain, 0.0f, 1.0f);
    float release = instrument.Release;

    if (attackMs >= 0)
        attack = (float)attackMs / 1000.0f;

    if (decayMs >= 0)
        decay = (float)decayMs / 1000.0f;

    if (releaseMs >= 0)
        release = (float)releaseMs / 1000.0f;

    if (attack > 0.0f && time < attack)
        return time / attack;

    time -= attack;

    if (decay > 0.0f && time < decay)
        return 1.0f - ((1.0f - sustain) * (time / decay));

    const float releaseStart = duration - release;

    if (release > 0.0f && time + attack >= releaseStart)
    {
        const float releaseTime = time + attack - releaseStart;
        return sustain * Clamp(1.0f - (releaseTime / release), 0.0f, 1.0f);
    }

    return sustain;
}

bool Synthesizer::Generate(int program, int note, int velocity, Sample& sample) const
{
    return GenerateInternal(program, note, velocity, sample, false);
}

bool Synthesizer::GenerateLoopSample(int program, int note, Sample& sample) const
{
    return GenerateInternal(program, note, 127, sample, true);
}

bool Synthesizer::GenerateInternal(int program, int note, int velocity, Sample& sample, bool loop) const
{
    sample.Clear();

    if (program < 0 || program >= InstrumentCount ||
        note < 0 || note > 127 ||
        velocity < 1 || velocity > 127)
        return false;

    const Instrument& instrument = m_Instruments[program];
    const int sampleRate = m_Settings.SampleRate;
    const int frameCount = loop
        ? LoopFrameCount
        : (sampleRate * m_Settings.DurationMs) / 1000;

    if (frameCount <= 0)
        return false;

    if (!sample.Data.Resize(frameCount)) 
        return false;

    sample.SampleRate = sampleRate;
    sample.LoopStart = loop ? 0 : 0;
    sample.LoopEnd = loop ? frameCount : 0;

    const float frequency = NoteToFrequency(note);
    const float velocityGain = (float)velocity / 127.0f;
    const float masterGain = loop ? 1.0f : (float)m_Settings.MasterGain / 100.0f;
    const float duration = (float)m_Settings.DurationMs / 1000.0f;

    unsigned int seed =
        0x12345678u ^ ((unsigned int)program << 16) ^ ((unsigned int)note << 8);

    for (int i = 0; i < frameCount; ++i)
    {
        float time;
        float phase;

        if (loop)
        {
            time = 0.0f;
            phase = TwoPi * LoopCycles * (float)i / (float)frameCount;
        }
        else
        {
            time = (float)i / (float)sampleRate;
            phase = TwoPi * frequency * time;
        }

        float value = GenerateWaveform(
            instrument, phase, frequency, time, note, velocity);

        if (!loop)
        {
            const float noise =
                GenerateNoise(seed) * instrument.Noise *
                ((float)m_Settings.NoiseLevel / 100.0f);

            value += noise;

            const float envelope = GenerateEnvelope(
                instrument, time, duration,
                m_Settings.AttackMs,
                m_Settings.DecayMs,
                m_Settings.ReleaseMs);

            value *= envelope * velocityGain * masterGain;
        }

        value = Clamp(value, -1.0f, 1.0f);
        sample.Data[i] = (int16_t)(value * 30000.0f);
    }

    return true;
}

}