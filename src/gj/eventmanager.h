#pragma once

#ifdef ESP32
  #include "ringbuffer.h"
#endif

#include "vector.h"
#include "gjlock.h"

class GJTimer;

class EventManager
{
public:

    EventManager(uint32_t maxEvents = 32);
    ~EventManager();

    typedef std::function<void()> Function;

    GJ_IRAM void Add(Function const &func);
    GJ_IRAM void DelayAdd(Function const &func, uint64_t delayUS);
    void WaitForEvents(uint32_t timeout = 0);
    bool IsIdle() const;

private:

    struct Event;

    struct DelayedEvent
    {
        uint64_t m_delay;
        uint64_t m_addTime;
        uint64_t m_time;
        Event *m_event;
    };

#ifdef ESP32
    GJLock m_lock;
    RingBuffer<Event*> m_events;
#elif defined(NRF)
    static EventManager *ms_instance;
    struct NRFEvent;
    static void NRFEventHandler(void * p_event_data, uint16_t event_size);
#endif

    const uint32_t m_maxEvents;    
    Vector<DelayedEvent> m_delayedEvents;
    uint32_t m_frameId = 0;

    Event *m_skippedEvent = nullptr;

    GJTimer *m_timer = nullptr;

    GJ_IRAM void AddEvent(Event *e);
    bool HandleEvent(Event *e);
    void SetNextTimer();
    void TimerCallback();
    void ProcessDelayedEvents();

    static void Command_dbg(); 
};

extern EventManager *GJEventManager;