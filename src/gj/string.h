#pragma once

#include "base.h"

#if !defined(ARDUINO_ESP32_DEV) && defined(CONFIG_IDF_CMAKE) || defined(GJ_IDF)
  #include <string>

  class String : public std::string
  {
    public:
      using std::string::string;
  };

  class GJString : public String
  {
    public:

      static String MakeString(const char *str, uint32_t length);
  };

  inline String GJString::MakeString(const char *str, uint32_t length)
  {
    String localString(str, length);

    return localString;
  }
#elif defined(ESP32) || defined(ESP_PLATFORM)
  #include "WString.h"


class GJString : public String
{
  public:
    GJString() = default;
    GJString(const char *str);
    GJString(const char *str, uint32_t length);

    GJString(String && other);

    inline uint32_t size() const {return String::length();}
    inline bool empty() const {return String::isEmpty();}

    GJString substring(unsigned int left, unsigned int right) const;
    friend GJString operator +(GJString const &left, GJString const &right);
    unsigned char concat(const GJString &s);

    static GJString MakeString(const char *str, uint32_t length);
};

inline GJString::GJString(const char *str)
{
  copy(str, strlen(str));
}

inline GJString::GJString(const char *str, uint32_t length)
{
  copy(str, length);
}

inline GJString GJString::MakeString(const char *str, uint32_t length)
{
  GJString localString(str, length);

  return localString;
}





#else

#include <string>

class GJString : public std::string
{
public:
  GJString();
  GJString(const char *str);
  GJString(const char *str, uint32_t length);

  ~GJString();
  
  //inline uint32_t size() const { return 0; }
  //inline void reserve(uint32_t size) {}
  //inline bool empty() const { return true; }
  //inline void clear() const { }


  static GJString MakeString(const char *str, uint32_t length);

  //const char *c_str() const { return ""; }

  GJString& operator +=(const char *);
  GJString& operator +=(GJString const &other);
  GJString operator +(GJString const &other) const;

  //inline char& operator [](uint32_t i) {return *(char*)nullptr;}

  void remove(uint32_t i, uint32_t l);

  //inline const char * begin() const { return nullptr; }
  //inline char * begin() { return nullptr; }


  unsigned char concat(const GJString &s);


  bool operator ==(const char *str) const;

  GJString substr(uint32_t begin, uint32_t len) const;
  GJString substring(uint32_t begin, uint32_t end) const;
  
};


#endif

GJString FormatString(const char *fmt, ...);
GJString FormatStringVA(const char *fmt, va_list);

inline GJString MakeString(const char *str, uint32_t length)
{
  return GJString::MakeString(str, length);
}

