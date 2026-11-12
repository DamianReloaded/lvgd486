
#include "MusFile.hpp"
#include <limits.h>
#include <stdio.h>

namespace vgd
{
    namespace
    {
        struct Reader
        {
            const unsigned char* Data;
            unsigned int Size;
            unsigned int Pos;

            Reader(const unsigned char* data, unsigned int size)
                : Data(data), Size(size), Pos(0)
            {
            }

            bool ReadByte(unsigned int& value)
            {
                if (Pos >= Size)
                {
                    return false;
                }

                value = Data[Pos++];
                return true;
            }

            bool ReadLE16(unsigned int& value)
            {
                if (Pos > Size || Size - Pos < 2)
                {
                    return false;
                }

                value = (unsigned int)Data[Pos] | ((unsigned int)Data[Pos + 1] << 8);
                Pos += 2;
                return true;
            }

            bool ReadDelay(unsigned int& value)
            {
                value = 0;

                for (int i = 0; i < 5; ++i)
                {
                    unsigned int b;

                    if (!ReadByte(b) || value > (UINT_MAX >> 7))
                    {
                        return false;
                    }

                    value = (value << 7) | (b & 0x7F);

                    if (!(b & 0x80))
                    {
                        return true;
                    }
                }

                return false;
            }
        };

        static const unsigned char ControllerMap[10] =
        {
            0, 0, 1, 7, 10, 11, 91, 93, 64, 67
        };

        static const unsigned char SystemControllerMap[5] =
        {
            120, 123, 126, 127, 121
        };

        static unsigned char ConvertChannel(unsigned int channel)
        {
            if (channel == 15)
            {
                return 9;
            }

            return (unsigned char)(channel < 9 ? channel : channel + 1);
        }
    }

    MusFile::MusFile()
        : ErrorText("no error")
    {
    }

    bool MusFile::Fail(const char* message)
    {
        ErrorText = message;
        return false;
    }

    bool MusFile::Load(const void* data, unsigned int size, MidiSong& song)
    {
        ErrorText = "no error";

        if (!data)
        {
            return Fail("invalid MUS data pointer");
        }

        if (size < 16)
        {
            return Fail("MUS header is truncated");
        }

        const unsigned char* bytes = (const unsigned char*)data;

        if (bytes[0] != 'M' || bytes[1] != 'U' || bytes[2] != 'S' || bytes[3] != 0x1A)
        {
            return Fail("invalid MUS signature");
        }

        Reader header(bytes, size);
        header.Pos = 4;

        unsigned int scoreLength = 0;
        unsigned int scoreStart = 0;
        unsigned int primaryChannels = 0;
        unsigned int secondaryChannels = 0;
        unsigned int instrumentCount = 0;
        unsigned int reserved = 0;

        if (!header.ReadLE16(scoreLength) ||
            !header.ReadLE16(scoreStart) ||
            !header.ReadLE16(primaryChannels) ||
            !header.ReadLE16(secondaryChannels) ||
            !header.ReadLE16(instrumentCount) ||
            !header.ReadLE16(reserved))
        {
            return Fail("truncated MUS header");
        }

        (void)reserved;

        if (primaryChannels > 15 || secondaryChannels > 15 ||
            primaryChannels + secondaryChannels > 15)
        {
            return Fail("invalid MUS channel counts");
        }

        if (instrumentCount > (size - 16) / 2)
        {
            return Fail("invalid MUS instrument table");
        }

        const unsigned int instrumentTableEnd = 16 + instrumentCount * 2;

        if (scoreStart < instrumentTableEnd || scoreStart > size)
        {
            return Fail("invalid MUS score offset");
        }

        if (scoreLength > size - scoreStart)
        {
            return Fail("MUS score extends beyond input data");
        }

        if (scoreLength == 0)
        {
            return Fail("empty MUS score");
        }

        Reader reader(bytes + scoreStart, scoreLength);

        song.Clear();

        // MUS time units are 1/140 second.
        if (!song.BeginTimeline(140))
        {
            song.Clear();
            return Fail("could not initialize song timeline");
        }

        if (!song.AddTempoChange(0, 1000000))
        {
            song.Clear();
            return Fail("could not set MUS timeline tempo");
        }

        unsigned int currentTick = 0;
        bool scoreEnded = false;
        unsigned char channelVelocity[16];

        for (int i = 0; i < 16; ++i)
        {
            channelVelocity[i] = 64;
        }

        while (reader.Pos < reader.Size && !scoreEnded)
        {
            unsigned int descriptor = 0;

            if (!reader.ReadByte(descriptor))
            {
                song.Clear();
                return Fail("truncated MUS event descriptor");
            }

            const bool lastInGroup = (descriptor & 0x80) != 0;
            const unsigned int eventType = (descriptor >> 4) & 0x07;
            const unsigned int musChannel = descriptor & 0x0F;
            const unsigned char midiChannel = ConvertChannel(musChannel);

            unsigned int value1 = 0;
            unsigned int value2 = 0;
            unsigned char status = 0;
            unsigned char data1 = 0;
            unsigned char data2 = 0;
            bool emitEvent = true;

            switch (eventType)
            {
                // Release note.
                case 0:
                    if (!reader.ReadByte(value1))
                    {
                        song.Clear();
                        return Fail("truncated MUS note-off event");
                    }

                    data1 = (unsigned char)(value1 & 0x7F);
                    data2 = 0;
                    status = (unsigned char)(0x80 | midiChannel);
                    break;

                // Play note.
                case 1:
                    if (!reader.ReadByte(value1))
                    {
                        song.Clear();
                        return Fail("truncated MUS note-on event");
                    }

                    data1 = (unsigned char)(value1 & 0x7F);

                    if (value1 & 0x80)
                    {
                        if (!reader.ReadByte(value2) || value2 > 127)
                        {
                            song.Clear();
                            return Fail("invalid MUS note velocity");
                        }

                        channelVelocity[musChannel] = (unsigned char)value2;
                    }

                    data2 = channelVelocity[musChannel];
                    status = (unsigned char)(0x90 | midiChannel);
                    break;

                // Pitch wheel.
                case 2:
                    if (!reader.ReadByte(value1))
                    {
                        song.Clear();
                        return Fail("truncated MUS pitch wheel event");
                    }

                    data1 = (unsigned char)((value1 & 1) << 6);
                    data2 = (unsigned char)(value1 >> 1);
                    status = (unsigned char)(0xE0 | midiChannel);
                    break;

                // System event.
                case 3:
                    if (!reader.ReadByte(value1))
                    {
                        song.Clear();
                        return Fail("truncated MUS system event");
                    }

                    if (value1 < 10 || value1 > 14)
                    {
                        song.Clear();
                        return Fail("invalid MUS system event");
                    }

                    data1 = SystemControllerMap[value1 - 10];
                    data2 = (value1 == 12) ? 1 : 0;
                    status = (unsigned char)(0xB0 | midiChannel);
                    break;

                // Controller change or program change.
                case 4:
                    if (!reader.ReadByte(value1) || !reader.ReadByte(value2))
                    {
                        song.Clear();
                        return Fail("truncated MUS controller event");
                    }

                    if (value1 > 9 || value2 > 127)
                    {
                        song.Clear();
                        return Fail("invalid MUS controller event");
                    }

                    if (value1 == 0)
                    {
                        status = (unsigned char)(0xC0 | midiChannel);
                        data1 = (unsigned char)value2;
                        data2 = 0;
                    }
                    else
                    {
                        status = (unsigned char)(0xB0 | midiChannel);
                        data1 = ControllerMap[value1];
                        data2 = (unsigned char)value2;
                    }
                    break;

                // Reserved event type.
                case 5:
                    song.Clear();
                    return Fail("reserved MUS event type 5");

                // End of score.
                case 6:
                    scoreEnded = true;
                    emitEvent = false;
                    break;

                // Reserved event type.
                case 7:
                default:
                {
                    char error[64];
                    sprintf(error, "unsupported MUS event type: %u", eventType);
                    song.Clear();
                    return Fail(error);
                }
            }

            if (emitEvent)
            {
                if (!song.AddTimelineEvent(currentTick, status, data1, data2))
                {
                    song.Clear();
                    return Fail("could not store converted MUS event");
                }
            }

            if (lastInGroup && !scoreEnded)
            {
                unsigned int delay = 0;

                if (!reader.ReadDelay(delay))
                {
                    song.Clear();
                    return Fail("invalid or truncated MUS time delay");
                }

                if (currentTick > UINT_MAX - delay)
                {
                    song.Clear();
                    return Fail("MUS timeline duration overflow");
                }

                currentTick += delay;
            }
        }

        if (!scoreEnded)
        {
            song.Clear();
            return Fail("MUS score has no end event");
        }

        if (song.GetMidiEventCount() <= 0)
        {
            song.Clear();
            return Fail("MUS score contains no playable events");
        }

        if (!song.FinishTimeline(currentTick))
        {
            song.Clear();
            return Fail("could not finalize MUS timeline");
        }

        return true;
    }
}
