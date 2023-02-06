#pragma once

#include "crc.h"

inline int64_t GetProfileTime()
{
#if defined(ESP32) || defined(ESP_PLATFORM)
  return esp_timer_get_time();
#elif defined(NRF)
  return 0;
#else
  #error "asd"
#endif
}

class GJProfileBase
{
public:
    
    static bool Enable(bool enable);
    static bool Report();
    
protected:
  inline static bool IsEnabled() { return ms_enable; }

  struct Cumul
  {
    void Add(int64_t begin);
    int64_t m_total;
    int64_t m_max;
    int32_t m_count;
  };
  
  struct Instance
  {
    Instance();
    static Instance *ms_first;
    
    Instance *m_next = nullptr;
    const char *m_name = nullptr;
    Cumul m_cumul = {};
  };
  
private:

  static bool ms_enable;
};

template <uint32_t ID>
class GJProfile : private GJProfileBase
{
static constexpr uint32_t _ID = ID;
public:
  inline GJProfile(const char *name)
  {
    ms_instance.m_name = name;//not great
  }
  inline ~GJProfile()
  {
    if (IsEnabled())
    {
      ms_instance.m_cumul.Add(m_begin);
    }
  }
  int64_t const m_begin = GetProfileTime();
  static Instance ms_instance;
};

template <uint32_t ID> GJProfileBase::Instance GJProfile<ID>::ms_instance = {};

#if defined(ESP32)
  #define GJ_PROFILE(name) GJProfile<static_crc(#name)> const profile##__LINE__(#name)
#elif defined(NRF)
  #define GJ_PROFILE(name)
#endif

void InitProfiling();
void UpdateProfile();