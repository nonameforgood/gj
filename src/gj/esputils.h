#pragma once
#include "string.h"
#include "vector.h"
#include <functional>

bool IsSleepWakeUp();
bool IsSleepEXTWakeUp();
bool IsSleepTimerWakeUp();
bool IsSleepUlpWakeUp();
bool IsErrorReset();
bool IsPowerOnReset();
uint32_t GetResetReason();


enum class SoftResetReason
{
  None,
  Reboot,
  HardFault
};

SoftResetReason GetSoftResetReason();

int32_t GetSleepTimestamp();
void SetSleepTimestamp();
int32_t GetSleepDuration();
bool IsEqualSleepDuration(int32_t duration, int32_t percentPrecision = 5);
bool IsPeriodOver(int32_t period, int32_t maxPeriod, int32_t percentPrecision = 5);

void PrintWakeupReason();
void PrintResetReason();
void PrintBootReason();
void PrintShortBootReason();

void HandleResetError();
void SetResetTimeout(uint32_t seconds);

#define POWERON_RUNTIME (60)

uint32_t GetElapsedRuntime();
void ResetSpawnTime();

bool IsAutoSleepEnabled();
void EnableAutoSleep(bool enable);

uint32_t GetRuntime();
void SetRuntime(uint32_t value);

void InitESPUtils();
void EraseChar(GJString &str, char c);

void SetCPUFreq(uint32_t freq);

Vector<GJString> Tokenize(const char *string, char token);
const char* RemoveNewLineLineFeed(const char *input, GJString &storage);
char* ReplaceLFCR(char *input, uint32_t len, char replace);
char* ReplaceNonPrint(char *input, uint32_t len, char replace);

void DigitalWriteHold(int32_t pin, int32_t value);

void LogRam();
uint32_t GetAvailableRam();

typedef std::function<void()> ExitCallback;
void RegisterExitCallback(ExitCallback cb);

bool CheckRTCMemoryVariable(void const *address, const char *name);
void Reboot();

void InitCrashDataCommand();

#ifdef NRF

#define GJ_READ_PC(dest) \
{ \
  uint32_t _pcValue; \
 \
    __ASM volatile( \
    "   .syntax unified                        \n" \
    "   mov   %0, pc                           \n" \
    "   .align                                 \n" \
    :  "=r"(_pcValue)); \
 \
  (dest) = _pcValue; \
}

#define GJ_READ_LR(dest) \
{ \
  uint32_t _lrValue; \
 \
    __ASM volatile( \
    "   .syntax unified                        \n" \
    "   mov   %0, lr                           \n" \
    "   .align                                 \n" \
    :  "=r"(_lrValue)); \
 \
  (dest) = _lrValue; \
}

void CallAppErrorFaultHandler(uint32_t errCode, uint32_t pc, uint32_t lr);

#define GJ_CHECK_ERROR(errCode) \
  do                                                      \
  {                                                       \
      const uint32_t LOCAL_ERR_CODE = (errCode);         \
      if (LOCAL_ERR_CODE != NRF_SUCCESS)                  \
      {                                                   \
          uint32_t pcValue;                               \
          GJ_READ_PC(pcValue);                               \
          uint32_t lrValue;                               \
          GJ_READ_PC(lrValue);                               \
          CallAppErrorFaultHandler(errCode, pcValue, lrValue);     \
      }                                                   \
  } while (0)
#define GJ_CHECK_ERROR_BOOL(ret) \
  do                                                      \
  {                                                       \
      const uint32_t LOCAL_BOOLEAN_VALUE = (ret); \
      if (!LOCAL_BOOLEAN_VALUE)                             \
      {                                                     \
        uint32_t pcValue;                               \
        GJ_READ_PC(pcValue);                               \
        uint32_t lrValue;                               \
        GJ_READ_PC(lrValue);                               \
        CallAppErrorFaultHandler(0, pcValue, lrValue);     \
      }                                                     \
  } while (0)

  uint32_t GetCrashAddress();
  uint32_t GetCrashReturnAddress();
#else
  #define GJ_CHECK_ERROR(errCode)
#endif