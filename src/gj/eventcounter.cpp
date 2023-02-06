#include "eventcounter.h"
#include "millis.h"
#include "datetime.h"
#include "base.h"
#include "test.h"

struct EventCounter::SavedState
{
  PeriodChunk m_chunk;
  uint16_t m_periodIndex = 0;
};

EventCounter::EventCounter(uint32_t uid, const char *desc, uint32_t periodLength)
: Counter(uid, desc)
{
  m_chunk.m_periodLength = periodLength;
  Reset();
}

void EventCounter::SetPeriodLength(uint32_t length)
{
  m_chunk.m_periodLength = length;
}

uint32_t EventCounter::GetPeriodLength() const
{
  return m_chunk.m_periodLength;
}
  
void EventCounter::Init()
{
  memset(m_chunk.m_periods, 0, sizeof(m_chunk.m_periods));
  m_periodIndex = 0xffff;
  m_chunk.m_unixtime = 0;
}

void EventCounter::Reset()
{
  Init();
}

void EventCounter::EnableLog(bool enable)
{
  m_enableLog = enable;
}

void EventCounter::Load(void const *data, uint32_t size)
{
  if (!data)
    return;

  if (size != sizeof(SavedState))
  {
    LOG("ERROR:Event counter load supplied with invalid size of %d bytes(expected %d bytes)\n\r", size, sizeof(SavedState));
    return;
  }

  SavedState &savedState = *(SavedState*)data;

  {
    LOG("ec l:");
    PrintState(savedState, "");

    memcpy(&m_chunk, &savedState.m_chunk, sizeof(m_chunk));
    m_periodIndex = savedState.m_periodIndex;

    char date[20];
    int32_t unixtime = GetUnixtime();
    ConvertEpoch(unixtime, date);
    {
      int32_t const seconds = unixtime - m_chunk.m_unixtime;
      m_timeOffset = seconds * 1000;
      SER_COND(m_enableLog, "  new time offset:%d seconds (%s)\n\r", seconds, date);
    }
  }
}

bool EventCounter::NeedsCommit() const
{
  //flag as "NeedsCommit" as soon as the last slot is used
  uint32_t const maxSlot = PeriodChunk::MaxPeriod - 1;
  return m_periodIndex != 0xffff && m_periodIndex >= maxSlot;
}

bool EventCounter::CanCommit() const
{
  return HasData();
}

void EventCounter::Save(Vector<uint8_t> &data) const
{
  if (!HasData())
    return;
  
  SavedState savedState;
  memcpy(&savedState.m_chunk, &m_chunk, sizeof(m_chunk));
  savedState.m_periodIndex = m_periodIndex;
  
  data.resize(sizeof(SavedState));
  memcpy(data.data(), &savedState, sizeof(SavedState));
  
  LOG("ec s:");
  PrintState(savedState, "");
}

void EventCounter::PrintState( SavedState const &state, const char *prefix ) const
{
  PrintPeriodChunk(state.m_chunk, prefix);
  LOG("\n\r");
}

void EventCounter::GetChunk(Chunk &chunk) const
{
  PeriodChunkDataFormat format;
  
  chunk.m_format = format.GetID();
  chunk.m_uid = GetUID();

  chunk.m_data.resize(sizeof(m_chunk));
  memcpy(chunk.m_data.data(), &m_chunk, sizeof(m_chunk));
}

void EventCounter::IncEvent()
{
  AddEvents(1);
}

void EventCounter::AddEvents(uint32_t count)
{
  if (!count)
  {
    return;
  }

  if (m_chunk.m_unixtime == 0)
  {
    m_chunk.m_unixtime = GetUnixtime();
  }

  int32_t ms = GJMillis();
  int32_t elapsed = ms + m_timeOffset;
  int32_t period = elapsed / m_chunk.m_periodLength;

  int32_t const previousPeriod = (m_periodIndex == 0xffff) ? -1 : m_chunk.m_periods[m_periodIndex].m_index;

  if (period != previousPeriod)
  {
    uint32_t const maxSlot = PeriodChunk::MaxPeriod - 1;
    
    if (m_periodIndex == 0xffff)
    {
      m_periodIndex = 0;
    }
    else if (m_periodIndex >= maxSlot)
    {
      DO_ONCE(LOG("ERROR:Event counter %x(%s) is full, events no longer counted\n\r", GetUID(), GetDesc()));
      return;
    }
    else
    {
      m_periodIndex++;
      LOG("New event counter index:%d\n\r", m_periodIndex);
    }

    m_chunk.m_periods[m_periodIndex].m_index = period;
  }

  LOG("ec add '%.4s'(0x%x) %ds,%d,%d\n\r", GetDesc(), GetUID(), elapsed / 1000, period, count);
  
  TESTSTEP("EventCounter::AddEvents");

  m_chunk.m_periods[m_periodIndex].m_count += count;
}

bool EventCounter::HasData() const
{
  return m_periodIndex != 0xffff;
}
