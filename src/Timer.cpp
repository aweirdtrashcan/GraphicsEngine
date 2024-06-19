#include "Timer.h"

Timer::Timer()
{
    LARGE_INTEGER countsPerSecond;
    QueryPerformanceFrequency(&countsPerSecond);
    m_SecondsPerCount = 1.0 / static_cast<double>(countsPerSecond.QuadPart);
}

float Timer::GetDeltaTime() const
{
    return static_cast<float>(m_DeltaTime);
}

void Timer::Reset()
{
    LARGE_INTEGER currTime;
    QueryPerformanceCounter(&currTime);

    m_BaseTime = currTime;
    m_PreviousTime = currTime;
}

void Timer::Tick()
{
    QueryPerformanceCounter(&m_CurrentTime);

    m_DeltaTime = (m_CurrentTime.QuadPart - m_PreviousTime.QuadPart) * m_SecondsPerCount;
    m_PreviousTime = m_CurrentTime;

    if (m_DeltaTime < 0.0) m_DeltaTime = 0.0;
}
