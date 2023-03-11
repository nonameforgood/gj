#include "stringview.h"

#if defined(USE_GJ_STRING_VIEW)

#include <cstring>

StringView::StringView()
: m_str(nullptr)
, m_len(0)
{

}
StringView::StringView(const char *str)
: m_str(str)
, m_len(strlen(str))
{

}
StringView::StringView(const char *str, uint32_t len)
: m_str(str)
, m_len(len)
{

}

StringView::StringView(const StringView &other)
: m_str(other.m_str)
, m_len(other.m_len)
{

}

StringView& StringView::operator=(const StringView &other)
{
  m_str = other.m_str;
  m_len = other.m_len;

  return *this;
}

const char *StringView::data() const
{
  return m_str;
}

uint32_t StringView::size() const
{
  return m_len;
}

uint32_t StringView::length() const
{
  return m_len;
} 

bool operator ==(const StringView &stringView, const char *string)
{
  const uint32_t len = strlen(string);
  if (len != stringView.size())
    return false;

  return strncmp(stringView.data(), string, len) == 0;
}
  
#endif //USE_GJ_STRING_VIEW