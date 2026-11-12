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

    if (frameCount <= 0 || !sample.Data.Resize(frameCount))
        return false;

    sample.SampleRate = sampleRate;
    sample.LoopStart = 0;
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

// -----------------------------------------------------------------------------
// Procedural General MIDI percussion
// -----------------------------------------------------------------------------

bool Synthesizer::GenerateDrum(int note, int velocity, Sample& sample) const
{
    sample.Clear();

    if (note < 0 || note > 127 || velocity < 1 || velocity > 127)
        return false;

    enum DrumType
    {
        DrumKick,
        DrumSnare,
        DrumTom,
        DrumHat,
        DrumCymbal,
        DrumClap,
        DrumRim,
        DrumCowbell,
        DrumClick,
        DrumFallback
    };

    DrumType type = DrumFallback;
    float duration = 0.35f;
    float baseFrequency = 180.0f;
    float noiseAmount = 0.0f;
    float toneAmount = 0.0f;
    float decay = 8.0f;

    switch (note)
    {
        case 35:
        case 36:
            type = DrumKick;
            duration = 0.65f;
            baseFrequency = note == 35 ? 72.0f : 92.0f;
            toneAmount = 1.0f;
            noiseAmount = 0.12f;
            decay = 7.0f;
            break;
    }

    // The remaining General MIDI notes are grouped by their physical sound.
    if (note == 37 || note == 39 || note == 56 || note == 60 || note == 61 ||
        note == 62 || note == 63 || note == 64 || note == 65 || note == 66 ||
        note == 67 || note == 68 || note == 69 || note == 70 || note == 71 ||
        note == 72 || note == 73 || note == 74 || note == 75 || note == 76 ||
        note == 77 || note == 78 || note == 79 || note == 80 || note == 81)
    {
        type = note == 37 ? DrumRim : DrumFallback;
        duration = 0.18f;
        baseFrequency = 900.0f;
        noiseAmount = 0.55f;
        toneAmount = 0.35f;
        decay = 18.0f;
    }

    switch (note)
    {
        case 38:
        case 40:
            type = DrumSnare;
            duration = 0.48f;
            baseFrequency = note == 38 ? 185.0f : 205.0f;
            toneAmount = 0.55f;
            noiseAmount = 0.45f; // Was 0.9f
            decay = 10.0f;
            break;

        case 41:
        case 43:
        case 45:
        case 47:
        case 48:
        case 50:
            type = DrumTom;
            duration = 0.55f;
            baseFrequency = 95.0f + (float)(note - 41) * 13.0f;
            toneAmount = 0.95f;
            noiseAmount = 0.12f;
            decay = 6.5f;
            break;

        case 42:
        case 44:
            type = DrumHat;
            duration = 0.12f;
            baseFrequency = 6500.0f;
            toneAmount = 0.65f;
            noiseAmount = 0.25f;
            decay = 38.0f;
            break;

        case 46:
            type = DrumHat;
            duration = 0.15f;
            baseFrequency = 6200.0f;
            toneAmount = 0.75f;
            noiseAmount = 0.2f;
            decay = 5.5f;
            break;

        case 49:
        case 52:
        case 55:
        case 57:
        case 59:
            type = DrumCymbal;
            duration = 0.35f;
            baseFrequency = 3600.0f;
            toneAmount = 0.35f;
            noiseAmount = 0.20f;
            decay = 9.0f;
            break;

        case 51:
        case 53:
        case 54:
        case 58:
            type = DrumCymbal;
            duration = 0.28f;
            baseFrequency = 2600.0f;
            toneAmount = 0.30f;
            noiseAmount = 0.15f;
            decay = 11.0f;
            break;
    }

    if (note == 39)
    {
        type = DrumClap;
        duration = 0.32f;
        noiseAmount = 1.0f;
        toneAmount = 0.0f;
        decay = 13.0f;
    }

    if (note == 37)
    {
        type = DrumRim;
        duration = 0.16f;
        baseFrequency = 1550.0f;
        toneAmount = 0.85f;
        noiseAmount = 0.18f;
        decay = 28.0f;
    }

    if (note == 56)
    {
        type = DrumCowbell;
        duration = 0.42f;
        baseFrequency = 560.0f;
        toneAmount = 1.0f;
        noiseAmount = 0.0f;
        decay = 7.0f;
    }

    if (note == 75 || note == 76 || note == 77 || note == 78 ||
        note == 79 || note == 80 || note == 81)
    {
        type = DrumClick;
        duration = 0.12f;
        baseFrequency = 1300.0f + (float)(note - 75) * 180.0f;
        toneAmount = 0.75f;
        noiseAmount = 0.35f;
        decay = 24.0f;
    }

    const int sampleRate = m_Settings.SampleRate;
    const int frameCount = (int)((float)sampleRate * duration);

    if (frameCount <= 0 || !sample.Data.Resize(frameCount))
        return false;

    sample.SampleRate = sampleRate;
    sample.LoopStart = 0;
    sample.LoopEnd = 0;

    const float velocityGain = (float)velocity / 127.0f;
    const float masterGain = (float)m_Settings.MasterGain / 100.0f;
    const float noiseGain = (float)m_Settings.NoiseLevel / 100.0f;
    const float brightness = (float)m_Settings.Brightness / 100.0f;

    unsigned int seed =
        0xA341316Cu ^ ((unsigned int)note * 0x9E3779B9u) ^
        ((unsigned int)velocity << 16);

    float noiseLow = 0.0f;
    float previousNoise = 0.0f;
    float phase1 = 0.0f;
    float phase2 = 0.0f;
    float phase3 = 0.0f;

    const float metallicFrequencies[6] =
    {
        baseFrequency,
        baseFrequency * 1.342f,
        baseFrequency * 1.731f,
        baseFrequency * 2.113f,
        baseFrequency * 2.487f,
        baseFrequency * 3.019f
    };

    for (int i = 0; i < frameCount; ++i)
    {
        const float time = (float)i / (float)sampleRate;
        const float progress = time / duration;
        const float noise = GenerateNoise(seed);

        const float noiseCutoff =
            type == DrumHat || type == DrumCymbal ? 0.12f : 0.035f;

        noiseLow += (noise - noiseLow) * noiseCutoff;
        const float highNoise = noise - noiseLow;

        float envelope = expf(-time * decay);
        float tone = 0.0f;
        float transient = 0.0f;

        switch (type)
        {
            case DrumKick:
            {
                const float pitch = baseFrequency *
                    (0.42f + 0.58f * expf(-time * 22.0f));

                phase1 += TwoPi * pitch / (float)sampleRate;
                phase2 += TwoPi * (pitch * 1.98f) / (float)sampleRate;

                tone = sinf(phase1) * 0.9f +
                       sinf(phase2) * 0.1f;

                transient = highNoise * expf(-time * 100.0f) * 0.25f;
                envelope = expf(-time * decay);
                break;
            }

            case DrumSnare:
            {
                const float pitch = baseFrequency *
                    (0.82f + 0.18f * expf(-time * 35.0f));

                phase1 += TwoPi * pitch / (float)sampleRate;
                phase2 += TwoPi * (pitch * 1.61f) / (float)sampleRate;

                tone = sinf(phase1) * 0.72f +
                    sinf(phase2) * 0.28f;

                transient = highNoise * expf(-time * 12.0f);
                envelope = expf(-time * decay);
                break;
            }

            case DrumTom:
            {
                const float pitch = baseFrequency *
                    (0.68f + 0.32f * expf(-time * 17.0f));

                phase1 += TwoPi * pitch / (float)sampleRate;
                phase2 += TwoPi * (pitch * 2.02f) / (float)sampleRate;

                tone = sinf(phase1) * 0.85f +
                       sinf(phase2) * 0.15f;

                transient = highNoise * expf(-time * 55.0f) * 0.2f;
                envelope = expf(-time * decay);
                break;
            }

            case DrumHat:
            case DrumCymbal:
            {
                float metallic = 0.0f;

                for (int p = 0; p < 6; ++p)
                {
                    const float frequency = metallicFrequencies[p];
                    const float weight = 1.0f / (float)(p + 1);

                    if (type == DrumHat && frequency > 0.46f * sampleRate)
                        continue;

                    if (type == DrumCymbal && frequency > 0.46f * sampleRate)
                        continue;

                    metallic += sinf(
                        TwoPi * frequency * time) * weight;
                }

                tone = metallic * 0.22f;
                transient = highNoise * 0.85f;
                envelope = expf(-time * decay);
                break;
            }

            case DrumClap:
            {
                const float burst1 = expf(-time * 65.0f);
                const float burst2 = time > 0.018f
                    ? expf(-(time - 0.018f) * 85.0f) : 0.0f;
                const float burst3 = time > 0.036f
                    ? expf(-(time - 0.036f) * 105.0f) : 0.0f;
                const float burst4 = time > 0.055f
                    ? expf(-(time - 0.055f) * 130.0f) : 0.0f;

                transient = highNoise *
                    (burst1 + burst2 * 0.75f +
                     burst3 * 0.55f + burst4 * 0.35f);

                envelope = expf(-time * 8.0f);
                break;
            }

            case DrumRim:
            case DrumCowbell:
            case DrumClick:
            case DrumFallback:
            {
                phase1 += TwoPi * baseFrequency / (float)sampleRate;
                phase2 += TwoPi * (baseFrequency * 1.47f) / (float)sampleRate;
                phase3 += TwoPi * (baseFrequency * 2.13f) / (float)sampleRate;

                tone = sinf(phase1) * 0.55f +
                       sinf(phase2) * 0.30f +
                       sinf(phase3) * 0.15f;

                transient = highNoise * 0.35f;
                envelope = expf(-time * decay);
                break;
            }
        }

        float value = tone * toneAmount * envelope;
        value += transient * noiseAmount * noiseGain;

        if (type == DrumHat || type == DrumCymbal)
            value *= 0.55f + brightness * 0.45f;

        if (type == DrumKick)
            value += previousNoise * 0.0f;

        previousNoise = noise;

        value *= velocityGain * masterGain * 0.85f;
        value = Clamp(value, -1.0f, 1.0f);

        sample.Data[i] = (int16_t)(value * 30000.0f);
    }

    return true;
}

}