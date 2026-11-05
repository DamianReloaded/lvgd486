#include "Application.hpp"

#include "../../vgd/Font/FontArcade01.hpp"
#include "../../vgd/UI/FontManager.hpp"
#include "../../vgd/Util/Clock.hpp"

#include <stdint.h>
#include <utility>
#include <unistd.h>
#include <stdio.h>

static bool LaunchAndExit(const char* supervisorPath, const char* gamePath)
{
    char* const args[] =
    {
        (char*)supervisorPath,
        (char*)gamePath,
        0
    };

    execv(supervisorPath, args);
    perror("exec Supervisor");
    return false;
}

Application::Application()
    : m_Initialized(false),
      m_DisplayInitialized(false),
      m_KeyboardInitialized(false),
      m_SoundInitialized(false),
      m_FontManagerInitialized(false),
      m_DisplayPaletteSet(false),
      m_ExitRequested(false),
      m_LaunchGameRequested(false),
      m_Frame(0),
      m_FontIndex(-1),
      m_Background(0),
      m_TitleLabel(0),
      m_Interpolation(0.0)
{
    for (int i = 0; i < GameInfo::VisibleCount; ++i)
    {
        m_Thumbnails[i] = 0;
        m_GameTitles[i] = 0;
    }
}

Application::~Application()
{
    Shutdown();
}

bool Application::Initialize()
{
    m_DisplayPaletteSet = false;

    if (!m_Display.Initialize())
        return false;

    m_DisplayInitialized = true;

    if (!m_Display.SetFrameRate(60))
        return false;

    if (!m_Keyboard.Initialize())
        return false;

    m_KeyboardInitialized = true;

    if (!vgd::UI::FontManager::Initialize())
        return false;

    m_FontManagerInitialized = true;

    vgd::FontArcade01 font;
    m_FontIndex = vgd::UI::FontManager::AddFont(font);

    if (m_FontIndex < 0)
        return false;

    m_MainPanel.MakeNewInstance(
        &m_Display,
        0,
        0,
        m_Display.GetWidth(),
        m_Display.GetHeight()
    );

    // Background
    vgd::UniquePtr<Background> background;

    background.MakeNewInstance(
        &m_Display,
        0,
        0,
        m_Display.GetWidth(),
        m_Display.GetHeight()
    );

    m_Background = background.Get();

    if (!m_MainPanel->AddChild(std::move(background)))
        return false;

    // Heading
    vgd::UniquePtr<vgd::UI::Label> title;

    title.MakeNewInstance(
        &m_Display,
        0,
        8,
        m_Display.GetWidth(),
        8,
        "LVGD486 - GAME SELECT"
    );

    title->SetFont(m_FontIndex);
    title->SetAlignment(vgd::UI::Label::AlignCenter);
    title->SetColor(15);

    m_TitleLabel = title.Get();

    if (!m_MainPanel->AddChild(std::move(title)))
        return false;

    // Game list controls
    for (int i = 0; i < GameInfo::VisibleCount; ++i)
    {
        const int rowY = 20 + i * 60;

        vgd::UniquePtr<Thumbnail> thumbnail;

        thumbnail.MakeNewInstance(
            &m_Display,
            8,
            rowY,
            64,
            40
        ); 

        m_Thumbnails[i] = thumbnail.Get();

        if (!m_MainPanel->AddChild(std::move(thumbnail)))
            return false;

        vgd::UniquePtr<vgd::UI::Label> gameTitle;

        gameTitle.MakeNewInstance(
            &m_Display,
            120,
            rowY + 16,
            m_Display.GetWidth() - 88,
            8,
            "Loading..."
        );

        gameTitle->SetFont(m_FontIndex);
        gameTitle->SetAlignment(vgd::UI::Label::AlignLeft);
        gameTitle->SetColor(7);

        m_GameTitles[i] = gameTitle.Get();

        if (!m_MainPanel->AddChild(std::move(gameTitle)))
            return false;
    }

    // Start streaming the game list.
    if (!m_GameInfo.Initialize("games.lst"))
    {
        fprintf(stderr, "Could not initialize games.lst\n");
        return false;
    }

    // Music
    if (!m_MidiFile.Load("music.mid", m_Song))
    {
        fprintf(stderr, "Could not load music.mid: %s\n",
                m_MidiFile.GetError());
        return false;
    }

    if (!m_Song.SetLooping(true))
        return false;

    if (!m_Sound.Initialize())
        return false;

    m_SoundInitialized = true;

    if (!m_Sound.GetPlayer().Play(m_Song))
        return false;

    m_Initialized = true;
    return true;
}

void Application::Shutdown()
{
    // Stop the worker before destroying anything it might supply.
    m_GameInfo.Shutdown();

    // Controls must be destroyed before their display and font dependencies.
    m_Background = 0;
    m_TitleLabel = 0;

    for (int i = 0; i < GameInfo::VisibleCount; ++i)
    {
        m_Thumbnails[i] = 0;
        m_GameTitles[i] = 0;
    }

    m_MainPanel.Reset();

    if (m_SoundInitialized)
    {
        m_Sound.Shutdown();
        m_SoundInitialized = false;
    }

    if (m_FontManagerInitialized)
    {
        vgd::UI::FontManager::Shutdown();
        m_FontManagerInitialized = false;
    }

    if (m_KeyboardInitialized)
    {
        m_Keyboard.Shutdown();
        m_KeyboardInitialized = false;
    }

    if (m_DisplayInitialized)
    {
        m_Display.Shutdown();
        m_DisplayInitialized = false;
    }

    m_DisplayPaletteSet = false;
    m_Initialized = false;
}

void Application::RefreshGameList()
{
    const int gameCount = m_GameInfo.GetGameCount();

    if (gameCount <= 0)
    {
        for (int i = 0; i < GameInfo::VisibleCount; ++i)
        {
            m_Thumbnails[i]->SetImage(0);
            m_GameTitles[i]->SetText("Loading...");
            m_GameTitles[i]->SetColor(7);
        }

        return;
    }

    const int selectedIndex = m_GameInfo.GetSelectedIndex();

    int startIndex = selectedIndex - 1;

    if (startIndex < 0)
        startIndex = 0;

    const int maxStart = gameCount > GameInfo::VisibleCount
        ? gameCount - GameInfo::VisibleCount
        : 0;

    if (startIndex > maxStart)
        startIndex = maxStart;

    for (int i = 0; i < GameInfo::VisibleCount; ++i)
    {
        const int gameIndex = startIndex + i;

        if (gameIndex >= gameCount)
        {
            m_Thumbnails[i]->SetImage(0);
            m_GameTitles[i]->SetText("");
            m_GameTitles[i]->SetColor(7);
            continue;
        }

        const GameInfo::Game* game = m_GameInfo.GetGame(gameIndex);

        if (game == 0)
        {
            m_Thumbnails[i]->SetImage(0);
            m_GameTitles[i]->SetText("Loading...");
            m_GameTitles[i]->SetColor(7);
            continue;
        }

        m_Thumbnails[i]->SetImage(&game->Image);
        m_GameTitles[i]->SetText(game->Title.CStr());

        m_GameTitles[i]->SetColor(
            gameIndex == selectedIndex ? 15 : 7
        );
    }
}

void Application::LaunchSelectedGame()
{
    const GameInfo::Game* game = m_GameInfo.GetSelectedGame();

    if (game == 0 || game->Binary.Empty())
        return;

    m_LaunchGamePath = game->Binary;
    m_LaunchGameRequested = true;
    Exit();
}

void Application::Update()
{
    m_Keyboard.Update();

    if (m_Keyboard.IsDown(vgd::Keyboard::KeyEscape))
        Exit();

    if (m_Keyboard.WasPressed(vgd::Keyboard::KeyUp))
        m_GameInfo.MoveSelection(-1);

    if (m_Keyboard.WasPressed(vgd::Keyboard::KeyDown))
        m_GameInfo.MoveSelection(1);

    if (m_Keyboard.WasPressed(vgd::Keyboard::KeyEnter) ||
        m_Keyboard.WasPressed(vgd::Keyboard::KeyD))
    {
        LaunchSelectedGame();
    }

    if (m_Sound.GetPlayer().HasError())
        Exit();

    // Receive completed file and image loads from the worker.
    m_GameInfo.Update();

    // Apply the palette once it becomes available.
    if (!m_DisplayPaletteSet)
    {
        unsigned char palette[256 * 3];

        if (m_GameInfo.GetPalette(palette))
        {
            if (m_Display.SetPalette(palette))
            {
                m_DisplayPaletteSet = true;
                printf("[Application] Display palette applied\n");
            }
        }
    }

    // Refresh visible controls from the current cache window.
    RefreshGameList();

    if (m_MainPanel.IsValid())
        m_MainPanel->Update();

    ++m_Frame;
}

void Application::Draw()
{
    if (m_Background)
        m_Background->SetFrame(m_Frame, m_Interpolation);

    if (m_MainPanel.IsValid())
        m_MainPanel->Draw();
}

void Application::Exit()
{
    m_ExitRequested = true;
}

int Application::Run()
{
    const uint64_t UpdateRate = 60;
    const uint64_t UpdateThreshold = 1000000000ULL;

    m_ExitRequested = false;
    m_LaunchGameRequested = false;
    m_LaunchGamePath.Clear();
    m_Frame = 0;
    m_Interpolation = 0.0;
    m_DisplayPaletteSet = false;

    if (!Initialize())
    {
        Shutdown();
        return 1;
    }

    int result = 0;

    uint64_t previousTime = vgd::Clock::GetTimeNanoseconds();
    uint64_t accumulator = 0;

    while (!m_ExitRequested)
    {
const uint64_t currentTime = vgd::Clock::GetTimeNanoseconds();

if (currentTime < previousTime)
{
    printf("CLOCK WENT BACKWARDS! Previous: %llu Current: %llu\n",
           (unsigned long long)previousTime,
           (unsigned long long)currentTime);
}

const uint64_t elapsed = currentTime - previousTime;

if (elapsed > 100000000ULL)
{
    printf("LARGE ELAPSED: %llu ns\n",
           (unsigned long long)elapsed);
}

previousTime = currentTime;

const uint64_t delta = elapsed * UpdateRate;

if (delta > UpdateThreshold * 10)
{
    printf("LARGE DELTA: %llu\n",
           (unsigned long long)delta);
}

accumulator += delta;

if (accumulator > UpdateThreshold * 10)
{
    printf("LARGE ACCUMULATOR: %llu\n",
           (unsigned long long)accumulator);
}

        while (accumulator >= UpdateThreshold && !m_ExitRequested)
        {
            Update();
            accumulator -= UpdateThreshold;
        }

        if (m_ExitRequested)
            break;

        m_Interpolation =
            (double)accumulator / (double)UpdateThreshold;

        Draw();

        if (!m_Display.Present())
        {
            result = 1;
            break;
        }

        if (m_Sound.GetPlayer().HasError())
        {
            result = 1;
            break;
        }
    }

    Shutdown();

    if (m_LaunchGameRequested)
    {
        if (!LaunchAndExit(
                "/games/lvgd/Supervisor",
                m_LaunchGamePath.CStr()))
        {
            result = 1;
        }
    }

    return result;
}