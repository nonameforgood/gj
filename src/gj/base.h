#pragma once

#if defined(ESP8266) || defined(ESP32) || defined(ESP_PLATFORM) || defined(NRF)
  #include <stdint.h>
#else
  typedef unsigned long uint32_t;
  typedef unsigned int uint16_t;
  typedef unsigned char uint8_t;
#endif

typedef uint32_t U32;
typedef uint16_t U16;
typedef uint8_t U8;

#if !defined(ARDUINO_ESP32_DEV) && defined(CONFIG_IDF_CMAKE) || defined(GJ_IDF)
  #include "sdkconfig.h"
  #include "freertos/FreeRTOS.h"
  #include "esp_system.h"

  #define GJ_LOG_ENABLE
#elif defined(NRF)

  #ifdef _LIBCPP_VERSION
    //when compiled with LIBCPP
    #define __math_h
    #define _LIBCPP_MATH_H

    //void _Exit(int status);
    //long double        strtold (const char* nptr, char** endptr);
  #endif

  #include "nordic_common.h"
  #include "nrf.h"
  #include "app_error.h"

  //#define GJ_LOG_ENABLE
#else
  #include <arduino.h>
#endif

//#include <file.h>
#include "platform.h" 
#include "serial.h" 
#include "profiling.h"
#include "function_ref.hpp"

#define GJ_ASSERT( cond, format, ... ) if (!(cond)) { GJ_ERROR(format, ##__VA_ARGS__ ); Delay(3000);exit(0); }

extern bool enableLog;

void __attribute__ ((noinline)) gjFormatLogString(const char *format, ...);
void __attribute__ ((noinline)) gjFormatErrorString(const char *format, ...);
void __attribute__ ((noinline)) gjLogLargeString(const char *string);
void __attribute__ ((noinline)) gjLogStringOnChange(uint32_t &crc, const char *format, ...);

//validates 2 things: 
//  -the first arg to LOG_ON_ERROR is type esp_err_t 
//  -the value is ESP_OK
#if defined(ESP32) || defined(ESP_PLATFORM)
typedef esp_err_t ErrorType;
inline bool IsError(esp_err_t err) { return err != ESP_OK; }
inline const char *ErrorToName(esp_err_t ret) { return esp_err_to_name(ret); }
#elif defined(NRF)
typedef uint32_t ErrorType;
inline bool IsError(uint32_t err) { return err != NRF_SUCCESS; }
const char *ErrorToName(uint32_t ret);
#else
#error "Unsupported platform"
#endif

#if  defined(GJ_LOG_ENABLE)
  #define LOG(s, ...) { if (enableLog) gjFormatLogString(s, ##__VA_ARGS__); SER(s, ##__VA_ARGS__); }
  #define LOG_LARGE(s) { if (enableLog) gjLogLargeString(s); SER_LARGE(s); }
  #define LOG_COND(cond, s, ...) if (cond) { LOG(s, ##__VA_ARGS__); }
  #define LOG_ON_ERROR(ret, s, ...) { bool const isOK = !IsError(ret); GJ_ERROR_COND(!isOK, "%s(%d):", __FILE__,__LINE__); GJ_ERROR_COND(!isOK, s, ##__VA_ARGS__); }
  #define LOG_ON_ERROR_AUTO(s) { const ErrorType ret = s; GJ_ERROR_COND(IsError(ret), "ERROR %s(%d):'%s' result:%s", __FILE__,__LINE__, #s, ErrorToName(ret)); }
  #define LOG_ON_CHANGE(f, ...) {static uint32_t s_lastCrc = {}; gjLogStringOnChange(s_lastCrc, f, ##__VA_ARGS__); }

  #define ON_LOG(s) if(enableLog) {s;}

  #define GJ_ERROR(s, ...) { if (enableLog) {gjFormatErrorString(s, ##__VA_ARGS__); LOG(s, ##__VA_ARGS__); } }

  #define GJ_FLUSH_LOG() FlushLog()
#else
  #define LOG(s, ...) { SER(s, ##__VA_ARGS__); }
  #define LOG_LARGE(s) { SER_LARGE(s); }
  #define LOG_COND(cond, s, ...) if (cond) { LOG(s, ##__VA_ARGS__); }
  #define LOG_ON_ERROR(ret, s, ...) { bool const isOK = !IsError(ret); GJ_ERROR_COND(!isOK, "%s(%d):", __FILE__,__LINE__); GJ_ERROR_COND(!isOK, s, ##__VA_ARGS__); }
  #define LOG_ON_ERROR_AUTO(s) { const ErrorType ret = s; GJ_ERROR_COND(IsError(ret), "ERROR %s(%d):'%s' result:%s", __FILE__,__LINE__, #s, ErrorToName(ret)); }
  #define LOG_ON_CHANGE(f, ...) {}

  #define ON_LOG(s)

  #define GJ_ERROR(s, ...) { LOG(s, ##__VA_ARGS__); }

  #define GJ_FLUSH_LOG() while(false){} 
#endif


#define GJ_ERROR_COND(cond, s, ...) { if (cond) { GJ_ERROR(s, ##__VA_ARGS__); } }

#define DO_EVERY(duration, s) {static uint32_t last = 0; if (DoEvery(last, duration)) {s;} }
#define DO_ONCE(s) {static bool done = false; if (!done) {done = true; s;} }

template <typename T>
T Min( T l, T r )
{
  return ( l < r ) ? l : r;
}

template <typename T>
T Max( T l, T r )
{
  return ( l > r ) ? l : r;
}


void InitLog(const char *prefix = nullptr);
void FlushLog();
void TermLog();
//void SendLog(tl::function_ref<void(const char *)> cb);

bool DoEvery(uint32_t &last, uint32_t duration);

void SetVerbose(bool enable);
bool IsVerbose();

void InitDebugLoc();
void SetDebugLoc(const char* loc);

class ScopedDebugLoc
{
public:
    ScopedDebugLoc(const char *loc);
    ~ScopedDebugLoc();
private:
  const char *m_prev;
};

#define GJ_DEBUGLOC(loc) ScopedDebugLoc dbgLoc##__LINE__(loc)