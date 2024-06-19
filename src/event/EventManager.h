#pragma once
#include <vector>

#include "IEventListener.h"

class EventManager
{
public:
    EventManager() = delete;
    EventManager(const EventManager&) = delete;

    static bool RegisterListener(EventCode code, IEventListener* listener);
    static bool UnregisterListener(EventCode code, IEventListener* listener);

    static void FireEvent(EventCode code, const Event& event);
    
    static void ClearAllListeners();
private:
    static inline std::vector<IEventListener*> s_Listeners[EventCode::EVENT_ENUM_MAX];
};
