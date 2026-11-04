#pragma once

#include <pthread.h>

#include "../../vgd/Image/Image.hpp"
#include "../../vgd/Util/FixedString.hpp"

class GameInfo
{
public:
    static const int CacheSize = 5;
    static const int VisibleCount = 3;

    typedef vgd::FixedString<128> String;

    struct Game
    {
        String Title;
        String Binary;
        String Thumbnail;
        vgd::Image Image;
    };

    GameInfo();
    ~GameInfo();

    GameInfo(const GameInfo&) = delete;
    GameInfo& operator=(const GameInfo&) = delete;

    bool Initialize(const char* path);
    void Shutdown();
    void Update();

    bool IsReady() const;
    bool IsFailed();

    int GetGameCount() const;
    int GetSelectedIndex() const;

    const Game* GetGame(int index) const;
    const Game* GetSelectedGame() const;

    bool MoveSelection(int offset);

    bool HasPalette();
    bool GetPalette(unsigned char* palette);

private:
    static void* WorkerEntry(void* parameter);
    void WorkerLoop();

    bool IsStopped();
    void SetFailed();

    pthread_t m_Thread;
    pthread_mutex_t m_Mutex;

    bool m_MutexInitialized;
    bool m_ThreadStarted;
    bool m_StopWorker;
    bool m_RequestPending;
    bool m_ResultReady;
    bool m_CountUpdated;
    bool m_WorkerFailed;

    int m_RequestedIndex;
    int m_PendingGameCount;

    int m_GameCount;
    int m_SelectedIndex;

    int m_CacheStart;
    int m_CacheCount;

    int m_ResultStart;
    int m_ResultCount;

    char m_Path[256];

    Game m_Cache[CacheSize];
    Game m_Results[CacheSize];

    unsigned char m_Palette[256 * 3];
    bool m_PaletteReady;    
};