#pragma once


void SetupPin(uint32_t pin, bool input, int32_t pull);
void SetupPinEx(uint32_t pin, bool input, int32_t pull, bool highDrive);
 
uint32_t SavePinConf(uint32_t pin);
void RestorePinConf(uint32_t pin, uint32_t conf);
  
#if defined(ESP32) || defined(ESP_PLATFORM)
  #define GJ_PERSISTENT RTC_DATA_ATTR
  #define GJ_PERSISTENT_NO_INIT RTC_NOINIT_ATTR
  #define GJ_IRAM IRAM_ATTR

  #define GJ_PIN_COUNT 40

  #define GJ_DBG_PRINT(f, ...) printf(f, ##__VA_ARGS__)

  #define GJ_OPTIMIZE_OFF

  inline uint64_t GetElapsedMillis()
  {
    return millis();
  }

  inline uint64_t GetElapsedMicros()
  {
    return micros();
  }

  inline bool IsLFClockAvailable()
  {
    return true;
  }

  inline void Delay(uint32_t duration)
  {
    delay(duration);
  }

  uint32_t ReadPin(uint16_t pin);
  void WritePin(uint16_t pin, uint32_t value);

  

#elif defined(NRF)
  #include <app_timer.h>

  #define GJ_PERSISTENT
  #define GJ_PERSISTENT_NO_INIT
  #define GJ_IRAM

  #define GJ_PIN_COUNT 32

  #define GJ_DBG_PRINT(f, ...) SEGGER_RTT_printf(0, f, ##__VA_ARGS__)

  #define GJ_OPTIMIZE_OFF _Pragma("GCC optimize (\"O0\")")

  extern "C" {
    int SEGGER_RTT_printf(unsigned BufferIndex, const char * sFormat, ...);
  }

  bool IsLFClockAvailable();
  
  uint64_t GetElapsedMillis();

  uint64_t GetElapsedMicros();

  void Delay(uint32_t duration);

  uint32_t ReadPin(uint16_t pin);
  void WritePin(uint16_t pin, uint32_t value);

  inline uint32_t GJAtomicAdd(uint32_t &data, uint32_t val)
  {
    __disable_irq();

      uint32_t prev = data;
      data += val;
    __enable_irq();

    return prev;
  }

  inline uint32_t GJAtomicSub(uint32_t &data, uint32_t val)
  {
    __disable_irq();

      uint32_t prev = data;
      data -= val;
    __enable_irq();

    return prev;
  }

  bool ForceLinkSymbol(const void*ptr);

  
#else
  #error "asd"
#endif