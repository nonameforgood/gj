#include "valuecounter.h"
#include "datetime.h"
#include <string.h>
#include <rom/crc.h>

struct ValueChunk
{
  uint32_t m_unixtime;
  uint32_t m_value;
};

ValueCounter::ValueCounter(uint32_t uid, const char *desc)
: Counter(uid, desc)
{

}

void ValueCounter::Reset()
{
  SER("ValueCounter::Reset\n\r");
  
  m_unixtime = 0xffffffff;
  m_value = 0;
}
    
void ValueCounter::SetValue(uint32_t value)
{
  m_unixtime = GetUnixtime();
  m_value = value;
}

bool ValueCounter::HasData() const
{
  return m_unixtime != 0xffffffff;
}

void ValueCounter::GetChunk(Chunk &chunk) const
{
  ValueChunk valueChunk = {m_unixtime, m_value};
  ValueCounterDataFormat format;

  chunk.m_uid = GetUID();
  chunk.m_format = format.GetID();
  chunk.m_data.resize(sizeof(valueChunk));
  memcpy(chunk.m_data.data(), &valueChunk, sizeof(valueChunk));
}

bool ValueCounter::NeedsCommit() const
{
  return HasData();
}

bool ValueCounter::CanCommit() const
{
  return HasData();
}

const char *ValueCounterDataFormat::GetDescription() const
{
  return "ValueCounterDataFormat";
}

uint16_t ValueCounterDataFormat::GetID() const
{
  static uint16_t s_id = 0;
  
  if (s_id == 0)
  {
    char buffer[256];
    
    uint32_t ret = sprintf(buffer, 
      "ValueChunk=%d bytes,"
      "uint32_t m_unixtime,"
      "uint32_t m_value",
      sizeof(ValueChunk)
      );
    
    /*REPLACE ME*/
    s_id = crc16_le(0, (uint8_t*)buffer, ret);

    SER("ValueCounterDataFormat ID:0x%04x\n\r", s_id);
  }
  
  return s_id;
}

String ValueCounterDataFormat::ToURIString(const uint8_t *data, uint32_t size) const
{
  ValueChunk const *chunk = (ValueChunk*)data;

  String uri;

  //uri += "time=";
  uri += chunk->m_unixtime;
  //uri += "&value=";
  uri += ",";
  uri += chunk->m_value;
  
  return uri;
}