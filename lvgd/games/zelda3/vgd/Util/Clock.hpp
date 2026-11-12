#ifndef VGD_CLOCK_HPP
#define VGD_CLOCK_HPP

#include <stdint.h>

namespace vgd
{

class Clock
{
public:
    Clock();

    static uint64_t GetTimeNanoseconds();
    static double GetTimeSeconds();
    static uint64_t GetTimeMilliseconds();

    void Reset();
    uint64_t GetElapsedNanoseconds() const;
    double GetElapsedSeconds() const;
    uint64_t GetElapsedMilliseconds() const;

private:
    uint64_t m_StartTime;
};

}

#endif