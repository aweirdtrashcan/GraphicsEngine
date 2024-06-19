#pragma once
#include "StimplyExceptionBase.h"

class WindowException : public StimplyExceptionBase
{
public:
    WindowException(const char* reason) : StimplyExceptionBase(reason) {}
    virtual const char* get_exception_type() const override { return "Window Exception"; }
};