#include "chunkstorage.h"

class FlashStorage : public ChunkStorage
{
public:
  FlashStorage(uint16_t sector);

  virtual void Format();
  
  virtual uint8_t ReadByte(int const address);
  virtual void WriteByte(int const address, uint8_t const val);

  virtual void ReadBytes(int const address, void *mem, uint16_t size );
  virtual void WriteBytes(int const address, void const *mem, uint16_t size);

private:
  uint16_t const m_sector;
};
