#include "../base.h"
#include "tests.h"

void TestPinCommands();
void TestPins();
void TestCommands();
void TestConfig();
void TestAppendOnlyFile();
void TestEventManager();

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

  TestPinCommands();
  TestPins();
  TestCommands();
  TestConfig();
  TestAppendOnlyFile();
  TestEventManager();
  
  SER("All test finished: Success %d/%d\n", s_testCount - s_failCount, s_testCount);
}