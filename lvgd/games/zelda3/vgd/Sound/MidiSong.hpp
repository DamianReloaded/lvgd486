#ifndef VGD_MIDI_SONG_HPP
#define VGD_MIDI_SONG_HPP

#include "../Util/Vector.hpp"

namespace vgd
{
    class MidiSong
    {
    public:
        enum PitchMode { PitchFixed = 0, PitchMelody = 1, PitchBass = 2 };

        struct Instrument
        {
            int Channel;
            int Program; // -1 leaves the instrument program unchanged
            int Volume;  // -1 leaves channel volume unchanged
            int BasePitch;
        };

        struct Track
        {
            int InstrumentIndex;
            int DisplayRow;
        };

        struct StepEvent
        {
            int TrackIndex;
            unsigned int Step;
            int PitchMode;
            int Pitch;
            int Velocity;
            unsigned int DurationTicks;
        };

        struct MidiEvent
        {
            unsigned int Tick;
            unsigned char Status;
            unsigned char Data1;
            unsigned char Data2;
            int Priority; // note-off before note-on at the same tick
        };

        struct TempoChange
        {
            unsigned int Tick;
            unsigned int MicrosecondsPerBeat;
        };

        MidiSong();
        ~MidiSong();

        void Clear();
        bool SetTempo(int bpm);
        bool SetTicksPerBeat(unsigned int ticks);
        bool SetStepsPerBeat(unsigned int steps);
        bool SetBars(unsigned int bars);
        bool SetLooping(bool looping);

        int AddInstrument(int channel, int program, int volume, int basePitch);
        int AddTrack(int instrumentIndex, int displayRow);
        bool AddStepEvent(int trackIndex, unsigned int step, int pitchMode,
                          int pitch, int velocity, unsigned int durationTicks);
        bool SetMelody(const int* values, int count);
        bool SetBassRoots(const int* values, int count);
        bool Build();

        // Timeline mode is used by MidiFile; imported events keep their exact ticks.
        bool BeginTimeline(unsigned int ticksPerQuarter);
        bool AddTimelineEvent(unsigned int tick, unsigned char status,
                              unsigned char data1, unsigned char data2);
        bool AddTempoChange(unsigned int tick, unsigned int microsecondsPerBeat);
        bool FinishTimeline(unsigned int lengthTicks);

        int GetTempo() const { return TempoBPM; }
        unsigned int GetTicksPerBeat() const { return TicksPerBeat; }
        unsigned int GetStepsPerBeat() const { return StepsPerBeat; }
        unsigned int GetStepsPerBar() const { return StepsPerBeat * 4; }
        unsigned int GetBars() const { return Bars; }
        unsigned int GetLengthSteps() const { return GetStepsPerBar() * Bars; }
        unsigned int GetLengthTicks() const { return TimelineMode ? TimelineLength : GetLengthSteps() * TicksPerStep(); }
        unsigned int TicksPerStep() const { return TicksPerBeat / StepsPerBeat; }
        double GetTickDuration() const;
        double GetDurationSeconds() const;
        double GetSecondsAtTick(unsigned int tick) const;
        bool IsLooping() const { return Looping; }
        bool IsTimeline() const { return TimelineMode; }
        bool IsBuilt() const { return Built; }

        int GetInstrumentCount() const { return Instruments.Size(); }
        int GetTrackCount() const { return Tracks.Size(); }
        int GetStepEventCount() const { return StepEvents.Size(); }
        int GetMidiEventCount() const { return MidiEvents.Size(); }
        int GetTempoChangeCount() const { return Tempos.Size(); }
        const Instrument* GetInstrument(int i) const { return Instruments.Get(i); }
        const Track* GetTrack(int i) const { return Tracks.Get(i); }
        const StepEvent* GetStepEvent(int i) const { return StepEvents.Get(i); }
        const MidiEvent* GetMidiEvent(int i) const { return MidiEvents.Get(i); }
        const TempoChange* GetTempoChange(int i) const { return Tempos.Get(i); }
        int GetMelodyValue(int i) const;
        int GetBassRoot(int i) const;

    private:
        int TempoBPM;
        unsigned int TicksPerBeat;
        unsigned int StepsPerBeat;
        unsigned int Bars;
        bool Looping;
        bool Built;
        bool TimelineMode;
        unsigned int TimelineLength;
        Vector<Instrument> Instruments;
        Vector<Track> Tracks;
        Vector<StepEvent> StepEvents;
        Vector<MidiEvent> MidiEvents;
        Vector<TempoChange> Tempos;
        Vector<int> Melody;
        Vector<int> BassRoots;

        bool AddMidiEvent(unsigned int tick, unsigned char status,
                          unsigned char data1, unsigned char data2, int priority);
        static int CompareMidiEvents(const void* a, const void* b);
        static int CompareTempoChanges(const void* a, const void* b);
        int ResolvePitch(const StepEvent& event, unsigned int absoluteStep) const;
        MidiSong(const MidiSong&);
        MidiSong& operator=(const MidiSong&);
    };
}

#endif
