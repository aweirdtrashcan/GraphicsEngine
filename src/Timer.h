#pragma once

#include <Windows.h>

class Timer
{
public:
    Timer();
    ~Timer() = default;

    float GetDeltaTime() const;

    void Reset();
    void Tick();
    
private:
    double m_SecondsPerCount = 0.0;
    double m_DeltaTime = 0.0;
    
    LARGE_INTEGER m_BaseTime{};
    LARGE_INTEGER m_PreviousTime{}; 
    LARGE_INTEGER m_CurrentTime{};
};
