#if defined(NRF)
#include "base.h"

struct BootInfo
{
  char m_idString[16] = {};
  uint32_t m_partition = 0;
};


struct BootPartition
{
  uint32_t m_index;
  uint32_t m_offset;
  uint32_t m_size;
};

struct BootSelPartition
{
  uint32_t m_partitionCount;
  uint32_t m_offset;
  uint32_t m_size;
};


#ifdef NRF51
  #define NRF_FLASH_SECTOR_SIZE 1024
  #define NRF_FLASH_SIZE 0x40000
#elif defined(NRF52)
  #define NRF_FLASH_SECTOR_SIZE 4096
  #define NRF_FLASH_SIZE 0x80000
#endif

#define DEFINE_BOOT_SEL_PARTITION(count, off) const BootSelPartition __attribute__((used)) bootSelPartition = {count, off, NRF_FLASH_SECTOR_SIZE};  const BootSelPartition* GetBootSelPartition() { return &bootSelPartition; }
  
#define BEGIN_BOOT_PARTITIONS() const BootPartition __attribute__((used)) bootPartitions[] = {
#define END_BOOT_PARTITIONS() }; const BootPartition* GetBootPartitions(){ return &bootPartitions[0];} uint32_t GetBootPartitionCount(){ return sizeof(bootPartitions) / sizeof(bootPartitions[0]);}
#define DEFINE_BOOT_PARTITION(index, off, size) {index, off, size},


#define GJ_MAX_PARTITIONS 2
static const char *bootIdString = "GJBootValid";
static BootInfo bootInfo = {};

void BootIntoPartition();

uint32_t GetPartitionIndex();
BootPartition GetNextPartition();
void WriteBootForNextPartition();

void EraseSector(uint32_t byteOffset, uint32_t count = 1);
void WriteToSector(uint32_t byteOffset, const uint8_t *data, uint32_t size);
bool IsFlashIdle();
void FlushSectorWrite();


struct FileSectorsDef
{
  const char* m_path;
  uint32_t m_nameCrc;
  uint32_t m_sector;
  uint32_t m_sectorCount;
};

//section gj_file_sectors_def is marked as: keep="Yes", 
//so a simple unused function referencing the variable is enough to prevent dead code stripping
#define DEFINE_FILE_SECTORS(name, path, sec, secCount) const FileSectorsDef __attribute__ ((section(".gj_file_sectors_def"))) fileSectors_##name = {path, static_crc(path), sec, secCount};  const void * ForceLink_file_sectors__##name() {return &fileSectors_##name;}

const FileSectorsDef* GetFileSectorDef(const char *path);
const FileSectorsDef* BeginFileSectors();
const FileSectorsDef* EndFileSectors();

void InitSoftDevice(uint32_t centralLinks, uint32_t periphLinks);
void InitFStorage();

void InitMultiboot();

#endif