#pragma once
#include <exception>

class StimplyExceptionBase : public std::exception
{
public:
    StimplyExceptionBase(const char* reason) : m_Reason(reason) {}
    virtual ~StimplyExceptionBase() override;
    virtual const char* what() const noexcept override;
    virtual const char* get_exception_type() const = 0;

protected:
    const char* m_Reason;
    const char* m_What = nullptr;
};
