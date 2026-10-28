#include "MidiPlayer.hpp"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>
#include <errno.h>

namespace vgd
{
    MidiPlayer::MidiPlayer()
        : Song(0), Initialized(false), ThreadStarted(false),
          StopRequested(0), Error(0), StartTime(0.0) {}

    MidiPlayer::~MidiPlayer() { Shutdown(); }

    double MidiPlayer::ClockSeconds()
    {
        struct timeval tv; gettimeofday(&tv, 0);
        return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
    }

    bool MidiPlayer::Initialize(IMidi* midi)
    {
        if (Initialized) return true;

        Output = midi;

        Initialized = midi->IsInitialized();
        return true;
    }

    bool MidiPlayer::Play(const MidiSong& song)
    {
        if (!Initialized || ThreadStarted || !song.IsBuilt() || song.GetMidiEventCount() <= 0)
            return false;
        Song = &song; StopRequested = 0; Error = 0; StartTime = ClockSeconds();
        Output->SilenceAll();
        for (int i = 0; i < song.GetInstrumentCount(); ++i)
        {
            const MidiSong::Instrument* instrument = song.GetInstrument(i);
            if (!instrument) return false;
            if (instrument->Program >= 0 && !Output->ProgramChange(instrument->Channel, instrument->Program))
                return false;
            if (instrument->Volume >= 0 && !Output->ControlChange(instrument->Channel, 7, instrument->Volume))
                return false;
        }
        int rc = pthread_create(&Thread, 0, ThreadEntry, this);
        if (rc != 0)
        {
            fprintf(stderr, "MIDI: pthread_create failed: %s\n", strerror(rc));
            Song = 0; return false;
        }
        ThreadStarted = true;
        return true;
    }

    void MidiPlayer::Stop()
    {
        StopRequested = 1;
        if (ThreadStarted) { pthread_join(Thread, 0); ThreadStarted = false; }
        if (Initialized) Output->SilenceAll();
        Song = 0;
    }

    void MidiPlayer::Shutdown()
    {
        Stop();
        if (Initialized) { Initialized = false; }
    }

    bool MidiPlayer::IsPlaying() const { return ThreadStarted && !StopRequested && !Error; }
    bool MidiPlayer::HasError() const { return Error != 0; }
    double MidiPlayer::GetStartTime() const { return StartTime; }
    double MidiPlayer::GetSongDuration() const { return Song ? Song->GetDurationSeconds() : 0.0; }

    void* MidiPlayer::ThreadEntry(void* p) { ((MidiPlayer*)p)->Run(); return 0; }

    bool MidiPlayer::WaitUntil(double deadline)
    {
        while (!StopRequested)
        {
            double remaining = deadline - ClockSeconds();
            if (remaining <= 0.0) return true;
            if (remaining > 0.01) remaining = 0.01;
            struct timeval timeout;
            timeout.tv_sec = (long)remaining;
            timeout.tv_usec = (long)((remaining - timeout.tv_sec) * 1000000.0);
            if (select(0, 0, 0, 0, &timeout) < 0 && errno != EINTR) return false;
        }
        return false;
    }

    void MidiPlayer::Run()
    {
        if (!Song) { Error = 1; return; }
        const int count = Song->GetMidiEventCount();
        if (count <= 0) { Error = 1; return; }
        unsigned long long loop = 0;
        int index = 0;
        while (!StopRequested)
        {
            const MidiSong::MidiEvent* event = Song->GetMidiEvent(index);
            if (!event) { Error = 1; break; }
            const double deadline = StartTime + loop * Song->GetDurationSeconds() +
                                    Song->GetSecondsAtTick(event->Tick);
            if (!WaitUntil(deadline)) { if (!StopRequested) Error = 1; break; }
            if (StopRequested) break;
            if (!Output->SendMessage(event->Status, event->Data1, event->Data2))
            { Error = 1; break; }
            ++index;
            if (index >= count)
            {
                if (!Song->IsLooping()) break;
                index = 0; ++loop;
            }
        }
    }
}
