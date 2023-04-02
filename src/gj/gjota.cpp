#include "gjota.h"
#include "string.h"
#include "datetime.h"
#include "config.h"

#if defined(ESP32)
  #include <esp_spi_flash.h>
  #include "update.h"

  #define GJ_OTA_CHUNK_SIZE SPI_FLASH_SEC_SIZE
  #define GJ_OTA_BUFFER_SIZE SPI_FLASH_SEC_SIZE
#elif defined(NRF)
  #include "esputils.h"
  #include "nrf51utils.h"
  #include "eventmanager.h"
  #if defined(NRF51)
    #define GJ_OTA_CHUNK_SIZE 1024
  #elif defined(NRF52)
    #define GJ_OTA_CHUNK_SIZE 4096
  #endif
  #define GJ_OTA_BUFFER_SIZE 32
#endif




//printf("%6d ", (uint32_t)GetElapsedMillis());

GJString CreateOTAErrorResponse(const char *command, const char *fmt, ...)
{
  va_list argptr;
  va_start(argptr, fmt);
  GJString err = FormatStringVA(fmt, argptr);
  va_end(argptr);

  GJ_DBG_PRINT(err.c_str());
  GJ_DBG_PRINT("\n");

  GJString response = FormatString("%s:%s eol", command, err.c_str());

  return response;
}

GJOTA::GJOTA()
{

}

DEFINE_CONFIG_BOOL(ota.dbg, ota_dbg, false);
DEFINE_CONFIG_BOOL(ota.checkpartcrc, ota_checkpartcrc, true);

void OtaSerial(const char *fmt, ...)
{
  if ( GJ_CONF_BOOL_VALUE(ota_dbg) == false)
    return;

  printf("ota %dms ", (uint32_t)GetElapsedMillis());

  va_list argptr;
  va_start(argptr, fmt);
  vprintf(fmt, argptr);
  va_end(argptr);

  printf("\n\r");
}

#define OTA_SER(...) OtaSerial(__VA_ARGS__)

void GJOTA::Init()
{
  #if defined(NRF)
    m_sendOptionalResponse = false;
  #endif
}

bool GJOTA::HandleMessage(const char *msg, uint32_t size, GJString &response)
{
  if (msg[size] != 0)
  { 
    //sscanf and the likes need terminating null chars
    GJ_ERROR("GJOTA ERROR:non-null terminated string\n");
  
    response = "otafail:invalidcmd";
    return false;
  }
  
  auto isOp = [&](const char *r)
  {
    uint32_t len = strlen(r);
    return !strncmp((char*)msg, r, len);
  };

  //if the add data cmd transfers data "cancel7887a",
  //because the data is prefixed with ota, the transfered data
  //looks like "otacancel7887a" and the cancel command is executed.
  //To avoid that, the data command is renamed "otad" so that
  //no data can be misinterpreted.
  if (isOp("otad")) //same as "ota" but 
  {
    if (m_waitRestart)
    {
      return false;
    }
    else if (!m_size)
    {
      response = "otafail:notstarted";
      return false;
    }
    else if (!m_partExpectedCrc || !m_partSize)
    {
      response = "otafail:partnotstarted";
      return false;
    }

    AddData(msg + 4, size - 4, response);

    return true;
  }
  else if (isOp("otabegin:"))
  {
    OTA_SER("otabegin");
  
    const char *sub = msg + 9;

    uint32_t otaSize;
    uint32_t ret = sscanf(sub, "%d", &otaSize);
    if (ret != 1)
    {
      response = "otabeginfail";
      return false;
    }

    return Begin(otaSize, response);
  }
  else if (isOp("otaend:"))
  {
    OTA_SER("otaend");
  
    return End();
  }
  else if (isOp("otabeginpart:"))
  {
    OTA_SER("otabeginpart");
  
    if (m_waitRestart)
    {
      return false;
    }

    const char *sub = msg + 13;

    uint32_t partExpectedCrc, partSize;

    uint32_t ret = sscanf(sub, "%u,%d", &partExpectedCrc, &partSize);

    if (ret != 2)
    {
      response = "otabeginpartfail";
      return false;
    }

    return BeginPart(partExpectedCrc, partSize, response);
  }
  else if (isOp("otaendpart:"))
  {
    OTA_SER("otaendpart");
  
    return EndPart(response);
  }
  else if (isOp("otacancel:"))
  {
    OTA_SER("otacancel");
  
    m_offset = 0;
    m_size = 0;

    return true;
  }
  else if (isOp("otaretrypart:"))
  {
    OTA_SER("otaretrypart");
  
    if (m_waitRestart)
    {
      return false;
    }

    const char *sub = msg + 13;

    uint32_t offset,partExpectedCrc, partSize;

    uint32_t ret = sscanf(sub, "%u,%u,%d", &offset, &partExpectedCrc, &partSize);

    if (ret != 3)
    {
      response = CreateOTAErrorResponse("otafail", "RTY@%d", m_retryPart);
      return false;
    }

    if (offset != m_retryPart)
    {
      response = CreateOTAErrorResponse("otafail", "RTY@%d", m_retryPart);
      return false;
    }

    m_retryPart = 0xffffffff;
    m_offset = offset;
    m_partSize = 0;
    m_partOffset = 0;

#if defined(NRF)
    EraseSector(m_flashOffset + m_offset);
#endif
    return BeginPart(partExpectedCrc, partSize, response);
  }
  else if (isOp("ota"))
  {
    if (m_waitRestart)
    {
      return false;
    }
    else if (!m_size)
    {
      response = "otafail:notstarted";
      return false;
    }
    else if (!m_partExpectedCrc || !m_partSize)
    {
      response = "otafail:partnotstarted";
      return false;
    }

    AddData(msg + 3, size - 3, response);

    return true;
  }

  OTA_SER("otafail:invalidcmd");
  
  response = "otafail:invalidcmd";
  return false;
}

bool GJOTA::Begin(uint32_t size, GJString &response)
{
  OTA_SER("Begin:size=%d", size);
  
  if (size == 0)
  {
    response = "otabeginfail:empty";
    return false;
  }
  
  m_offset = 0;
  m_size = size;
  m_progress = 0;
  m_retryPart = 0xffffffff;
  m_partSize = 0;
  m_partOffset = 0;
  m_waitRestart = false;

  m_buffer.resize(GJ_OTA_BUFFER_SIZE);

  char r[32];
  sprintf(r, "otabeginok:%d", GJ_OTA_CHUNK_SIZE);
  response = r;

#if defined(ESP32)
  Update.abort(); //cancel previous failed update if any

  const char *partition_label = NULL;
  if (!Update.begin(m_size, U_FLASH, -1, LOW, partition_label)) 
  {
      response = CreateOTAErrorResponse("otabeginfail", "Update begin ERROR: %s\n", Update.errorString());
      return false;
  }
#elif defined(NRF)
  const BootPartition partition = GetNextPartition();

  if (partition.m_offset == 0)
  {
      response = CreateOTAErrorResponse("otabeginfail", "Update begin ERROR: no defined partition\n");
      return false;
  }
  else if (partition.m_size < size)
  {
      response = CreateOTAErrorResponse("otabeginfail", "Update begin ERROR: binary too large for partition\n");
      return false;
  }

  m_flashOffset = partition.m_offset;
  m_flashOffsetEnd = m_flashOffset + partition.m_size;

  EraseSector(m_flashOffset);
#endif

  char date[20];
  ConvertEpoch(GetUnixtime(), date);

  LOG("OTA Start updating %s\n\r", date);
#if  defined(GJ_LOG_ENABLE)
  FlushLog();
#endif
  return true;
}

bool GJOTA::End()
{
  m_offset = 0;

  char date[20];
  ConvertEpoch(GetUnixtime(), date);

  LOG("OTA End:%s\n\r", date);
#if  defined(GJ_LOG_ENABLE)
  FlushLog();
#endif

  return true;
}

bool GJOTA::BeginPart(uint32_t crc, uint32_t size, GJString &response)
{
  if (m_retryPart != 0xffffffff)
  {
    OTA_SER("BeginPart skipped");
    return false;
  }

  OTA_SER("BeginPart:crc=%u size=%d", crc, size);
  
  if (size > GJ_OTA_CHUNK_SIZE)
  {
    response = CreateOTAErrorResponse("otafail", "RTY@%u, part size %u > %u", m_offset, size, GJ_OTA_CHUNK_SIZE);
    return false;
  }
  else if (m_partOffset != m_partSize)
  {
    response = CreateOTAErrorResponse("otafail", "RTY@%u, incomplete part, size %u < %u", m_offset, m_partOffset, m_partSize);
    return false;
  }

  m_partExpectedCrc = crc;
  m_partSize = size;
  m_partOffset = 0;
  m_partCrc = 0xFFFFFFFF;

  if (m_sendOptionalResponse)
    response = "otabeginpartsuccess";

  return true;
}

bool GJOTA::AddData(const void *data, uint32_t size, GJString &response)
{
  if (m_retryPart != 0xffffffff)
  {
    OTA_SER("AddData skipped");
    return false;
  }

  OTA_SER("AddData:size=%d", size);
  
  m_partCrc = ComputeCrcDebug((unsigned char*)data, size, m_partCrc);
  
#if defined(ESP32)
  memcpy(m_buffer.data() + m_partOffset, data, size);
#elif defined(NRF)

  FlushSectorWrite();
  memcpy(m_buffer.data(), data, size);

  if ((m_offset + m_partOffset) == 0)
  {
    //check that the binary is the correct version (binA goes into partition A and binB goes into partition B)

    //the second word of the binary is the entry address of the program (reset handler in vector table)
    const uint32_t *data = (uint32_t*)m_buffer.data();
    const uint32_t entryAdr = data[1];
    
    if (entryAdr < m_flashOffset || entryAdr >= m_flashOffsetEnd)
    {
      response = CreateOTAErrorResponse("otafail:ESTOP", "Write ERROR: binary entry address doesn't fit target partition");
      m_waitRestart = true;
      return false;
    }
  }
  
  if (m_partOffset < m_partSize)
  {
    WriteToSector(m_flashOffset + m_offset + m_partOffset, (uint8_t*)m_buffer.data(), size);
  }
#endif
  
  m_partOffset += size;

  if (m_partOffset > m_partSize)
  {
    m_retryPart = m_offset;
    response = CreateOTAErrorResponse("otafail", "RTY@%d, overflow", m_retryPart);
    return false;
  }
  else if (m_partOffset == m_partSize)
  {
    bool checkCrc = GJ_CONF_BOOL_VALUE(ota_checkpartcrc);
    if (checkCrc && m_partExpectedCrc != m_partCrc)
    {
      m_retryPart = m_offset;
      response = CreateOTAErrorResponse("otafail", "RTY@%d, crc %u != %u", m_retryPart, m_partExpectedCrc, m_partCrc);
      return false;
    } 
    else
    {
#if defined(NRF)
      FlushSectorWrite();

      uint32_t flashCrc = ComputeCrcDebug((unsigned char*)(m_flashOffset + m_offset), m_partSize);
      if (m_partExpectedCrc != flashCrc)
      {
        m_retryPart = m_offset;
        response = CreateOTAErrorResponse("otafail", "RTY@%d, flash crc %u != %u", m_retryPart, m_partExpectedCrc, flashCrc);
        return false;
      } 
#endif

      m_offset += m_partSize;

#if defined(NRF)
      if (m_offset == m_size)
      {
          WriteBootForNextPartition();
          //WriteBootSector(0);

          EventManager::Function f;
          f = std::bind(&GJOTA::Ending, this, 0);
          GJEventManager->Add(f);
      }
      else
      {
        //erase next sector
        EraseSector(m_flashOffset + m_offset);
      }
 #endif
      uint32_t progress = (m_offset * 100 / m_size);
      if (progress != m_progress)
      {
          m_progress = progress;
          LOG("OTA Progress: %u%%\n\r", progress);
      }   
    }
  }

  return true;
}

void GJOTA::Ending(uint32_t step)
{
#if defined(NRF)
  EventManager::Function f;
  
  if (!IsFlashIdle())
  {
    f = std::bind(&GJOTA::Ending, this, 0);
    const uint64_t delayMS = 500;
    GJEventManager->Add(f);
  }
  else if (step == 0)
  {
    char date[20];
    ConvertEpoch(GetUnixtime(), date);

    //trigger last date file update
    SetUnixtime(GetUnixtime());

    LOG("OTA End:%s\n\r", date);
    GJ_FLUSH_LOG();

    const uint64_t delayMS = 500;
    f = std::bind(&GJOTA::Ending, this, step + 1);
    GJEventManager->DelayAdd(f, delayMS * 1000);
  }
  else
  {
      Reboot();
  }
#endif
}

void WriteBootSector(uint32_t partition);

bool GJOTA::EndPart(GJString &response)
{
  OTA_SER("EndPart:exp=%u crc=%u size=%d", m_partExpectedCrc, m_partCrc, m_partOffset);
  
  bool checkCrc = GJ_CONF_BOOL_VALUE(ota_checkpartcrc);

  if (checkCrc && m_partExpectedCrc != m_partCrc)
  {
    response = CreateOTAErrorResponse("otaendpartfail", "RTY@%d, crc %u != %u", m_offset, m_partExpectedCrc, m_partCrc);
    return false;
  } 
  else if (m_partOffset != m_partSize)
  {
    response = CreateOTAErrorResponse("otaendpartfail", "RTY@ received %u bytes / expected %u", m_offset, m_partOffset, m_partSize);
    return false;
  } 

#if defined(ESP32)
  if (!Update.isFinished()) 
  {
      uint32_t written = Update.write((uint8_t*)m_buffer.data(), m_partSize);
      if (written > 0) 
      {
          if(written != m_partSize)
          {
            response = CreateOTAErrorResponse("otaendpartfail:ERESTART", "Update class didn't write enough, %u != %u\n", written, m_partSize);
            return false;
          }    
      } 
      else 
      {
        response = CreateOTAErrorResponse("otaendpartfail:ERESTART", "Write ERROR: %s", Update.errorString());
        return false;
      }
  }

  if (Update.isFinished() && Update.end()) {
      
      delay(10);
      char date[20];
      ConvertEpoch(GetUnixtime(), date);
  
      LOG("OTA End:%s\n\r", date);
  #if  defined(GJ_LOG_ENABLE)
    FlushLog();
  #endif
      
      //if(_rebootOnSuccess)
      {
          //let serial/network finish tasks that might be given in _end_callback
          delay(100);
          ESP.restart();
      }
  } 
#elif defined(NRF)
  m_flashOffset += m_partSize;

  if (m_flashOffset == m_flashOffsetEnd)
  {
      WriteBootForNextPartition();
      //WriteBootSector(0);
      
      EventManager::Function f;
      f = std::bind(&GJOTA::Ending, this, 0);
      GJEventManager->Add(f);
  }
#endif

  m_offset += m_partOffset;
  uint32_t progress = (m_offset * 100 / m_size);
  if (progress != m_progress)
  {
      m_progress = progress;
      LOG("OTA Progress: %u%%\n\r", progress);
  }   
  
  if (m_sendOptionalResponse)
    response = "otaendpartsuccess";
  return true;
}
