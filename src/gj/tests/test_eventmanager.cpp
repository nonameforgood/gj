#include "../base.h"
#include "../appendonlyfile.h"
#include "../eventmanager.h"
#include "tests.h"

static volatile int32_t s_timer0Time = 0;
static volatile int32_t s_timer1Time = 0;

static void OnTimer0()
{
  s_timer0Time = GetElapsedMillis();
}

static void OnTimer1()
{
  s_timer1Time = GetElapsedMillis();
}

static void WaitForTimer(const volatile int32_t &timerTime, int32_t timeout)
{
  const uint32_t begin = GetElapsedMillis();
  while(timerTime == 0)
  {
    GJEventManager->WaitForEvents(0);

    const uint32_t end = GetElapsedMillis();

    if ((end - begin) > timeout)
      break;
  }
}

void TestEventManager()
{
  int32_t beginTime = GetElapsedMillis();

  GJEventManager->DelayAdd(OnTimer0, 300 * 1000);
  GJEventManager->DelayAdd(OnTimer1, 10 * 1000);

  WaitForTimer(s_timer0Time, 500);

  int32_t timer0Delta = s_timer0Time - beginTime;
  int32_t timer1Delta = s_timer1Time - beginTime;

  TEST_CASE("GJEventManager timer 0 is called", s_timer0Time != 0);
  TEST_CASE("GJEventManager timer 1 is called", s_timer1Time != 0);
  TEST_CASE("GJEventManager timer0 > timer1",s_timer0Time > s_timer1Time);
  TEST_CASE_VALUE_INT32("GJEventManager timer 0 expected delay", timer0Delta, 295, 305);
  TEST_CASE_VALUE_INT32("GJEventManager timer 1 expected delay", timer1Delta, 9, 11);

  s_timer0Time = 0;
  beginTime = GetElapsedMillis();
  GJEventManager->DelayAdd(OnTimer0, 2000 * 1000);
  WaitForTimer(s_timer0Time, 2500);
  timer0Delta = s_timer0Time - beginTime;

  TEST_CASE("GJEventManager timer 0 is called", s_timer0Time != 0);
  TEST_CASE_VALUE_INT32("GJEventManager timer 0 expected delay", timer0Delta, 1995, 2005);
}