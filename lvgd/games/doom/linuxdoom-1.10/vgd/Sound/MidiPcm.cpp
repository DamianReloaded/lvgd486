#include "MidiPcm.hpp"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

namespace vgd
{

static const double Pi = 3.14159265358979323846;

MidiPcm::MidiPcm()
    : m_Mixer(0),
      m_Initialized(false),
      m_SampleBankLoaded(false),
      m_VoiceAge(0),
      m_SampleAge(0),
      m_RunningStatus(0),
      m_MessageDataCount(0),
      m_MessageDataRequired(0),
      m_InSysEx(false),
      m_MasterVolume(127)
{
    memset(m_Channels, 0, sizeof(m_Channels));
    memset(m_Voices, 0, sizeof(m_Voices));
    memset(m_MessageData, 0, sizeof(m_MessageData));

    for (int i = 0; i < InstrumentSlotCount; ++i)
    {
        m_InstrumentSlots[i].Program = -1;
        m_InstrumentSlots[i].Note = -1;
        m_InstrumentSlots[i].Age = 0;
        m_InstrumentSlots[i].Procedural = false;
    }
}

MidiPcm::~MidiPcm()
{
    Shutdown();
}

void MidiPcm::SetMixer(AudioMixer* mixer)
{
    if (m_Mixer == mixer)
        return;

    SilenceAll();
    m_Mixer = mixer;

    if (m_Mixer)
        m_Mixer->SetUsesPcmMidi(true);
}

Synthesizer& MidiPcm::GetSynthesizer()
{
    return m_Synthesizer;
}

const Synthesizer& MidiPcm::GetSynthesizer() const
{
    return m_Synthesizer;
}

bool MidiPcm::Initialize(const char* path)
{
    (void)path;

    if (m_Initialized)
        return true;

    InitializeChannels();

    memset(m_Voices, 0, sizeof(m_Voices));

    for (int i = 0; i < InstrumentSlotCount; ++i)
    {
        m_InstrumentSlots[i].Program = -1;
        m_InstrumentSlots[i].Note = -1;
        m_InstrumentSlots[i].Age = 0;
        m_InstrumentSlots[i].Procedural = false;
        m_InstrumentSlots[i].Sample.Clear();
    }

    m_VoiceAge = 0;
    m_SampleAge = 0;
    m_RunningStatus = 0;
    m_MessageDataCount = 0;
    m_MessageDataRequired = 0;
    m_InSysEx = false;
    m_SampleBankLoaded = false;
    m_Initialized = true;

    return true;
}

bool MidiPcm::IsInitialized() const
{
    return m_Initialized;
}

int MidiPcm::GetPortCount() const
{
    return m_Initialized ? 1 : 0;
}

// -----------------------------------------------------------------------------
// Channel initialization
// -----------------------------------------------------------------------------

void MidiPcm::InitializeChannels()
{
    for (int i = 0; i < ChannelCount; ++i)
        ResetChannel(i);
}

void MidiPcm::ResetChannel(int channel)
{
    if (!IsValidChannel(channel))
        return;

    ChannelState& state = m_Channels[channel];

    state.Program = 0;
    state.Volume = 100;
    state.Expression = 127;
    state.Pan = 64;
    state.PitchBend = 8192;
    state.Modulation = 0;
    state.Sustain = false;
}

// -----------------------------------------------------------------------------
// Instrument cache and synthesis
// -----------------------------------------------------------------------------

int MidiPcm::FindInstrumentSlot(int program, int note) const
{
    for (int i = 0; i < InstrumentSlotCount; ++i)
    {
        if (m_InstrumentSlots[i].Program == program &&
            m_InstrumentSlots[i].Note == note)
            return i;
    }

    return -1;
}

int MidiPcm::AllocateInstrumentSlot(int program, int note)
{
    int slot = FindInstrumentSlot(program, note);

    if (slot >= 0)
        return slot;

    // Prefer unused slots.
    for (int i = 0; i < InstrumentSlotCount; ++i)
    {
        if (m_InstrumentSlots[i].Program < 0)
            return i;
    }

    // Reuse the oldest slot that is not referenced by an active voice.
    int oldestSlot = -1;
    unsigned int oldestAge = 0xFFFFFFFFU;

    for (int i = 0; i < InstrumentSlotCount; ++i)
    {
        bool inUse = false;

        for (int v = 0; v < MaxVoices; ++v)
        {
            if (m_Voices[v].Active &&
                m_Voices[v].InstrumentSlot == i)
            {
                inUse = true;
                break;
            }
        }

        if (!inUse && m_InstrumentSlots[i].Age < oldestAge)
        {
            oldestAge = m_InstrumentSlots[i].Age;
            oldestSlot = i;
        }
    }

    return oldestSlot;
}

bool MidiPcm::GenerateInstrument(int program, int note, int slot)
{
    if (program < 0 || program >= InstrumentCount ||
        note < 0 || note > 127 ||
        slot < 0 || slot >= InstrumentSlotCount)
        return false;

    InstrumentSlot& instrumentSlot = m_InstrumentSlots[slot];

    if (!m_Synthesizer.GenerateLoopSample(
            program, note, instrumentSlot.Sample))
    {
        return false;
    }

    instrumentSlot.Procedural = true;
    instrumentSlot.Program = program;
    instrumentSlot.Note = note;
    instrumentSlot.Age = ++m_SampleAge;

    m_SampleBankLoaded = true;

    fprintf(stderr,
        "MidiPcm: Generated program=%d note=%d slot=%d rate=%d frames=%d\n",
        program,
        note,
        slot,
        instrumentSlot.Sample.SampleRate,
        instrumentSlot.Sample.GetFrameCount());

    return true;
}

bool MidiPcm::GetInstrumentSamples(int program, int note, int& slot)
{
    slot = FindInstrumentSlot(program, note);

    if (slot >= 0)
    {
        m_InstrumentSlots[slot].Age = ++m_SampleAge;
        return true;
    }

    slot = AllocateInstrumentSlot(program, note);

    if (slot < 0)
        return false;

    if (!GenerateInstrument(program, note, slot))
        return false;

    return true;
}

// -----------------------------------------------------------------------------
// MIDI stream parsing
// -----------------------------------------------------------------------------

bool MidiPcm::SendByte(unsigned char value)
{
    if (!m_Initialized)
        return false;

    if (value >= 0xF8)
        return true;

    if (value & 0x80)
    {
        if (value == 0xF0)
        {
            m_InSysEx = true;
            m_RunningStatus = 0;
            m_MessageDataCount = 0;
            return true;
        }

        if (value == 0xF7)
        {
            m_InSysEx = false;
            return true;
        }

        if (value >= 0xF0)
        {
            m_RunningStatus = 0;
            m_MessageDataCount = 0;
            m_InSysEx = false;
            return true;
        }

        m_InSysEx = false;
        m_RunningStatus = value;
        m_MessageDataCount = 0;

        const unsigned char type = value & 0xF0;
        m_MessageDataRequired =
            (type == 0xC0 || type == 0xD0) ? 1 : 2;

        return true;
    }

    if (m_InSysEx)
        return true;

    if (m_RunningStatus == 0)
        return false;

    if (m_MessageDataCount >= 2)
        m_MessageDataCount = 0;

    m_MessageData[m_MessageDataCount++] = value;

    if (m_MessageDataCount == m_MessageDataRequired)
    {
        const unsigned char data1 = m_MessageData[0];
        const unsigned char data2 =
            m_MessageDataRequired == 2 ? m_MessageData[1] : 0;

        m_MessageDataCount = 0;
        return DispatchMessage(m_RunningStatus, data1, data2);
    }

    return true;
}

bool MidiPcm::SendMessage(unsigned char status, unsigned char data1, unsigned char data2)
{
    if (!m_Initialized ||
        (status & 0x80) == 0 ||
        data1 > 127 ||
        data2 > 127)
        return false;

    return DispatchMessage(status, data1, data2);
}

bool MidiPcm::DispatchMessage(unsigned char status, unsigned char data1, unsigned char data2)
{
    if (status < 0x80 || status >= 0xF0)
        return false;

    const int channel = status & 0x0F;

    switch (status & 0xF0)
    {
        case 0x80:
            return NoteOff(channel, data1);

        case 0x90:
            return NoteOn(channel, data1, data2);

        case 0xA0:
            return true;

        case 0xB0:
            return ControlChange(channel, data1, data2);

        case 0xC0:
            return ProgramChange(channel, data1);

        case 0xD0:
            return true;

        case 0xE0:
            m_Channels[channel].PitchBend =
                static_cast<int>(data1) |
                (static_cast<int>(data2) << 7);

            for (int i = 0; i < MaxVoices; ++i)
            {
                if (m_Voices[i].Active &&
                    m_Voices[i].Channel == channel)
                    UpdateMixerVoice(i);
            }

            return true;
    }

    return false;
}

// -----------------------------------------------------------------------------
// Validation and voice allocation
// -----------------------------------------------------------------------------

bool MidiPcm::IsValidChannel(int channel) const
{
    return channel >= 0 && channel < ChannelCount;
}

bool MidiPcm::IsValidNote(int note) const
{
    return note >= 0 && note <= 127;
}

bool MidiPcm::IsValidValue(int value) const
{
    return value >= 0 && value <= 127;
}

int MidiPcm::FindFreeVoice() const
{
    for (int i = 0; i < MaxVoices; ++i)
    {
        if (!m_Voices[i].Active)
            return i;
    }

    int oldestVoice = -1;
    unsigned int oldestAge = 0xFFFFFFFFU;

    for (int i = 0; i < MaxVoices; ++i)
    {
        if (!m_Voices[i].KeyDown &&
            m_Voices[i].Age < oldestAge)
        {
            oldestAge = m_Voices[i].Age;
            oldestVoice = i;
        }
    }

    if (oldestVoice >= 0)
        return oldestVoice;

    oldestVoice = 0;
    oldestAge = m_Voices[0].Age;

    for (int i = 1; i < MaxVoices; ++i)
    {
        if (m_Voices[i].Age < oldestAge)
        {
            oldestAge = m_Voices[i].Age;
            oldestVoice = i;
        }
    }

    return oldestVoice;
}

int MidiPcm::FindVoice(int channel, int note) const
{
    int oldestVoice = -1;
    unsigned int oldestAge = 0xFFFFFFFFU;

    for (int i = 0; i < MaxVoices; ++i)
    {
        const VoiceState& voice = m_Voices[i];

        if (!voice.Active ||
            voice.Channel != channel ||
            voice.Note != note ||
            !voice.KeyDown)
            continue;

        if (voice.Age < oldestAge)
        {
            oldestAge = voice.Age;
            oldestVoice = i;
        }
    }

    return oldestVoice;
}

bool MidiPcm::AllocateVoice(int channel, int note, int velocity)
{
    const int index = FindFreeVoice();

    if (index < 0 || index >= MaxVoices)
        return false;

    if (m_Voices[index].Active)
        ReleaseVoice(index);

    int slot = -1;

    if (!GetInstrumentSamples(m_Channels[channel].Program, note, slot))
        return false;

    VoiceState& voice = m_Voices[index];

    memset(&voice, 0, sizeof(VoiceState));

    voice.Active = true;
    voice.KeyDown = true;
    voice.Sustained = false;
    voice.Channel = channel;
    voice.Note = note;
    voice.Velocity = velocity;
    voice.Program = m_Channels[channel].Program;
    voice.MixerVoice = index;
    voice.InstrumentSlot = slot;
    voice.Age = ++m_VoiceAge;

    StartMixerVoice(index);

    return voice.Active;
}

void MidiPcm::ReleaseVoice(int index)
{
    if (index < 0 || index >= MaxVoices)
        return;

    if (m_Voices[index].Active && m_Mixer)
    {
        m_Mixer->ScheduleStop(
            m_Mixer->GetMixedFramePosition(),
            m_Voices[index].MixerVoice,
            true);
    }

    memset(&m_Voices[index], 0, sizeof(VoiceState));
}

void MidiPcm::ReleaseChannel(int channel)
{
    for (int i = 0; i < MaxVoices; ++i)
    {
        if (m_Voices[i].Active &&
            m_Voices[i].Channel == channel)
            ReleaseVoice(i);
    }
}

// -----------------------------------------------------------------------------
// Mixer playback
// -----------------------------------------------------------------------------

int MidiPcm::GetSourceRate(const VoiceState& voice) const
{
    const int bend = m_Channels[voice.Channel].PitchBend - 8192;
    const double bendSemitones = (static_cast<double>(bend) / 8192.0) * 2.0;
    const double pitchBend = pow(2.0, bendSemitones / 12.0);

    const InstrumentSlot& slot = m_InstrumentSlots[voice.InstrumentSlot];

    double sourceRate;

    if (slot.Procedural)
    {
        const double frequency = Synthesizer::NoteToFrequency(voice.Note);

        sourceRate = frequency *
            (static_cast<double>(Synthesizer::LoopFrameCount) /
             Synthesizer::LoopCycles);
    }
    else
    {
        const int semitoneDifference = voice.Note - slot.Note;
        sourceRate = slot.Sample.SampleRate *
            pow(2.0, semitoneDifference / 12.0);
    }

    sourceRate *= pitchBend;

    return static_cast<int>(sourceRate + 0.5);
}

void MidiPcm::GetStereoVolume(const VoiceState& voice, int& left, int& right) const
{
    const ChannelState& channel = m_Channels[voice.Channel];

    long long volume =
        (long long)voice.Velocity *
        channel.Volume *
        channel.Expression *
        m_MasterVolume;

    volume = (volume * 32767) / (127LL * 127 * 127 * 127);

    left = (int)volume;
    right = (int)volume;

    if (channel.Pan < 64)
        right = right * channel.Pan / 64;
    else if (channel.Pan > 64)
        left = left * (127 - channel.Pan) / 63;
}

void MidiPcm::StartMixerVoice(int index)
{
    if (!m_Mixer || index < 0 || index >= MaxVoices)
        return;

    VoiceState& voice = m_Voices[index];

    if (!voice.Active ||
        voice.InstrumentSlot < 0 ||
        voice.InstrumentSlot >= InstrumentSlotCount)
        return;

    const Synthesizer::Sample& sample =
        m_InstrumentSlots[voice.InstrumentSlot].Sample;

    if (sample.Data.Empty())
    {
        ReleaseVoice(index);
        return;
    }

    int left;
    int right;
    GetStereoVolume(voice, left, right);

    if (!m_Mixer->SchedulePlay(
            m_Mixer->GetMixedFramePosition(),
            voice.MixerVoice,
            &sample.Data[0],
            sample.Data.Size(),
            GetSourceRate(voice),
            left,
            right,
            true,
            true))
    {
        fprintf(stderr,
            "MidiPcm: SchedulePlay failed, voice=%d\n",
            voice.MixerVoice);

        memset(&voice, 0, sizeof(VoiceState));
    }
}

void MidiPcm::UpdateMixerVoice(int index)
{
    if (!m_Mixer || index < 0 || index >= MaxVoices)
        return;

    const VoiceState& voice = m_Voices[index];

    if (!voice.Active)
        return;

    int left;
    int right;
    GetStereoVolume(voice, left, right);

    m_Mixer->ScheduleUpdate(
        m_Mixer->GetMixedFramePosition(),
        voice.MixerVoice,
        GetSourceRate(voice),
        left,
        right,
        true);
}

// -----------------------------------------------------------------------------
// MIDI note and program messages
// -----------------------------------------------------------------------------

bool MidiPcm::NoteOn(int channel, int note, int velocity)
{
    if (!m_Initialized ||
        !IsValidChannel(channel) ||
        !IsValidNote(note) ||
        !IsValidValue(velocity))
        return false;

    if (velocity == 0)
        return NoteOff(channel, note);

    return AllocateVoice(channel, note, velocity);
}

bool MidiPcm::NoteOff(int channel, int note)
{
    if (!m_Initialized ||
        !IsValidChannel(channel) ||
        !IsValidNote(note))
        return false;

    const int index = FindVoice(channel, note);

    if (index < 0)
        return true;

    VoiceState& voice = m_Voices[index];
    voice.KeyDown = false;

    if (m_Channels[channel].Sustain)
        voice.Sustained = true;
    else
        ReleaseVoice(index);

    return true;
}

bool MidiPcm::ProgramChange(int channel, int program)
{
    if (!m_Initialized ||
        !IsValidChannel(channel) ||
        !IsValidNote(program))
        return false;

    m_Channels[channel].Program = program;
    return true;
}

// -----------------------------------------------------------------------------
// Controller messages
// -----------------------------------------------------------------------------

bool MidiPcm::ControlChange(int channel, int control, int value)
{
    if (!m_Initialized ||
        !IsValidChannel(channel) ||
        !IsValidValue(control) ||
        !IsValidValue(value))
        return false;

    ChannelState& state = m_Channels[channel];

    switch (control)
    {
        case 1:
            state.Modulation = value;
            break;

        case 7:
            state.Volume = value;
            break;

        case 10:
            state.Pan = value;
            break;

        case 11:
            state.Expression = value;
            break;

        case 64:
        {
            const bool enabled = value >= 64;

            if (state.Sustain && !enabled)
            {
                for (int i = 0; i < MaxVoices; ++i)
                {
                    VoiceState& voice = m_Voices[i];

                    if (voice.Active &&
                        voice.Channel == channel &&
                        voice.Sustained &&
                        !voice.KeyDown)
                        ReleaseVoice(i);
                }
            }

            state.Sustain = enabled;
            break;
        }

        case 120:
            ReleaseChannel(channel);
            break;

        case 121:
            state.Volume = 100;
            state.Expression = 127;
            state.Pan = 64;
            state.PitchBend = 8192;
            state.Modulation = 0;
            state.Sustain = false;

            for (int i = 0; i < MaxVoices; ++i)
            {
                VoiceState& voice = m_Voices[i];

                if (voice.Active &&
                    voice.Channel == channel &&
                    voice.Sustained &&
                    !voice.KeyDown)
                    ReleaseVoice(i);
            }
            break;

        case 123:
            for (int i = 0; i < MaxVoices; ++i)
            {
                VoiceState& voice = m_Voices[i];

                if (!voice.Active || voice.Channel != channel)
                    continue;

                voice.KeyDown = false;

                if (state.Sustain)
                    voice.Sustained = true;
                else
                    ReleaseVoice(i);
            }
            break;

        default:
            break;
    }

    if (control == 7 ||
        control == 10 ||
        control == 11 ||
        control == 121)
    {
        for (int i = 0; i < MaxVoices; ++i)
        {
            if (m_Voices[i].Active &&
                m_Voices[i].Channel == channel)
                UpdateMixerVoice(i);
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
// Instrument and voice information
// -----------------------------------------------------------------------------

bool MidiPcm::IsInstrumentLoaded(int program) const
{
    if (program < 0 || program >= InstrumentCount)
        return false;

    for (int i = 0; i < InstrumentSlotCount; ++i)
    {
        if (m_InstrumentSlots[i].Program == program)
            return true;
    }

    return false;
}

bool MidiPcm::IsInstrumentSynthesized(int program) const
{
    return program >= 0 && program < InstrumentCount;
}

bool MidiPcm::GetInstrumentInfo(int program, InstrumentInfo& info) const
{
    if (program < 0 || program >= InstrumentCount)
        return false;

    const Synthesizer::Instrument* instrument =
        m_Synthesizer.GetInstrument(program);

    if (!instrument)
        return false;

    info.Loaded = IsInstrumentLoaded(program);
    info.Synthesized = true;
    info.RootNote = 60;

    switch (instrument->Type)
    {
        case Synthesizer::WaveSine:
            info.Type = WaveformSine;
            break;

        case Synthesizer::WaveTriangle:
            info.Type = WaveformTriangle;
            break;

        case Synthesizer::WaveSaw:
            info.Type = WaveformSawtooth;
            break;

        case Synthesizer::WaveSquare:
            info.Type = WaveformSquare;
            break;

        case Synthesizer::WavePulse:
            info.Type = WaveformPulse;
            break;

        case Synthesizer::WaveNoise:
            info.Type = WaveformNoise;
            break;

        case Synthesizer::WavePlucked:
            info.Type = WaveformPlucked;
            break;

        default:
            info.Type = WaveformSine;
            break;
    }

    info.Attack = (int)(instrument->Attack * 1000.0f);
    info.Decay = (int)(instrument->Decay * 1000.0f);
    info.Sustain = (int)(instrument->Sustain * 100.0f);
    info.Release = (int)(instrument->Release * 1000.0f);

    return true;
}

int MidiPcm::GetActiveVoiceCount() const
{
    int count = 0;

    for (int i = 0; i < MaxVoices; ++i)
    {
        if (m_Voices[i].Active)
            ++count;
    }

    return count;
}

bool MidiPcm::GetVoiceInfo(int index, VoiceInfo& info) const
{
    if (index < 0 || index >= MaxVoices)
        return false;

    const VoiceState& voice = m_Voices[index];

    info.Active = voice.Active;
    info.KeyDown = voice.KeyDown;
    info.Sustained = voice.Sustained;
    info.Channel = voice.Channel;
    info.Note = voice.Note;
    info.Velocity = voice.Velocity;
    info.Program = voice.Program;
    info.Age = voice.Age;

    return true;
}

// -----------------------------------------------------------------------------
// Silence and shutdown
// -----------------------------------------------------------------------------

bool MidiPcm::SilenceAll()
{
    if (!m_Initialized)
        return false;

    for (int i = 0; i < MaxVoices; ++i)
        ReleaseVoice(i);

    for (int i = 0; i < ChannelCount; ++i)
        m_Channels[i].Sustain = false;

    return true;
}

void MidiPcm::Shutdown()
{
    if (m_Initialized)
        SilenceAll();

    m_Initialized = false;
    m_SampleBankLoaded = false;

    for (int i = 0; i < InstrumentSlotCount; ++i)
    {
        m_InstrumentSlots[i].Program = -1;
        m_InstrumentSlots[i].Note = -1;
        m_InstrumentSlots[i].Age = 0;
        m_InstrumentSlots[i].Procedural = false;
        m_InstrumentSlots[i].Sample.Clear();
    }

    m_RunningStatus = 0;
    m_MessageDataCount = 0;
    m_MessageDataRequired = 0;
    m_InSysEx = false;
}

bool MidiPcm::SetMasterVolume(int volume)
{
    if (volume < 0)
        volume = 0;
    else if (volume > 127)
        volume = 127;

    if (m_MasterVolume == volume)
        return true;

    m_MasterVolume = volume;

    for (int i = 0; i < MaxVoices; ++i)
    {
        if (m_Voices[i].Active)
            UpdateMixerVoice(i);
    }

    return true;
}

bool MidiPcm::SetInstrument(int program, int rootNote, const int16_t* samples, int sampleFrames, int sampleRate)
{
    if (program < 0 || program >= InstrumentCount ||
        rootNote < 0 || rootNote > 127 ||
        !samples || sampleFrames <= 0 || sampleRate <= 0)
        return false;

    const int slot = AllocateInstrumentSlot(program, rootNote);

    if (slot < 0)
        return false;

    InstrumentSlot& instrumentSlot = m_InstrumentSlots[slot];

    instrumentSlot.Sample.Data.Assign(samples, samples + sampleFrames);
    instrumentSlot.Sample.SampleRate = sampleRate;
    instrumentSlot.Sample.LoopStart = 0;
    instrumentSlot.Sample.LoopEnd = sampleFrames;
    instrumentSlot.Program = program;
    instrumentSlot.Note = rootNote;
    instrumentSlot.Age = ++m_SampleAge;
    instrumentSlot.Procedural = false;

    m_SampleBankLoaded = true;
    return true;
}

}