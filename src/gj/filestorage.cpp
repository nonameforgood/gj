#include "filestorage.h"
#include "base.h"

FileStorage::FileStorage(const char *filepath)
: ChunkStorage(32*1024)
, m_file()
, m_filepath(filepath)
{
  
}

bool FileStorage::AutoOpenRead()
{
  if (!m_file)
  {
    if (!GJFile::Exists(m_filepath))
      return false;
      
    m_file.Open(m_filepath, GJFile::Mode::Read);
    
    m_reading = m_file;
  }
  
  return m_file;
}

bool FileStorage::AutoOpenWrite()
{
  if (m_reading)
  {
    m_file.Close();
    m_reading = false;
  }
  
  if (!m_file)
  {
    m_file.Open(m_filepath, GJFile::Mode::Write);
  }
  
  return m_file;
}

void FileStorage::Format()
{
  if (m_file)
    m_file.Close();
  if (GJFile::Exists(m_filepath))
  {
    LOG("Deleting file storage %s...\n\r", m_filepath);
    GJFile::Delete(m_filepath);
  }
  m_reading = false;
}
  
uint8_t FileStorage::ReadByte(int const offset)
{
  uint8_t dest = 0;
  
  if (!TryReadByte(offset, dest))
  {
    LOG("ERROR: FileStorage::ReadByte @0x%x failed\n\r", offset);
  }

  return dest;
}

bool FileStorage::TryReadByte(int const offset, uint8_t &value)
{
  if (!AutoOpenRead())
  {
    return false;
  }

  m_file.Seek(offset, fs::SeekSet);
  
  uint32_t ret = m_file.Read(&value, 1);

  return ret == 1;
}

void FileStorage::WriteByte(int const offset, uint8_t const val)
{
  if (!AutoOpenWrite())
  {
    LOG("ERROR: FileStorage::WriteByte: can't open %s for write\n\r", m_filepath);
    return;
  }

  m_file.Seek(offset, fs::SeekSet);
  
  uint32_t ret = m_file.Write(&val, 1);
  if (ret != 1)
  {
    LOG("ERROR: FileStorage::WriteByte @0x%x failed:%d\n\r", offset, ret);
  }
}

void FileStorage::ReadBytes(int const offset, void *mem, uint16_t size )
{
  if (!AutoOpenRead())
  {
    return;
  }

  uint8_t dest = 0;
  
  m_file.Seek(offset, fs::SeekSet);
  
  uint32_t ret = m_file.Read(mem, size);
  if (ret != size)
  {
    //LOG("ERROR: FileStorage::ReadBytes @0x%x failed:%d\n\r", offset, ret);
  }
}

void FileStorage::WriteBytes(int const offset, void const *mem, uint16_t size)
{
  if (!AutoOpenWrite())
  {
    LOG("ERROR: FileStorage::WriteBytes: can't open %s for write\n\r", m_filepath);
    return;
  }

  m_file.Seek(offset, fs::SeekSet);
  
  uint32_t ret = m_file.Write(mem, size);
  if (ret != size)
  {
    LOG("ERROR: FileStorage::WriteBytes @0x%x failed:%d\n\r", offset, ret);
  }
}

void FileStorage::Finish()
{
  m_file.Close();
}
  

