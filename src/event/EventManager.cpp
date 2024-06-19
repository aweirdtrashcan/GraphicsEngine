#include "EventManager.h"

bool EventManager::RegisterListener(EventCode code, IEventListener* listener)
{
    if (listener == nullptr) return false;
    if (code >= EventCode::EVENT_ENUM_MAX || code < 0) return false;
    s_Listeners[code].push_back(listener);
    return true;
}

bool EventManager::UnregisterListener(EventCode code, IEventListener* listener)
{
    if (listener == nullptr) return false;
    if (code >= EventCode::EVENT_ENUM_MAX || code < 0) return false;
    
    auto vecListener = std::find(s_Listeners[code].begin(), s_Listeners[code].end(), listener);

    if (*vecListener == listener)
    {
        s_Listeners[code].erase(vecListener);
        return true;
    }

    return false;
}

void EventManager::FireEvent(EventCode code, const Event& event)
{
    for (IEventListener* listener : s_Listeners[code])
    {
        listener->OnEvent(code, event);
    }
}

void EventManager::ClearAllListeners()
{
    for (auto& eventVectors : s_Listeners)
    {
        eventVectors.clear();
    }
}
