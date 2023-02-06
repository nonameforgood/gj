#include "string.h"
#include <cstdarg>
#include <utility>

#if !defined(ARDUINO_ESP32_DEV) && defined(CONFIG_IDF_CMAKE) || defined(GJ_IDF)

#elif defined(ESP32) || defined(ESP_PLATFORM)

GJString::GJString(String && other)
: String(std::move(other))
{

}

GJString GJString::substring(unsigned int left, unsigned int right) const {
    
    String out = String::substring(left, right);

    GJString out2(std::move(out));

    return out2;
}

GJString operator +(GJString const &left, GJString const &right)
{
  String out = *(String const *)&left + *(String const *)&right;

  GJString out2(std::move(out));

  return out2;
}

unsigned char GJString::concat(const GJString &s)
{
  return String::concat(s);
}


#else

GJString::GJString() = default;

GJString::GJString(const char *str)
: std::string(str)
{

}
GJString::GJString(const char *str, uint32_t length)
: std::string(str, length)
{
  
}

GJString::~GJString() = default;


GJString& GJString::operator +=(const char *str) 
{
  std::string::operator +=(str);

  return *this;
}

GJString& GJString::operator +=(GJString const &other) 
{
  std::string::operator +=(other.c_str());

  return *this;
}
GJString GJString::operator +(GJString const &other)  const
{
  GJString out(c_str());

  out += other; 

  return out;
}


unsigned char GJString::concat(const GJString &s)
{
  std::string::operator +=(s.c_str());

  return true;
}

GJString GJString::substr(uint32_t begin, uint32_t len) const 
{ 
  GJString out(std::string::substr(begin, len).c_str());

  return out;
}
GJString GJString::substring(uint32_t begin, uint32_t end) const 
{ 
  GJString out(std::string::substr(begin, end - begin).c_str());

  return out;
}
void GJString::remove(uint32_t i, uint32_t l)
{
  erase(i, l);
}

bool GJString::operator ==(const char *str) const 
{ 
  return *(std::string*)this == (str); 
}

#endif

uint32_t gj_vsprintf(char *target, uint32_t targetSize, const char *format, va_list vaList );



GJString FormatString(const char *fmt, ...)
{
  char buffer[1024];

  va_list argptr;
  va_start(argptr, fmt);
  gj_vsprintf(buffer, sizeof(buffer), fmt, argptr);
  va_end(argptr);

  GJString ret(buffer);
  return ret;
}

GJString FormatStringVA(const char *fmt, va_list va)
{
  char buffer[1024];
  gj_vsprintf(buffer, sizeof(buffer), fmt, va);

  GJString ret(buffer);
  return ret;
}