#pragma once
#include "StimplyExceptionBase.h"

class ImGuiManagerException : public StimplyExceptionBase
{
public:
    ImGuiManagerException(const char* error) : StimplyExceptionBase(error) {}
    const char* get_exception_type() const override { return "ImGuiManagerException"; }
};
