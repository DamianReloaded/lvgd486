#include "Clock.hpp"
#include <sys/time.h>
#include <pthread.h>

namespace vgd
{

static pthread_mutex_t s_ClockMutex = PTHREAD_MUTEX_INITIALIZER;

Clock::Clock()
{
    Reset();
}

uint64_t Clock::GetTimeNanoseconds()
{
    pthread_mutex_lock(&s_ClockMutex);

    struct timeval tv;
    gettimeofday(&tv, nullptr);

    const uint64_t currentTime =
        (uint64_t)tv.tv_sec * 1000000000ULL +
        (uint64_t)tv.tv_usec * 1000ULL;

    static uint64_t lastTime = 0;
    uint64_t result = currentTime;

    if (currentTime < lastTime)
    {
        if (lastTime - currentTime < 1000000000ULL)
            result = lastTime;
    }

    lastTime = result;

    pthread_mutex_unlock(&s_ClockMutex);

    return result;
}

double Clock::GetTimeSeconds()
{
    return (double)GetTimeNanoseconds() / 1000000000.0;
}

uint64_t Clock::GetTimeMilliseconds()
{
    return GetTimeNanoseconds() / 1000000ULL;
}

void Clock::Reset()
{
    m_StartTime = GetTimeNanoseconds();
}

uint64_t Clock::GetElapsedNanoseconds() const
{
    const uint64_t currentTime = GetTimeNanoseconds();

    return currentTime >= m_StartTime
        ? currentTime - m_StartTime
        : 0;
}

double Clock::GetElapsedSeconds() const
{
    return (double)GetElapsedNanoseconds() / 1000000000.0;
}

uint64_t Clock::GetElapsedMilliseconds() const
{
    return GetElapsedNanoseconds() / 1000000ULL;
}

}