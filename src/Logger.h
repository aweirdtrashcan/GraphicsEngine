#pragma once

class Logger
{
public:
    Logger() = delete;
    ~Logger() = delete;

    static void Error(const char* format, ...);
    static void Debug(const char* format, ...);
    static void Info(const char* format, ...);
};
