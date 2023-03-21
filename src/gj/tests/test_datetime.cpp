#include "../base.h"
#include "tests.h"
#include "../datetime.h"

void TestDatetime()
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