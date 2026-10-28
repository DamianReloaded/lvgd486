#ifndef VGD_MIDI_FILE_HPP
#define VGD_MIDI_FILE_HPP

#include "MidiSong.hpp"

namespace vgd
{
    // Standard MIDI File (SMF) loader. Supports format 0/1, PPQN timing,
    // channel voice messages, running status, and tempo meta-events.
    class MidiFile
    {
    public:
        MidiFile();
        bool Load(const char* path, MidiSong& song);
        const char* GetError() const { return ErrorText; }
        unsigned int GetTrackCount() const { return TrackCount; }
        unsigned int GetTicksPerQuarter() const { return TicksPerQuarter; }
        unsigned int GetFormat() const { return Format; }

    private:
        const char* ErrorText;
        unsigned int TrackCount, TicksPerQuarter, Format;
        bool Fail(const char* message);
    };
}
#endif
