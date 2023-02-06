#include "gjlock.h"

#if defined(NRF)
    #include <nrf_nvic.h>
#endif

GJLock::GJLock(const char *name)
{
#if defined(ESP32) || defined(ESP_PLATFORM)
    m_handle = xSemaphoreCreateMutex();
#endif
}

GJLock::~GJLock()
{
#if defined(ESP32) || defined(ESP_PLATFORM)
    vSemaphoreDelete(m_handle);
#endif
}

void GJLock::Lock()
{
#if defined(ESP32) || defined(ESP_PLATFORM)
    xSemaphoreTake(m_handle, portMAX_DELAY);
#elif defined(NRF)
    uint8_t isNested = 0;
    sd_nvic_critical_region_enter(&isNested);
    m_nestedCount++;
#endif
}

void GJLock::Unlock()
{
#if defined(ESP32) || defined(ESP_PLATFORM)
    xSemaphoreGive(m_handle);
#elif defined(NRF)
    uint8_t isNested = m_nestedCount > 1;
    m_nestedCount--;
    sd_nvic_critical_region_exit(isNested);
#endif
}

GJScopedLock::GJScopedLock(GJLock &lock)
: m_lock(lock)
{
    lock.Lock();
}

GJScopedLock::~GJScopedLock()
{
    m_lock.Unlock();
}