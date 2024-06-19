#pragma once
#include "Event.h"

class IEventListener
{
public:
    virtual ~IEventListener() = default;
    virtual void OnEvent(EventCode code, const Event& event) = 0;
};
