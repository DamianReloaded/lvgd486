#pragma once
#include "../../vgd/Util/UniquePtr.hpp"
#include "../../vgd/Sound/Sound.hpp"
#include "../../vgd/Sound/AudioMixer.hpp"
#include "../../vgd/Sound/IMidi.hpp"
#include "../../vgd/Sound/MidiPlayer.hpp"

namespace vgd
{

class MenuSound
{
public:
    MenuSound();
    ~MenuSound();

    bool Initialize();
    void Shutdown();

    bool IsInitialized() const;

    IMidi* GetMidi();
    AudioMixer& GetMixer();
    MidiPlayer& GetPlayer();

private:
    Sound m_Sound;
    AudioMixer m_Mixer;
    UniquePtr<IMidi> m_Midi;
    MidiPlayer m_Player;

    bool m_Initialized;
};

}
