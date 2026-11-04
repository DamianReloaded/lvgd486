#include "Clock.hpp"
#include <sys/time.h>

namespace vgd
{

Clock::Clock()
{
    Reset();
}

uint64_t Clock::GetTimeNanoseconds()
{
    struct timeval tv;
    gettimeofday(&tv, 0);

    return (uint64_t)tv.tv_sec * 1000000000ULL +
           (uint64_t)tv.tv_usec * 1000ULL;
}

double Clock::GetTimeSeconds()
{
    struct timeval tv;
    gettimeofday(&tv, 0);

    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

uint64_t Clock::GetTimeMilliseconds()
{
    struct timeval tv;
    gettimeofday(&tv, 0);

    return (uint64_t)tv.tv_sec * 1000ULL +
           (uint64_t)tv.tv_usec / 1000ULL;
}

void Clock::Reset()
{
    m_StartTime = GetTimeNanoseconds();
}

uint64_t Clock::GetElapsedNanoseconds() const
{
    return GetTimeNanoseconds() - m_StartTime;
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