#include "sleepmanager.h"
#include "countercontainer.h"
#include "base.h"
#include "gjulp_main.h"
#include "counter.h"
#include "esputils.h"
#include "datetime.h"
#include "millis.h"
#include "gjwifi.h"
#include "config.h"

#include "test.h"
#include "commands.h"
#include <esp32/ulp.h>
#include <esp_wifi.h>
//#include <ulptool.h>
#include <driver/adc.h>
#include <esp_bt.h>
#include <soc/rtc_cntl_reg.h>

extern "C"
{
esp_err_t ulptool_load_binary(uint32_t load_addr, const uint8_t* program_binary, size_t program_size);
}

SleepManager *SleepManager::ms_instance = nullptr;

void SleepManager::Command_sleep()
{
SER("Forcing sleep\n\r");
      ms_instance->EnterDeepSleep();
}
void SleepManager::Command_autosleepoff()
{
  ms_instance->EnableAutoSleep(false);
      SER("Auto sleep disabled\n\r");
}
void SleepManager::Command_autosleepon()
{
  ms_instance->EnableAutoSleep(true);
      SER("Auto sleep enabled\n\r");
}

SleepManager::SleepManager(void *rtcMemory, uint32_t size)
: m_sleepMemory(rtcMemory, size)
{
  ms_instance = this;

  DEFINE_COMMAND_NO_ARGS(sleep, Command_sleep);
  DEFINE_COMMAND_NO_ARGS(autosleepoff, Command_autosleepoff);
  DEFINE_COMMAND_NO_ARGS(autosleepon, Command_autosleepon);

  REFERENCE_COMMAND(sleep);
  REFERENCE_COMMAND(autosleepoff);
  REFERENCE_COMMAND(autosleepon);
}

DEFINE_CONFIG_BOOL(sleep.ulp, sleep_ulp, false);

void SleepManager::Init(CounterContainer *counters)
{
  m_counters = counters;
  LoadCounters();

  if (IsSleepEXTWakeUp())
  {
    uint64_t const mask = esp_sleep_get_ext1_wakeup_status();
    SER("Ext1 wakeup status:0x%08x%08x\n\r", (uint32_t)(mask >> 32), (uint32_t)(mask & 0xffffFFFF));
  }
}

void SleepManager::AddOnSleep(TOnSleepCallback callback)
{
  m_onSleep.push_back(callback);
}
  
void SleepManager::LoadCounters() const
{
  auto onCounter = [&](Counter &counter)
  {
    Vector<uint8_t> sleepData;

    bool const read = m_sleepMemory.Load(counter.GetUID(), sleepData);
    
    if (read)
      counter.Load(sleepData.data(), sleepData.size());
  };

  if (m_counters)
    m_counters->ForEachCounter(onCounter);  
}

bool SleepManager::SaveCounters()
{
  m_sleepMemory.Clear();
  
  bool saved = false;
  
  auto onCounter = [&](Counter const &counter)
  {
    Vector<uint8_t> state;
    counter.Save(state);
    
    if (state.size())
    {
      saved = true;
      StoreState(counter.GetUID(), state.data(), state.size());
    }
  };

  if (m_counters)
    m_counters->ForEachCounter(onCounter);  
  
  return saved;
}

void SleepManager::StoreState(uint32_t uid, void const *data, uint32_t size)
{
  if (!m_sleepMemory.Store(uid, data, size))
  {
    LOG("ERROR:Cannot save %d bytes state in RTC memory for UID 0x%x\n\r", size, uid);
    //todo:write to file instead
  }
}

void SleepManager::EnableAutoSleep(bool enable)
{
  m_autoSleep = enable;
}

bool SleepManager::IsAutoSleepEnabled() const
{
  return m_autoSleep;
}

void SleepManager::SetWakeFromTimer(uint32_t seconds)
{
  //take the shortest duration
  m_wakeTimer = std::min(m_wakeTimer, seconds);
}

void SleepManager::SetWakeFromAllLowEXT1(uint64_t gpioMask)
{
  m_allLowEXT1 = gpioMask;
}
void SleepManager::SetWakeFromAnyHighEXT1(uint64_t gpioMask)
{
  m_anyHighEXT1 = gpioMask;
}

void SleepManager::Update()
{
  GJ_PROFILE(SleepManager::Update);
  
  if (m_autoSleep)
  {
    uint32_t elapsed = GetElapsedRuntime();
    if (elapsed >= GetRuntime())
    {
      SER("Runtime of %d seconds elapsed, enabling deep sleep\n\r", GetRuntime());
      EnterDeepSleep();
    }
  }
}
  
void SleepManager::InitUlp()
{
  if (!IsSleepWakeUp())
  {
    esp_err_t err;
    err = ulptool_load_binary(0, ulp_main_bin_start, (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
    SER("Ulp program uploaded:%d bytes\n\r", (ulp_main_bin_end - ulp_main_bin_start));
  }
}

void SleepManager::RunUlp(uint32_t *address)
{
  if (!address)
  {
    GJ_ERROR("SleepManager::RunUlp: no program specified\n\r");
    return;
  }
  
  m_ulpAddress = address;
  m_wakeFromUlp = true;
}

#if 0
void SleepManager::EnterLightSleep()
{
    //goto light sleep for a 1minute when powering to enable modem wake for web access
    
    //esp_err_t err;

    //esp_pm_config_esp32_t pmConfig = {};
    //pmConfig.max_freq_mhz = 160;
    //pmConfig.min_freq_mhz = 80;
    pmConfig.light_sleep_enable = true;

    err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    SER("esp_wifi_set_ps: %s\n\r", esp_err_to_name(err)); 
    SER_COND(err == ESP_OK, "Wifi power safe mode enabled\n\r");

    //err = esp_pm_configure(&pmConfig);
    //SER("esp_pm_configure: %s\n\r", esp_err_to_name(err)); 
    //if (err == ESP_OK)
      //SER("Automatic light sleep enabled\n\r");
    SER("Automatic light sleep enabled\n\r");
    //else
    //  SER("Automatic light sleep failed\n\r");
    
    SetupTimerWake(metricsManager);
    ON_SER(Serial.flush());
    esp_light_sleep_start();
    SER("After light sleep\n\r");

}
#endif

void SleepManager::EnterDeepSleep()
{
  esp_err_t ret;
  
  {
    SleepInfo prepareSleepInfo = {*this, true, m_wakeFromUlp, SleepInfo::EventType::Prepare};
    for(auto &cb : m_onSleep)
    {
      cb(prepareSleepInfo);
    }
  }
  
  //printf("bef IsWifiConnected\n");
  if (!IsWifiConnected())
  {
    //  when time cannot be retrieved online, 
    //  don't deep sleep for too long to prevent time from drifting too much.
    //  Must do periodic RTC clock calibrations.
    //printf("bef online unavailable\n");

    SetWakeFromTimer(60 * 60 * 4); //4 hours
    LOG("Set wake up from timer cause:online unavailable\n\r");

    //printf("aft online unavailable\n");
  }
  
  //btStop();     //adding bt function calls adds 80k of code
  //esp_bt_controller_disable();

  //printf("bef adc_power_off\n");

  //adc_power_release();
  //adc_power_off();

  //printf("aft adc_power_off\n");

  bool const countersSaved = SaveCounters();

  PrepareDateTimeSleep();
  LOG("RT:%ds\n\r", millis() / 1000 );
  
  TESTSTEP("Enter deep sleep");

  LOG("Sleep\n\r");
  SER_COND(countersSaved, "  Counters saved in RTC\n\r");
  
  if (m_wakeTimer != 0xffffffff)
  {
    ret = esp_sleep_enable_timer_wakeup(m_wakeTimer * 1000000ULL);
    LOG_ON_ERROR(ret, "esp_sleep_enable_timer_wakeup:%s\n\r", esp_err_to_name(ret));
    LOG("  Timer:%ds\n\r", m_wakeTimer);
    TESTSTEP("Deep sleep timer enabled");
  }

  if (m_allLowEXT1 != 0)
  {
    if (m_anyHighEXT1)
      LOG("  WARNING:wake from 'any high' (0x%08x%08x) not applied, 'all low' is already used\n\r",
        (uint32_t)(m_anyHighEXT1 >> 32), (uint32_t)(m_anyHighEXT1 & 0xffffffff));

    ret = esp_sleep_enable_ext1_wakeup(m_allLowEXT1,ESP_EXT1_WAKEUP_ALL_LOW); //1 = High, 0 = Low
    LOG_ON_ERROR(ret, "esp_sleep_enable_ext1_wakeup:%s\n\r", esp_err_to_name(ret));
    LOG("  EXT1 ON\n\r");

    TESTSTEP("Deep sleep all low ext1 enabled");
  }
  else if (m_anyHighEXT1 != 0)
  {
    ret = esp_sleep_enable_ext1_wakeup(m_anyHighEXT1,ESP_EXT1_WAKEUP_ANY_HIGH); //1 = High, 0 = Low
    LOG("  Wake up on EXT1 any high(0x%08x%08x) enabled:%s\n\r", 
      (uint32_t)(m_anyHighEXT1 >> 32), (uint32_t)(m_anyHighEXT1 & 0xffffffff),
      esp_err_to_name(ret));

      TESTSTEP("Deep sleep any high ext1 enabled");
  }

  if (m_wakeFromUlp)
  {
    //enable ulp start timer in case it was disabled (is this needed?)
    REG_SET_BIT(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_SLP_TIMER_EN);

    if (ulp_sensor_events)
      ulp_sensor_events->Reset(0);

    esp_err_t ret;
    ret = ulp_run((m_ulpAddress - RTC_SLOW_MEM) / sizeof(uint32_t));
    SER("  Ulp started:%s\n\r", esp_err_to_name(ret));
    
    ret = esp_sleep_enable_ulp_wakeup();
    LOG_ON_ERROR(ret, "esp_sleep_enable_ulp_wakeup:%s\n\r", esp_err_to_name(ret));
    LOG("  ULP on\n\r");

    TESTSTEP("ULP enabled");
  }
  else
  {
    //disable ulp start timer, otherwise 0.4 mAh is consumed when esp32 is asleep and ulp not actively running.
    REG_CLR_BIT(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_SLP_TIMER_EN);
  }
  
  LOG("  Entering...\n\r");
  SetSleepTimestamp();
  ON_SER(Serial.flush());
  TermLog();  //flush pending writes

  {
    SleepInfo prepareSleepInfo = {*this, true, m_wakeFromUlp, SleepInfo::EventType::Enter};
    for(auto &cb : m_onSleep)
    {
      cb(prepareSleepInfo);
    }
  }

  esp_deep_sleep_start();
  
  //this should never be reached
  LOG("  ERROR:Deep sleep failed!\n\r");
}
