#pragma once
#include "StimplyExceptionBase.h"

class RendererException : public StimplyExceptionBase 
{
public:
    RendererException(const char* reason) : StimplyExceptionBase(reason) {}
    const char* get_exception_type() const override { return "RendererException"; }
};

