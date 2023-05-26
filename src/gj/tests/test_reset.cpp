#include "../base.h"
#include "../eventmanager.h"
#include "../commands.h"
#include "../esputils.h"
#include "tests.h"

#if defined(NRF)
#include <nrf_power.h>
#endif

//For nrf devices:
//
//this requires that the JLINK resets SDRAM to 0 on debugging start
//this is currently done by adding the following calls:
//
// for nrf51:
// TargetInterface.pokeUint32(0x40000400, 0xffffffff) //this clears the RESETREAS register 
// TargetInterface.pokeBinary(0x20000000, "nullsector16384.bin")
//
// Under Project Options -> Debug -> Target script -> reset script
// Where nullsector16384.bin is 16KB large and contains all zeros

#define RESET_PATTERN  0xBEEF0000
GJ_PERSISTENT_NO_INIT static uint32_t s_ResetPattern; 

void Command_resetReason(const char *command)
{
  const uint32_t resetReason = GetResetReason();
  SER("Reset reason:%d\n\r", resetReason);
}

void TestReboot()
{
  SER("Triggering reset...\n\r");
  Delay(50);
  Reboot();
}

DEFINE_COMMAND_ARGS(resetreason, Command_resetReason);

void TestReset()
{
  BEGIN_TEST(Reset)

  REFERENCE_COMMAND(resetreason);

  const uint32_t resetReason = GetResetReason();

#ifdef NRF
  if (s_ResetPattern == 0)
  {
    TEST_CASE_VALUE_BOOL("soft, not an error reset", IsErrorReset(), false);
    TEST_CASE_VALUE_BOOL("soft, soft reason none", GetSoftResetReason() == SoftResetReason::None, true);  //this is None only because of SDRAM reset mentionned above
    TEST_CASE_VALUE_BOOL("power on, pattern 0", (resetReason == 0) || (resetReason == NRF_POWER_RESETREAS_SREQ_MASK), true);
    s_ResetPattern = RESET_PATTERN;
    TestReboot();
  }
  else if ((s_ResetPattern & 0xffff0000) == RESET_PATTERN)
  {
    const uint32_t resetStep = s_ResetPattern & 0xffff;
    s_ResetPattern += 1;

    if (resetStep == 0)
    {
      TEST_CASE_VALUE_BOOL("soft, not an error reset", IsErrorReset(), false);
      TEST_CASE_VALUE_BOOL("soft, soft reason reboot", GetSoftResetReason() == SoftResetReason::Reboot, true);
      TEST_CASE_VALUE_BOOL("soft, expected soft req", resetReason == NRF_POWER_RESETREAS_SREQ_MASK, true);
      
      SER("Triggering crash...\n\r");
      Delay(50);

      //trigger invalid instruction address
      void (*pgn)() = (void (*)())0x20010000;
      (*pgn)();
    }
    else if (resetStep == 1)
    {
      TEST_CASE_VALUE_BOOL("hard fault, is an error reset", IsErrorReset(), true);
      TEST_CASE_VALUE_BOOL("hard fault, soft reason fault", GetSoftResetReason() == SoftResetReason::HardFault, true);
      TEST_CASE_VALUE_BOOL("hard fault, expected soft req", resetReason == NRF_POWER_RESETREAS_SREQ_MASK, true);
      TEST_CASE_VALUE_INT32("hard fault, crash address", GetCrashAddress(),  0x20010000, 0x20010000);
      TEST_CASE_VALUE_BOOL("hard fault, return crash address != 0", GetCrashReturnAddress() != 0, true);

      Delay(100);

      GJ_CHECK_ERROR(NRF_ERROR_INVALID_PARAM);
    }
    else if (resetStep == 2)
    {
      TEST_CASE_VALUE_BOOL("app error, is an error reset", IsErrorReset(), true);
      TEST_CASE_VALUE_BOOL("app error, soft reason fault", GetSoftResetReason() == SoftResetReason::AppError, true);
      TEST_CASE_VALUE_BOOL("app error, expected soft req", resetReason == NRF_POWER_RESETREAS_SREQ_MASK, true);
      TEST_CASE_VALUE_INT32("app error, crash address != 0", GetCrashAddress(),  1, 0x80000);
      TEST_CASE_VALUE_INT32("app error, return crash address != 0", GetCrashReturnAddress(), 1, 0x80000);

      Delay(100);

      GJ_CHECK_ERROR_BOOL(false);
    }
    else if (resetStep == 3)
    {
      TEST_CASE_VALUE_BOOL("app error bool, is an error reset", IsErrorReset(), true);
      TEST_CASE_VALUE_BOOL("app error bool, soft reason fault", GetSoftResetReason() == SoftResetReason::AppError, true);
      TEST_CASE_VALUE_BOOL("app error bool, expected soft req", resetReason == NRF_POWER_RESETREAS_SREQ_MASK, true);
      TEST_CASE_VALUE_INT32("app error bool, crash address != 0", GetCrashAddress(),  1, 0x80000);
      TEST_CASE_VALUE_INT32("app error bool, return crash address != 0", GetCrashReturnAddress(), 1, 0x80000);

      Delay(100);

      TestReboot();
    }
    else if (resetStep == 4)
    {
      TEST_CASE_VALUE_BOOL("reboot, is not an error reset", IsErrorReset(), false);
      TEST_CASE_VALUE_BOOL("reboot, soft reason reboot", GetSoftResetReason() == SoftResetReason::Reboot, true);
      TEST_CASE_VALUE_BOOL("reboot, expected soft req", resetReason == NRF_POWER_RESETREAS_SREQ_MASK, true);
    }
  }
#endif
}    