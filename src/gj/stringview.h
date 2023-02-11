#pragma once

#if defined(ESP32)
  #define USE_GJ_STRING_VIEW
#endif

#if defined(USE_GJ_STRING_VIEW)
  #include "base.h"

  class StringView
  {
    public:
      StringView();
      StringView(const char *str);
      StringView(const char *str, uint32_t len);

      StringView(const StringView &other);
      StringView& operator=(const StringView &other);

      const char *data() const;
      uint32_t size() const;
      uint32_t length() const;
       
    private:
      const char * m_str;
      uint32_t m_len;
  };

  bool operator ==(const StringView &stringView, const char *string);

#else
  #include <string_view>
  typedef std::string_view StringView;
#endif