#pragma once

#include "base.h"

class ChunkStorage
{
public:
  inline ChunkStorage(uint16_t size)
  : m_size(size)
  {
    
  }

  inline uint16_t GetSize() const { return m_size;} 

  virtual inline void Format()
  {
    uint8_t mem[16] = {};

    for ( uint32_t i = 0 ; i < GetSize() ; i += 16 )
    {
      WriteBytes( i, mem, 16 );
    }
  }
  
  virtual uint8_t ReadByte(int const address) = 0;
  virtual void WriteByte(int const address, uint8_t const val) = 0;

  virtual void ReadBytes(int const address, void *mem, uint16_t size ) = 0;
  virtual void WriteBytes(int const address, void const *mem, uint16_t size) = 0;

  virtual bool TryReadByte(int const address, uint8_t &value) { value = ReadByte(address); return true; }
  
  virtual void Finish() {}
  
private:
  uint16_t const m_size;
};
