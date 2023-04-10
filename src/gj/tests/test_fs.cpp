#include "../base.h"
#include "../eventmanager.h"
#include "../commands.h"
#include "tests.h"

static volatile int32_t g_testFSSerialHandlerCount = 0;
static void TestFSTerminalHandler(const char *string)
{
  g_testFSSerialHandlerCount++;
} 

static void WaitTestFSOutputCount(uint32_t count, uint32_t timeout)
{
  uint32_t begin = GetElapsedMillis();

  uint32_t currentCount = g_testFSSerialHandlerCount;

  while(g_testFSSerialHandlerCount != count)
  {
    GJEventManager->WaitForEvents(0);

    Delay(5);

    if (currentCount != g_testFSSerialHandlerCount)
      begin = GetElapsedMillis();

    const uint32_t end = GetElapsedMillis();

    if ((end - begin) > timeout)
      break;
  }
}

void TestFS()
{
  const uint32_t terminalHandle = AddTerminalHandler(TestFSTerminalHandler);

  g_testFSSerialHandlerCount = 0;
  InterpretCommand("fs info");
  WaitTestFSOutputCount(3, 50);
  TEST_CASE_VALUE_INT32("fs info, 3 outputs", g_testFSSerialHandlerCount, 3, 12);

  g_testFSSerialHandlerCount = 0;
  InterpretCommand("fs dump /test");
  WaitTestFSOutputCount(10, 50);
  TEST_CASE_VALUE_INT32("fs dump /test, 3 outputs", g_testFSSerialHandlerCount, 1, 10);

  g_testFSSerialHandlerCount = 0;
  InterpretCommand("fs dumphex /test");
  WaitTestFSOutputCount(34, 50);
  TEST_CASE_VALUE_INT32("fs dumphex /test, 3 outputs", g_testFSSerialHandlerCount, 1, 34);

  RemoveTerminalHandler(terminalHandle);
}    