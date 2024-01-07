#include "esputils.h"
#include "eventmanager.h"

#if defined(ESP32)
  #include <esp_task_wdt.h>
  #include <driver/rtc_io.h>
  #include <rom/rtc.h>
#elif defined(NRF)
  #include <nrf_nvic.h>
  #include <nrf_power.h>
  #include <hardfault.h>
#endif

#include "base.h"
#include "millis.h"
#include "datetime.h"
#include "commands.h"
#include "test.h"
#include "eventlisteners.h"
#include "config.h"


uint32_t spawnTime = 0;
uint32_t runTime = POWERON_RUNTIME;
GJ_PERSISTENT uint32_t minRunTime = 0;
GJ_PERSISTENT int32_t g_sleepTimestamp = 0;
int32_t g_errorTimeout = 30;
//prevent var init when reset is not WAKE
GJ_PERSISTENT_NO_INIT uint32_t g_errorResetTag;

static constexpr uint32_t s_errorResetState1 = 0xBADDBEEF;
static constexpr uint32_t s_errorResetState2 = 0xDEADBEEF;

uint32_t GetElapsedRuntime()
{
  uint32_t elapsed = ( GJMillis() - spawnTime ) / 1000;
  return elapsed;
}

void ResetSpawnTime()
{
  spawnTime = GetElapsedMillis();
}

uint32_t GetRuntime()
{
  return runTime;
}

void SetRuntime(uint32_t value)
{
  runTime = Max(value, minRunTime);
}

int32_t GetSleepTimestamp()
{
  return g_sleepTimestamp;
}
void SetSleepTimestamp()
{
  g_sleepTimestamp = GetUnixtime();
}

int32_t GetSleepDuration()
{
  int32_t sleepTimestamp = GetSleepTimestamp();

  if (sleepTimestamp == 0)
    return 0;

  return GetUnixtime() - sleepTimestamp;
}

bool IsEqualSleepDuration(int32_t duration, int32_t percentPrecision)
{
  int32_t const sleepDuration = GetSleepDuration();
  return IsPeriodOver(duration, sleepDuration, percentPrecision);
}

bool IsPeriodOver(int32_t period, int32_t maxPeriod, int32_t percentPrecision)
{
  int32_t const delta = abs(maxPeriod - period);
  int32_t const threshold = maxPeriod * percentPrecision / 100;
  
  return period >= maxPeriod || delta <= threshold;
}

bool IsSleepWakeUp()
{
#if defined(ESP32)
  esp_sleep_wakeup_cause_t const wakeup_reason = esp_sleep_get_wakeup_cause();
  bool isDeepWake = false;

  if ( wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 ||
       wakeup_reason == ESP_SLEEP_WAKEUP_EXT1 ||
       wakeup_reason == ESP_SLEEP_WAKEUP_TIMER ||
       wakeup_reason == ESP_SLEEP_WAKEUP_TOUCHPAD ||
       wakeup_reason == ESP_SLEEP_WAKEUP_ULP )
       isDeepWake = true;

  return isDeepWake;
#else
  const uint32_t resetReason = GetResetReason();
  const uint32_t wakeMask = NRF_POWER_RESETREAS_OFF_MASK |
                            NRF_POWER_RESETREAS_LPCOMP_MASK |
                            NRF_POWER_RESETREAS_DIF_MASK;
  return (resetReason & wakeMask) != 0;
#endif
}

bool IsSleepEXTWakeUp()
{
#if defined(ESP32)
  esp_sleep_wakeup_cause_t const wakeup_reason = esp_sleep_get_wakeup_cause();
  bool ret = false;

  if ( wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 ||
       wakeup_reason == ESP_SLEEP_WAKEUP_EXT1 )
       ret = true;

  return ret;
#else
  return false;
#endif
}

bool IsSleepTimerWakeUp()
{
#if defined(ESP32)
  esp_sleep_wakeup_cause_t const wakeup_reason = esp_sleep_get_wakeup_cause();
  bool ret = false;

  if ( wakeup_reason == ESP_SLEEP_WAKEUP_TIMER )
       ret = true;

  return ret;
#else
  return false;
#endif
}

bool IsSleepUlpWakeUp()
{
#if defined(ESP32)
  esp_sleep_wakeup_cause_t const wakeup_reason = esp_sleep_get_wakeup_cause();
  bool ret = false;

  if ( wakeup_reason == ESP_SLEEP_WAKEUP_ULP )
       ret = true;

  return ret;
#else
  return false;
#endif
}

bool ClearResetReason()
{
#if defined(NRF)
  //this call will crash if called after the soft device is enabled
  nrf_power_resetreas_clear(0xffffffff);  //clear register
#endif

  return true;
}

uint32_t GetResetReason()
{
#if defined(ESP32)
  return (uint32_t)esp_reset_reason();
#elif defined(NRF)
  static const uint32_t reason = nrf_power_resetreas_get();
  static const bool reasonReset = ClearResetReason();
  return reason;
#else
  return 0;
#endif
}

#if defined(ESP32)
const char *GetResetReasonDesc(esp_reset_reason_t resetReason)
{
  const char *resetReasons[] = {
    "Reset reason can not be determined." ,                     //ESP_RST_UNKNOWN,  0
    "Reset due to power-on event.",                             //ESP_RST_POWERON,  1
    "Reset by external pin (not applicable for ESP32)",         //ESP_RST_EXT,      2
    "Software reset via esp_restart.",                          //ESP_RST_SW,       3
    "Software reset due to exception/panic.",                   //ESP_RST_PANIC,    4
    "Reset (software or hardware) due to interrupt watchdog.",  //ESP_RST_INT_WDT,  5
    "Reset due to task watchdog.",                              //ESP_RST_TASK_WDT, 6
    "Reset due to other watchdogs.",                            //ESP_RST_WDT,      7
    "Reset after exiting deep sleep mode.",                     //ESP_RST_DEEPSLEEP 8
    "Brownout reset (software or hardware)",                    //ESP_RST_BROWNOUT, 9
    "Reset over SDIO."  };                                      //ESP_RST_SDIO,     10

  return resetReasons[resetReason];
}


const char *GetShortResetReasonDesc(esp_reset_reason_t resetReason)
{
  const char *resetReasons[] = {
    "UNKNOWN",
    "POWERON",
    "EXT",
    "SW",
    "PANIC",
    "INT_WDT",
    "TASK_WDT",
    "WDT",
    "DEEPSLEEP",
    "BROWNOUT",
    "SDIO" };

  return resetReasons[resetReason];
}

const char *GetWakeReasonUpToName(esp_sleep_wakeup_cause_t wakeup_reason)
{
  const char *names[] = {
    "UNDEFINED"  ,    //!< In case of deep sleep, reset was not caused by exit from deep sleep
    "ALL",          //!< Not a wakeup cause, used to disable all wakeup sources with esp_sleep_disable_wakeup_source
    "EXT0",         //!< Wakeup caused by external signal using RTC_IO
    "EXT1",         //!< Wakeup caused by external signal using RTC_CNTL
    "TIMER",        //!< Wakeup caused by timer
    "TOUCHPAD",     //!< Wakeup caused by touchpad
    "ULP",          //!< Wakeup caused by ULP program
    "GPIO",         //!< Wakeup caused by GPIO (light sleep only)
    "UART" };  

  return names[wakeup_reason];
}
#elif defined(NRF)


const char *GetResetReasonDesc(uint32_t reason)
{
  if (reason & NRF_POWER_RESETREAS_RESETPIN_MASK)
    return "Reset pin";
  else if (reason & NRF_POWER_RESETREAS_DOG_MASK)
    return "Watch dog";
  else if (reason & NRF_POWER_RESETREAS_SREQ_MASK)
    return "Soft request";
  else if (reason & NRF_POWER_RESETREAS_LOCKUP_MASK)
    return "Lockup";
  else if (reason & NRF_POWER_RESETREAS_OFF_MASK)
    return "Off wake";
  else if (reason & NRF_POWER_RESETREAS_LPCOMP_MASK)
    return "LDCOMP";
  else if (reason & NRF_POWER_RESETREAS_DIF_MASK)
    return "Dif";
#if defined(POWER_RESETREAS_NFC_Msk)
  else if (reason & NRF_POWER_RESETREAS_NFC_MASK)
    return "NFC";
#endif
#if defined(POWER_RESETREAS_VBUS_Msk)
  else if (reason & NRF_POWER_RESETREAS_VBUS_MASK)
    return "VBUS";
#endif
  else if (reason != 0)
    return "Other";
  else
    return "Power on";
}

#endif

bool IsErrorResetState1()
{
  return g_errorResetTag == s_errorResetState1;
}

bool IsErrorResetState2()
{
  return g_errorResetTag == s_errorResetState2;
}

void ClearErrorResetTag()
{
  g_errorResetTag = 0;
}

void SetErrorResetTag()
{
  //unknown -> 0xBADDBEEF -> 0xDEADBEEF
  if (g_errorResetTag == s_errorResetState1)
    g_errorResetTag = s_errorResetState2;
  else
    g_errorResetTag = s_errorResetState1;
}

void PrintWakeupReason()
{
#if defined(ESP32)
  esp_sleep_wakeup_cause_t const wakeup_reason = esp_sleep_get_wakeup_cause();
  uint64_t const wakePins = esp_sleep_get_ext1_wakeup_status();

  TESTSTEP("Wake up reason:%s", GetWakeReasonUpToName(wakeup_reason));

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : LOG("Wakeup caused by external signal using RTC_IO\n\r"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : LOG("Wakeup caused by external signal using RTC_CNTL mask:0x%08x%08x\n\r", (uint32_t)(wakePins >> 32), (uint32_t)wakePins); break;
    case ESP_SLEEP_WAKEUP_TIMER : LOG("Wakeup caused by timer\n\r"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : LOG("Wakeup caused by touchpad\n\r"); break;
    case ESP_SLEEP_WAKEUP_ULP : LOG("Wakeup caused by ULP program\n\r"); break;
    default : LOG("Wakeup was not caused by deep sleep: %d\n\r", (int)wakeup_reason); break;
  }
#endif
}

void PrintShortWakeupReason()
{
#if defined(ESP32)
  esp_sleep_wakeup_cause_t const wakeup_reason = esp_sleep_get_wakeup_cause();

  TESTSTEP("Wake up reason:%s", GetWakeReasonUpToName(wakeup_reason));

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : LOG("Wakeup:EXT0\n\r"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : LOG("Wakeup:EXT1\n\r"); break;
    case ESP_SLEEP_WAKEUP_TIMER : LOG("Wakeup:TIMER\n\r"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : LOG("Wakeup:TCHPAD\n\r"); break;
    case ESP_SLEEP_WAKEUP_ULP : LOG("Wakeup:ULP\n\r"); break;
    default : LOG("Wakeup:%d\n\r", (int)wakeup_reason); break;
  }
#endif
}

void PrintResetReason()
{
#if defined(ESP32)
  esp_reset_reason_t resetReason = esp_reset_reason();
  LOG("Reset reason: %s(%d)\n\r", GetResetReasonDesc(resetReason), (int)resetReason);
  TESTSTEP("Reset reason: %s(%d)\n\r", GetResetReasonDesc(resetReason), (int)resetReason);
#elif defined(NRF)
  const uint32_t resetReason = GetResetReason();
  SER("Reset reason: %s(%d)\n\r", GetResetReasonDesc(resetReason), resetReason);
#endif
}

void PrintShortResetReason()
{
#if defined(ESP32)
  esp_reset_reason_t resetReason = esp_reset_reason();
  LOG("Reset: %s(%d)\n\r", GetShortResetReasonDesc(resetReason), (int)resetReason);
  TESTSTEP("Reset: %s(%d)\n\r", GetResetReasonDesc(resetReason), (int)resetReason);
#endif
}

void PrintBootReason()
{
#if defined(ESP32)
  esp_reset_reason_t const resetReason = esp_reset_reason();

  if (resetReason == ESP_RST_DEEPSLEEP)
  {
    PrintWakeupReason();
  }
  else
  {
    PrintResetReason();
  }
  
  GJ_ERROR_COND(IsErrorReset() && IsErrorResetState2(), "Timeout of 30 seconds was applied following an error reset\n\r");
#endif
}

void PrintShortBootReason()
{
#if defined(ESP32)
  esp_reset_reason_t const resetReason = esp_reset_reason();

  if (resetReason == ESP_RST_DEEPSLEEP)
  {
    PrintShortWakeupReason();
  }
  else
  {
    PrintShortResetReason();
  }
  
  GJ_ERROR_COND(IsErrorReset() && IsErrorResetState2(),"ERROR RESET:%ds timeout was applied\n\r", g_errorTimeout);
#endif
}

void SetResetTimeout(uint32_t seconds)
{
  g_errorTimeout = seconds;
}

GJ_PERSISTENT_NO_INIT static SoftResetReason s_softResetReason;
SoftResetReason GetSoftResetReason()
{
  static const SoftResetReason reason = s_softResetReason;
  s_softResetReason = SoftResetReason::None;    //otherwise debug session get previous value
  return reason;
}

static void SetSoftResetReason(SoftResetReason reason)
{
  s_softResetReason = reason;
}

//this is not initialized on purpose,
//It must retain its value after a crash reset
GJ_PERSISTENT_NO_INIT static CrashData s_crashData;

CrashData GetCrashData()
{
  return s_crashData;
}

#if defined(NRF)

void HardFault_process(HardFault_stack_t * p_stack)
{
  s_crashData.address = p_stack->pc;
  s_crashData.returnAddress = p_stack->lr;

  SetSoftResetReason(SoftResetReason::HardFault);
  // Restart the system by default
  NVIC_SystemReset();
}

void CallAppErrorFaultHandler(uint32_t errCode, uint32_t pc, uint32_t lr)
{
  s_crashData.address = pc;
  s_crashData.returnAddress = lr;

  SetSoftResetReason(SoftResetReason::AppError);

  APP_ERROR_HANDLER(errCode);
}

void Command_CrashData(const char *command)
{
  SER("Last crash: pc=0x%x ret=0x%x\n\r", s_crashData.address, s_crashData.returnAddress);
}

DEFINE_COMMAND_ARGS(crashdata, Command_CrashData);

void InitCrashDataCommand()
{
  REFERENCE_COMMAND(crashdata);
}

#elif defined(ESP32)

void InitCrashDataCommand()
{
  
}

#endif //defined(NRF)

bool IsErrorReset()
{
#if defined(ESP32)
  esp_reset_reason_t const resetReason = esp_reset_reason();

  return resetReason == ESP_RST_BROWNOUT ||
         resetReason == ESP_RST_PANIC ||
         resetReason == ESP_RST_INT_WDT ||
         resetReason == ESP_RST_TASK_WDT ||
         resetReason == ESP_RST_WDT;
#else
  const uint32_t reason = GetResetReason();
  const uint32_t errorMask = NRF_POWER_RESETREAS_DOG_MASK |
                             NRF_POWER_RESETREAS_LOCKUP_MASK;
  const bool isResetError = (reason & errorMask) != 0;
  const bool isSoftResetError = (reason == NRF_POWER_RESETREAS_SREQ_MASK) && 
                                ((GetSoftResetReason() == SoftResetReason::HardFault) || (GetSoftResetReason() == SoftResetReason::AppError));
  return isResetError || isSoftResetError;
#endif
}

bool IsPowerOnReset()
{
#if defined(ESP32)
  esp_reset_reason_t const resetReason = esp_reset_reason();

  //RTCWDT_RTC_RESET happens when flash chip accessed too quickly by esp32 chip

  return resetReason == ESP_RST_POWERON ||
         resetReason == (esp_reset_reason_t)RTCWDT_RTC_RESET;
#else
  const uint32_t resetReason = GetResetReason();
  return resetReason == 0;
#endif
}

void HandleResetError()
{
  //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //to disable brownout detector   

  if (IsErrorReset())
  {
    //don't timeout on the first error reset
    //Sometimes the rom is unable to read at offset 0x1000 on first boot.
    //But it works after the second reset (RTCWDT_RTC_RESET)
    SetErrorResetTag();

    if (IsErrorResetState2())
    {
      //on error, stall for 30 seconds to prevent infinite rapid error loops
      for (int i = 0 ; i < g_errorTimeout ; ++i)
      {
        Delay(1000);
      #if defined(ESP32)
        esp_task_wdt_reset();
      #endif
      }
    }
  }
  else
  {
    ClearErrorResetTag();
  }
}

Vector<GJString> Tokenize2(const char *userString, char token)
{
  GJString string(userString);

  uint32_t len = string.size();
  int32_t lastToken = -1;
  int last = 0;
  
  Vector<GJString> tokens;

  for (int32_t i = 0 ; i < len ; ++i)
  {
    if (string[i] == token)
    {
      lastToken = i;

      if (lastToken != -1 && last != i)
      {
        tokens.push_back(string.substring(last, i));
      }
    }
    else if (string[i] != token && lastToken != -1)
    {
      last = i;
      lastToken = -1;
    }
  }

  if (last != len)
  {
    tokens.push_back( string.substring(last, len));
  }

  return tokens;
}


Vector<GJString> Tokenize(const char *string, char token)
{
  uint32_t len = strlen(string);
  int32_t lastToken = -1;
  int last = 0;
  
  Vector<GJString> tokens;

  for (int32_t i = 0 ; i < len ; ++i)
  {
    if (string[i] == token)
    {
      lastToken = i;

      if (lastToken != -1 && last != i)
      {
        tokens.push_back(GJString(string + last, i - last));
      }
    }
    else if (string[i] != token && lastToken != -1)
    {
      last = i;
      lastToken = -1;
    }
  }

  if (last != len)
  {
    tokens.push_back( GJString(string + last, len - last));
  }

  return tokens;
}

void EraseChar(GJString &str, char c)
{
  for (int i = 0 ; i < str.size() ; )
  {
    if (str[i] == c)
    {
      str.remove(i, 1);
    }
    else
    {
      ++i;
    }
  }
}

const char* RemoveNewLineLineFeed(const char *input, GJString &storage)
{
  if (input[0] == 0)
    return input;

  const char *it = input;
  
  storage.clear();
  
  while(it[1])
  {
    char cur = it[0];
    char next = it[1];
    
    if (cur == '\n' && next == '\r')
    {
      storage.concat(input);
      break;
    }
    
    ++it;
  }
  
  if (storage.empty())
  {
    return input;
  }
  
  //adjust needed
  char *it2 = &storage[0] + (it - input);
  
  while(it2[1])
  {
    char cur = it2[0];
    char next = it2[1];
    
    if (cur == '\n' && next == '\r')
    {
      it2[0] = ' '; //faster than moving entire string
      it2[1] = '\n';
    }
    
    ++it2;
  }
  
  return storage.c_str();
}

char* ReplaceLFCR(char *input, uint32_t len, char replace)
{
  char *it2 = input;

  if (len < 2)
    return input;

  len--;
  
  while(it2[1] && len)
  {
    if (it2[0] == '\n' && it2[1] == '\r')
    {
      it2[0] = replace; //faster than moving entire string
      it2[1] = '\n';
    }
    else if (it2[0] == '\r' && it2[1] == '\n')
    {
      it2[0] = replace; //faster than moving entire string
    }

    ++it2;
    --len;
  }

  return input;
}

char* ReplaceNonPrint(char *input, uint32_t len, char replace)
{
  char *it2 = input;
  
  while(it2[0] && len)
  {
    if (!isprint(it2[0]) && it2[0] != '\n' && it2[0] != '\r')
      it2[0] = replace; //faster than moving entire string
    
    ++it2;
    --len;
  }

  return input;
}

void SetCPUFreq(uint32_t freq)
{
#if defined(ESP32)

  //esp_pm_config_esp32_t pmConfig;

  //pmConfig.max_freq_mhz = 80;
  //pmConfig.min_freq_mhz = 80;
  //pmConfig.light_sleep_enable = false;
  //LOG_ON_ERROR_AUTO(esp_pm_configure(&pmConfig));

  setCpuFrequencyMhz(freq);

  LOG("CPU freq set to %dMhz\n\r", getCpuFrequencyMhz());
#endif
}

uint32_t GetCPUFreq()
{
#if defined(ESP32)
  return getCpuFrequencyMhz();
#else
  return 0;
#endif
}

void DigitalWriteHold(int32_t pin, int32_t value)
{
#if defined(ESP32)
  rtc_gpio_hold_dis((gpio_num_t)pin);
  digitalWrite(pin, value); //cut power to servo
  rtc_gpio_hold_en((gpio_num_t)pin);
#endif
}

void Command_MinRuntimeOn() 
{
  minRunTime = 5;
  SER("Minruntime 5s\n\r");
} 

void Command_MinRuntimeOff() 
{
  minRunTime = 0;
  SER("Minruntime 0s\n\r");
}

void Command_SetRuntime(const char *command) 
{
  CommandInfo info;
  GetCommandInfo(command, info);

  if (info.m_argCount == 1)
  {
    uint32_t runtime = atoi(info.m_argsBegin[0]);
    SetRuntime(runtime);
    SER("Runtime %ds\n\r", runtime);
  }
  else
  {
    SER("arg err\n\r");
  }
}

void Command_ResetSpawnTime() 
{
  ResetSpawnTime();
  SER("Spawn time reset\n\r");
}

EventListeners<ExitCallback> s_exitEvent("Exit");

void RegisterExitCallback(ExitCallback cb)
{
  //SER("RegisterExitCallback\n");
  s_exitEvent.AddListener(cb);
}

bool CheckRTCMemoryVariable(void const *address, const char *name)
{
#if defined(ESP32)
  if ((uint32_t)address < (uint32_t)0x50000800)
  {
    LOG("WARNING:RTC slow memory variable '%s' address < 0x50000800,\n\r"
        "Is ld script reserving memory for the ulp program?\n\r", name);
    return false;
  }
#endif
  return true;
}

void Reboot() 
{
  s_exitEvent.CallListeners();
#if defined(ESP32)
  ON_SER(Serial.flush());
  TermLog();  //flush pending writes
  //sleep(100);

  esp_restart();
#elif defined(NRF)
  SetSoftResetReason(SoftResetReason::Reboot);
  sd_nvic_SystemReset();
#endif
}

void Command_Reboot() 
{
  SER("Rebooting...\n\r");

  Reboot();
}

void Command_ReadPin(const CommandInfo &info)
{
  if (info.m_argCount < 1)
  {
    SER("pin read <index>\n\r");
    return;
  }

  uint32_t pinIndex = strtol(info.m_argsBegin[0], nullptr, 0);
  const int32_t pinValue = ReadPin(pinIndex);

  SER("Pin %02d value=%d\n\r", pinIndex, pinValue);
}

void Command_SetupPin(const CommandInfo &info)
{
  if (info.m_argCount < 2)
  {
    SER("pin setup <index> <isInput> [pull]\n\r");
    return;
  }

  uint32_t pinIndex = strtol(info.m_argsBegin[0], nullptr, 0);
  const bool input = strtol(info.m_argsBegin[1], nullptr, 0) != 0;

  int32_t pull = 0;
  if (info.m_argCount > 2)
    pull = strtol(info.m_argsBegin[2], nullptr, 0);

  SetupPin(pinIndex, input, pull);

  SER("Pin %02d set to %s pull=%d\n\r", pinIndex, input ? "input" : "output", pull);
}

void Command_WritePin(const CommandInfo &info)
{
  if (info.m_argCount < 1)
  {
    SER("pin write <index> <value>\n\r");
    return;
  }

  const GJString pinIndexString(info.m_argsBegin[0], info.m_argsLength[0]);
  const uint32_t pinIndex = atoi(pinIndexString.c_str());

  uint32_t pinValue = 1;

  if (info.m_argCount >= 2)
  {
    const GJString pinValueString(info.m_argsBegin[1], info.m_argsLength[1]);
    pinValue = atoi(pinValueString.c_str());
  }  

  bool input = false;
  int32_t pull = 0;
  SetupPin(pinIndex, input, pull);
  WritePin(pinIndex, pinValue);

  SER("Pin %02d set to value %d\n\r", pinIndex, pinValue);
}

void ReadAllPins(uint32_t index)
{
  //for ( ; index < GJ_PIN_COUNT ; ++index)
  
    int32_t nextIndex = index;

    if (AreTerminalsReady())
    {
      int32_t pinValue = ReadPin(index);
      SER("Pin %02d value=%d\n\r", index, pinValue);

      nextIndex++;

    }

  if (nextIndex < GJ_PIN_COUNT)
  {
    EventManager::Function fct;

    fct = std::bind(ReadAllPins, nextIndex);
    GJEventManager->DelayAdd(fct, 10000);
  }
}

void Command_ReadAllPins(const CommandInfo &info)
{
  ReadAllPins(0);
}

void Command_Pin(const char *command)
{
  static constexpr const char * const s_argsName[] = {
    "read",
    "write",
    "setup",
    "readall"
  };

  static void (*const s_argsFuncs[])(const CommandInfo &commandInfo){
    Command_ReadPin,
    Command_WritePin,
    Command_SetupPin,
    Command_ReadAllPins
    };

  const SubCommands subCommands = {4, s_argsName, s_argsFuncs};

  SubCommandForwarder(command, subCommands);
}

uint32_t minAvailRam(0xffffffff);
uint32_t trackedAllocations(0);

uint32_t GetAvailableRam()
{
  uint32_t avail = 0;
#if defined(ESP32)
  avail = heap_caps_get_free_size(MALLOC_CAP_8BIT);
#elif defined(NRF)
  extern uint32_t __heap_start__;
  avail = (&__heap_start__)[1];
#endif

  return avail;
}

void LogRam()
{
  LOG("RAM avail:%d bytes (min:%d bytes, tracked:%d bytes)\n\r", GetAvailableRam(), minAvailRam, trackedAllocations);
}

static void Command_Ram()
{
  LogRam();
}

DEFINE_COMMAND_NO_ARGS(minruntimeon,Command_MinRuntimeOn ); 
DEFINE_COMMAND_NO_ARGS(minruntimeoff,Command_MinRuntimeOff); 
DEFINE_COMMAND_ARGS   (setruntime,Command_SetRuntime); 
DEFINE_COMMAND_NO_ARGS(resetspawntime,Command_ResetSpawnTime); 
DEFINE_COMMAND_NO_ARGS(reboot,Command_Reboot); 
DEFINE_COMMAND_ARGS   (pin,Command_Pin); 
DEFINE_COMMAND_NO_ARGS(ram,Command_Ram); 

DEFINE_CONFIG_INT32(cpufreq, cpufreq, 1);

void InitESPUtils()
{
#if !defined(NRF)
  REFERENCE_COMMAND(minruntimeon ); 
  REFERENCE_COMMAND(minruntimeoff);  
  REFERENCE_COMMAND(setruntime); 
  REFERENCE_COMMAND(resetspawntime); 
#endif
  REFERENCE_COMMAND(reboot); 
  REFERENCE_COMMAND(pin); 
  REFERENCE_COMMAND(ram); 

  LOG_COND(!IsSleepWakeUp(), "RAM avail:%d bytes\n\r", GetAvailableRam());

  if (GJ_CONF_INT32_VALUE(cpufreq) == 1)
  {
    GJ_CONF_INT32_VALUE(cpufreq) = GetCPUFreq();
  }

  SetCPUFreq(GJ_CONF_INT32_VALUE(cpufreq));
}