#include "../base.h"
#include "tests.h"
#include "../datetime.h"
#include "../commands.h"

void TestSetGetUnixtime()
{
  {
    const uint32_t beginTime = GetUnixtime();
    Delay(1500);
    const uint32_t endTime = GetUnixtime();
    TEST_CASE_VALUE_INT32("GetUnixtime elapsed 1s", endTime - beginTime, 1, 2);
  }

  {
    const uint32_t newTime = 1672549200;
    SetUnixtime(newTime);
    const uint32_t time = GetUnixtime();
    TEST_CASE_VALUE_INT32("SetUnixtime", time, newTime, newTime + 1);

    Delay(1500);
    const uint32_t endTime = GetUnixtime();
    TEST_CASE_VALUE_INT32("SetUnixtime elapsed 1s", endTime, newTime, newTime + 2);
  }
}    

static const char *g_serialTestString = "";
static bool g_serialTestStringFound = false;
static void TestDateTimeTerminalHandler(const char *string)
{
  if (!strncmp(string, g_serialTestString, strlen(g_serialTestString)))
    g_serialTestStringFound = true;
} 

void TestCommandUnixtime()
{
  const uint32_t terminalHandle = AddTerminalHandler(TestDateTimeTerminalHandler);

  g_serialTestString = "SetUnixtime:1(1969-12-31T20:00:01)";
  InterpretCommand("unixtime 1");
  TEST_CASE_VALUE_BOOL("Test SetUnixtime command, 1", g_serialTestStringFound, true);
  g_serialTestStringFound = false;

  g_serialTestString = "SetUnixtime:1680494400(2023-04-03T00:00:00)";
  InterpretCommand("unixtime 1680494400");
  TEST_CASE_VALUE_BOOL("Test SetUnixtime command, 1680494400", g_serialTestStringFound, true);
  g_serialTestStringFound = false;

  g_serialTestString = "Unixtime:";
  InterpretCommand("unixtime ");
  TEST_CASE_VALUE_BOOL("Test Unixtime command space, string found", g_serialTestStringFound, true);
  g_serialTestStringFound = false;

  RemoveTerminalHandler(terminalHandle);
}

void TestDatetime()
{
  TestSetGetUnixtime();
  TestCommandUnixtime();
}