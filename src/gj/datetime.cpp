#include <stdint.h>
#include "datetime.h"
#include "string.h"
#include "base.h"
#include "http.h"

#include <ctime>
#include <time.h>
#include "esputils.h"
#include "file.h"
#include "commands.h"
#include "config.h"
#include "eventmanager.h"
#include "appendonlyfile.h"

#ifdef ESP32
  #include <soc/rtc.h>
#elif defined(NRF)
  #include <app_util_platform.h>  //APP_IRQ_PRIORITY_LOWEST
  #include <nrf_delay.h>
#endif

#include <sys/time.h>

#include <time.h>

struct RTCCal
{
  uint64_t rtc = 0;
  uint32_t ms = 0;
  uint32_t rtcFreq = 0;
  uint32_t wait = 1500;
};

struct ClockSync
{
  uint64_t m_rtc = 0;
  uint32_t m_unixtime = 0;
};

//these global variables persist after deep sleep
GJ_PERSISTENT ClockSync g_clockSync;
GJ_PERSISTENT RTCCal g_rtcCal;

RTCCal InitRTCCal();
void CalibrateRTCFrequency( RTCCal &cal );


DEFINE_CONFIG_INT32(time.writeinterval, time_writeinterval, 15 * 60);
DEFINE_CONFIG_INT32(tz, tz, -4 * 60 * 60);

int32_t GetUnixtime()
{
  time_t nowUnix;
  time(&nowUnix);

  return nowUnix;
}

int32_t GetLocalUnixtime()
{
  int32_t time = GetUnixtime();
  time += GJ_CONF_INT32_VALUE(tz);
  return time;
}

int32_t GetEpoch( const char *dateTime )
{
#ifdef ESP32
  std::tm tm = {};

  strptime(dateTime, "%Y-%m-%dT%H:%M:%S", &tm);
  std::time_t epo = std::mktime( &tm );
  return (int32_t)epo;
#else
  return 0;
#endif
}

void ConvertEpoch(int32_t epoch, char *dateTime)
{
  std::time_t stdTime = epoch + GJ_CONF_INT32_VALUE(tz); 
  std::tm *retTM = std::localtime(&stdTime);
  if (!retTM)
  {
    LOG("ERR:gmtime\n\r");
  }
  std::tm tm = *retTM;

 //sprintf(dateTime, "%04d-%02d-%02dT%02d:%02d:%02d", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec );

  strftime(dateTime, 20, "%Y-%m-%dT%H:%M:%S", &tm);
}

void ConvertToTM(const char *dateTime, std::tm &tm)
{
  sscanf(dateTime, "%4d-%2d-%2dT%2d:%2d:%2d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec );
}

#if defined(NRF)
  #define GJ_DATE_USE_APPEND_ONLY_FILE
#endif

void WriteLastDateFile(int32_t unixtime)
{
#if defined(GJ_DATE_USE_APPEND_ONLY_FILE)
  AppendOnlyFile file("/lastdate");

  if (!file.BeginWrite(4))
  {
    file.Erase();
    bool ret = file.BeginWrite(4);
    APP_ERROR_CHECK_BOOL(ret);
  }

  file.Write(&unixtime, 4);
  file.EndWrite();

#else
  GJFile file("/lastdate", GJFile::Mode::Write);
  //for (int i = 0 ; i < 1024 ; ++i)
    file.Write(&unixtime, 4);
  file.Flush();
#endif
  LOG("lastdate file written\n\r");
}

void WriteLastDateFile()
{
  WriteLastDateFile(GetUnixtime());
}

void UpdateLastDateFile(int32_t unixtime)
{
  GJ_PERSISTENT static int32_t lastUnixtime = 0;
  int32_t const writeFreq = 60 * 60; //1 hour

  if ((unixtime - lastUnixtime) >= writeFreq)
  {
    lastUnixtime = unixtime;
    WriteLastDateFile(unixtime);
  }  
}

void PrepareDateTimeSleep()
{
#ifdef ESP32
  CalibrateRTCFrequency(g_rtcCal);

  g_clockSync.m_rtc = rtc_time_get();
  g_clockSync.m_unixtime = GetUnixtime();

  SER("Sleep stamp unix:%d rtc:0x%08x%08x rtc freq:%d\n\r", 
    g_clockSync.m_unixtime, (uint32_t)(g_clockSync.m_rtc >> 32), (uint32_t)(g_clockSync.m_rtc & 0xffffFFFF ), (uint32_t)g_rtcCal.rtcFreq);
#endif

  UpdateLastDateFile(g_clockSync.m_unixtime);
}

void SetUnixtime(uint32_t unixtime, bool writeFile)
{
  //time is local
#ifdef ESP32
  timezone tz = {0,0};
#elif defined(NRF)
  int tz = 0;
#endif

  timeval epochInit = {(int32_t)unixtime, 0};
  
  settimeofday(&epochInit, &tz);

  char date[20];
  ConvertEpoch(unixtime, date);
  LOG("SetUnixtime:%d(%s)\n\r", unixtime, date);

  if (writeFile)
  {
    WriteLastDateFile(unixtime);
  }
}

void SetUnixtime(uint32_t unixtime)
{
  SetUnixtime(unixtime, true);
}

RTCCal InitRTCCal()
{
  RTCCal cal = {};

#ifdef ESP32
  cal.rtc = rtc_time_get();
  cal.ms = millis();
  cal.rtcFreq = rtc_clk_slow_freq_get_hz();
#endif

  return cal;
}

void CalibrateRTCFrequency( RTCCal &cal )
{        
#ifdef ESP32
  uint64_t const slowMC_FP = rtc_clk_cal(RTC_CAL_RTC_MUX, 10000);  //returns fixed point 13.19
  cal.rtcFreq = (uint32_t)(1000000ULL * ( 1ULL << RTC_CLK_CAL_FRACT ) / slowMC_FP);
#else
  //this is calibration done manually.
  //It was used before I knew about rtc_clk_cal :)
  //both are pretty close, mine doesn't do an average.
  uint32_t slowHzMS = cal.rtcFreq / 1000;

  uint32_t newSlowHz = 0;

  RTCCal newRTC = InitRTCCal();

  uint32_t rtcTime = 0;
  uint32_t ms = GetElapsedMillis();

  {
    uint32_t const rtcElapsed = (uint32_t)(rtcTime - cal.rtc);
    uint32_t const msElapsed = ms - cal.ms;

    uint32_t const rtcElapsedS = rtcElapsed / cal.rtcFreq;
    uint32_t const rtcElapsedMSPart = (rtcElapsed % cal.rtcFreq) / slowHzMS;
    uint32_t const rtcElapsedMS = rtcElapsedS * 1000 + rtcElapsedMSPart;

    int32_t const diff = (int32_t)msElapsed - (int32_t)rtcElapsedMS;

    newSlowHz = rtcElapsed * 1000 / msElapsed;

    //Serial.print("elapsed rtc:"); Serial.print(rtcElapsedMS); 
    //Serial.print(" millis:"); Serial.print(msElapsed);  
    //Serial.print(" divergence:"); Serial.print(diff); 
    //Serial.print(" calc:"); Serial.print(newSlowHz); Serial.println("");
  }

  cal.rtcFreq = newSlowHz;
#endif

#ifdef ESP32
  //start a new cycle
  cal.rtc = rtc_time_get();
  cal.ms = millis();
#endif
}

GJ_PERSISTENT uint32_t lastOnlineUnixtime = 0;
static bool isOnlineDatetimeNeeded = false;

bool IsOnlineDateNeeded()
{
  return isOnlineDatetimeNeeded;
}

static OnlineDateUpdater s_onlineTimeUpdateFunc = nullptr;

void UpdateTimeOnline()
{
  if (s_onlineTimeUpdateFunc)
    (*s_onlineTimeUpdateFunc)();
  else
  {
    GJ_ERROR("ERROR:no online date updater");
  }
}

void LogTime()
{
  char date[20];
  ConvertEpoch(GetUnixtime(), date);
  LOG("%s\n\r", date);
}

void WriteLastDateFileHandler()
{
  WriteLastDateFile();

  char date[20];
  ConvertEpoch(GetUnixtime(), date);
  LOG("%s\n\r", date);

  int64_t delay = GJ_CONF_INT32_VALUE(time_writeinterval) * 1000 * 1000;
  GJEventManager->DelayAdd(WriteLastDateFileHandler, delay);
}


static void printTime()
{
  char date[20];
  ConvertEpoch(GetUnixtime(), date);
  SER("%s\n\r", date);
};

static void printUnixtime()
{
  SER("%d\n\r", GetUnixtime());
};

#if defined(NRF)
  static timeval s_baseUnixtime = {};
  static uint64_t s_baseMillis = 0; 
  int GJ_set_time_of_day(const struct timeval *tp)
  {
    s_baseUnixtime = *tp;
    s_baseMillis = GetElapsedMillis();

    return 0;
  }

  int GJ_get_time_of_day(struct timeval *tp)
  {
    uint64_t millis = (GetElapsedMillis() - s_baseMillis);
    *tp = s_baseUnixtime;
    tp->tv_sec += millis / 1000;
    tp->tv_usec += (millis % 1000) * 1000;

    return 0;
  }

#define RTC1_IRQ_PRI            APP_IRQ_PRIORITY_LOWEST                        /**< Priority of the RTC1 interrupt (used for checking for timeouts and executing timeout handlers). */
#define APP_TIMER_PRESCALER             0
static void rtc1_init(void)
{
    //stop RTC1 first
    NVIC_DisableIRQ(RTC1_IRQn);

    NRF_RTC1->EVTENCLR = RTC_EVTEN_COMPARE0_Msk;
    NRF_RTC1->INTENCLR = RTC_INTENSET_COMPARE0_Msk;

    NRF_RTC1->TASKS_STOP = 1;
    nrf_delay_us(47);

    NRF_RTC1->TASKS_CLEAR = 1;

    nrf_delay_us(47);

    //start RTC1
    NRF_RTC1->PRESCALER = APP_TIMER_PRESCALER;
    NVIC_SetPriority(RTC1_IRQn, RTC1_IRQ_PRI);

    NRF_RTC1->EVTENSET = RTC_EVTEN_COMPARE0_Msk;
    NRF_RTC1->INTENSET = RTC_INTENSET_COMPARE0_Msk;

    NVIC_ClearPendingIRQ(RTC1_IRQn);
    NVIC_EnableIRQ(RTC1_IRQn);

    NRF_RTC1->TASKS_START = 1;
    nrf_delay_us(47);
}
#endif 

DEFINE_COMMAND_NO_ARGS(time, printTime);
DEFINE_COMMAND_NO_ARGS(unixtime, printUnixtime);

void InitializeDateTime(OnlineDateUpdater updateFunc)
{ 
  s_onlineTimeUpdateFunc = updateFunc;
  
#if defined(NRF)

  rtc1_init();

  if (__user_set_time_of_day == nullptr)
  {
    __user_set_time_of_day = GJ_set_time_of_day;
    __user_get_time_of_day = GJ_get_time_of_day;
  }
#endif

  /*
  if (isDeepWake)
  {
    //check rtc clock divergence
    //g_clockSync should contain stamps from previous run

    ClockSync clockSync;
    clockSync.m_rtc = rtc_time_get();
    clockSync.m_unixtime = GetUnixtime();

    int32_t const unixElapsed = clockSync.m_unixtime - g_clockSync.m_unixtime;
    int32_t const rtcElapsed = (uint32_t)(clockSync.m_rtc - g_clockSync.m_rtc) / g_rtcCal.rtcFreq;

    SER("Clock divergence\n\r");
    SER("  Sleep stamps unix:%d rtc:%d", g_clockSync.m_unixtime, (uint32_t)g_clockSync.m_rtc);
    SER("  Wake stamps unix:%d rtc:%d\n\r", clockSync.m_unixtime, (uint32_t)clockSync.m_rtc);
    SER("  elapsed unix:%dms rtc:%dms\n\r", unixElapsed, rtcElapsed); 
    SER("  divergence:%dms\n\r", rtcElapsed-unixElapsed);
  }
  */

  CheckRTCMemoryVariable(&g_clockSync, "g_clockSync");
  CheckRTCMemoryVariable(&g_rtcCal, "g_rtcCal");
  CheckRTCMemoryVariable(&lastOnlineUnixtime, "lastOnlineUnixtime");

  if (g_rtcCal.rtcFreq == 0)
    g_rtcCal = InitRTCCal();

  int32_t unixtime = 0;

  if (IsSleepWakeUp())
  {
    if (g_clockSync.m_unixtime == 0)
    {
      LOG("   ERR:PrepareDateTimeSleep not called\n\r");
    }
#ifdef ESP32
    uint64_t const rtc = rtc_time_get();
    int32_t const rtcElapsed = (uint32_t)((rtc - g_clockSync.m_rtc) / g_rtcCal.rtcFreq);

    //SER("  sleep unixtime:%d calculated elapsed:%d\n\r", g_clockSync.m_unixtime, rtcElapsed);
    unixtime = g_clockSync.m_unixtime + rtcElapsed;

    if ((unixtime - lastOnlineUnixtime) > 240 * 60)
    {
      //after a while, reading time online is preferred since RTC drift is about 15 minutes / day
      LOG("WARNING: InitializeDateTime:elapsed time is large, online date is preferred.\n\r");
      isOnlineDatetimeNeeded = true;
    }
#endif
  }
  else
  {
    //unknown date, online time preferred.

    isOnlineDatetimeNeeded = true;

#if defined(GJ_DATE_USE_APPEND_ONLY_FILE)
    {
      AppendOnlyFile file("/lastdate");

      if (file.IsValid())
      {
        auto onDate = [&](uint32_t size, const void *data)
        {
          unixtime = *(uint32_t*)data;
        };

        file.ForEach(onDate);
      }
    }
#else
    {
      GJFile file("/lastdate", GJFile::Mode::Read);
      if (file)
        file.Read(unixtime);
    }
#endif

    if (unixtime != 0)
    {
      SER("File unixtime %d\n\r", unixtime);
    }
    else
    {
      LOG("/lastdate not read\n\r");
    }

    if (unixtime == 0)
    {
#if defined(ESP32)
      std::tm tm = {};

      strptime(__DATE__, "%b %d %Y", &tm);
      std::time_t epo = std::mktime( &tm );
      unixtime = (int32_t)epo;

      LOG("Init unixtime to %d from build date '%s'", unixtime, __DATE__);
#else
      //use some arbitrary unixtime
      unixtime = (int32_t)1676332800; //(2023-02-14T00:00:00) 

      SER("Init unixtime to %d '%s'", unixtime, "(2023-02-14T11:12:23)");
#endif
    }

    unixtime += GetElapsedMillis() / 1000;  //add elapsed time since boot
  }

  bool writeFile(false);
  SetUnixtime(unixtime, writeFile);


  REFERENCE_COMMAND(time);
  REFERENCE_COMMAND(unixtime);

  if (GJEventManager)
  {
    int64_t delay = GJ_CONF_INT32_VALUE(time_writeinterval) * 1000 * 1000;
    GJEventManager->DelayAdd(WriteLastDateFileHandler, delay);
  }
  
}


