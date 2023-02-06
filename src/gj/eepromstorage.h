#include "memchunk.h"
#include "chunkstorage.h"
#include <EEPROM.h>

class EEPROMStorage : public EEPROMClass, public ChunkStorage
{
public:
  EEPROMStorage();

  virtual uint8_t ReadByte(int const address);
  virtual void WriteByte(int const address, uint8_t const val);

  virtual void ReadBytes(int const address, void *mem, uint16_t size );
  virtual void WriteBytes(int const address, void const *mem, uint16_t size);
};
