#include <stdint.h>
#include "datetime.h"
#include "string.h"
#include "base.h"
#include "http.h"
#include <soc/rtc.h>
#include <ctime>
#include <time.h>
#include "esputils.h"
#include "gjwifi.h"


#include <sys/time.h>

#include <sntp.h>


void SetUnixtime(uint32_t unixtime);
void WriteLastDateFile(int32_t unixtime);

int32_t FiletimeToUnixtime(uint64_t filetime)
{
  filetime /= 10;   //to micro, filetime is in 100s of nanos
  filetime /= 1000; //to millis
  filetime /= 1000; //to seconds
  
  int32_t unixTime = filetime - 11644473600;  //from year 1601 to 1970 

  return unixTime;
}


bool GetDateTimeFromWorldTimeApi(int32_t &unixTime)
{
  GJString response;
  const char *uri = "http://worldtimeapi.org/api/timezone/America/New_York.txt";
  if (HttpGet(uri, response ))
  {     
    unixTime = 0;
    const char *it = strstr(response.c_str(), "unixtime:");
    if (it)
    {
      sscanf(it, "unixtime: %d", &unixTime );

      it = strstr(response.c_str(), "raw_offset:");
      if (it)
      {
        int32_t rawOffset = 0;
        sscanf(it, "raw_offset: %d", &rawOffset );

        unixTime += rawOffset;
      }
      
      it = strstr(response.c_str(), "dst_offset:");
      if (it)
      {
        int32_t dst_offset = 0;
        sscanf(it, "dst_offset: %d", &dst_offset );
        unixTime += dst_offset;
      }

      LOG("Datetime retrieved from worldtimeapi.org (unixtime api)\n\r");
      return true;
    }
  }

  LOG("GetDateTimeFromWorldTimeApi failed\n\r");
  return false;
}

uint64_t Scan64(const char *string)
{
  uint64_t filetime = 0;
  uint32_t filetimeStringLength = strlen(string);
  uint64_t scale = (uint64_t)pow(10, filetimeStringLength - 1 );

  const char *strIt = string;
  while(*strIt)
  {
    filetime += (*strIt - '0' ) * scale;
    scale /= 10;
    strIt++;
  }
  return filetime;
}


uint64_t ExtractFiletime(const char* datetimeString)
{
  char fileTimeString[64];
  char *strIt = fileTimeString;
  const char *it = datetimeString;

  while(*it >= '0' && *it <= '9')
  {
    *strIt = *it;
    strIt++;
    it++;
  }

  *strIt = 0;
  
  uint64_t const filetime = Scan64(fileTimeString);
  return filetime;
}

bool GetDateTimeFromWorldClockApi(int32_t &unixTime)
{
  GJString response;
  const char *uri = "http://worldclockapi.com/api/json/est/now";
  if (HttpGet(uri, response ))
  {     
    unixTime = 0;
    const char *it = strstr(response.c_str(), "currentFileTime\":");
    if (it)
    {
      it += 17;
      uint64_t const filetime = ExtractFiletime(it); 
      unixTime = FiletimeToUnixtime(filetime);
      LOG("Datetime retrieved from worldclockapi.com (filetime api)\n\r");
      return unixTime != 0;
    }
    
    return false;
  }
  
  LOG("GetDateTimeFromWorldClockApi failed\n\r");
  return false;
}
extern RTC_DATA_ATTR uint32_t lastOnlineUnixtime;

void sntp_sync_time_cb(struct timeval *tv)
{
  uint32_t t = GetUnixtime();
  
  char date[20];
  ConvertEpoch(t, date);

  LOG("sntp update:%s (%d)\n\r", date, t);
  WriteLastDateFile(GetUnixtime());
  lastOnlineUnixtime = GetUnixtime();
}

void UpdateDatetimeWithSNTP()
{
  int32_t unixtime = 0;

  const char* ntpServer = "pool.ntp.org";
  const long  gmtOffset_sec = 3600 * -5;
  const int   daylightOffset_sec = 0;

  sntp_set_time_sync_notification_cb(&sntp_sync_time_cb);
  sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  LOG("SNTP interval:%ds\n\r", sntp_get_sync_interval() / 1000);
}

void UpdateDatetimeWithHTTP()
{
  int32_t unixtime = 0;

  if (!GetDateTimeFromWorldClockApi(unixtime))
  {
    if (!GetDateTimeFromWorldTimeApi(unixtime))
    {
      GJ_ERROR("ERROR:No online time provider available\n\r");
      return;
    }
  }

  SetUnixtime(unixtime);
  lastOnlineUnixtime = GetUnixtime();
}
