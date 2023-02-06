#include "memchunk.h"
#include "datetime.h"
#include <rom/crc.h>

void PrintPeriodChunk(PeriodChunk const &chunk, const char *prefix)
{
  //char date[20];
  //ConvertEpoch(chunk.m_unixtime, date);
  LOG("%s t:%d ", prefix, chunk.m_unixtime);
 
  for ( int i = 0 ; i < PeriodChunk::MaxPeriod ; ++i )
  {
    Period const &p = chunk.m_periods[i];
    LOG(" %d,%d,", p.m_index, p.m_count);
  }
}

const char *PeriodChunkDataFormat::GetDescription() const
{
  return "PeriodChunkDataFormat";
}

uint16_t PeriodChunkDataFormat::GetID() const
{
  static uint16_t s_id = 0;
  
  if (s_id == 0)
  {
    char buffer[256];
    
    uint32_t ret = sprintf(buffer, 
      "PeriodChunk=%d bytes,"
      "int32_t m_periodLength,"
      "int32_t m_unixtime,"
      "MaxPeriods=%d,"
      "uint16_t Period::m_index,"
      "uint16_t Period::m_count",
      sizeof(PeriodChunk),
      PeriodChunk::MaxPeriod
      );
    
    s_id = crc16_le(0, (uint8_t*)buffer, ret);

    //SER("PeriodChunkDataFormat ID:0x%04x\n\r", s_id);
  }
  
  return s_id;
}

String PeriodChunkDataFormat::ToURIString(const uint8_t *data, uint32_t size) const
{
  PeriodChunk const &periodChunk = *(PeriodChunk*)data;
  String uri;

  //uri += "time=";
  uri += periodChunk.m_unixtime;
  //uri += "&periods=";
  uri += ",";
  for ( int i = 0 ; i < PeriodChunk::MaxPeriod ; ++i )
  {
    uri += periodChunk.m_periods[i].m_index;
    uri += ",";
    uri += periodChunk.m_periods[i].m_count;
    uri += ",";
  }

  uri.remove(uri.length()-1); //remove trailing comma

  return uri;
}