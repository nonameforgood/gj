#pragma once

#include "base.h"
#include "vector.h"

class GJString;

class GJOTA
{
public:
  GJOTA();

  void Init();
  
  bool HandleMessage(const char *msg, uint32_t size, GJString &response);

private:

  bool Begin(uint32_t size, GJString &response);
  bool End();

  bool BeginPart(uint32_t crc, uint32_t size, GJString &response);
  bool AddData(const void *data, uint32_t size, GJString &response);
  bool EndPart(GJString &response);

  Vector<uint8_t> m_buffer;

  uint32_t m_offset = 0;
  uint32_t m_size = 0;
  uint32_t m_partExpectedCrc = 0;
  uint32_t m_partSize = 0;
  uint32_t m_partOffset = 0;
  uint32_t m_partCrc = 0;
  uint32_t m_progress = 0;
  bool m_sendOptionalResponse = true;
  bool m_waitRestart = false;
  uint32_t m_retryPart = 0xffffffff;

  void Ending(uint32_t step);

  #if defined(NRF)
    uint32_t m_flashOffset = 0;
    uint32_t m_flashOffsetEnd = 0;
  #endif
};
