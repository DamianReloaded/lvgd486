#include "Midi.hpp"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

// OSS sequencer definitions
#include <sys/soundcard.h>

namespace vgd
{
    // -----------------------------------------------------------------------------
    // Construction and destruction
    // -----------------------------------------------------------------------------

    Midi::Midi()
        : fd(-1),
        deviceNumber(0),
        initialized(false)
    {
    }

    Midi::~Midi()
    {
        Shutdown();
    }

    // -----------------------------------------------------------------------------
    // Initialization
    // -----------------------------------------------------------------------------

    bool Midi::Initialize(const char* path)
    {
        int midiCount = 0;

        if (initialized)
            return true;

        fd = open(path, O_WRONLY);

        if (fd < 0)
        {
            fprintf(stderr,
                    "MIDI: cannot open %s: %s\n",
                    path, strerror(errno));
            return false;
        }

        if (ioctl(fd, SNDCTL_SEQ_NRMIDIS, &midiCount) < 0)
        {
            fprintf(stderr,
                    "MIDI: MIDI port query failed: %s\n",
                    strerror(errno));

            close(fd);
            fd = -1;
            return false;
        }

        if (midiCount < 1)
        {
            fprintf(stderr,
                    "MIDI: no raw MIDI ports found.\n");

            close(fd);
            fd = -1;
            return false;
        }

        deviceNumber = 0;
        initialized = true;

        printf("MIDI: OSS MIDI ports: %d\n", midiCount);

        return true;
    }

    bool Midi::IsInitialized() const
    {
        return initialized;
    }

    int Midi::GetPortCount() const
    {
        // The class currently selects port zero and only needs to know
        // whether at least one port was found.
        return initialized ? 1 : 0;
    }

    // -----------------------------------------------------------------------------
    // Low-level MIDI output
    // -----------------------------------------------------------------------------

    bool Midi::SendByte(unsigned char value)
    {
        unsigned char event[4];
        ssize_t written;

        if (!initialized)
            return false;

        event[0] = SEQ_MIDIPUTC;
        event[1] = value;
        event[2] = (unsigned char)deviceNumber;
        event[3] = 0;

        written = write(fd, event, sizeof(event));

        if (written != (ssize_t)sizeof(event))
        {
            if (written < 0)
            {
                fprintf(stderr,
                        "MIDI: write failed: %s\n",
                        strerror(errno));
            }
            else
            {
                fprintf(stderr,
                        "MIDI: incomplete MIDI event.\n");
            }

            return false;
        }

        return true;
    }

    bool Midi::SendMessage(
        unsigned char status,
        unsigned char data1,
        unsigned char data2)
    {
        if (!SendByte(status))
            return false;

        if (!SendByte(data1))
            return false;

        // Program Change and Channel Pressure use one data byte.
        if ((status & 0xF0) == 0xC0 ||
            (status & 0xF0) == 0xD0)
        {
            return true;
        }

        return SendByte(data2);
    }

    // -----------------------------------------------------------------------------
    // MIDI channel messages
    // -----------------------------------------------------------------------------

    bool Midi::NoteOn(int channel, int note, int velocity)
    {
        return SendMessage(
            (unsigned char)(0x90 | (channel & 0x0F)),
            (unsigned char)(note & 0x7F),
            (unsigned char)(velocity & 0x7F));
    }

    bool Midi::NoteOff(int channel, int note)
    {
        return SendMessage(
            (unsigned char)(0x80 | (channel & 0x0F)),
            (unsigned char)(note & 0x7F),
            0);
    }

    bool Midi::ProgramChange(int channel, int program)
    {
        return SendMessage(
            (unsigned char)(0xC0 | (channel & 0x0F)),
            (unsigned char)(program & 0x7F),
            0);
    }

    bool Midi::ControlChange(int channel, int control, int value)
    {
        return SendMessage(
            (unsigned char)(0xB0 | (channel & 0x0F)),
            (unsigned char)(control & 0x7F),
            (unsigned char)(value & 0x7F));
    }

    // -----------------------------------------------------------------------------
    // Shutdown and cleanup
    // -----------------------------------------------------------------------------

    bool Midi::SilenceAll()
    {
        int channel;
        bool success = true;

        if (!initialized)
            return false;

        for (channel = 0; channel < 16; ++channel)
        {
            if (!ControlChange(channel, 123, 0))
                success = false;

            if (!ControlChange(channel, 120, 0))
                success = false;
        }

        return success;
    }

    void Midi::Shutdown()
    {
        if (initialized)
        {
            SilenceAll();
            close(fd);
        }

        fd = -1;
        deviceNumber = 0;
        initialized = false;
    }
}
