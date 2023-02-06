#pragma once

#include "base.h"

class GJLock
{
public:
    GJLock(const char *name = nullptr);
    ~GJLock();

    void Lock();
    void Unlock();

private:
#if defined(ESP32) || defined(ESP_PLATFORM)
    SemaphoreHandle_t m_handle = nullptr;
#elif defined(NRF)
    uint8_t m_nestedCount = 0;
#endif
};

class GJScopedLock
{
public:

    GJScopedLock(GJLock &lock);
    ~GJScopedLock();

private:

    GJLock &m_lock;
};

#define GJ_AUTO_LOCK(lock) GJScopedLock scopedLock(lock)