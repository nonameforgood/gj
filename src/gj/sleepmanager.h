#pragma once

#include "sleepmemorymanager.h" 
#include "vector.h"
#include "function_ref.hpp"

class CounterContainer;
class SleepManager;

struct SleepInfo
{
  SleepManager &m_sleepManager;
  bool m_deep;
  bool m_runUlp;

  enum class EventType
  {
    Prepare,
    Enter
  };
  EventType m_eventType;
};
  
class SleepManager
{
public:
  SleepManager(void *rtcMemory = nullptr, uint32_t size = 0);

  void Init(CounterContainer *counters);
  void InitUlp();
  
  void Update();
  
  void EnableAutoSleep(bool enable);
  bool IsAutoSleepEnabled() const;
  
  typedef tl::function_ref<void(SleepInfo const &info)> TOnSleepCallback;
  void AddOnSleep(TOnSleepCallback callback);
  
  void SetWakeFromTimer(uint32_t seconds);
  void SetWakeFromAllLowEXT1(uint64_t gpioMask);
  void SetWakeFromAnyHighEXT1(uint64_t gpioMask);
  void RunUlp(uint32_t *address);

  void EnterDeepSleep();
private:

  SleepMemoryManager m_sleepMemory;

  static SleepManager *ms_instance;

  bool m_autoSleep = false;

  CounterContainer *m_counters = nullptr;
  
  bool m_wakeFromUlp = false;
  uint32_t m_wakeTimer = 0xffffffff;
  uint64_t m_allLowEXT1 = 0;
  uint64_t m_anyHighEXT1 = 0;
  uint32_t *m_ulpAddress = nullptr;
  
  void LoadCounters() const;
  bool SaveCounters();
  
  void StoreState(uint32_t uid, void const *data, uint32_t size);
  
  Vector<TOnSleepCallback> m_onSleep;
  static void Command_sleep();
  static void Command_autosleepoff();
  static void Command_autosleepon();

};
