#include "../base.h"
#include "tests.h"

void TestPins();
void TestCommands();
void TestConfig();
void TestAppendOnlyFile();

static int32_t s_testCount = 0;
static int32_t s_failCount = 0; 

void LogTest(const char *name, bool success)
{
  s_testCount++;
  s_failCount += success ? 0 : 1;
}

void TestGJ()
{
  SER("Starting tests...\n");

  TestPins();
  TestCommands();
  TestConfig();
  TestAppendOnlyFile();

  SER("All test finished: Success %d/%d\n", s_testCount - s_failCount, s_testCount);
}