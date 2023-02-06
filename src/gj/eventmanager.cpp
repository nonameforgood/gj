#include "eventmanager.h"
#include "commands.h"
#include "config.h"
#include "datetime.h"

#define EM_USE_APP_TIMER 

//#define EVENTMANAGER_DBG

#if defined(EVENTMANAGER_DBG)
  #define EM_SER(...) if (GJ_CONF_BOOL_VALUE(eventmgr_dbg) == true) {  GJ_DBG_PRINT(__VA_ARGS__); }
  DEFINE_CONFIG_BOOL(eventmgr.dbg, eventmgr_dbg, true);
#else
  #define EM_SER(...)
#endif

void AddDebugEvent(uint32_t index, uint64_t expectedDelay)
{
  uint64_t begin = GetElapsedMicros();
  
  auto delayedEvent = [=]()
  {
      uint64_t delay = GetElapsedMicros() - begin;
      GJ_DBG_PRINT("Delay event %d fired, delay:%d (expected:%d)\n\r", index, (uint32_t)delay, (uint32_t)expectedDelay);
  };

  EventManager::Function f(delayedEvent);
  GJEventManager->DelayAdd(f, expectedDelay);
}

void Command_TestDelayEvent()
{
  uint32_t index = 1;
  AddDebugEvent(index++, 4000000);
  AddDebugEvent(index++, 1000000);
  AddDebugEvent(index++, 1000000);
  AddDebugEvent(index++, 30000000);
  AddDebugEvent(index++, 31000000);
  AddDebugEvent(index++, 1);
}

DEFINE_COMMAND_NO_ARGS(testdelayevent, Command_TestDelayEvent);

#if defined(ESP32)
  #include "ringbuffer.hpp"
#elif defined(NRF) && defined(EM_USE_APP_TIMER)
  #include <app_scheduler.h>
  #include <app_timer.h>

  APP_TIMER_DEF(EM_Timer);
#elif defined(NRF)
  #include <app_scheduler.h>
  #include "nrf_drv_timer.h"
#endif

class GJTimer
{
public:
    typedef std::function<void()> Callback;

    GJTimer(Callback cb);
    ~GJTimer();

    void Set(uint64_t delayUS);

private:
#if defined(ESP32)
    esp_timer_handle_t m_handle = nullptr;
    #define GJ_TIMER_CALLBACK_ARGS void*t
#elif defined(NRF) && defined(EM_USE_APP_TIMER)
     #define GJ_TIMER_CALLBACK_ARGS void * t
#elif defined(NRF)
    const nrf_drv_timer_t m_handle = NRF_DRV_TIMER_INSTANCE(1);
    #define GJ_TIMER_CALLBACK_ARGS nrf_timer_event_t event_type, void*t
#endif
    Callback m_cb;

    static void InternalCallback(GJ_TIMER_CALLBACK_ARGS);
};

GJTimer::GJTimer(Callback cb)
{
#if defined(ESP32)
    esp_timer_create_args_t createArgs;
    createArgs.callback = InternalCallback;
    createArgs.arg = this;
    createArgs.dispatch_method = ESP_TIMER_TASK;
    createArgs.name = "GJTimer";
    createArgs.skip_unhandled_events = true;
    
    esp_err_t ret = esp_timer_create(&createArgs, &m_handle);
    LOG_ON_ERROR(ret, "Cannot create esp timer, err:%d\n\r", ret);
#elif defined(NRF)&& defined(EM_USE_APP_TIMER)

    app_timer_create(&EM_Timer, APP_TIMER_MODE_SINGLE_SHOT, InternalCallback);

#elif defined(NRF)

  #if defined(TIMER1_ENABLED) && TIMER1_ENABLED != 0
      nrf_drv_timer_config_t timer_cfg = NRF_DRV_TIMER_DEFAULT_CONFIG;
      timer_cfg.mode = NRF_TIMER_MODE_TIMER;
      timer_cfg.frequency = NRF_TIMER_FREQ_31250Hz;
      timer_cfg.bit_width = NRF_TIMER_BIT_WIDTH_16;   //TIMER1 is 16 bit max
      timer_cfg.p_context = this;
      int err_code = nrf_drv_timer_init(&m_handle, &timer_cfg, InternalCallback);
      APP_ERROR_CHECK(err_code);
  #endif
#endif

    m_cb = cb;
}

GJTimer::~GJTimer()
{
#if defined(ESP32)
    esp_timer_delete(m_handle);
#elif defined(NRF) && defined(EM_USE_APP_TIMER)
#elif defined(NRF) 
    nrf_drv_timer_uninit(&m_handle);
#endif //defined(ESP32)
}

void GJTimer::Set(uint64_t delayUS)
{
#if defined(ESP32)
    esp_timer_stop(m_handle);
    esp_err_t ret = esp_timer_start_once(m_handle, delayUS);
    LOG_ON_ERROR(ret, "Cannot start esp timer once, err:%d\n\r", ret);
#elif defined(NRF) && defined(EM_USE_APP_TIMER)

    app_timer_stop(EM_Timer);

    const uint64_t maxDelayUS = 500 * 1000 * 1000; //rtc1 timer max is 512 seconds
    delayUS = Min(delayUS, maxDelayUS); //timer will reschedule itself

    uint32_t delayMS = Max<uint32_t>(delayUS / 1000, 1);

    uint32_t ticks = APP_TIMER_TICKS(delayMS, 0);

    app_timer_start(EM_Timer, ticks, this);

    EM_SER("EM GJTimer:%dus %dtks\n\r", (uint32_t)delayUS, ticks);

#elif defined(NRF) 

  #if defined(TIMER1_ENABLED) && TIMER1_ENABLED != 0
    nrf_drv_timer_disable(&m_handle);

    uint32_t time_ticks = nrf_drv_timer_us_to_ticks(&m_handle, delayUS);

    EM_SER("EM GJTimer::Set us:%d total ticks:%d\n\r", (uint32_t)delayUS, time_ticks);

    time_ticks = Min<uint32_t>(time_ticks, 65535 * 4);

    EM_SER("EM GJTimer::Set min ticks: %d\n\r", time_ticks);

    time_ticks = 65535 * 2;

#if 1
    bool enableInterrupts = true;
    nrf_drv_timer_extended_compare(
      &m_handle, NRF_TIMER_CC_CHANNEL0, 65534, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, enableInterrupts); 
      nrf_drv_timer_extended_compare(
      &m_handle, NRF_TIMER_CC_CHANNEL1, 65535, NRF_TIMER_SHORT_COMPARE1_CLEAR_MASK, enableInterrupts); 
#else 
    uint32_t step = 0;
    while(time_ticks)
    {
      uint32_t b = Min<uint32_t>(time_ticks, 65535);

      time_ticks -= b;

      nrf_timer_short_mask_t shortMask;

      if (time_ticks)
        shortMask = (nrf_timer_short_mask_t)(NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK + step);
      else
        shortMask = (nrf_timer_short_mask_t)(NRF_TIMER_SHORT_COMPARE0_STOP_MASK + step);

      bool enableInterrupts = time_ticks == 0;
      enableInterrupts = true;
      nrf_drv_timer_extended_compare(
        &m_handle, (nrf_timer_cc_channel_t)(NRF_TIMER_CC_CHANNEL0 + step), b, shortMask, enableInterrupts); 

      EM_SER("EM compare value:%d\n\r", b);

      step++;
    }
#endif
    if (delayUS != 0)
    {
      nrf_drv_timer_enable(&m_handle);
    }
  #endif
#endif
}

void GJTimer::InternalCallback(GJ_TIMER_CALLBACK_ARGS)
{
  GJTimer *timer = (GJTimer*)t;

  #if defined(NRF) && defined(EM_USE_APP_TIMER)
    //EM_SER("GJTimer callback %d\n\r", (uint32_t)GetElapsedMicros());
  #elif defined(NRF) 
    EM_SER("GJTimer callback %d %d\n\r", (uint32_t)event_type, (uint32_t)GetElapsedMicros());
  #endif

  timer->m_cb();
}

EventManager *GJEventManager = nullptr;

static uint32_t nextId = 1;

struct EventManager::Event
{
    uint32_t m_id = 0;
    uint32_t m_frameId = 0;
    Function m_function;
};

#if defined(NRF)
struct EventManager::NRFEvent
{
  Event *m_event;
};
#endif

void EventManager::Command_dbg()
{
  if (!GJEventManager)
    return;

  SER("Event manager:\n\r");
  SER("  Frame:%d\n\r", GJEventManager->m_frameId);
#if defined(NRF)
  SER("  Pending:%d\n\r", GJEventManager->m_maxEvents - app_sched_queue_space_get());
#endif
  SER("  Skipped:\n\r");
  if (GJEventManager->m_skippedEvent)
  {
    SER("  Id:%d frame:%d\n\r", GJEventManager->m_skippedEvent->m_id, GJEventManager->m_skippedEvent->m_frameId);
  }
  else
  {
    SER("    none\n\r");
  }

  SER("  Delayed events:%d\n\r", GJEventManager->m_delayedEvents.size());
  for (const DelayedEvent &de : GJEventManager->m_delayedEvents)
  {
    uint32_t unixtime = GetUnixtime();
    int64_t elapsedUS = GetElapsedMicros();

    int64_t expireUnix = (de.m_time - elapsedUS) / 1000 / 1000 + unixtime;

    SER("    id:%d Frame:%d Delay:%d Added:%d Expire:%d (unix expire:%d unix:%d)\n\r",
      de.m_event->m_id, de.m_event->m_frameId,
      (uint32_t)de.m_delay, (uint32_t)de.m_addTime, (uint32_t)de.m_time,
      (uint32_t)expireUnix, unixtime);
  }
}


EventManager::EventManager(uint32_t maxEvents)
: m_maxEvents(maxEvents)
#if defined(ESP32)
, m_lock("EventManager")
, m_events(maxEvents)
#endif
{
#if defined(EVENTMANAGER_DBG)
    DEFINE_COMMAND_NO_ARGS(emdbg, Command_dbg);
    REFERENCE_COMMAND(emdbg);

    REFERENCE_COMMAND(testdelayevent);
#endif

#if defined(NRF)
    ms_instance = this;
    uint32_t bufferSize = APP_SCHED_BUF_SIZE((sizeof(NRFEvent)), (maxEvents));
    void *schedulerBuffer = new char[bufferSize];                  
    uint32_t ERR_CODE = app_sched_init((sizeof(NRFEvent)), (maxEvents), schedulerBuffer);             
    APP_ERROR_CHECK(ERR_CODE);   
#endif
}

EventManager::~EventManager()
{

}

bool EventManager::IsIdle() const
{
#if defined(ESP32)
  return false;
#elif defined(NRF)
  uint32_t spaceLeft = app_sched_queue_space_get();
  return spaceLeft == m_maxEvents && !m_skippedEvent;
#endif
  return false;
}

void EventManager::Add(Function const &func)
{
    Event *e = new Event;
    e->m_id = nextId++;
    e->m_frameId = m_frameId;
    e->m_function = func;

    AddEvent(e);

    EM_SER("Event %d added\n\r", e->m_id);
}

void EventManager::AddEvent(Event *e)
{
#if defined(ESP32)
    m_events.Add(e);
#elif defined(NRF)
    NRFEvent event = {e};
    uint32_t err_code;
    err_code = app_sched_event_put(&event, sizeof(NRFEvent), NRFEventHandler);
    APP_ERROR_CHECK(err_code);
#endif
}

void EventManager::DelayAdd(Function const &func, uint64_t delayUS)
{
    DelayedEvent e;
    e.m_delay = delayUS;
    e.m_addTime = GetElapsedMicros();
    e.m_time = GetElapsedMicros() + delayUS;
    e.m_event = new Event;
    e.m_event->m_id = nextId++;
    e.m_event->m_frameId = m_frameId;
    e.m_event->m_function = func;
    
    {
#ifdef ESP32
        GJ_AUTO_LOCK(m_lock);
#endif

        m_delayedEvents.push_back(e);

        
        SetNextTimer();
        EM_SER("EM:delayed event %d added\n\r", e.m_event->m_id);
    }
}

void EventManager::TimerCallback()
{
    //don't modify the timer API while within its callback
    Function f = std::bind(&EventManager::ProcessDelayedEvents, this);
    Add(f);
}

void EventManager::ProcessDelayedEvents()
{
#ifdef ESP32
    GJ_AUTO_LOCK(m_lock);
#endif

    auto it = m_delayedEvents.begin();
    
    for (; it != m_delayedEvents.end() ; )
    {
      auto &e = *it;

      if (GetElapsedMicros() >= e.m_time)
      {
        e.m_event->m_frameId = m_frameId;
        AddEvent(e.m_event);
        EM_SER("Delayed event id %d added, expected:%ds elapsed:%ds\n\r", 
          e.m_event->m_id, (uint32_t)(e.m_time / 1000 / 1000), (uint32_t)(GetElapsedMicros() / 1000 / 1000));
        it = m_delayedEvents.erase(it);
      }
      else
      {
        it++;
      }
    }

    SetNextTimer();
}

void EventManager::SetNextTimer()
{
    int64_t inv = 0x0fffFFFFffffFFFF;
    int64_t next = inv;

    int32_t i = 0;

    for (DelayedEvent &e : m_delayedEvents)
    {
        int64_t delay = (int64_t)e.m_time - (int64_t)GetElapsedMicros();

        if (delay < next)
        {
            next = std::min(next, delay);
        }

        ++i;
    }

    if (next != inv)
    {
        if (next < 0)
            next = 0;

        if (!m_timer)
        {
            GJTimer::Callback f = std::bind(&EventManager::TimerCallback, this);
            m_timer = new GJTimer(f);
        }

        m_timer->Set(next);
    }
}

#if defined(NRF)

EventManager *EventManager::ms_instance = nullptr;

void EventManager::NRFEventHandler(void * p_event_data, uint16_t event_size)
{
  NRFEvent *event = (NRFEvent*)p_event_data;

  if (!ms_instance->HandleEvent(event->m_event))
  {
    //make app scheduler exit its loop and stop processing other events
    app_sched_pause();
  }
}
#endif

bool EventManager::HandleEvent(Event *e)
{
  if (e->m_frameId == m_frameId)
  {
      if(m_skippedEvent)
      {
        GJ_ERROR("Evt Manager already has a skipped event!!\n\r");
      }
      m_skippedEvent = e;
      return false;
  }
  
  e->m_function();
  EM_SER("EventManager:event id %d done\n\r", e->m_id);
  delete e;

  return true;
}

void EventManager::WaitForEvents(uint32_t timeout)
{
    GJ_PROFILE(EventManager::WaitForEvents);

    m_frameId++;

    //events are always processed on the next loop.
    //This allows events to reschedule themselves.
    //Usefull for work spreading algos.
    if (m_skippedEvent)
    {
        Event *e = m_skippedEvent;
        m_skippedEvent = nullptr;
        EM_SER("EventManager:handling skipped event\n\r");   
        HandleEvent(e);
    }

#if defined(ESP32)
    Event *e = nullptr;
    while(m_events.Remove(e, timeout))
    {
        GJ_PROFILE(EventManager::WaitForEvents::Dispatch);

        if (!HandleEvent(e))
          break;
    }
#elif defined(NRF)
    app_sched_execute();
    app_sched_resume(); //in case an event needed to be skipped
#endif    
}

