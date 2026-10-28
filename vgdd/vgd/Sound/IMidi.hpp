#ifndef VGD_IMIDI_HPP
#define VGD_IMIDI_HPP

namespace vgd
{
    class IMidi
    {
    public:
        virtual ~IMidi() {}

        virtual bool Initialize(const char* path) = 0;
        virtual bool IsInitialized() const = 0;
        virtual int GetPortCount() const = 0;

        virtual bool SendByte(unsigned char value) = 0;
        virtual bool SendMessage(unsigned char status, unsigned char data1, unsigned char data2) = 0;

        virtual bool NoteOn(int channel, int note, int velocity) = 0;
        virtual bool NoteOff(int channel, int note) = 0;
        virtual bool ProgramChange(int channel, int program) = 0;
        virtual bool ControlChange(int channel, int control, int value) = 0;

        virtual bool SilenceAll() = 0;
        virtual void Shutdown() = 0;
    };
}

#endif