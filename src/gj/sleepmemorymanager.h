#pragma once
#include <stdint.h>
#include "Vector.h"


class SleepMemoryManager
{
public:

  SleepMemoryManager( void *rtcMemory = nullptr, uint32_t size = 0 );
	
  struct Header;
  struct Chunk;

  void Clear();

  bool Load( uint32_t uid, Vector<uint8_t> &data ) const;
  
  bool Load( uint32_t uid, void *data, uint32_t expectedSize );
  bool Store( uint32_t uid, void const *data, uint32_t size );

private:

  void * const m_memory;
  uint32_t const m_size;

	bool Write( void *dest, void const *data, uint32_t size );

  template <typename T>
  void ForEach(T &callable) const;
};