#include "appendonlyfile.h"


#if defined(NRF)

  #if defined(NRF_SDK12)
    #include <fstorage.h>
    #include "softdevice_handler.h"
  #elif defined(NRF_SDK17)
    #include <nrf_fstorage.h>
    #include <nrf_sdh.h>
  #endif
  

#include "nrf51utils.h"
#endif


AppendOnlyFile::AppendOnlyFile(const char *name)
{
  m_nameCrc = 0;
  m_offset = 0;
  m_offsetEnd = 0;

#if defined(NRF)
  const FileSectorsDef *def = GetFileSectorDef(name);
  if (def)
  {
    m_nameCrc = def->m_nameCrc;
    m_offset = def->m_sector;
    m_offsetEnd = def->m_sector + (def->m_sectorCount * 1024);
  }
#endif
}

AppendOnlyFile::~AppendOnlyFile()
{
  Flush();
};

void AppendOnlyFile::DebugFlush()
{
  Flush();
}

bool AppendOnlyFile::IsValid() const
{
#if defined(NRF)
  if (m_offset >= NRF_FLASH_SIZE)
    return false;

  if (m_offsetEnd > NRF_FLASH_SIZE)
    return false;
    
  if (m_offsetEnd == 0)
    return false;

#endif

  return true;
}

void AppendOnlyFile::Erase()
{
#if defined(NRF)
  uint32_t off = m_offset;

  while(off < m_offsetEnd)
  {
    EraseSector(off);
    FlushSectorWrite();
    off += NRF_FLASH_SECTOR_SIZE;
  }

  m_headerForWrite.m_magic = Magic;
  m_headerForWrite.m_crc = m_nameCrc;

  WriteToSector(m_offset, (uint8_t*)&m_headerForWrite, sizeof(Header));

  DebugFlush();

  m_blockOffset = 0;
  m_writeOffset = 0;
  m_writeOffsetEnd = 0;
#endif
}

bool AppendOnlyFile::BeginWrite(uint32_t size)
{
  if (!IsValid())
    return false;

  if (m_blockOffset == 0)
  {
    const Header *header = (Header*)m_offset;

    if (header->m_magic != Magic)
      return false;

    if (header->m_crc != m_nameCrc)
      return false;
    
    //find next avail memory
    m_blockOffset = (m_offset + sizeof(Header));
    auto onBlock = [&](uint32_t size, const void *data)
    {
      m_blockOffset = (uint32_t)((char*)data + size);
    };

    bool readAll(true);
    ForEachInternal(onBlock, readAll);
  } 

  const uint32_t remainingSize = m_offsetEnd - m_blockOffset;
  const uint32_t requiredSize = size + sizeof(BlockHeader);

  if (remainingSize < requiredSize)
    return false;

  m_blockHeaderForWrite.m_size = size;
  m_blockHeaderForWrite.m_commit = 0xff;

  #if defined(NRF)
  WriteToSector(m_blockOffset, (uint8_t*)&m_blockHeaderForWrite, sizeof(BlockHeader));
  #endif

  DebugFlush();

  m_writeOffset = sizeof(BlockHeader); //skip block header
  m_writeOffsetEnd = requiredSize;

  return true;
}

bool AppendOnlyFile::EndWrite()
{
  bool ret = false;

  if (m_writeOffsetEnd != 0 && m_writeOffset == m_writeOffsetEnd)
  {
    //commit block
    m_blockHeaderForWrite.m_commit = 0x0;
#if defined(NRF)
    WriteToSector(m_blockOffset, (uint8_t*)&m_blockHeaderForWrite, sizeof(BlockHeader));
#endif
    DebugFlush();

    m_blockOffset = m_blockOffset + m_writeOffsetEnd;

    ret = true;
  }

  m_writeOffset = 0;
  m_writeOffsetEnd = 0;

  return ret;
}

bool AppendOnlyFile::Write(const void *data, uint32_t size)
{
  if (m_blockOffset == 0)
    return false;

  uint32_t remainingSize = m_writeOffsetEnd - m_writeOffset;

  if (remainingSize < size)
    return false;

#if defined(NRF)
  WriteToSector(m_blockOffset + m_writeOffset, (uint8_t*)data, size);
#endif
  DebugFlush();

  m_writeOffset += size;

  return true;
}

void AppendOnlyFile::Flush()
{
#if defined(NRF) && defined(FSTORAGE_ENABLED) && FSTORAGE_ENABLED
  FlushSectorWrite();
#endif
}


const AppendOnlyFile::BlockHeader* AppendOnlyFile::FirstBlock(bool readAll)
{
  if (!IsValid())
      return nullptr;

  Flush();

  const Header *header = (Header*)m_offset;

  if (header->m_magic != Magic)
    return nullptr;

  if (header->m_crc != m_nameCrc)
    return nullptr;
  
  uint32_t *it = (uint32_t*)(m_offset);
  uint32_t *itEnd = (uint32_t*)(m_offsetEnd);

  it += sizeof(Header) / 4;

  return ReturnBlock((const BlockHeader*)it, readAll);
}


const AppendOnlyFile::BlockHeader* AppendOnlyFile::NextBlock(const BlockHeader *block, bool readAll)
{
  uint32_t *it = (uint32_t*)(block);
  it += (sizeof(BlockHeader) + block->m_size) / 4;

  return ReturnBlock((const BlockHeader*)it, readAll);
}


const AppendOnlyFile::BlockHeader* AppendOnlyFile::ReturnBlock(const BlockHeader *block, bool readAll)
{
  uint32_t *it = (uint32_t*)(block);
  uint32_t *itEnd = (uint32_t*)(m_offsetEnd);

  while(it < itEnd)
  {
    block = (const BlockHeader*)it;

    uint32_t remain = m_offsetEnd - (uint32_t)it;

    if ( block->m_size == 0xffffff ||
        !block->m_size ||                 //<- invalid data
          block->m_size > remain)          //<- invalid data
      break;

    if (block->m_commit == 0 || readAll)
    {
      return block;
    }

    it += (sizeof(BlockHeader) + block->m_size) / 4;
  }

  return nullptr;
}
