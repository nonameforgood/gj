#include "base.h"
#include "commands.h"

const char *debugLoc = "";
uint32_t debugLocTime = 0;

void SetDebugLoc(const char* loc)
{
  debugLoc = loc;
  debugLocTime = GetElapsedMillis();
}

ScopedDebugLoc::ScopedDebugLoc(const char *loc)
: m_prev(debugLoc)
{
  SetDebugLoc(loc);
}
ScopedDebugLoc::~ScopedDebugLoc()
{
  SetDebugLoc(m_prev);
}

void DebugTask(void*)
{
  LOG("Debug task started\n\r");

  while(true)
  {
    Delay(1000);

    if (debugLocTime > GetElapsedMillis())
    {
      printf("debugLocTime(%d) > millis()\n\r", debugLocTime);
      debugLocTime = GetElapsedMillis();
    }
    
    if ((GetElapsedMillis() - debugLocTime) > 5000)
    {
      //SER("***************************\n\r");
      //SER("A Thread is hung\n\r");
      printf("A Thread is hung:Debug Location:'%s'\n\r", debugLoc);
      if (enableLog) 
        gjFormatLogString("A Thread is hung:Debug Location:'%s'\n\r", debugLoc);
        
      //SER("***************************\n\r");
      debugLocTime = GetElapsedMillis();
    }
  }
}

#if defined(ESP32)
static TaskHandle_t s_debugTask;
#endif

void InitDebugTask()
{
  #if defined(ESP32)
  xTaskCreatePinnedToCore(
        DebugTask,
        "DebugTask",
        1024 * 8,
        nullptr,
        1,  //prio
        &s_debugTask,
        1);

  #endif
}


static void dbgloctest()
{
  SetDebugLoc("dbgloctest");
  Delay(6000);
}

DEFINE_COMMAND_NO_ARGS(dbgloctest, dbgloctest);

void InitDebugLoc()
{
  InitDebugTask();

  REFERENCE_COMMAND(dbgloctest);
}
