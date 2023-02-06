#include "chunkstorage.h"
#include "file.h"

class FileStorage : public ChunkStorage
{
public:
  FileStorage(const char *filepath);

  virtual void Format();
  
  virtual uint8_t ReadByte(int const offset);
  virtual void WriteByte(int const offset, uint8_t const val);

  virtual void ReadBytes(int const offset, void *mem, uint16_t size );
  virtual void WriteBytes(int const offset, void const *mem, uint16_t size);

  virtual bool TryReadByte(int const address, uint8_t &value);
  
  virtual void Finish();
  
private:
  const char *m_filepath = nullptr;
  GJFile m_file;

  bool m_reading = false;
  
  bool AutoOpenRead();
  bool AutoOpenWrite();
};
