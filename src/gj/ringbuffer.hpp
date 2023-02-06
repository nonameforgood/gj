#pragma once

template <class T>
RingBuffer<T>::RingBuffer(uint32_t count)
{
  #if defined(ESP32) || defined(ESP_PLATFORM)
    uint32_t memorySize = sizeof(T) * count;
    m_handle = xRingbufferCreate(memorySize, RINGBUF_TYPE_BYTEBUF);

    GJ_ERROR_COND(!m_handle, "Cannot create RingBuffer of %d bytes\n\r", memorySize);
  #endif
}

template <class T>
RingBuffer<T>::~RingBuffer()
{
  #if defined(ESP32) || defined(ESP_PLATFORM)
    vRingbufferDelete(m_handle);
  #endif
}

template <class T>
bool RingBuffer<T>::Add(T const &element)
{
  #if defined(ESP32) || defined(ESP_PLATFORM)
    GJ_ERROR_COND(!m_handle, "Trying to store element in null RingBuffer\n\r");

    bool ret = xRingbufferSend(m_handle, &element, sizeof(T), 0);

    GJ_ERROR_COND(!ret, "Cannot add element(%d bytes) to RingBuffer\n\r", sizeof(T));

    return ret;
  
  #else
    return false;
  #endif
}

template <class T>
bool RingBuffer<T>::Remove(T &item, uint32_t timeout)
{
  #if defined(ESP32) || defined(ESP_PLATFORM)
    GJ_ERROR_COND(!m_handle, "Trying to store element in null RingBuffer\n\r");

    size_t itemSize = {};
    size_t maxSize = sizeof(T);

    uint32_t tickWait = pdMS_TO_TICKS(timeout);

    void *data = xRingbufferReceiveUpTo(m_handle, &itemSize, tickWait, maxSize);

    if (data != nullptr)
    {
        //SER("RingBuffer received %d bytes\n\r", itemSize);

        if (itemSize == maxSize)
        {
            memcpy(&item, data, maxSize);
        }

        vRingbufferReturnItem(m_handle, data);

        return true;
    }
  #endif
    return false;
}

