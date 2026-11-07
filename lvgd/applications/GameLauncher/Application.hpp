#pragma once

#include "../../vgd/Display/DisplayMode13h.hpp"
#include "../../vgd/Input/Keyboard.hpp"
#include "../../vgd/Input/Joystick.hpp"
#include "../../vgd/UI/Panel.hpp"
#include "../../vgd/UI/Label.hpp"
#include "../../vgd/Sound/MidiSong.hpp"
#include "../../vgd/Sound/MidiFile.hpp"
#include "../../vgd/Util/UniquePtr.hpp"

#include "MenuSound.hpp"
#include "Background.hpp"
#include "Thumbnail.hpp"
#include "GameInfo.hpp"

class Application
{
public:
    Application();
    virtual ~Application();

    int Run();
    void Exit();

protected:
    virtual bool Initialize();
    virtual void Shutdown();
    virtual void Update();
    virtual void Draw();

    void RefreshGameList();
    void LaunchSelectedGame();

    vgd::DisplayMode13h m_Display;
    vgd::Keyboard m_Keyboard;
    vgd::Joystick m_Joystick;
    vgd::MenuSound m_Sound;

    vgd::MidiFile m_MidiFile;
    vgd::MidiSong m_Song;

    GameInfo m_GameInfo;
    GameInfo::String m_LaunchGamePath;

    bool m_Initialized;
    bool m_DisplayInitialized;
    bool m_KeyboardInitialized;
    bool m_JoystickInitialized;
    bool m_SoundInitialized;
    bool m_FontManagerInitialized;
    bool m_ExitRequested;
    bool m_LaunchGameRequested;

    unsigned long m_Frame;
    int m_FontIndex;

    vgd::UniquePtr<vgd::UI::Panel> m_MainPanel;

    Background* m_Background;
    vgd::UI::Label* m_TitleLabel;

    Thumbnail* m_Thumbnails[GameInfo::VisibleCount];
    vgd::UI::Label* m_GameTitles[GameInfo::VisibleCount];

    double m_Interpolation;

    bool m_DisplayPaletteSet;
};