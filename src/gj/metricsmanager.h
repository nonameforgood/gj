#pragma once

#include "string.h"
#include "vector.h"
#include "function_ref.hpp"

class ChunkStorage;
class CounterContainer;
struct SleepInfo;
struct Chunk;
class MetricsDataFormat;

class MetricsManager
{
  public:

    MetricsManager(ChunkStorage &mem);

    void SetIotID(const char *id);

    void Init(CounterContainer *counters, const char *dataServerUrl = nullptr);
    void SetDataFormats(Vector<MetricsDataFormat const*> const &dataFormats);
    
    void Update();
    void CommitCounters();
    void PrepareSleep(SleepInfo const &info);
    
    void Reset();	//causion:erases all metrics from provided ChunkStorage

    typedef tl::function_ref<bool(Chunk const &chunk)> OnChunk;
    void Visit( OnChunk visitor );

private:
  GJString m_iotID;

  CounterContainer *m_counters = nullptr;
  Vector<MetricsDataFormat const*> m_dataFormats;
  ChunkStorage &m_localStorage;
  GJString m_remoteStorageUri;
  
  bool m_maxRetention = false;

  static MetricsManager *ms_instance;

  enum class PendingData
  {
    Unknown,
    No,
    Yes
  };
  mutable PendingData m_hasPendingData = PendingData::Unknown;
 
  uint32_t GetRetentionPeriod() const;
  void SetRetentionPeriod(uint32_t interval);
    
  bool RetentionPeriodReached() const;
  void AddChunk(Chunk const &chunk);
  bool IsDataServerEnabled() const;
  bool HasDataServer() const;
  void EnableDataServer(bool enable);
    
  bool HasPendingData() const;
  bool SendPendingData();
  bool SendChunk(Chunk const &chunk);
  bool StringizeChunk(GJString &dst, Chunk const &chunk, const char* suffix) const;
  bool SendChunkUrl(const char *url) const;
  
  void PrepareLocalStorage(uint32_t &offset);
  bool ReadHeader(uint32_t &offset) const;
  bool SkipChunks(uint32_t &offset) const;
  bool ReadChunk(uint32_t &offset, Chunk &chunk) const;
  bool SkipChunk(uint32_t &offset) const;

  void InternalUpdate(bool *countersHaveData = nullptr);
    
  void ForEachChunk(OnChunk cb) const;

  static void Command_shortretention();
  static void Command_mediumretention();
  static void Command_normalretention();
  static void Command_commitcounters();
  static void Command_dataserveron();
  static void Command_dataserveroff();
  static void Command_sendpendingmetrics();
};

void SendPendingData(const char *serverUrl, MetricsManager &metricsManager);
