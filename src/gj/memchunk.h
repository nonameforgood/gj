#pragma once

#include "base.h"
#include "metricsdataformat.h"

struct Period
{
  uint16_t m_index;
  uint16_t m_count;
};

struct PeriodChunk
{
  //char const m_id[4] = {'G','J','P','0'};
  //uint32_t m_uid = 0;
  //char m_desc[4] = {};
  int32_t m_periodLength = 0;
  int32_t m_unixtime = 0;

  static const uint16_t MaxPeriod = 4;
  Period m_periods[MaxPeriod];
};

class PeriodChunkDataFormat : public MetricsDataFormat
{
  public:
  
  uint16_t GetID() const;
  virtual const char *GetDescription() const;
  String ToURIString(const uint8_t *data, uint32_t size) const;
};

void PrintPeriodChunk(PeriodChunk const &chunk, const char *prefix = "");