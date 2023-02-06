#pragma once

#include <ctime>

typedef void (*OnlineDateUpdater)();

void UpdateDatetimeWithSNTP();
void UpdateDatetimeWithHTTP();

void InitializeDateTime(OnlineDateUpdater updateFunc = nullptr);
bool IsOnlineDateNeeded();
void UpdateTimeOnline();

void PrepareDateTimeSleep();

int32_t GetUnixtime();
void SetUnixtime(uint32_t unixtime);

void ConvertEpoch(int32_t epoch, char *dateTime);
void ConvertToTM(const char *dateTime, std::tm &tm);

void LogTime();