#ifndef VGD_MUS_FILE_HPP
#define VGD_MUS_FILE_HPP

#include "MidiSong.hpp"

namespace vgd
{
    class MusFile
    {
    public:
        MusFile();

        bool Load(const void* data, unsigned int size, MidiSong& song);
        const char* GetError() const { return ErrorText; }

    private:
        const char* ErrorText;

        bool Fail(const char* message);
    };
}

#endif