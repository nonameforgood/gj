#include "chunkstorage.h"

class RAMStorage : public ChunkStorage
{
public:
  RAMStorage( void *mem, uint16_t size );
  virtual uint8_t ReadByte(int const address);
  virtual void WriteByte(int const address, uint8_t const val);

  virtual void ReadBytes(int const address, void *mem, uint16_t size );
  virtual void WriteBytes(int const address, void const *mem, uint16_t size);

private:
  uint8_t * const m_mem;
};