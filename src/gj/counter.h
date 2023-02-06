#pragma once
#include "vector.h"
#include "base.h"

struct Chunk
{
  uint32_t m_uid;
  uint16_t m_format;

  Vector<uint8_t> m_data;
};

class Counter
{
public:
  Counter(uint32_t uid, const char *desc);
  uint32_t GetUID() const;
  const char *GetDesc() const;

  void SetDataConverter(uint32_t id);
  
  virtual void Init() {}
  virtual void Reset() {}
  virtual void Load(void const *data, uint32_t size) {}

  virtual bool HasData() const = 0;
  virtual void GetChunk(Chunk &chunk) const = 0;
  virtual void Save(Vector<uint8_t> &data) const {}
  
  virtual bool NeedsCommit() const;
  virtual bool CanCommit() const;

private:
  uint32_t const m_uid;
  char m_desc[5] = {};
};

