#include "StimplyExceptionBase.h"

#include <cstdio>
#include <cstring>

StimplyExceptionBase::~StimplyExceptionBase()
{
    delete[] m_What;
}

const char* StimplyExceptionBase::what() const noexcept
{
    if (m_What) return m_What;
    
    const char* type = get_exception_type();
    size_t typeSize = strlen(type);
    size_t reasonSize = strlen(m_Reason);
    // add null terminator of type, reason and m_What
    size_t totalSize = typeSize + reasonSize + 3;
    
    char*& what = const_cast<char*&>(m_What);
    what = new char[totalSize];
    memset(what, 0, totalSize);

    (void)snprintf(what, totalSize, "%s: %s", type, m_Reason);
    return m_What;
}
