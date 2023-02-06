#include "metricsmanager.h"
#include "chunkstorage.h"
#include "datetime.h"
#include "millis.h"
#include "http.h"
#include "esputils.h"
#include "counter.h"
#include "countercontainer.h"
#include "sleepmanager.h"
#include "commands.h"
#include "test.h"
#include "gjwifi.h"
#include "metricsdataformat.h"

struct MemoryHeader
{
  uint8_t const m_id[2] = {'G','J'};
  uint16_t const m_version = 4;
};

struct ChunkHeader
{
  static uint16_t constexpr ID = 0xC1C1;

  uint16_t m_id;
  uint16_t m_size;
  uint16_t m_format;
  uint32_t m_uid;
};

#define NORMAL_RETENTION (60 * 60 * 3)
RTC_DATA_ATTR int32_t dataRetention = NORMAL_RETENTION;

RTC_DATA_ATTR static int32_t g_lastDataCommitTime = 0;
RTC_DATA_ATTR bool enableDataServer = true;
RTC_DATA_ATTR int32_t g_lastCommitTime = 0;
RTC_DATA_ATTR int32_t g_lastCommit = 0;
RTC_DATA_ATTR int32_t g_commitPeriod = 60 * 15;


static void command_shortcommit()
{
    g_commitPeriod = 15;  
    SER("Commit period set to %d seconds\n\r", g_commitPeriod);
}
static void command_mediumcommit()
{
    g_commitPeriod = 60;  
    SER("Commit period set to %d seconds\n\r", g_commitPeriod);
}
static void command_normalcommit()
{
    g_commitPeriod = 60 * 15;  
    SER("Commit period set to %d seconds\n\r", g_commitPeriod);
}

DEFINE_COMMAND_NO_ARGS(shortcommit,command_shortcommit);
DEFINE_COMMAND_NO_ARGS(mediumcommit,command_mediumcommit);
DEFINE_COMMAND_NO_ARGS(normalcommit,command_normalcommit);


void MetricsManager::Command_shortretention()
{
  ms_instance->SetRetentionPeriod(20);
      SER("Data refresh interval set to 20 seconds\n\r");
}
void MetricsManager::Command_mediumretention()
{
  ms_instance->SetRetentionPeriod(60);
      SER("Data refresh interval set to 60 seconds\n\r");
}
void MetricsManager::Command_normalretention()
{
  ms_instance->SetRetentionPeriod(NORMAL_RETENTION);
      SER("Data refresh interval set to %d seconds\n\r", NORMAL_RETENTION);
}
void MetricsManager::Command_commitcounters()
{
  ms_instance->CommitCounters();  
      SER("Counters commited\n\r");
}
void MetricsManager::Command_dataserveron()
{
  ms_instance->EnableDataServer(true);
      SER("Data server on\n\r");
}
void MetricsManager::Command_dataserveroff()
{
  ms_instance->EnableDataServer(false);
      SER("Data server off\n\r");
}
void MetricsManager::Command_sendpendingmetrics()
{
  bool const ref = ms_instance->SendPendingData();
  SER("Pending data send:%s\n\r", ref ? "SUCCESS" : "FAILED");
}

MetricsManager *MetricsManager::ms_instance = nullptr;

MetricsManager::MetricsManager(ChunkStorage &mem)
: m_localStorage(mem)
{
  ms_instance = this;

  /*
  REGISTER_COMMAND("shortretention",
    [=]() {
      this->SetRetentionPeriod(20);
      SER("Data refresh interval set to 20 seconds\n\r");
    } );
  REGISTER_COMMAND("mediumretention",
    [=]() {
      this->SetRetentionPeriod(60);
      SER("Data refresh interval set to 60 seconds\n\r");
    } );
  REGISTER_COMMAND("normalretention",
    [=]() {
      this->SetRetentionPeriod(NORMAL_RETENTION);
      SER("Data refresh interval set to %d seconds\n\r", NORMAL_RETENTION);
    } );
  REGISTER_COMMAND("commitcounters",
    [=]() {
      this->CommitCounters();  
      SER("Counters commited\n\r");
    } );
  REGISTER_COMMAND("dataserveron",
    [=]() {
      this->EnableDataServer(true);
      SER("Data server on\n\r");
    } );
  REGISTER_COMMAND("dataserveroff",
    [=]() {
      this->EnableDataServer(false);
      SER("Data server off\n\r");
    } );

  REGISTER_COMMAND("sendpendingmetrics",
    [=]() {
      bool const ref = this->SendPendingData();
      SER("Pending data send:%s\n\r", ref ? "SUCCESS" : "FAILED");
    } );
*/

  REFERENCE_COMMAND(shortcommit);
  REFERENCE_COMMAND(mediumcommit);
  REFERENCE_COMMAND(normalcommit);
} 

void MetricsManager::SetIotID(const char *id)
{
  m_iotID = id;
}


void MetricsManager::Init(CounterContainer *counters, const char *dataServerUrl)
{
  m_counters = counters;
  m_remoteStorageUri = dataServerUrl;
  m_maxRetention = RetentionPeriodReached();//store it for consistency accross entire runtime
  
  if (!IsSleepWakeUp() && !IsErrorReset()) 
  {
    SendPendingData();
  }

  //SER("MetricsManager::Init\n\r");
  //SER("  maxRet:%d\n\r", m_maxRetention);
  //SER("  hasPendingData:%d\n\r", HasPendingData());

  bool countersHaveData = {};
  InternalUpdate(&countersHaveData);

  if (countersHaveData && (IsSleepEXTWakeUp() || IsSleepUlpWakeUp()))
  {
    //postpone commit on EXT and Ulp wakeup.
    g_lastCommit = GetUnixtime();
    LOG("MM:commit res\n\r");
  }
}

void MetricsManager::SetDataFormats(Vector<MetricsDataFormat const*> const &dataFormats)
{
  m_dataFormats = dataFormats;
}
  
void MetricsManager::PrepareSleep(SleepInfo const &sleepInfo)
{
  if (sleepInfo.m_eventType != SleepInfo::EventType::Prepare)
    return;
    
  SER("MetricsManager::PrepareSleep\n\r");
  
  //when retention reached, InternalUpdate will try to send the data
  InternalUpdate();
  
  if (!sleepInfo.m_runUlp)
  {
    uint32_t timer = 0xffffffff;
    if (g_lastCommit != 0)
    {
      //don't keep counter data in volatile RTC mem for too long
      //Write pending data to non volatile memory after a while
      timer = g_commitPeriod;
      LOG("MM:vlt data timer:%d\n\r", timer);
    }
    else if (g_lastDataCommitTime != 0)
    {
      //pendingData: don't keep counter data around for too long before sending to server
      timer = std::max(0, (dataRetention) - ( GetUnixtime() - g_lastDataCommitTime));
      
      LOG("MM:pending data timer:%d\n\r", timer);
    }

    if (timer != 0xffffffff)
      sleepInfo.m_sleepManager.SetWakeFromTimer(timer);
  }
}

void MetricsManager::InternalUpdate(bool *userCountersHaveData)
{
  //commit data to non volatile memory before activating online to prevent data loss
  //commit data after retention otherwise continuous new data would prevent online transfer
  int32_t const elapsedCommit = (g_lastCommit != 0)  ? (GetUnixtime() - g_lastCommit) : 0;
  bool const commitElapsed = IsPeriodOver(elapsedCommit, g_commitPeriod);
  bool const mustCommit = commitElapsed || m_maxRetention;

  bool countersHaveData = false;
  
  auto onCounter =[&](Counter &counter)
  {
    if(counter.NeedsCommit() || ( counter.HasData() && mustCommit ) )
    {
      LOG("Committing counter 0x%08x\n\r", counter.GetUID());
      TESTSTEP("Committing counter 0x%08x", counter.GetUID());

      Chunk chunk;
      counter.GetChunk(chunk);
      AddChunk(chunk);
      counter.Reset();
    }
    else
    {
      countersHaveData |= counter.HasData();
    }
  };

  if (m_counters)
  {
    m_counters->ForEachCounter(onCounter);
  }

  if (countersHaveData)
  {
    //g_lastCommit must be checked in case app is in main loop
    if (!g_lastCommit)
    {
      g_lastCommit = GetUnixtime();
      LOG("MM:commit res\n\r");
    }
  }
  else
    g_lastCommit = 0;

  if (m_maxRetention)
  {     
    m_maxRetention = false;

    DO_ONCE(LOG("MetricsManager:Online requested, local pending data to send\n\r"));
    TESTSTEP("MetricsManager:Online requested, local pending data to send");

    RequestWifiAccess();
    
    SendPendingData();

    if (!IsOnline())
    {
      //cannot get online, wait for longer before retrying 
      LOG("MetricsManager:WARNING, cannot get online, data retention prolonged\n\r");
      TESTSTEP("MetricsManager:data retention prolonged");
    }    
  }
  
  //must call HasPendingData again in case SendPendingData failed
  bool const hasPendingData = HasPendingData();
  if ( (countersHaveData || hasPendingData) && (g_lastDataCommitTime == 0))
  {
    g_lastDataCommitTime = GetUnixtime();
    LOG("MM:data ret reset\n\r");
  }

  if (userCountersHaveData)
    *userCountersHaveData = countersHaveData;
}

void MetricsManager::Update()
{
  GJ_PROFILE(MetricsManager::Update);
  
  InternalUpdate();
}

void MetricsManager::Visit(OnChunk cb)
{
  LOG("MetricsManager::Visit\n\r");

  ForEachChunk(cb);
}

void MetricsManager::PrepareLocalStorage(uint32_t &offset)
{
  if (!ReadHeader(offset))  
  {
    //LOG("Header not found, formatting @%x\n\r", offset);
    m_localStorage.Format();
    
    MemoryHeader header;

    m_localStorage.WriteBytes(0, &header, sizeof(header));
    offset += sizeof(header);
  }
  else
  {
    SkipChunks(offset);
  }
}

bool MetricsManager::ReadHeader( uint32_t &offset ) const
{
  MemoryHeader const headerTest;
  char buffer[sizeof(MemoryHeader)] = {};
  
  m_localStorage.ReadBytes( offset, buffer, sizeof(MemoryHeader) );

  if ( memcmp( &headerTest, buffer, sizeof(MemoryHeader) ))
    return false;
  
  //LOG("ReadHeader: found header\n\r");
  
  offset += sizeof(MemoryHeader);

  return true;
}

bool MetricsManager::SkipChunks( uint32_t &offset ) const
{
  bool found(false);

  {
    while (SkipChunk(offset))
    {
      found = true;
    }
  }

  if (!found)
  {
    LOG("MetricsManager::SkipChunks: nothing found in memory\n\r");
  }

  return found;
}

bool MetricsManager::SkipChunk(uint32_t &offset) const
{
  //SER("Skipchunk read at 0x%x\n\r", offset);

  ChunkHeader header = {};

  m_localStorage.ReadBytes(offset, &header, sizeof(header));
  
  if (header.m_id != ChunkHeader::ID)
  {
    //SER("Skipchunk id KOK 0x%x i 0x%x s 0x%x f 0x%x\n\r", offset, header.m_id, header.m_size, header.m_format);

    return false;
  }

  offset += sizeof(header);
  offset += header.m_size;

  return true;
}

bool MetricsManager::ReadChunk(uint32_t &offset, Chunk &chunk) const
{
  ChunkHeader header = {};

  //SER("Reading chunk at 0x%x\n\r", offset);

  m_localStorage.ReadBytes(offset, &header, sizeof(header));
  offset += sizeof(header);

  if (header.m_id != ChunkHeader::ID)
  {
    //SER("id KOK 0x%x i 0x%x s 0x%x f 0x%x\n\r", offset, header.m_id, header.m_size, header.m_format);

    return false;
  }

  chunk.m_format = header.m_format;
  chunk.m_uid = header.m_uid;
  chunk.m_data.resize(header.m_size);
  m_localStorage.ReadBytes(offset, chunk.m_data.data(), header.m_size);    
  offset += header.m_size;

  return true;
}

void MetricsManager::ForEachChunk(OnChunk cb) const
{
  uint32_t offset = 0;
  if (!ReadHeader(offset))  
  {
    return;
  }

  {
    Chunk chunk;

    while (ReadChunk(offset, chunk))
    {
      bool const canContinue = cb(chunk);
      if (!canContinue)
        break;
    }
  }

  m_localStorage.Finish();  //ex:close file
}
  
bool MetricsManager::HasPendingData() const
{
  if (m_hasPendingData != PendingData::Unknown)
    return m_hasPendingData == PendingData::Yes; 

  bool hasChunk = false;
  auto onChunk = [&](Chunk const &chunk) -> bool
  {
    hasChunk = true;
    return false;
  };
  
  ForEachChunk(onChunk);

  m_hasPendingData = hasChunk ? PendingData::Yes : PendingData::No;

  //SER("HasPendingData update:enum=%d\n\r", m_hasPendingData);

  return hasChunk;
}

void MetricsManager::AddChunk(Chunk const &chunk)
{
  //if (!StoreOnRemoteStorage(chunk))
  {
    //LOG("WARNING Remote storage failed: Committing chunk to local storage\n\r");
    
    uint32_t offset(0);
    PrepareLocalStorage(offset);

    //SER("New chunk at 0x%x\n\r", offset);
    
    TESTSTEP("MM:New chunk at 0x%x", offset);

    ChunkHeader header;
    header.m_id = ChunkHeader::ID;
    header.m_size = (uint16_t)chunk.m_data.size();
    header.m_format = chunk.m_format;
    header.m_uid = chunk.m_uid;

    m_localStorage.WriteBytes(
        offset, 
        &header,
        sizeof(header) );

    offset += sizeof(header);

    m_localStorage.WriteBytes(
        offset, 
        chunk.m_data.data(),
        chunk.m_data.size() );

    offset += chunk.m_data.size();

    m_localStorage.Finish();  //ex:close file

    m_hasPendingData = PendingData::Yes;
  }
}

bool MetricsManager::StringizeChunk(GJString &postData, Chunk const &chunk, const char* suffix) const
{
  MetricsDataFormat const *dataFormat = nullptr;
  
  for (MetricsDataFormat const *format : m_dataFormats)
  {
    if (format->GetID() == chunk.m_format)
    {
      LOG("  Data format '%s' selected for chunk 0x%08x\n\r", format->GetDescription(), chunk.m_uid);
      dataFormat = format;
      break;
    }
  }
  
  if (!dataFormat)
  {
    LOG("  Not sending chunk 0x%08x, format 0x%04x unknown\n\r", chunk.m_uid, chunk.m_format);
    return false;
  }

  //TODO: send period length
  
  char buffer[128];
  sprintf(buffer, "mod%s=%s&cnt%s=%d&chunk%s=", suffix, m_iotID.c_str(), suffix, chunk.m_uid, suffix);

  postData += buffer;

  //postData += "mod=";
  //postData += m_iotID;
  //postData += "&cnt=";
  //postData += chunk.m_uid;
  //postData += "&";
  
  postData += dataFormat->ToURIString(chunk.m_data.data(), chunk.m_data.size());

  return true;
}

bool MetricsManager::SendPendingData()
{
  g_lastDataCommitTime = 0;

  bool const isOnline = IsWifiConnected();
  if (!isOnline || !HasDataServer())
    return true;

  uint32_t offset = 0;
  
  if (!ReadHeader(offset))  
    return true;

  LOG("MetricsManager::SendPendingData\n\r");
  
  bool found(false);

  Chunk chunk;

  GJString urlData;

  
  uint32_t suffix = 0;

  while (ReadChunk(offset,chunk))
  {
    //SER("Chunk read at %x\n\r", offset);

    char suffixString[8];

    sprintf(suffixString, "%d", suffix);

    StringizeChunk(urlData, chunk, suffixString);

    urlData += "&";

    suffix++;
    found = true; 

    //url length < 2048 will work everywhere
    if (urlData.length() >= 1600)
    {
      urlData.remove(urlData.length()-1); //remove trailing '&'
      if (!SendChunkUrl(urlData.c_str()))
        return false;

      suffix = 0;
      urlData.clear();
    }
  }

  if (!urlData.isEmpty())
    urlData.remove(urlData.length()-1); //remove trailing '&'

  if (!SendChunkUrl(urlData.c_str()))
    return false;
  
  //if (found)
  {
    LOG("  All data sent, formatting memory\n\r");
    m_localStorage.Format();
  }
  
  m_hasPendingData = PendingData::No;

  return true;
}

bool MetricsManager::SendChunkUrl(const char *url) const
{
  bool ret = false;
  bool const isOnline = IsWifiConnected();
  if (isOnline && HasDataServer())
  {
    LOG("Sending chunk to remote storage:");
    LOG_LARGE(url);
    LOG("\n\r");
    
    auto logPostResponse = [](GJString &postResponse, bool error)
    {
      char *it = postResponse.begin();
      char *itEnd = postResponse.end();

      uint32_t maxSize = 256;
      while(it < itEnd)
      {
        uint32_t remain = itEnd - it;
        uint32_t batch = std::min(remain, maxSize);

        char prev = it[batch];

        it[batch] = 0;

        if (error)
        {
          GJ_ERROR(it);
        }
        else
        {
          LOG(it);
        }
        
        it[batch] = prev;

        it += batch;
      }
    };

    GJString postResponse;
    if (HttpPost( m_remoteStorageUri.c_str(), url, strlen(url), postResponse ))
    {
      if (strncmp(postResponse.c_str(), "OK", 2) == 0)
      {
        LOG("  SUCCESS\n\r");
        TESTSTEP("MetricsManager:Send chunk success");
        ret = true;
      }
      else
      {
        LOG("  ERROR:Server is inactive\n\r");
        LOG("  Http data server reponse:");
        logPostResponse(postResponse, false);
        LOG("\n\r");  
      }
    }
    else
    {
      GJ_ERROR("  ERROR:HttpPost() failed:");
      logPostResponse(postResponse, true);
      GJ_ERROR("\n\r");  
    }
  }

  return ret;
}

bool MetricsManager::SendChunk(Chunk const &chunk)
{
  bool ret = false;

  GJString postData;

  //MetricsDataFormat const *dataFormat = nullptr;
  //
  //for (MetricsDataFormat const *format : m_dataFormats)
  //{
  //  if (format->GetID() == chunk.m_format)
  //  {
  //    LOG("  Data format '%s' selected for chunk 0x%08x\n\r", format->GetDescription(), chunk.m_uid);
  //    dataFormat = format;
  //    break;
  //  }
  //}
  //
  //if (!dataFormat)
  //{
  //  LOG("  Not sending chunk 0x%08x, format 0x%04x unknown\n\r", chunk.m_uid, chunk.m_format);
  //  return true;
  //}

  //TODO: send period length
  
  //postData = "mod=";
  //postData += m_iotID;
  //postData += "&cnt=";
  //postData += chunk.m_uid;
  //postData += "&";
  //
  //postData += dataFormat->ToURIString(chunk.m_data.data(), chunk.m_data.size());

  if (!StringizeChunk(postData, chunk, "0"))
  {
    LOG("  StringizeChunk failed, chunk 0x%08x discarded, format 0x%04x\n\r", chunk.m_uid, chunk.m_format);
    return true;
  }

  ret = SendChunkUrl(postData.c_str());

  return ret;
}

void MetricsManager::Reset()
{
  if (m_counters)
  {
    auto onCounter = [](Counter &counter)
    {
      counter.Reset();
    };
    
    m_counters->ForEachCounter(onCounter);
  }
    
  m_localStorage.Format();
  Init(m_counters);
}

bool MetricsManager::HasDataServer() const
{
  if (m_remoteStorageUri.isEmpty())
    return false;

  return IsDataServerEnabled();
}

void MetricsManager::EnableDataServer(bool enable)
{
  enableDataServer = enable;
}

bool MetricsManager::IsDataServerEnabled() const
{
  return enableDataServer;
}

uint32_t MetricsManager::GetRetentionPeriod() const
{
  return dataRetention;
}
void MetricsManager::SetRetentionPeriod(uint32_t interval)
{
  dataRetention = interval;
}

void MetricsManager::CommitCounters()
{
  auto onCounter =[&](Counter &counter)
  {
    if (!counter.HasData())
      return;

    if( counter.CanCommit() )
    {
      Chunk chunk;
      
      counter.GetChunk(chunk);
      AddChunk(chunk);
      counter.Reset();
    }
  };
  
  if (m_counters)
  {
    m_counters->ForEachCounter(onCounter);
  }
}
  
bool MetricsManager::RetentionPeriodReached() const
{
  if (g_lastDataCommitTime == 0)
    return false;
    
  uint32_t const elapsed = GetUnixtime() - g_lastDataCommitTime;
  
  return elapsed >= GetRetentionPeriod();
}

