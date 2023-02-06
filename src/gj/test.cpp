#include "base.h"
#include "serial.h"
#include "test.h"
#include <string.h>
#include "commands.h"

uint32_t gj_vsprintf(char *target, uint32_t targetSize, const char *format, va_list vaList );

RTC_DATA_ATTR bool enableTest = false;
RTC_DATA_ATTR bool enableTestDebug = false;

#define TEST_SER_DEBUG(f, ...) if (enableTestDebug){ SER("[TEST DEBUG] "); SER(f, ##__VA_ARGS__); }

void Command_testdebug() {
  SER("Test system debugging enabled...\n\r");
  enableTestDebug = true;
}

DEFINE_COMMAND_NO_ARGS(testdebug,Command_testdebug );

void InitTestSystem()
{
  REFERENCE_COMMAND(testdebug );
}

TestStep::TestStep(const char *string)
: m_string(string)
{

}
TestStep::TestStep(const char *string, Flags flags)
: m_string(string)
, m_flags(flags)
{
  
}

class TestContext
{
public:

  void Begin();
  void HandleStep(const char *string);
  void Advance();
  void ProcessMessages();

  TestStep const *m_steps = nullptr;
  uint32_t m_count = 0;
  uint32_t m_index = 0;
};

RTC_DATA_ATTR TestContext s_testContext;

void TestContext::Begin()
{
  ProcessMessages();
}
  
void TestContext::ProcessMessages()
{
  TEST_SER_DEBUG("ProcessMessages\n\r");
  while(m_index < m_count)
  {
    if (IsSet(m_steps[m_index].m_flags, TestStep::Flags::Msg))
    {
      TEST_SER_DEBUG("Is Msg\n\r");
      gjSerialString("[TEST] ");
      gjSerialString(m_steps[m_index].m_string);
      gjSerialString("\n\r");
      ++m_index;
    }
    else if (IsSet(m_steps[m_index].m_flags, TestStep::Flags::Command))
    {
      TEST_SER_DEBUG("Is Command\n\r");
      InterpretCommand(m_steps[m_index].m_string);
      ++m_index;
    }
    else
    {
      break;
    }
  }
}

void TestContext::HandleStep(const char *string)
{
  TEST_SER_DEBUG("Handle step\n\r");

  TestStep const *step = &m_steps[m_index];

  TEST_SER_DEBUG("Expected:'%s' got:'%s'\n\r", step->m_string, string);

  if (strcmp(step->m_string, string) == 0)
  {
    ++m_index;
    ProcessMessages();
    //Advance();
  }
  else
  {
    TestStep const *previousStep = step - 1;

    if (!m_index ||
        !IsSet(previousStep->m_flags, TestStep::Flags::Repeat) || 
        strcmp(previousStep->m_string, string))
    {
      static bool reported(false);
      SER_COND(!reported, "[!!!!TEST ERROR!!!!]:stopping, previous step mismatch, expected:'%s' at index %d\n\r", step->m_string, m_index);
      reported = true;
      //ON_SER(Serial.flush());
      //TermLog();
      //esp_deep_sleep_start();
    }
  }

  if (m_index >= m_count)
  {
    TEST_SER_DEBUG("[TEST] End\n\r");
    enableTest = false;
  }
}

void TestContext::Advance()
{
  TEST_SER_DEBUG("Advance\n\r");
  
  m_index++;
  while(m_index < m_count)
  {
    TestStep const *step = &m_steps[m_index];

    if (IsSet(step->m_flags, TestStep::Flags::Msg))
    {
      TEST_SER_DEBUG("Is Msg\n\r");
      gjSerialString(step->m_string);
      gjSerialString("\n\r");
    }
    else if (IsSet(step->m_flags, TestStep::Flags::Command))
    {
      TEST_SER_DEBUG("Is Command\n\r");
      InterpretCommand(step->m_string);
    }
    else
    {
      break;
    }

    m_index++;
  }
}


void __attribute__ ((noinline)) gjFormatTest(const char *format, ...)
{
  char buffer[384];

  va_list argptr;
  va_start(argptr, format);
  gj_vsprintf(buffer, sizeof(buffer), format, argptr);
  va_end(argptr);

  gjSerialString("[TEST] ");
  gjSerialString(buffer);
  gjSerialString("\n\r");

  s_testContext.HandleStep(buffer);
}

void BeginTests(TestStep const *steps, uint32_t count)
{
  enableTest = true;
  s_testContext.m_steps = steps;
  s_testContext.m_count = count;
  s_testContext.m_index = 0;

  s_testContext.Begin();

  TEST_SER_DEBUG("[TEST] Begin\n\r");
}