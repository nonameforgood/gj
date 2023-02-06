#pragma once

#include "enums.h"

extern bool enableTest;
#define TESTSTEP(f, ...) {if (enableTest) gjFormatTest(f,  ##__VA_ARGS__);}

void __attribute__ ((noinline)) gjFormatTest(const char *format, ...);

struct TestStep
{
  enum class Flags
  {
    None = 0,
    Repeat = 1,
    Msg = 2,
    Command = 4
  };

  TestStep(const char *string);
  TestStep(const char *string, Flags flags);
  
  const char *m_string = nullptr;
  Flags m_flags = Flags::None;
};

ENABLE_BITMASK_OPERATORS(TestStep::Flags);

void InitTestSystem();
void BeginTests(TestStep const *steps, uint32_t count);