#include "Logger.h"

#include "Engine.h"

// NOTE: setvbuf(stdout, NULL, _IONBF, 0) will disable buffering because it was 'eating' some characters

#define INTERNAL_LOGGER(x, color)                                               \
    {                                                                           \
        va_list va;                                                             \
        va_start(va, format);                                                   \
        setvbuf(stdout, NULL, _IONBF, 0);                                       \
                                                                                \
        size_t bufferSize = 4096;                                               \
        char* buffer = (char*)alloca(bufferSize);                               \
                                                                                \
        vsnprintf(buffer, bufferSize, format, va);                              \
                                                                                \
        HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);                        \
                                                                                \
        SetConsoleTextAttribute(stdOut, color); \
        printf("%s", buffer);                                                   \
        SetConsoleTextAttribute(stdOut, 7);                                     \
    }

void Logger::Error(const char* format, ...)
{
    INTERNAL_LOGGER(format, FOREGROUND_RED | FOREGROUND_INTENSITY);
}

void Logger::Debug(const char* format, ...)
{
    INTERNAL_LOGGER(format, FOREGROUND_GREEN | FOREGROUND_INTENSITY)
}

void Logger::Info(const char* format, ...)
{
    INTERNAL_LOGGER(format, FOREGROUND_INTENSITY);
}
