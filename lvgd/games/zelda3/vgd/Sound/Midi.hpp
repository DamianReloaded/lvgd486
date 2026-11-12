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
        bool SendRawMessage(unsigned char status, unsigned char data1, unsigned char data2);

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

        virtual bool SetMasterVolume(int volume);

        virtual bool SilenceAll();
        virtual void Shutdown();

    private:
        int fd;
        int deviceNumber;
        bool initialized;
        int m_ChannelVolumes[16];
        int m_MasterVolume;
    };
}