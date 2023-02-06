
#pragma once

#include "counter.h"
#include "memchunk.h"

class EventCounter : public Counter
{
public:
  EventCounter( uint32_t uid, const char *desc, uint32_t periodLength );

  void SetPeriodLength(uint32_t length);
  uint32_t GetPeriodLength() const;
  
  void Init();
  void Reset();//caution, this erases all data
  void Load(void const *data, uint32_t size);
  
  void IncEvent();
  void AddEvents(uint32_t count);

  void EnableLog(bool enable);

  virtual bool HasData() const;
  virtual void GetChunk(Chunk &chunk) const;
  void Save(Vector<uint8_t> &data) const;
  
  bool NeedsCommit() const;
  bool CanCommit() const;

private:
  
  bool m_enableLog = true;
  int32_t m_timeOffset = 0;

  PeriodChunk m_chunk;
  uint16_t m_periodIndex = 0xffff;
  
  struct SavedState;

  void PrintState( SavedState const &state, const char *prefix = "" ) const;
};

