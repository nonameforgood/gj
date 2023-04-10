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
  const uint32_t begin = GetElapsedMillis();

  while(g_testFSSerialHandlerCount != count)
  {
    GJEventManager->WaitForEvents(0);

    Delay(5);

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
  TEST_CASE_VALUE_INT32("fs info, 3 outputs", g_testFSSerialHandlerCount, 3, 3);

  g_testFSSerialHandlerCount = 0;
  InterpretCommand("fs dump /test");
  WaitTestFSOutputCount(10, 500);
  TEST_CASE_VALUE_INT32("fs dump /test, 3 outputs", g_testFSSerialHandlerCount, 3, 10);

  g_testFSSerialHandlerCount = 0;
  InterpretCommand("fs dumphex /test");
  WaitTestFSOutputCount(34, 500);
  TEST_CASE_VALUE_INT32("fs dumphex /test, 3 outputs", g_testFSSerialHandlerCount, 34, 34);

  RemoveTerminalHandler(terminalHandle);
}    