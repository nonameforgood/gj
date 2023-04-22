#include "../base.h"
#include "../esputils.h"
#include "tests.h"

void TestReset();
void TestPinCommands();
void TestPins();
void TestCommands();
void TestConfig();
void TestAppendOnlyFile();
void TestEventManager();
void TestDatetime();
void TestFS();

//this is set to no init because of test suite "TestReset" which triggers resets 
GJ_PERSISTENT_NO_INIT static int32_t s_testCount;
GJ_PERSISTENT_NO_INIT static int32_t s_failCount; 

void LogTest(const char *name, bool success)
{
  s_testCount++;
  s_failCount += success ? 0 : 1;
}

void TestGJ()
{
  if (IsPowerOnReset())
  {
    s_testCount = 0; 
    s_failCount = 0; 
  }
  
  SER("Starting tests...\n");

  TestReset();    //better if first since it triggers resets
  TestPinCommands();
  TestPins();
  TestCommands();
  TestConfig();
  TestAppendOnlyFile();
  TestEventManager();
  TestDatetime();
  TestFS();
  
  SER("All test finished: Success %d/%d\n", s_testCount - s_failCount, s_testCount);
}