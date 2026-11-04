#include "GameInfo.hpp"

#include "../../vgd/Image/BmpLoader.hpp"
#include "../../vgd/Util/PropertyFile.hpp"

#include <string.h>
#include <stdio.h>
#include <unistd.h>

namespace
{
    typedef vgd::PropertyFile<1, 3, 128> GamePropertyFile;
}

GameInfo::GameInfo()
    : m_MutexInitialized(false),
      m_ThreadStarted(false),
      m_StopWorker(false),
      m_RequestPending(false),
      m_ResultReady(false),
      m_CountUpdated(false),
      m_WorkerFailed(false),
      m_RequestedIndex(0),
      m_PendingGameCount(0),
      m_GameCount(0),
      m_SelectedIndex(0),
      m_CacheStart(-1),
      m_CacheCount(0),
      m_ResultStart(-1),
      m_ResultCount(0)
{
    m_Path[0] = '\0';
    m_PaletteReady = false;
    memset(m_Palette, 0, sizeof(m_Palette));
}

GameInfo::~GameInfo()
{
    Shutdown();
}

bool GameInfo::Initialize(const char* path)
{
    Shutdown();

    if (path == NULL || strlen(path) >= sizeof(m_Path))
    {
        return false;
    }

    strcpy(m_Path, path);

    if (pthread_mutex_init(&m_Mutex, NULL) != 0)
    {
        return false;
    }

    m_MutexInitialized = true;

    m_StopWorker = false;
    m_RequestPending = true;
    m_ResultReady = false;
    m_CountUpdated = false;
    m_WorkerFailed = false;

    m_RequestedIndex = 0;
    m_PendingGameCount = 0;
    m_GameCount = 0;
    m_SelectedIndex = 0;

    m_CacheStart = -1;
    m_CacheCount = 0;

    m_ResultStart = -1;
    m_ResultCount = 0;

    m_PaletteReady = false;
    memset(m_Palette, 0, sizeof(m_Palette));

    if (pthread_create(&m_Thread, NULL, WorkerEntry, this) != 0)
    {
        pthread_mutex_destroy(&m_Mutex);
        m_MutexInitialized = false;
        return false;
    }

    m_ThreadStarted = true;
    return true;
}

void GameInfo::Shutdown()
{
    if (m_ThreadStarted)
    {
        pthread_mutex_lock(&m_Mutex);
        m_StopWorker = true;
        pthread_mutex_unlock(&m_Mutex);

        pthread_join(m_Thread, NULL);
        m_ThreadStarted = false;
    }

    m_CacheCount = 0;
    m_CacheStart = -1;
    m_ResultCount = 0;
    m_ResultStart = -1;

    m_GameCount = 0;
    m_SelectedIndex = 0;

    m_ResultReady = false;
    m_RequestPending = false;
    m_CountUpdated = false;
    m_WorkerFailed = false;

    m_PaletteReady = false;
    memset(m_Palette, 0, sizeof(m_Palette));

    m_Path[0] = '\0';

    if (m_MutexInitialized)
    {
        pthread_mutex_destroy(&m_Mutex);
        m_MutexInitialized = false;
    }
}

void GameInfo::Update()
{
    if (!m_MutexInitialized)
    {
        return;
    }

    pthread_mutex_lock(&m_Mutex);

    if (m_CountUpdated)
    {
        m_GameCount = m_PendingGameCount;
        m_CountUpdated = false;

        if (m_SelectedIndex >= m_GameCount)
        {
            m_SelectedIndex = m_GameCount > 0 ? m_GameCount - 1 : 0;
        }
    }

    if (m_ResultReady)
    {
        m_CacheStart = m_ResultStart;
        m_CacheCount = m_ResultCount;

        for (int i = 0; i < m_ResultCount; ++i)
        {
            m_Cache[i].Title = m_Results[i].Title;
            m_Cache[i].Binary = m_Results[i].Binary;
            m_Cache[i].Thumbnail = m_Results[i].Thumbnail;
            m_Cache[i].Image.Swap(m_Results[i].Image);
        }

        m_ResultReady = false;
    }

    pthread_mutex_unlock(&m_Mutex);
}

bool GameInfo::IsReady() const
{
    return GetSelectedGame() != NULL;
}

bool GameInfo::IsFailed()
{
    if (!m_MutexInitialized)
    {
        return m_WorkerFailed;
    }

    pthread_mutex_lock(&m_Mutex);
    bool failed = m_WorkerFailed;
    pthread_mutex_unlock(&m_Mutex);

    return failed;
}

int GameInfo::GetGameCount() const
{
    return m_GameCount;
}

int GameInfo::GetSelectedIndex() const
{
    return m_SelectedIndex;
}

const GameInfo::Game* GameInfo::GetGame(int index) const
{
    if (index < 0 || index < m_CacheStart ||
        index >= m_CacheStart + m_CacheCount)
    {
        return NULL;
    }

    return &m_Cache[index - m_CacheStart];
}

const GameInfo::Game* GameInfo::GetSelectedGame() const
{
    return GetGame(m_SelectedIndex);
}

bool GameInfo::MoveSelection(int offset)
{
    if (!m_MutexInitialized || m_GameCount <= 0 || offset == 0)
    {
        return false;
    }

    int index = m_SelectedIndex + offset;

    if (index < 0 || index >= m_GameCount)
    {
        return false;
    }

    m_SelectedIndex = index;

    pthread_mutex_lock(&m_Mutex);

    m_RequestedIndex = index;
    m_RequestPending = true;

    pthread_mutex_unlock(&m_Mutex);

    return true;
}

void* GameInfo::WorkerEntry(void* parameter)
{
    GameInfo* gameInfo = static_cast<GameInfo*>(parameter);
    gameInfo->WorkerLoop();
    return NULL;
}

void GameInfo::WorkerLoop()
{
    GamePropertyFile file;

    if (!file.Open(m_Path))
    {
        printf("[GameInfo] Failed to open game list: %s\n", m_Path);
        SetFailed();
        return;
    }

    printf("[GameInfo] Opened game list: %s\n", m_Path);

    // Count the entries without retaining the entire list.
    int gameCount = 0;

    while (!IsStopped() && file.LoadNextGroup())
    {
        ++gameCount;
    }

    file.Close();

    if (IsStopped())
    {
        return;
    }

    pthread_mutex_lock(&m_Mutex);
    m_PendingGameCount = gameCount;
    m_CountUpdated = true;
    pthread_mutex_unlock(&m_Mutex);

    while (!IsStopped())
    {
        int requestedIndex = -1;

        pthread_mutex_lock(&m_Mutex);

        if (m_RequestPending && !m_ResultReady)
        {
            requestedIndex = m_RequestedIndex;
            m_RequestPending = false;
        }

        pthread_mutex_unlock(&m_Mutex);

        if (requestedIndex < 0)
        {
            usleep(10000);
            continue;
        }

        if (requestedIndex >= gameCount)
        {
            continue;
        }

        // Keep the selected game near the middle of the cache.
        int startIndex = requestedIndex - 1;

        if (startIndex < 0)
        {
            startIndex = 0;
        }

        const int maxStart = gameCount > CacheSize
            ? gameCount - CacheSize
            : 0;

        if (startIndex > maxStart)
        {
            startIndex = maxStart;
        }

        Game loaded[CacheSize];
        int loadedCount = 0;

        if (!file.Open(m_Path))
        {
            SetFailed();
            return;
        }

        // Skip entries before the requested window.
        bool valid = true;

        for (int i = 0; i < startIndex; ++i)
        {
            if (!file.LoadNextGroup())
            {
                valid = false;
                break;
            }
        }

        // Load the cache window.
        while (valid && loadedCount < CacheSize &&
               startIndex + loadedCount < gameCount)
        {
            if (IsStopped())
            {
                file.Close();
                return;
            }

            if (!file.LoadNextGroup())
            {
                valid = false;
                break;
            }

            const GamePropertyFile::EntryType& entry = file.GetEntry(0);
            Game& game = loaded[loadedCount];

            game.Title = entry.GetProperty(0);
            game.Binary = entry.GetProperty(1);
            game.Thumbnail = entry.GetProperty(2);

            // A missing or invalid thumbnail does not invalidate the game.
            if (!vgd::BmpLoader::Load(game.Thumbnail.CStr(), game.Image))
            {
                printf("[GameInfo] Failed to load thumbnail: %s\n", game.Thumbnail.CStr());
                printf("[GameInfo] prop1: %s\n", entry.GetProperty(0));
                printf("[GameInfo] prop2: %s\n", entry.GetProperty(1));
                printf("[GameInfo] prop3: %s\n", entry.GetProperty(2));
            }
            else
            {
                // Only load a palette if one has not been obtained yet.
                pthread_mutex_lock(&m_Mutex);
                bool needPalette = !m_PaletteReady;
                pthread_mutex_unlock(&m_Mutex);

                if (needPalette)
                {
                    unsigned char palette[256 * 3];

                    if (vgd::BmpLoader::LoadPalette(game.Thumbnail.CStr(), palette))
                    {
                        pthread_mutex_lock(&m_Mutex);

                        if (!m_PaletteReady)
                        {
                            memcpy(m_Palette, palette, sizeof(m_Palette));
                            m_PaletteReady = true;
                            printf("[GameInfo] Loaded thumbnail palette: %s\n", game.Thumbnail.CStr());
                        }

                        pthread_mutex_unlock(&m_Mutex);
                    }
                    else
                    {
                        printf("[GameInfo] Failed to load thumbnail palette: %s\n", game.Thumbnail.CStr());
                    }
                }
            }

            ++loadedCount;
        }

        file.Close();

        if (!valid)
        {
            SetFailed();
            return;
        }

        if (IsStopped())
        {
            return;
        }

        pthread_mutex_lock(&m_Mutex);

        // If selection changed during loading, discard this window.
        if (!m_StopWorker &&
            !m_RequestPending &&
            m_RequestedIndex == requestedIndex)
        {
            m_ResultStart = startIndex;
            m_ResultCount = loadedCount;

            for (int i = 0; i < loadedCount; ++i)
            {
                m_Results[i].Title = loaded[i].Title;
                m_Results[i].Binary = loaded[i].Binary;
                m_Results[i].Thumbnail = loaded[i].Thumbnail;
                m_Results[i].Image.Swap(loaded[i].Image);
            }

            m_ResultReady = true;
        }

        pthread_mutex_unlock(&m_Mutex);
    }
}

bool GameInfo::IsStopped()
{
    pthread_mutex_lock(&m_Mutex);
    bool stopped = m_StopWorker;
    pthread_mutex_unlock(&m_Mutex);

    return stopped;
}

void GameInfo::SetFailed()
{
    pthread_mutex_lock(&m_Mutex);
    m_WorkerFailed = true;
    pthread_mutex_unlock(&m_Mutex);
}

bool GameInfo::HasPalette() 
{
    if (!m_MutexInitialized)
    {
        return false;
    }

    pthread_mutex_lock(&m_Mutex);
    bool ready = m_PaletteReady;
    pthread_mutex_unlock(&m_Mutex);

    return ready;
}

bool GameInfo::GetPalette(unsigned char* palette)
{
    if (palette == NULL || !m_MutexInitialized)
    {
        return false;
    }

    pthread_mutex_lock(&m_Mutex);

    bool ready = m_PaletteReady;

    if (ready)
    {
        memcpy(palette, m_Palette, sizeof(m_Palette));
    }

    pthread_mutex_unlock(&m_Mutex);

    return ready;
}