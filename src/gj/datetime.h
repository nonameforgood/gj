#pragma once

#include <ctime>

typedef void (*OnlineDateUpdater)();

void UpdateDatetimeWithSNTP();
void UpdateDatetimeWithHTTP();

void InitializeDateTime(OnlineDateUpdater updateFunc = nullptr);
void EnableDateTimeWriter(uint32_t freq = -1);        //updates datetime file at regular interval
void EnableFormattedTimeCommand();                    //enables the command thats prints detailed time ie:year, month, day, etc
bool IsOnlineDateNeeded();
void UpdateTimeOnline();

void PrepareDateTimeSleep();

int32_t GetUnixtime();
int32_t GetLocalUnixtime();
void SetUnixtime(uint32_t unixtime);

void ConvertEpoch(int32_t epoch, char *dateTime);
void ConvertToTM(const char *dateTime, std::tm &tm);

void LogTime();