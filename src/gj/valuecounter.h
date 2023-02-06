#pragma once

#include "counter.h"
#include "memchunk.h"

class ValueCounter : public Counter
{
  public:
    ValueCounter(uint32_t uid, const char *desc);

    void Reset();
    
    void SetValue(uint32_t value);

    virtual bool HasData() const;
    virtual void GetChunk(Chunk &chunk) const;
    
    virtual bool NeedsCommit() const;
    virtual bool CanCommit() const;

private:

  uint32_t m_unixtime = 0xffffffff;
  uint32_t m_value = 0;
};

class ValueCounterDataFormat : public MetricsDataFormat
{
  public:

    virtual uint16_t GetID() const;
    virtual const char *GetDescription() const;
    virtual String ToURIString(const uint8_t *data, uint32_t size) const;
};
