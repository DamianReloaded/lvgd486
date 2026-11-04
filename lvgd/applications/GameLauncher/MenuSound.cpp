#include "MenuSound.hpp"
#include "../../vgd/Sound/Midi.hpp"
#include "../../vgd/Sound/MidiPcm.hpp"

namespace vgd
{

MenuSound::MenuSound()
    : m_Initialized(false)
{
}

MenuSound::~MenuSound()
{
    Shutdown();
}

bool MenuSound::Initialize()
{
    if (m_Initialized)
        return true;

    Sound::Configuration configuration;

    configuration.SampleRate = 44100;
    configuration.Channels = 2;
    configuration.BufferFrames = 512;

    if (!m_Sound.Initialize(configuration))
        return false;

    if (!m_Mixer.Initialize(m_Sound.GetSampleRate(),
                            m_Sound.GetChannels()))
    {
        Shutdown();
        return false;
    }

    if (!m_Mixer.Attach(m_Sound))
    {
        Shutdown();
        return false;
    }

    m_Midi = UniquePtr<Midi>().MakeNewInstance();

    if (!m_Midi->Initialize("/dev/sound/sequencer"))
    {
        m_Midi = UniquePtr<MidiPcm>().MakeNewInstance();

        if (!m_Midi->Initialize(0))
        {
            Shutdown();
            return false;
        }

        m_Midi->SetMixer(&m_Mixer);
    }

    m_Midi->SetMasterVolume(32000);

    m_Mixer.Play();

    if (!m_Sound.StartWorker())
    {
        Shutdown();
        return false;
    }

    if (!m_Player.Initialize(m_Midi.Get()))
    {
        Shutdown();
        return false;
    }

    m_Initialized = true;
    return true;
}

void MenuSound::Shutdown()
{
    m_Player.Shutdown();

    if (m_Midi.Get())
        m_Midi->Shutdown();

    m_Sound.Shutdown();

    m_Initialized = false;
}

bool MenuSound::IsInitialized() const
{
    return m_Initialized;
}

IMidi* MenuSound::GetMidi()
{
    return m_Midi.Get();
}

AudioMixer& MenuSound::GetMixer()
{
    return m_Mixer;
}

MidiPlayer& MenuSound::GetPlayer()
{
    return m_Player;
}

}