#include "MidiSong.hpp"
#include <stdlib.h>

namespace vgd
{
    MidiSong::MidiSong()
        : TempoBPM(110), TicksPerBeat(96), StepsPerBeat(4), Bars(16),
          Looping(true), Built(false), TimelineMode(false), TimelineLength(0)
    {
    }

    MidiSong::~MidiSong() { Clear(); }

    void MidiSong::Clear()
    {
        Instruments.Clear(); Tracks.Clear(); StepEvents.Clear(); MidiEvents.Clear(); Tempos.Clear();
        Melody.Clear(); BassRoots.Clear(); Built = false; TimelineMode = false; TimelineLength = 0;
    }

    bool MidiSong::SetTempo(int bpm)
    {
        if (bpm < 1 || bpm > 300) return false;
        TempoBPM = bpm; Built = false; return true;
    }
    bool MidiSong::SetTicksPerBeat(unsigned int ticks)
    {
        if (ticks == 0 || ticks > 65535 || ticks % StepsPerBeat != 0) return false;
        TicksPerBeat = ticks; Built = false; return true;
    }
    bool MidiSong::SetStepsPerBeat(unsigned int steps)
    {
        if (steps == 0 || steps > 16 || TicksPerBeat % steps != 0) return false;
        StepsPerBeat = steps; Built = false; return true;
    }
    bool MidiSong::SetBars(unsigned int bars)
    {
        if (bars == 0 || bars > 4096) return false;
        Bars = bars; Built = false; return true;
    }
    bool MidiSong::SetLooping(bool looping) { Looping = looping; return true; }

    int MidiSong::AddInstrument(int channel, int program, int volume, int basePitch)
    {
        if (channel < 0 || channel > 15 || program < -1 || program > 127 ||
            volume < -1 || volume > 127 || basePitch < 0 || basePitch > 127) return -1;
        Instrument value; value.Channel = channel; value.Program = program;
        value.Volume = volume; value.BasePitch = basePitch;
        if (!Instruments.PushBack(value)) return -1;
        Built = false; return Instruments.Size() - 1;
    }

    int MidiSong::AddTrack(int instrumentIndex, int displayRow)
    {
        if (instrumentIndex < 0 || instrumentIndex >= Instruments.Size() || displayRow < 0) return -1;
        Track value; value.InstrumentIndex = instrumentIndex; value.DisplayRow = displayRow;
        if (!Tracks.PushBack(value)) return -1;
        Built = false; return Tracks.Size() - 1;
    }

    bool MidiSong::AddStepEvent(int trackIndex, unsigned int step, int pitchMode,
                                int pitch, int velocity, unsigned int durationTicks)
    {
        if (trackIndex < 0 || trackIndex >= Tracks.Size() || step >= GetStepsPerBar() ||
            pitchMode < PitchFixed || pitchMode > PitchBass || pitch < 0 || pitch > 127 ||
            velocity < 1 || velocity > 127 || durationTicks == 0) return false;
        StepEvent value; value.TrackIndex = trackIndex; value.Step = step;
        value.PitchMode = pitchMode; value.Pitch = pitch; value.Velocity = velocity;
        value.DurationTicks = durationTicks;
        if (!StepEvents.PushBack(value)) return false;
        Built = false; return true;
    }

    bool MidiSong::SetMelody(const int* values, int count)
    {
        if (count < 0 || (count && !values)) return false;
        Melody.Clear();
        for (int i = 0; i < count; ++i) if (!Melody.PushBack(values[i])) return false;
        Built = false; return true;
    }
    bool MidiSong::SetBassRoots(const int* values, int count)
    {
        if (count < 0 || (count && !values)) return false;
        BassRoots.Clear();
        for (int i = 0; i < count; ++i) if (!BassRoots.PushBack(values[i])) return false;
        Built = false; return true;
    }

    int MidiSong::GetMelodyValue(int i) const { const int* p = Melody.Get(i); return p ? *p : 0; }
    int MidiSong::GetBassRoot(int i) const { const int* p = BassRoots.Get(i); return p ? *p : 0; }
    double MidiSong::GetTickDuration() const { return 60.0 / ((double)TempoBPM * TicksPerBeat); }

    double MidiSong::GetSecondsAtTick(unsigned int tick) const
    {
        if (!TimelineMode || Tempos.Size() == 0) return tick * GetTickDuration();
        unsigned int previousTick = 0, usec = 500000;
        double seconds = 0.0;
        for (int i = 0; i < Tempos.Size(); ++i)
        {
            const TempoChange& tempo = Tempos[i];
            if (tempo.Tick > tick) break;
            if (tempo.Tick > previousTick)
                seconds += (double)(tempo.Tick - previousTick) * (double)usec / 1000000.0 / TicksPerBeat;
            previousTick = tempo.Tick; usec = tempo.MicrosecondsPerBeat;
        }
        if (tick > previousTick)
            seconds += (double)(tick - previousTick) * (double)usec / 1000000.0 / TicksPerBeat;
        return seconds;
    }

    double MidiSong::GetDurationSeconds() const { return GetSecondsAtTick(GetLengthTicks()); }

    bool MidiSong::BeginTimeline(unsigned int ticksPerQuarter)
    {
        if (!ticksPerQuarter || ticksPerQuarter > 32767) return false;
        Instruments.Clear(); Tracks.Clear(); StepEvents.Clear(); MidiEvents.Clear(); Tempos.Clear();
        Melody.Clear(); BassRoots.Clear(); TicksPerBeat = ticksPerQuarter;
        StepsPerBeat = 4; Bars = 1; TimelineLength = 0; TimelineMode = true; Built = false;
        TempoChange initial; initial.Tick = 0; initial.MicrosecondsPerBeat = 500000;
        return Tempos.PushBack(initial);
    }

    bool MidiSong::AddTimelineEvent(unsigned int tick, unsigned char status,
                                    unsigned char data1, unsigned char data2)
    {
        if (!TimelineMode || status < 0x80 || status >= 0xF0 || data1 > 127 || data2 > 127) return false;
        MidiEvent e; e.Tick = tick; e.Status = status; e.Data1 = data1; e.Data2 = data2;
        e.Priority = (((status & 0xF0) == 0x80) || (((status & 0xF0) == 0x90) && data2 == 0)) ? 0 : 1;
        if (!MidiEvents.PushBack(e)) return false;
        if (tick > TimelineLength) TimelineLength = tick;
        Built = false; return true;
    }

    bool MidiSong::AddTempoChange(unsigned int tick, unsigned int microsecondsPerBeat)
    {
        if (!TimelineMode || !microsecondsPerBeat || microsecondsPerBeat > 0xFFFFFF) return false;
        TempoChange t; t.Tick = tick; t.MicrosecondsPerBeat = microsecondsPerBeat;
        // Replace the default 120 BPM tempo at tick zero when the file specifies one.
        if (tick == 0 && Tempos.Size() == 1 && Tempos[0].Tick == 0 &&
            Tempos[0].MicrosecondsPerBeat == 500000)
            Tempos[0] = t;
        else if (!Tempos.PushBack(t)) return false;
        Built = false; return true;
    }

    int MidiSong::CompareTempoChanges(const void* a, const void* b)
    {
        const TempoChange* x = (const TempoChange*)a; const TempoChange* y = (const TempoChange*)b;
        if (x->Tick < y->Tick) return -1;
        if (x->Tick > y->Tick) return 1;
        return 0;
    }

    bool MidiSong::FinishTimeline(unsigned int lengthTicks)
    {
        if (!TimelineMode || MidiEvents.Size() == 0 || Tempos.Size() == 0) return false;
        if (lengthTicks > TimelineLength) TimelineLength = lengthTicks;
        qsort(&MidiEvents[0], MidiEvents.Size(), sizeof(MidiEvent), CompareMidiEvents);
        qsort(&Tempos[0], Tempos.Size(), sizeof(TempoChange), CompareTempoChanges);
        Vector<TempoChange> compact;
        for (int i = 0; i < Tempos.Size(); ++i)
        {
            if (compact.Size() && compact[compact.Size()-1].Tick == Tempos[i].Tick)
                compact[compact.Size()-1] = Tempos[i];
            else if (!compact.PushBack(Tempos[i])) return false;
        }
        Tempos = compact;
        const unsigned int us = Tempos[0].MicrosecondsPerBeat;
        TempoBPM = us ? (int)(60000000UL / us) : 120;
        if (TempoBPM < 1) TempoBPM = 1;
        Built = true; return true;
    }

    int MidiSong::ResolvePitch(const StepEvent& event, unsigned int absoluteStep) const
    {
        const Track* track = Tracks.Get(event.TrackIndex);
        const Instrument* instrument = track ? Instruments.Get(track->InstrumentIndex) : 0;
        if (!instrument) return -1;
        int offset = event.Pitch;
        if (event.PitchMode == PitchMelody)
        {
            if (Melody.Size() == 0) return -1;
            const unsigned int bar = absoluteStep / GetStepsPerBar();
            const unsigned int beat = (absoluteStep % GetStepsPerBar()) / StepsPerBeat;
            offset = Melody[(bar % (Melody.Size() / StepsPerBeat)) * StepsPerBeat + (beat % StepsPerBeat)];
            if (bar >= 8) offset += 12;
        }
        else if (event.PitchMode == PitchBass)
        {
            if (BassRoots.Size() == 0) return -1;
            const unsigned int bar = absoluteStep / GetStepsPerBar();
            const unsigned int beat = (absoluteStep % GetStepsPerBar()) / StepsPerBeat;
            offset = BassRoots[bar % BassRoots.Size()] + ((beat & 1) ? 7 : 0);
            if (bar >= 8 && beat == 3) offset += 2;
        }
        return instrument->BasePitch + offset;
    }

    bool MidiSong::AddMidiEvent(unsigned int tick, unsigned char status,
                                unsigned char data1, unsigned char data2, int priority)
    {
        MidiEvent e; e.Tick = tick; e.Status = status; e.Data1 = data1;
        e.Data2 = data2; e.Priority = priority;
        return MidiEvents.PushBack(e);
    }

    int MidiSong::CompareMidiEvents(const void* a, const void* b)
    {
        const MidiEvent* x = (const MidiEvent*)a; const MidiEvent* y = (const MidiEvent*)b;
        if (x->Tick < y->Tick) return -1;
        if (x->Tick > y->Tick) return 1;
        if (x->Priority < y->Priority) return -1;
        if (x->Priority > y->Priority) return 1;
        return 0;
    }

    bool MidiSong::Build()
    {
        if (TimelineMode) return FinishTimeline(TimelineLength);
        MidiEvents.Clear(); Built = false;
        if (TempoBPM <= 0 || TicksPerBeat == 0 || StepsPerBeat == 0 || Bars == 0) return false;
        for (int bar = 0; bar < (int)Bars; ++bar)
        {
            for (int i = 0; i < StepEvents.Size(); ++i)
            {
                const StepEvent& e = StepEvents[i];
                const unsigned int absoluteStep = (unsigned int)bar * GetStepsPerBar() + e.Step;
                if (absoluteStep >= GetLengthSteps()) continue;
                const Track* track = Tracks.Get(e.TrackIndex);
                const Instrument* instrument = track ? Instruments.Get(track->InstrumentIndex) : 0;
                if (!instrument) return false;
                const int note = ResolvePitch(e, absoluteStep);
                if (note < 0 || note > 127) return false;
                const unsigned int onTick = absoluteStep * TicksPerStep();
                const unsigned int offTick = onTick + e.DurationTicks;
                if (!AddMidiEvent(onTick, (unsigned char)(0x90 | instrument->Channel),
                                  (unsigned char)note, (unsigned char)e.Velocity, 1) ||
                    !AddMidiEvent(offTick, (unsigned char)(0x80 | instrument->Channel),
                                  (unsigned char)note, 0, 0)) return false;
            }
        }
        if (MidiEvents.Size() == 0) return false;
        qsort(&MidiEvents[0], MidiEvents.Size(), sizeof(MidiEvent), CompareMidiEvents);
        Built = true; return true;
    }
}
