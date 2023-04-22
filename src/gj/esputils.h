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
