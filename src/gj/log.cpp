#include "base.h"
#include "file.h"
#include <stdio.h>
#include "commands.h"
#include "file.h"
#include "log.h"
#include "esputils.h"
#include "datetime.h"
#include "config.h"
#include "eventmanager.h"

#if defined(ESP8266) || defined(ESP32) || defined(ESP_PLATFORM) 
#include <rom/crc.h>
#endif

DEFINE_CONFIG_BOOL(log.ets,log_ets, false);

bool enableLog = true;

GJFile internalLogFile;
GJFile &logFile = internalLogFile;
GJLog internalGJLog(internalLogFile);
GJLog &gjLog = internalGJLog;

#define LOG_FILE_OLD "/gj.log"
#define LOG_FILE_FMT "/%slog%d.log"
#define ERROR_LOG_FILE_FMT "/%serrors%d.log"
#define MAX_LOG_FILE_SIZE (20 * 1024)

GJ_PERSISTENT uint32_t g_lastLogIndex = 0;

void BloatLog();

uint32_t gj_vsprintf(char *target, uint32_t targetSize, const char *format, va_list vaList );

void gjFormatLogString(const char *format, ...)
{
#ifdef ESP32
  char buffer[384];

  va_list argptr;
  va_start(argptr, format);
  gj_vsprintf(buffer, sizeof(buffer), format, argptr);
  va_end(argptr);

  gjLog.Write(buffer);
#endif
}

void gjFormatErrorString(const char *format, ...)
{
  char buffer[384];

  int logIndex = 1;
  sprintf(buffer, ERROR_LOG_FILE_FMT, GetAppFolder(), logIndex);
  GJFile file(buffer, GJFile::Mode::Append);

  if (file.Size() >= MAX_LOG_FILE_SIZE)
  {
    file.Close();
    FileSystem::Delete(buffer);
    file.Open(buffer, GJFile::Mode::Append);
  }
  
  GJLog log(file);

  ConvertEpoch(GetUnixtime(), buffer);
  log.Write(buffer);
  log.Write("\n\r");

  va_list argptr;
  va_start(argptr, format);
  gj_vsprintf(buffer, sizeof(buffer), format, argptr);
  va_end(argptr);

  log.Write(buffer);
}

void gjLogLargeString(const char *str)
{
  gjLog.Write(str);
}

void __attribute__ ((noinline)) gjLogStringOnChange(uint32_t &crc, const char *format, ...)
{
  char buffer[384];

  va_list argptr;
  va_start(argptr, format);
  uint32_t ret = gj_vsprintf(buffer, sizeof(buffer), format, argptr);
  va_end(argptr);

#if defined(ESP8266) || defined(ESP32) || defined(ESP_PLATFORM) 
  uint32_t const newCrc = crc32_le(0, (uint8_t*)buffer, ret);
#else
  uint32_t const newCrc = ComputeCrc(buffer, ret);
#endif

  if (newCrc == crc)
    return;

    crc = newCrc;

  if (enableLog)
    gjLog.Write(buffer);

  if (IsSerialEnabled())
    gjSerialString(buffer);
}

bool GetLogFileIndex(const char *filepath, uint32_t &index)
{
  if (filepath[0] == '/')
    filepath++;

  if (strlen(filepath) == 0)
    return false;

  if (strncmp(filepath, "log", 3))
    return false;

  filepath += 3;

  if (!strstr(filepath, ".log"))
    return false;

  index = atoi(filepath);

  return index != 0;
}

struct LogInfo
{
  uint32_t m_first;
  uint32_t m_last;
  uint32_t m_lastLogSize;
  uint32_t m_totalSize;
};

void GetLogInfo(LogInfo &info)
{
  info.m_first = 0;
  info.m_last = 0;
  info.m_lastLogSize = 0;
  info.m_totalSize = 0;

  auto onFile = [&](File &file, uint32_t depth)
  {
    const char *filename = file.name();

    uint32_t logIndex = 0;
    if (GetLogFileIndex(filename, logIndex))
    {
      info.m_totalSize += file.size();

      if (info.m_first == 0)
        info.m_first = logIndex;
      else
        info.m_first = Min<int>(info.m_first, logIndex);

      if (logIndex > info.m_last)
      {
        info.m_last = logIndex;
        info.m_lastLogSize = file.size();
      }
    }
  };

  GJString path(FormatString("/%s", GetAppFolder()));

  FileSystem::ListDir(path.c_str(),8, onFile);

  //SER("LogInfo:first=%d last=%d lastsize=%d totalsize=%d\n\r", info.m_first, info.m_last, info.m_lastLogSize, info.m_totalSize);
}

uint32_t GetMaxLogSize()
{
  uint32_t const maxLogSize = 75 * 1024;
  return maxLogSize;
}

void FlushLog()
{
  logFile.Flush();
}

void TermLog()
{
  logFile.Close();
}

struct LogSendState
{
  std::function<void(const char *)> m_cb;
  GJFile m_logFile;

  uint32_t m_current = 0;
  uint32_t m_last = 0;
};

void SendLogHandler(LogSendState *info)
{
 //SER("Asynclog\n\r");
  GJ_PROFILE(SendLogHandler);

  if (info->m_logFile)
  {
    char buffer[257];
    uint32_t readSize = info->m_logFile.Read(buffer, 256);
  
    //SER("Asynclog read size:%d\n\r", readSize);

    if (readSize != 0)
    {
      buffer[readSize] = 0;
      info->m_cb(buffer);
      auto f = std::bind(SendLogHandler, info);
      GJEventManager->Add(f);
      return;
    }
  }

  info->m_logFile.Close();

  if (info->m_current >= info->m_last)
  {
    //SER("Asynclog:end %d\n\r", info->m_current);
    delete info;
    return;
  }

  info->m_current++;

  char buffer[32];
  sprintf(buffer, LOG_FILE_FMT, GetAppFolder(), info->m_current);
  info->m_logFile.Open(buffer, GJFile::Mode::Read);

  auto f = std::bind(SendLogHandler, info);
  GJEventManager->Add(f);

  //SER("Asynclog:next file %s\n\r", buffer);
}

void StartSendLog(std::function<void(const char *)> cb, uint32_t start, uint32_t last)
{
  FlushLog();

  LogSendState *info = new LogSendState;

  info->m_cb = cb;
  info->m_current = start;
  info->m_last = last;

  char buffer[32];
  sprintf(buffer, LOG_FILE_FMT, GetAppFolder(), info->m_current);
  info->m_logFile.Open(buffer, GJFile::Mode::Read);

  auto f = std::bind(SendLogHandler, info);
  GJEventManager->Add(f);
}

void SendLog(std::function<void(const char *)> cb)
{
  LogInfo logInfo;
  GetLogInfo(logInfo);

  if (logInfo.m_first == 0 || logInfo.m_last == 0)
    return;

  StartSendLog(cb, logInfo.m_first, logInfo.m_last);
}

void SendRecentLog(std::function<void(const char *)> cb)
{
  LogInfo logInfo;
  GetLogInfo(logInfo);

  if (logInfo.m_first == 0 || logInfo.m_last == 0)
    return;

  StartSendLog(cb, logInfo.m_last - 1, logInfo.m_last);
}

void OpenNewLogFile(uint32_t index)
{
  char logFilePath[32];
  logFile.Close();
  sprintf(logFilePath, LOG_FILE_FMT, GetAppFolder(), index);
  logFile.Open(logFilePath, GJFile::Mode::Append);
  SER("Single log file size reached max size, incremented file index to %d\n\r", index);
}

//test purposes
void BloatLog()
{
  SER("Bloating log ...\n\r");

  const char *bloatBuffer = "bloating...\n";
  uint32_t const bloatSize = strlen(bloatBuffer);

  LogInfo logInfo;
  GetLogInfo(logInfo);

  uint32_t totalSize = logInfo.m_totalSize;

  uint32_t lastKB = 0;

  uint32_t const newSize = GetMaxLogSize();

  while(totalSize < newSize)
  {
    logFile.Write(bloatBuffer);

    if (logFile.Size() >= MAX_LOG_FILE_SIZE)
    {
      OpenNewLogFile(++g_lastLogIndex);
    }

    totalSize += bloatSize;
    
    uint32_t newKB = totalSize;
    if (newKB != lastKB)
    {
      lastKB = newKB;
      SER("  Log size:%dKB\n\r", logFile.Size() / 1024);
    }
  }

  SER("  Done\n\r");
}

static void HandleLogSize()
{
  uint32_t const maxLogSize = GetMaxLogSize();

  LogInfo logInfo;
  GetLogInfo(logInfo);

  LOG("HandleLogSize total:%dKB first:%d last:%d\n\r", logInfo.m_totalSize / 1024, logInfo.m_first, logInfo.m_last);

  while(logInfo.m_totalSize > maxLogSize && logInfo.m_first < logInfo.m_last)
  {
    char logFilePath[32];
    sprintf(logFilePath, LOG_FILE_FMT, GetAppFolder(), logInfo.m_first);

    LOG("Deleting log file '%s' to free space.\n\r", logFilePath);

    uint32_t const usedBefore = FileSystem::Used();
    FileSystem::Delete(logFilePath);
    uint32_t const usedAfter = FileSystem::Used();

    if (usedBefore == usedAfter)
    {
      GJ_ERROR("  Delete of '%s' failed\n\r", logFilePath);
    }

    logInfo.m_totalSize -= usedBefore - usedAfter;
    ++logInfo.m_first;
  }
}

void LogEtsOutput(const char *output)
{
  gjLog.Write(output);
}

GJLog::GJLog(GJFile &file)
: m_file(file)
{

}

uint32_t GJLog::Write( char const *buffer )
{
  if (!m_file)
  {
    #if !defined(NRF)
      m_pending.concat(buffer);
      return strlen(buffer);
    #else
      return 0;
    #endif
  }

  if (!m_pending.empty())
  {
    m_file.Write(m_pending.c_str());
    m_pending.clear();
  }

  return m_file.Write(buffer);
}

uint32_t GJLog::Write( GJString const &string )
{
  return Write(string.c_str());
}

uint32_t GJLog::Write( int32_t value )
{
  uint32_t size = 0;
  if(value < 0) {
      Write('-');
      size++;
      value = -value;
  }
  
  return PrintNumber( value, 10 ) + size;
}

uint32_t GJLog::Write( uint32_t value )
{ 
  return PrintNumber( value, 10 );
}

uint32_t GJLog::PrintNumber(unsigned long n, uint8_t base) {
    char buf[8 * sizeof(long) + 1]; // Assumes 8-bit chars plus zero byte.
    char *str = &buf[sizeof(buf) - 1];

    *str = '\0';

    // prevent crash if called with base == 1
    if(base < 2)
        base = 10;

    do {
        unsigned long m = n;
        n /= base;
        char c = m - base * n;
        *--str = c < 10 ? c + '0' : c + 'A' - 10;
    } while(n);

    return Write(str);
}

void Command_dumplog() {
  auto send = [](const char* buffer)
  {
    SER("%s", buffer); //must use %s to print % chars from log
  };
  SendLog(send);
}

void Command_dumprecentlog() {
  auto send = [](const char* buffer)
  {
    SER("%s", buffer);  //must use %s to print % chars from log
  };
  SendRecentLog(send);
} 

void Command_test_bloatlog() {
  BloatLog();
} 

void Command_verbose() {
  SetVerbose(true);
  SER("Verbose mode enabled\n\r");
}

DEFINE_COMMAND_NO_ARGS(logdump,Command_dumplog );
DEFINE_COMMAND_NO_ARGS(logrecent,Command_dumprecentlog);
DEFINE_COMMAND_NO_ARGS(logbloat,Command_test_bloatlog);
DEFINE_COMMAND_NO_ARGS(logverbose,Command_verbose);

void RegisterLogCommands()
{
  REFERENCE_COMMAND(logdump);
  REFERENCE_COMMAND(logrecent);
  REFERENCE_COMMAND(logbloat);
  REFERENCE_COMMAND(logverbose);
}




void InitLog(const char *prefix)
{
  RegisterLogCommands();

  if (FileSystem::Exists(LOG_FILE_OLD))
  {
    SER("OLD LOG rename\n\r");
    char newLogFile[32];
    sprintf(newLogFile, LOG_FILE_FMT, GetAppFolder(), 1);
    FileSystem::Rename(LOG_FILE_OLD, newLogFile);
  }

  uint32_t logIndex = 0;
  if (g_lastLogIndex == 0)
  {
    //slow path
    LogInfo logInfo;
    GetLogInfo(logInfo);
    logIndex = logInfo.m_last;

    if (!logIndex)
    {
      logIndex = 1;
    }
  }
  else
  {
    //fast path
    logIndex = g_lastLogIndex;
  }

  char logFilePath[32];
  sprintf(logFilePath, LOG_FILE_FMT, GetAppFolder(), logIndex);
  logFile.Open(logFilePath, GJFile::Mode::Append);

  bool checkTotalSize = false;

  if (logFile.Size() >= MAX_LOG_FILE_SIZE)
  {
    OpenNewLogFile(++logIndex);
    checkTotalSize = true;
  }
  
  if (checkTotalSize || !IsSleepWakeUp())
  {
    HandleLogSize();
  }

  g_lastLogIndex = logIndex;

  
  if (GJ_CONF_BOOL_VALUE(log_ets))
  {
    AddEtsHandler(LogEtsOutput);
  }
}
