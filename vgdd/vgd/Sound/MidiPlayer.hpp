#ifndef VGD_MIDI_PLAYER_HPP
#define VGD_MIDI_PLAYER_HPP

#include "MidiSong.hpp"
#include "Midi.hpp"
#include <pthread.h>
#include <signal.h>

namespace vgd
{
    class MidiPlayer
    {
    public:
        MidiPlayer();
        ~MidiPlayer();
        bool Initialize(IMidi* midi);
        bool Play(const MidiSong& song);
        void Stop();
        void Shutdown();
        bool IsPlaying() const;
        bool HasError() const;
        double GetStartTime() const;
        double GetSongDuration() const;
        const MidiSong* GetSong() const { return Song; }

    private:
        IMidi* Output;
        const MidiSong* Song;
        pthread_t Thread;
        bool Initialized;
        bool ThreadStarted;
        volatile sig_atomic_t StopRequested;
        volatile sig_atomic_t Error;
        double StartTime;
        static void* ThreadEntry(void* p);
        void Run();
        bool WaitUntil(double deadline);
        static double ClockSeconds();
        MidiPlayer(const MidiPlayer&);
        MidiPlayer& operator=(const MidiPlayer&);
    };
}

#endif
