#pragma once
#include <cstdint>

enum EventCode
{
    // ctx u32[0] = key
    EVENT_KEY_PRESSED,
    // ctx u32[0] = key
    EVENT_KEY_RELEASED,
    // ctx i64[0] = x axis
    // ctx i64[1] = y axis
    EVENT_MOUSE_MOVED,
    EVENT_MOUSE_CLICKED,
    // ctx i8[0] = button
    EVENT_HIDE_CURSOR,
    EVENT_SHOW_CURSOR,
    EVENT_WINDOW_CLOSE,
    
    EVENT_ENUM_MAX,
};

class Event
{
public:
    Event() = default;
    Event(void* sender) : sender(sender) {}
        
    union ctx
    {
        uint64_t u64[2];
        uint32_t u32[4];
        uint16_t u16[8];
        uint8_t u8[16];
        
        char c8[16];
        
        int64_t i64[2];
        int32_t i32[4];
        int16_t i16[8];
        int8_t i8[16];
    } context{};
    
    void* sender = nullptr;
};