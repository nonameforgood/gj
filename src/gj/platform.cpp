#if defined(ESP32)
#include "base.h"
#include <driver/rtc_io.h>
#include <esp32-hal-gpio.h>

uint32_t ReadPin(uint16_t pin)
{
  return digitalRead(pin);
}

void WritePin(uint16_t pin, uint32_t value)
{
  digitalWrite(pin, value);
}

void SetupPinEx(uint32_t pin, bool input, int32_t pull, bool highDrive)
{
  if (input)
  {
    int32_t pullMode = 0;
    if (pull < 0)
      pullMode = INPUT_PULLDOWN;
    else if (pull > 0)
      pullMode = INPUT_PULLUP;
    pinMode(pin, pullMode);   //pull down important so that sensor can be detected reliably

    gpio_num_t gpio = (gpio_num_t)pin; 
    const int32_t rtcPin = digitalPinToRtcPin(gpio);
    if (rtcPin != -1)
    {
      esp_err_t ret;

      //using rtc pull because on some pins, GPIO pull is not working because of a silicon bug
      ret = rtc_gpio_init(gpio);
      LOG_ON_ERROR(ret, "rtc_gpio_init:%s\n\r", esp_err_to_name(ret));
      ret = rtc_gpio_set_direction(gpio, RTC_GPIO_MODE_INPUT_ONLY);
      LOG_ON_ERROR(ret, "rtc_gpio_set_direction:%s\n\r", esp_err_to_name(ret));

      if (pull < 0)
      {
        ret = rtc_gpio_pullup_dis(gpio);
        LOG_ON_ERROR(ret, "rtc_gpio_pullup_dis:%s\n\r", esp_err_to_name(ret));
        ret = rtc_gpio_pulldown_en(gpio);
        LOG_ON_ERROR(ret, "rtc_gpio_pulldown_en:%s\n\r", esp_err_to_name(ret));
      }
      else if (pull > 0)
      {
        ret = rtc_gpio_pulldown_dis(gpio);
        LOG_ON_ERROR(ret, "rtc_gpio_pullup_dis:%s\n\r", esp_err_to_name(ret));
        ret = rtc_gpio_pullup_en(gpio);
        LOG_ON_ERROR(ret, "rtc_gpio_pulldown_en:%s\n\r", esp_err_to_name(ret));
      }
    }
  }
  else
    pinMode(pin, OUTPUT);   //pull down important so that sensor can be detected reliably
}

void SetupPin(uint32_t pin, bool input, int32_t pull)
{
  SetupPinEx(pin, input, pull, false);
}

bool ForceLinkSymbol(const void*ptr)
{
  printf("", ptr);

  return true;
}


uint32_t SavePinConf(uint16_t pin)
{
  return 0;
}

void RestorePinConf(uint16_t pin, uint32_t conf)
{
  
}

#elif defined(NRF)

#include <app_util_platform.h>  //NRF_BREAKPOINT
#include <stdlib.h>             // malloc, free
#include "esputils.h"
#include <nrf_delay.h>
#include <nrf_gpio.h>
#include <nrf_drv_clock.h>



#define APP_TIMER_PRESCALER             0                                        /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE         4                                           /**< Size of timer operation queues. */


//#define GJ_DEBUG_ALLOC

#ifdef GJ_DEBUG_ALLOC
  static int s_allocId = 0; 
  static int s_allocBreakId = 0; 

  void BreakOnAllocId()
  {
    if (s_allocId == s_allocBreakId)
    {
      printf(" ");
    }
  }

  
  extern uint32_t trackedAllocations;   //to make sure everything uses these new and delete function

  #define GJ_LOG_ALLOC(delta, s)  SEGGER_RTT_printf(0, "Alloc id %d:%d(req:%d waste:%d) remain:%d\n\r", ++s_allocId, delta, s, delta ? delta-s : 0, GetAvailableRam());
  #define GJ_LOG_FREE(s)          SEGGER_RTT_printf(0, "Free:%d remain:%d\n\r", s, GetAvailableRam());
  #define GJ_ALLOC_BREAK()        BreakOnAllocId()
  #define GJ_ON_ALLOC_DBG(s)      s
#else
  #define GJ_LOG_ALLOC(delta, s)
  #define GJ_LOG_FREE(s)        
  #define GJ_ALLOC_BREAK()      
  #define GJ_ON_ALLOC_DBG(s)
#endif

extern uint32_t minAvailRam;

#define ALLOC_LOCK() uint8_t p_is_nested_critical_region; sd_nvic_critical_region_enter(&p_is_nested_critical_region)
#define ALLOC_UNLOCK() sd_nvic_critical_region_exit(p_is_nested_critical_region)

bool IsInISR()
{
  const uint32_t mask = SCB->ICSR & (SCB_ICSR_VECTACTIVE_Msk << SCB_ICSR_VECTACTIVE_Pos);

  return mask != 0;
}

void *operator new(uint32_t s)
{
  GJ_ON_ALLOC_DBG(const uint32_t before = GetAvailableRam());

  ALLOC_LOCK();
  void *d = malloc(s);
  ALLOC_UNLOCK();

  if (d == nullptr)
  {
    SEGGER_RTT_printf(0, "Out of RAM\n\r");
    APP_ERROR_CHECK_BOOL(false);
  }

#ifdef GJ_DEBUG_ALLOC
  uint32_t delta = before - GetAvailableRam();

  trackedAllocations += delta;
  if (delta)
  {
    GJ_LOG_ALLOC(delta, s);
  }

#endif

  minAvailRam = Min(minAvailRam, GetAvailableRam());

  return d;
}


void *operator new(uint32_t s, std::align_val_t)
{
  GJ_ON_ALLOC_DBG(const uint32_t before = GetAvailableRam());

  ALLOC_LOCK();
  void *d = malloc(s);
  ALLOC_UNLOCK();

  if (d == nullptr)
  {
    SEGGER_RTT_printf(0, "Out of RAM\n\r");
    APP_ERROR_CHECK_BOOL(false);
  }

#ifdef GJ_DEBUG_ALLOC
  uint32_t delta = before - GetAvailableRam();

  trackedAllocations += delta;

  if (delta)
  {
    GJ_LOG_ALLOC(delta, s);
  }
#endif

  minAvailRam = Min(minAvailRam, GetAvailableRam());

  return d;
}

void operator delete(void *ptr)
{
  GJ_ON_ALLOC_DBG(const uint32_t before = GetAvailableRam());
  ALLOC_LOCK();
  free(ptr);
  ALLOC_UNLOCK();

#ifdef GJ_DEBUG_ALLOC
  const uint32_t s = GetAvailableRam() - before;

  trackedAllocations -= s;
  if (s)
  {
    GJ_LOG_FREE(s);
  }
#endif
}


void operator delete(void *ptr, unsigned int, std::align_val_t)
{
  GJ_ON_ALLOC_DBG(const uint32_t before = GetAvailableRam());
  ALLOC_LOCK();
  free(ptr);
  ALLOC_UNLOCK();

#ifdef GJ_DEBUG_ALLOC
  const uint32_t s = GetAvailableRam() - before;

  trackedAllocations -= s;
  if (s)
  {
    GJ_LOG_FREE(s);
  }
#endif
}

APP_TIMER_DEF(PLATFORM_Timer);

struct PlatformTimer
{
  bool m_init = false;
  uint32_t m_period = 5;
  uint32_t m_cumulSeconds = 0;
  uint32_t m_base = 0;
};

static PlatformTimer s_platformTimer;


static uint64_t GetElapsedMicrosInternal()
{
  uint32_t counter = app_timer_cnt_get();
  uint32_t rtcElapsed;

  if (counter > s_platformTimer.m_base)
    rtcElapsed = counter - s_platformTimer.m_base;
  else
    rtcElapsed = 0xffffff - s_platformTimer.m_base + counter;

  uint64_t mc  = ((uint64_t)rtcElapsed * 1000 * 1000) / APP_TIMER_CLOCK_FREQ;

  mc += (uint64_t)s_platformTimer.m_cumulSeconds * 1000 * 1000;

  return mc;
}


static void PlatformTimerHandler(void *context)
{
  uint32_t counter = app_timer_cnt_get();
  uint32_t elapsed;

  if (counter > s_platformTimer.m_base)
    elapsed = counter - s_platformTimer.m_base;
  else
    elapsed = 0xffffff - s_platformTimer.m_base + counter;

  uint32_t secondsElapsed = elapsed / APP_TIMER_CLOCK_FREQ;

  elapsed -= secondsElapsed * APP_TIMER_CLOCK_FREQ;
  
  uint32_t cumulBef = s_platformTimer.m_cumulSeconds;
  uint32_t baseBef = s_platformTimer.m_base;

  s_platformTimer.m_base = (counter - elapsed) & 0xffffff;
  s_platformTimer.m_cumulSeconds += secondsElapsed;

  if (s_platformTimer.m_cumulSeconds < cumulBef)
    printf("PlatformTimerHandler elapsed:%d counter:%d baseBef:%d cumulBef:%d new base:%d new cumul:%d\n\r", 
      (uint32_t)(GetElapsedMicrosInternal() / 1000000), counter, baseBef, cumulBef,
      s_platformTimer.m_base, s_platformTimer.m_cumulSeconds);
}

bool IsLFClockAvailable()
{
  return nrf_drv_clock_lfclk_is_running();
}

void InitPlatformTimer()
{
  if (s_platformTimer.m_init)
    return;

  //app_timer requires RTC1.
  //RTC1 requires a CLK source (high or low frequency), use the low freq one to use less power
  if (!nrf_drv_clock_lfclk_is_running())
  {
    ret_code_t err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);

    nrf_drv_clock_lfclk_request(NULL);
  }

  s_platformTimer.m_init = true;

  APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, nullptr);

  app_timer_create(&PLATFORM_Timer, APP_TIMER_MODE_REPEATED, PlatformTimerHandler);

  //Create an event every few seconds to handle RTC counter overflows
  const uint32_t delayMS = s_platformTimer.m_period * 1000;
  const uint32_t ticks = APP_TIMER_TICKS(delayMS, 0);
  app_timer_start(PLATFORM_Timer, ticks, nullptr);
}


uint64_t GetElapsedMicros()
{
  InitPlatformTimer();

  uint8_t p_is_nested_critical_region;
  sd_nvic_critical_region_enter(&p_is_nested_critical_region);

  uint64_t mc = GetElapsedMicrosInternal();

  sd_nvic_critical_region_exit(p_is_nested_critical_region);
  
  return mc;
  
}
uint64_t GetElapsedMillis()
{
  uint64_t ms  = GetElapsedMicros() / 1000;

  return ms;
}


void Delay(uint32_t duration)
{
  nrf_delay_ms(duration);
}

uint32_t ReadPin(uint16_t pin)
{
  return nrf_gpio_pin_read(pin);
}

void WritePin(uint16_t pin, uint32_t value)
{
  nrf_gpio_pin_write(pin, value);
}

void SetupPinEx(uint32_t pin, bool input, int32_t pull, bool highDrive)
{
  if (input)
  {
    nrf_gpio_pin_pull_t inPull = NRF_GPIO_PIN_NOPULL;

    if (pull < 0)
      inPull = NRF_GPIO_PIN_PULLDOWN;
    else if (pull > 0)
      inPull = NRF_GPIO_PIN_PULLUP;

    nrf_gpio_cfg(
      pin, 
      NRF_GPIO_PIN_DIR_INPUT,
      NRF_GPIO_PIN_INPUT_CONNECT,
      inPull,
      NRF_GPIO_PIN_S0S1,
      NRF_GPIO_PIN_NOSENSE
    );
  }
  else
  {
    nrf_gpio_cfg(
      pin, 
      NRF_GPIO_PIN_DIR_OUTPUT,
      NRF_GPIO_PIN_INPUT_DISCONNECT,
      NRF_GPIO_PIN_NOPULL,
      highDrive ? NRF_GPIO_PIN_H0H1 : NRF_GPIO_PIN_S0S1,
      NRF_GPIO_PIN_NOSENSE
    );
  }
} 

void SetupPin(uint32_t pin, bool input, int32_t pull)
{
  SetupPinEx(pin, input, pull, false);
}

uint32_t SavePinConf(uint32_t pin)
{
  NRF_GPIO_Type *gpio = nrf_gpio_pin_port_decode(&pin);

  return gpio->PIN_CNF[pin];
}

void RestorePinConf(uint32_t pin, uint32_t conf)
{
  NRF_GPIO_Type *gpio = nrf_gpio_pin_port_decode(&pin);
  gpio->PIN_CNF[pin] = conf;
}

bool ForceLinkSymbol(const void*ptr)
{
  printf("", ptr);

  return true;
}

#endif