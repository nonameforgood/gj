#pragma once

#include "base.h"
#include "crc.h"

class AppendOnlyFile
{
  public:
    AppendOnlyFile(const char *name);
    ~AppendOnlyFile();

    bool IsValid() const;
    void Erase();

    template <typename C>
    bool ForEach(C c)
    {
      bool readAll = false;
      return ForEachInternal(c, readAll);
    }

    bool BeginWrite(uint32_t size);
    bool EndWrite();

    bool Write(const void *data, uint32_t size);

    void Flush();

private:
  static const uint32_t Magic = 0x66636A67;  //GJCF
  struct Header
  {
    uint32_t m_magic;
    uint32_t m_crc;
  };

  struct BlockHeader
  {
    uint32_t m_size : 24;
    uint32_t m_commit : 8;
  };

  uint32_t m_nameCrc;
  uint32_t m_offset;
  uint32_t m_offsetEnd;

  Header m_headerForWrite;
  BlockHeader m_blockHeaderForWrite;
  uint32_t m_blockOffset = 0;
  uint32_t m_writeOffset = 0;
  uint32_t m_writeOffsetEnd = 0;


    template <typename C>
    bool ForEachInternal(C c, bool readAll)
    {
      const BlockHeader *block = FirstBlock(readAll);
      while(block)
      {
        c(block->m_size, (void*)(block + 1));
        block = NextBlock(block, readAll);
      }
      return true;
      //return ForEachInternal(c, readAll);
    }

  void DebugFlush();

  const BlockHeader* FirstBlock(bool readAll);
  const BlockHeader* NextBlock(const BlockHeader* block, bool readAll);
  const BlockHeader* ReturnBlock(const BlockHeader* block, bool readAll);
};
