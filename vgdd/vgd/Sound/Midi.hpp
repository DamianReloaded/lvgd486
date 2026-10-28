#pragma once 
// -----------------------------------------------------------------------------
// Midi
//
// Reusable low-level MIDI output class.
//
// Responsibilities:
//   - Open and close an OSS sequencer device.
//   - Detect available raw MIDI ports.
//   - Send MIDI messages through 4-byte SEQ_MIDIPUTC records.
//   - Provide helpers for common MIDI channel messages.
//   - Silence active notes during shutdown.
//
// This class does NOT own song data, timing, scheduling, or graphics.
//
// Note: the selected OSS MIDI port must have a synthesizer or MIDI destination
// capable of producing audible sound. A raw MIDI port alone is not a synth.
// -----------------------------------------------------------------------------

#pragma once
#include "IMidi.hpp"

namespace vgd
{
    class Midi : public IMidi
    {
    public:
        Midi();
        virtual ~Midi();

    private:
        Midi(const Midi&);
        Midi& operator=(const Midi&);

    public:
        virtual bool Initialize(const char* path);
        virtual bool IsInitialized() const;
        virtual int GetPortCount() const;

        virtual bool SendByte(unsigned char value);
        virtual bool SendMessage(unsigned char status, unsigned char data1, unsigned char data2);

        virtual bool NoteOn(int channel, int note, int velocity);
        virtual bool NoteOff(int channel, int note);
        virtual bool ProgramChange(int channel, int program);
        virtual bool ControlChange(int channel, int control, int value);

        virtual bool SilenceAll();
        virtual void Shutdown();

    private:
        int fd;
        int deviceNumber;
        bool initialized;
    };
}