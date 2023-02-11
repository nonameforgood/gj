#pragma once

#include "base.h"

#if defined(ESP32) || defined(ESP_PLATFORM)
#include <freertos/ringbuf.h>
#endif

template <class T>
class RingBuffer
{
public:
    RingBuffer(uint32_t count = 16);
    ~RingBuffer();
    
    GJ_IRAM bool Add(T const &element);
    GJ_IRAM bool Remove(T &item, uint32_t timeout = 0);

private:

#if defined(ESP32) || defined(ESP_PLATFORM)
  RingbufHandle_t m_handle = {};
#endif
    
};