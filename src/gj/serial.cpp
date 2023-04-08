#include "base.h"
#include "serial.h"
#include "commands.h"
#include "datetime.h"
#include <string.h>
#include <stdlib.h>   
#if defined(ESP32) || defined(ESP_PLATFORM)
  #include <hardwareSerial.h>
  #include <driver/uart.h>
  constexpr uint32_t MaxTerminalHandlers = 4;
#elif defined(NRF)
  constexpr uint32_t MaxTerminalHandlers = 1;
typedef uint32_t (*nrf_log_timestamp_func_t)(void);
extern "C" {
  ret_code_t nrf_log_init(nrf_log_timestamp_func_t timestamp_func);
}

//#include <SEGGER_RTT_Conf.h>
//#include <SEGGER_RTT.h>

  extern "C" {
unsigned     SEGGER_RTT_WriteString             (unsigned BufferIndex, const char* s);
  }
  
  class SoftwareSerial
  {
  public:
      SoftwareSerial(int uart_nr) {}

      inline void begin(unsigned long baud, uint32_t config = 0, int8_t rxPin=-1, int8_t txPin=-1, bool invert=false, unsigned long timeout_ms = 20000UL, uint8_t rxfifo_full_thrhd = 112) {}
      inline void end(bool turnOffDebug = true) {}
      inline void updateBaudRate(unsigned long baud) {}
      inline int available(void) {return {};}
      inline int availableForWrite(void);
      inline int peek(void) {return {};}
      inline int read(void) {return {};}
      inline size_t read(uint8_t *buffer, size_t size) {return {};}
      inline size_t read(char * buffer, size_t size)
      {
          return read((uint8_t*) buffer, size);
      }
      inline void flush(void) {}
      inline void flush( bool txOnly) {}
      inline size_t write(uint8_t) {return {};}
      inline size_t write(const uint8_t *buffer, size_t size) {return {};}
      inline size_t write(const char * buffer, size_t size)
      {
          return write((uint8_t*) buffer, size);
      }
      inline size_t write(const char * s)
      {
          return write((uint8_t*) s, strlen(s));
      }
      inline size_t write(unsigned long n)
      {
          return write((uint8_t) n);
      }
      inline size_t write(long n)
      {
          return write((uint8_t) n);
      }
      inline size_t write(unsigned int n)
      {
          return write((uint8_t) n);
      }
      inline size_t write(int n)
      {
          return write((uint8_t) n);
      }
      inline uint32_t baudRate() {return {};}
      inline operator bool() const {return {};}

      inline void setDebugOutput(bool) {}
      
      inline void setRxInvert(bool) {}
      inline void setPins(uint8_t rxPin, uint8_t txPin) {}
      inline size_t setRxBufferSize(size_t new_size) {return {};}

      inline size_t readBytes(char *buffer, size_t length)
      {
        return 0;
      }

      template <typename T>
      size_t print(T t) {return {};}

  protected:
      int _uart_nr;
      size_t _rxBufferSize;
  };

  SoftwareSerial Serial(0);
#endif
#include "config.h"
#include "ringbuffer.h"
#include "ringbuffer.hpp"
#include "eventmanager.h"
#include "gjatomic.h"


DEFINE_CONFIG_BOOL(serial.read, serial_read, true);
DEFINE_CONFIG_BOOL(serial.write, serial_write, true);

#if defined(ESP32)
  DEFINE_CONFIG_BOOL(serial.ets, serial_ets, true);
#endif


static bool enableSerial = true;
static bool enableTrace = false;

bool IsSerialEnabled()
{
  return enableSerial;
}

uint32_t gj_vsprintf(char *target, uint32_t targetSize, const char *format, va_list vaList );


struct SerialInfo
{
#ifdef NRF
  static const uint32_t BufferSize = 256;
#else
  static const uint32_t BufferSize = 1024 * 5;
 #endif
  char m_buffer[BufferSize];
};

//thread_local
//thread_local crashes the ESP32
SerialInfo *g_serialInfo = nullptr;

TerminalHandler g_terminalHandlers[MaxTerminalHandlers];
TerminalReadyHandler g_terminalReadyHandlers[MaxTerminalHandlers];

#if defined(ESP32)
  constexpr uint32_t MaxEtsHandlers = 4;
  TerminalHandler g_etsHandlers[MaxEtsHandlers];
#endif

uint32_t AddTerminalHandler(TerminalHandler newHandler, TerminalReadyHandler ready)
{
  for (uint32_t i = 0 ; i < MaxTerminalHandlers ; ++i)
  {
    TerminalHandler &handler = g_terminalHandlers[i];
    if (!handler)
    {
      handler = newHandler;
      g_terminalReadyHandlers[i] = ready;
      SER("Terminal handler added\n\r");
      return i;
    }
  }
  SER("ERROR:Terminal handler not added\n\r");
  return -1;
}

void RemoveTerminalHandler(uint32_t index)
{
  if (index == -1 || index >= MaxTerminalHandlers)
    return;

  g_terminalHandlers[index] = nullptr;
  g_terminalReadyHandlers[index] = nullptr;

  for (int i = MaxTerminalHandlers - 1 ; i != index ; --i)
  {
    TerminalHandler &handler = g_terminalHandlers[i];
    if (handler)
    {
      g_terminalHandlers[index] = g_terminalHandlers[i];
      g_terminalHandlers[i] = nullptr;

      g_terminalReadyHandlers[index] = g_terminalReadyHandlers[i];
      g_terminalReadyHandlers[i] = nullptr;
      break;
    }
  }
}

#if defined(ESP32)
uint32_t AddEtsHandler(TerminalHandler newHandler)
{
  for (uint32_t i = 0 ; i < MaxEtsHandlers ; ++i)
  {
    TerminalHandler &handler = g_etsHandlers[i];
    if (!handler)
    {
      handler = newHandler;
      return i;
    }
  }
  
  return -1;
}

void RemoveEtsHandler(uint32_t index)
{
  if (index == -1 || index >= MaxEtsHandlers)
    return;

  g_etsHandlers[index] = nullptr;

  for (int i = MaxEtsHandlers - 1 ; i != index ; --i)
  {
    TerminalHandler &handler = g_etsHandlers[i];
    if (handler)
    {
      g_etsHandlers[index] = g_etsHandlers[i];
      g_etsHandlers[i] = nullptr;
      break;
    }
  }
}
#endif

bool AreTerminalsReady()
{
  for (TerminalReadyHandler &handler : g_terminalReadyHandlers)
  {
    if (!handler)
    {
      break;
    }

    if(!handler())
      return false;
  }

  return true;
}

void CallTerminalHandlers(const char *text)
{
  //printf("CallTerminalHandlers %s\n\r", text);
  for (TerminalHandler &handler : g_terminalHandlers)
  {
    if (!handler)
    {
      break;
    }

    handler(text);
  }
}

#if defined(ESP32)
void CallEtsHandlers(const char *text)
{
  //printf("CallTerminalHandlers %s\n\r", text);
  for (TerminalHandler &handler : g_etsHandlers)
  {
    if (!handler)
    {
      break;
    }

    handler(text);
  }
}
#endif

#if defined(ESP8266) || defined(ESP32) || defined(ESP_PLATFORM) 
#define GJ_PUT_DRIVER_ENABLED
#endif

#ifdef GJ_PUT_DRIVER_ENABLED

template <int size>
struct CharPutBuffer
{
public:
  char& operator[](int index)
  {
    index %= size;
    return m_buffer[index];
  }

private:
  char m_buffer[size];
};

struct CharPutDriver
{
  static const int BufSize = 4096;
  CharPutBuffer<BufSize> m_buffer;
  int m_read = 0;
  int m_write = 0;
  int m_offset = 0;
  uint32_t m_lock = 0;
};

CharPutDriver s_charPutDriver;

//char charDriverBuffer[4096] = {0};
//char *charDriverBufferIt = charDriverBuffer;
//char *charDriverBufferEndIt = &charDriverBuffer[4096];
void putChar(char c)
{
  int write = (s_charPutDriver.m_write + s_charPutDriver.m_offset) % CharPutDriver::BufSize;
  //int nextWrite = (write + 2) % CharPutDriver::BufSize;

  int nWrite = (write + 1) % CharPutDriver::BufSize;
//  int spaceLeft = (s_charPutDriver.m_read - write) % CharPutDriver::BufSize;

  if (nWrite == s_charPutDriver.m_read)
  {
    s_charPutDriver.m_write = write;
    s_charPutDriver.m_offset = 0;
    return;   //no more space
  }
  
  s_charPutDriver.m_buffer[write] = c;
  s_charPutDriver.m_offset = (s_charPutDriver.m_offset + 1) % CharPutDriver::BufSize;

  if (c == '\n' || c == '\r')
  {
    s_charPutDriver.m_write = (s_charPutDriver.m_write + s_charPutDriver.m_offset) % CharPutDriver::BufSize;
    s_charPutDriver.m_offset = 0;
  }
}

void PrintPutCharDriverString()
{
  if (s_charPutDriver.m_read == s_charPutDriver.m_write)
    return;

  if (AtomicCompareAndExchange(s_charPutDriver.m_lock, 1, 0) != 0)
    return;

  char *buffer = g_serialInfo->m_buffer;
  int write = s_charPutDriver.m_write;
  int len = 0;

  if (s_charPutDriver.m_read < write)
  {
    len = write - s_charPutDriver.m_read;
  }
  else
  {
    len = CharPutDriver::BufSize - s_charPutDriver.m_read;
  }

  //printf("putc %d\n", len);

  sprintf(buffer, "%.*s", len, &s_charPutDriver.m_buffer[s_charPutDriver.m_read]);  
  
  CallTerminalHandlers(buffer);

#if defined(ESP32)
  CallEtsHandlers(buffer);
#endif

  s_charPutDriver.m_read = (s_charPutDriver.m_read + len) % CharPutDriver::BufSize;

  AtomicCompareAndExchange(s_charPutDriver.m_lock, 0, 1);
}
#endif //GJ_PUT_DRIVER_ENABLED

void SerialHandler(const char *text)
{
  if (GJ_CONF_BOOL_VALUE(serial_write))
  {
    #if defined(ESP8266) || defined(ESP32) || defined(ESP_PLATFORM) 
      Serial.print(text);
      Serial.flush(true);
    #elif defined(NRF)
      //note that this can fail if the RTT buffer is not read fast enough by the debugger.
      SEGGER_RTT_WriteString(0, text);
    #endif
  }
};

void gjSerialString(const char *buffer)
{
  CallTerminalHandlers(buffer);
  SerialHandler(buffer);

#ifdef GJ_PUT_DRIVER_ENABLED
  //PrintPutCharDriverString();
  //PrintPutCharDriverString();
#endif
}

void InitSerialBuffer()
{
  if (!g_serialInfo)
  {
    g_serialInfo = new SerialInfo;
  }
}

void gjFormatSerialString(const char *format, ...)
{
  if (!IsSerialEnabled())
    return;

  InitSerialBuffer();

  char *buffer = g_serialInfo->m_buffer;

  va_list argptr;
  va_start(argptr, format);
  uint32_t len = gj_vsprintf(buffer, sizeof(SerialInfo::m_buffer), format, argptr);
  va_end(argptr);

  gjSerialString(buffer);
}

void gjOutputSerialLargeString(const char *str)
{
  gjSerialString(str);
}

void Command_setbaud(const char*command) {
  CommandInfo info;
  GetCommandInfo(command, info);
  if (info.m_argCount != 1)
  {
    SER("  setbaud usage:setbaud <rate>\n\r");
    return;
  }
  
  uint32_t rate = atoi(info.m_argsBegin[0]);
  Serial.updateBaudRate(rate);

  SER("Baud rate set to %d\n\r", rate);
} 

bool ReadHardwareSerial(char *buffer, uint32_t bufferSize);

void SerialTask(void *);

#if defined(ESP32) || defined(ESP_PLATFORM)
static TaskHandle_t s_serialTask;
#endif
RingBuffer<GJString*> *s_serialBuffer = nullptr;

void CreateSerialTask()
{
    s_serialBuffer = new RingBuffer<GJString*>;
#if defined(ESP32) || defined(ESP_PLATFORM)
    xTaskCreatePinnedToCore(
        SerialTask,
        "GJSerialTask",
        1024 * 4,
        nullptr,
        1,  //prio
        &s_serialTask,
        1);
#endif
}



void SerialTask(void *)
{
  //Serial.print("Serial task begin\n\r");

  while(true)
  {
    Delay(300);

    

    if (!GJ_CONF_BOOL_VALUE(serial_read))
    {
      continue;
    }
    
    char serialBuffer[128];
    if (ReadHardwareSerial(serialBuffer, sizeof(serialBuffer)))
    {
      

      GJString *newCommand = new GJString(serialBuffer);

      if (!strncmp(serialBuffer, "crash", 5))
      {
        printf("crash\n\r");
        char *p = (char*)0x99999999;
        if (*p == 5)
          p = nullptr;
      }
      else if (!strncmp(serialBuffer, "dbg", 3))
      {
        printf("dbg\n\r");
      }

      auto executeSerialCommand = [=]()
      {
          InterpretCommand(newCommand->c_str());
          delete newCommand;
      };

      EventManager::Function f(executeSerialCommand);

      GJEventManager->Add(f);

      //s_serialBuffer->Add(newCommand);

      //SER("Added serial command:'%s'\n\r",newCommand->c_str());

      //auto wm = uxTaskGetStackHighWaterMark(nullptr);
      //SER("watermark:'%d'\n\r",wm);

    }

    
  }

  //Serial.print("Serial task end\n\r");
}


bool ReadSerialRingBuffer(char *buffer)
{
  GJString *newCommand = {};

  if (s_serialBuffer->Remove(newCommand))
  {
    SER("Removed serial command:'%s'\n\r",newCommand->c_str());

    strcpy(buffer, newCommand->c_str());
    delete newCommand;

    return true;
  }

  return false;
}

#if defined(ESP32)
int GJ_esp_log_set_vprintf(const char *format, va_list l)
{
  if (GJ_CONF_BOOL_VALUE(serial_ets) == false)
    return 0;

  InitSerialBuffer();

  char *buffer = g_serialInfo->m_buffer;

  uint32_t len = gj_vsprintf(buffer, sizeof(SerialInfo::m_buffer), format, l);

  gjSerialString(buffer);

  return len;
}
#endif

bool ReadHardwareSerial(char *serialBuffer, uint32_t bufferSize)
{
  {
    GJ_PROFILE(UpdateSerial::available);

    //esp32-hal-uart.c uses the same mutex for reading and writing
    //And so calling Serial.available creates 1000ms freezes when outputting
    //text from the main task
    //bool avail = Serial.available();
    size_t avail = 0;

#if defined(ESP32) || defined(ESP_PLATFORM)
    uart_get_buffered_data_len(0, &avail);
#endif
    if (!avail)
      return false;
  }

  uint32_t serialSize;
  {
    GJ_PROFILE(UpdateSerial::readBytes);
    serialSize = Serial.readBytes(serialBuffer, bufferSize - 1);
  }
  if (serialSize == 0)
    return false;
  
  serialBuffer[serialSize] = 0;
  
  while(serialSize > 1)
  {
    char c = serialBuffer[serialSize - 1];
    if (c == '\r' || c == '\n')
    {
      serialBuffer[serialSize - 1] = 0;
      serialSize--;
    }
    else
    {
      break;
    }
  }

  return serialSize != 0;
}

DEFINE_COMMAND_ARGS(setbaud,Command_setbaud); 

void InitSerial(uint32_t rate)
{
  enableSerial = true;
  if (enableSerial)
  {
#if defined(ESP32) || defined(ESP_PLATFORM)
    esp_deep_sleep_disable_rom_logging();
#elif defined(NRF)
    int32_t err_code = nrf_log_init(NULL);
    APP_ERROR_CHECK(err_code);
#endif

    Serial.begin(rate);
    SER("\n\r");//skip prior serial monitor garbage

    //AddTerminalHandler(SerialHandler);

#if defined(ESP32) || defined(ESP_PLATFORM)
    REFERENCE_COMMAND(setbaud); 
#endif

    //ets_install_putc2(&putChar);
    //esp_log_set_vprintf(&GJ_esp_log_set_vprintf);

    
  
    CreateSerialTask();
    
    //if (!isWake)
    {
      //Serial.availableForWrite returns true even when USB is not connected
      //delay(1000);
      //LOG("Serial available:%s\n\r", Serial.availableForWrite() ? "yes" : "no" );
    }
  }
}


void UpdateSerial()
{
  GJ_PROFILE(UpdateSerial);
  
  if (!GJ_CONF_BOOL_VALUE(serial_read))
  {
    return;
  }

  char serialBuffer[128];
  //if (!ReadHardwareSerial(serialBuffer))
  if (!ReadSerialRingBuffer(serialBuffer))
  {
    return;
  }

  static uint32_t lastTime = 0;

  //when RX pin is floating, tons of garbage is received.
  //so dont execute serial commands too often.
  if ((GetUnixtime() - lastTime) >= 1)
  {
    lastTime = GetUnixtime();
    InterpretCommand(serialBuffer);
  }
}
