#if defined(NRF)
#include "nrf51utils.h"
#include "commands.h"
#include "appendonlyfile.h"
#include <boards.h> 
#include <nrf_drv_clock.h>
#include <ble_advertising.h>

#if defined(NRF_SDK12)
  #include <fstorage.h>
  #include "softdevice_handler.h"
#elif defined(NRF_SDK17)
  #include <nrf_fstorage.h>
  #include <nrf_sdh.h>
  #include <nrf_sdh_ble.h>
#endif

#define GJ_BOOT_APPEND_ONLY_FILE

extern "C" {
void nrf_nvmc_write_words(uint32_t address, const uint32_t * src, uint32_t num_words);
void nrf_nvmc_page_erase(uint32_t address);

}
void __attribute__ ((noinline)) nrf_bootloader_app_start_impl2(uint32_t start_addr);

static bool s_fsInit = false; 
void fsCallback(fs_evt_t const * const evt, fs_ret_t result) { }

//not using FS_REGISTER_CFG because it prevents dead var removal
#define GJ_FS_REGISTER_CFG(cfg_var) static cfg_var __attribute__ ((section(".fs_data"))) 

GJ_FS_REGISTER_CFG(fs_config_t fsConfig) = 
{
    .p_start_addr = (uint32_t*)0,
    .p_end_addr = (uint32_t*)0,
    .callback   = fsCallback, // Function for event callbacks.
    .num_pages = 1,      // Number of physical flash pages required.
    .priority  = 0xFE            // Priority for flash usage.
};


/** @brief Function for filling a page in flash with a value.
 *
 * @param[in] address Address of the first word in the page to be filled.
 * @param[in] value Value to be written to flash.
 */
static void flash_word_write(uint32_t * address, uint32_t value)
{
    // Turn on flash write enable and wait until the NVMC is ready:
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    *address = value;

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    // Turn off flash write enable and wait until the NVMC is ready:
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }
}

static void flash_page_erase(uint32_t * page_address)
{
    // Turn on flash erase enable and wait until the NVMC is ready:
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    // Erase page:
    NRF_NVMC->ERASEPAGE = (uint32_t)page_address;

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    // Turn off flash erase enable and wait until the NVMC is ready:
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }
}


void EraseSector(uint32_t byteOffset, uint32_t count)
{
  if (s_fsInit)
  {
    
    int32_t err_code;

    err_code = fs_erase( 
      &fsConfig,
      (uint32_t*)byteOffset,
      count,
      nullptr
    );

    if (err_code != 0)
    {
      SER("fs_erase (@0x%x) failed err=%d\n", byteOffset, err_code);
    }
  }
  else
  {
    uint32_t pg_size = NRF_FICR->CODEPAGESIZE;

    while(count)
    {
      flash_page_erase((uint32_t*)byteOffset);
      count--;
      byteOffset += pg_size;
    }
  }
}


void WriteToSector(uint32_t byteOffset, const uint8_t *data, uint32_t size)
{
  if (s_fsInit)
  {
    int32_t err_code;

    err_code = fs_store( 
      &fsConfig,
      (uint32_t*)byteOffset,
      (uint32_t*)data,
      size / 4,
      nullptr
    );

    if (err_code != 0)
    {
      SER("fs_store (@0x%x, %d) failed %s(%x)\n", byteOffset, size, ErrorToName(err_code), err_code);
    }
  }
  else
  {
    uint32_t *dstAddress = (uint32_t*)byteOffset;

    size = (size & ~3);

    const uint8_t *endData = data + size;

    while(data < endData)
    {
      uint32_t word;

      memcpy(&word, data, 4);

      flash_word_write(dstAddress, word);
      dstAddress++;
      data += 4;
    }
  }
}

bool IsFlashIdle()
{
  if (s_fsInit)
    return fs_queue_is_empty();
  else
    return true;
}

void sys_evt_dispatch(uint32_t sys_evt)
{
#if defined(NRF_SDK12)
    // Dispatch the system event to the fstorage module, where it will be
    // dispatched to the Flash Data Storage (FDS) module.
    fs_sys_event_handler(sys_evt);


    // Dispatch to the Advertising module last, since it will check if there are any
    // pending flash operations in fstorage. Let fstorage process system events first,
    // so that it can report correctly to the Advertising module.
    ble_advertising_on_sys_evt(sys_evt);
#endif
}

void ExecuteSystemEvents()
{
  if (s_fsInit)
  {
    for (;;)
    {
        uint32_t err_code;
        uint32_t evt_id;

        // Pull event from SOC.
        err_code = sd_evt_get(&evt_id);

        if (err_code == NRF_ERROR_NOT_FOUND)
        {
            break;
        }
        else if (err_code != NRF_SUCCESS)
        {
            APP_ERROR_HANDLER(err_code);
        }
        else
        {
            // Call application's SOC event handler.
  #if (NRF_MODULE_ENABLED(CLOCK) && defined(SOFTDEVICE_PRESENT))
            nrf_drv_clock_on_soc_event(evt_id);
  #endif
          sys_evt_dispatch(evt_id);
        }
    }
  }
}

void FlushSectorWrite()
{
  if (s_fsInit)
  {
    while(!fs_queue_is_empty())
    {
      ExecuteSystemEvents();
    }
  }
}

const BootSelPartition* GetBootSelPartition();
const BootPartition* GetBootPartitions();
uint32_t GetBootPartitionCount();

void WriteBootSector(uint32_t partition)
{
  partition += 1; //stored index is 1 based

#if defined(GJ_BOOT_APPEND_ONLY_FILE)
  AppendOnlyFile file("/boot");

  if (!file.BeginWrite(4))
  {
    file.Erase();
    bool ret = file.BeginWrite(4);
    APP_ERROR_CHECK_BOOL(ret);
  }
  
  file.Write(&partition, 4);
  file.EndWrite();
  file.Flush();
#else
  strcpy(bootInfo.m_idString, bootIdString);
  bootInfo.m_partition = partition;

  const BootSelPartition& bootSelPartition = *GetBootSelPartition();

  if (bootSelPartition.m_offset == 0)
    return;

  EraseSector(bootSelPartition.m_offset);
  WriteToSector(bootSelPartition.m_offset, (uint8_t*)&bootInfo, sizeof(bootInfo));
#endif
}

uint32_t GetPartitionIndex()
{
  uint32_t testAdr = (uint32_t)&GetPartitionIndex;

  uint32_t currentPartition = 0;
  const uint32_t partitionCount = GetBootPartitionCount();
  const BootPartition* bootPartitions = GetBootPartitions();

  //find address of function GetPartitionIndex() against defined partitions
  for (int32_t i = 0 ; i < partitionCount ; ++i)
  {
    if (testAdr >= bootPartitions[i].m_offset &&
        testAdr < (bootPartitions[i].m_offset + bootPartitions[i].m_size))
    {
      currentPartition = i;
    }
  }

  return currentPartition;
}

BootPartition GetNextPartition()
{
  const BootPartition* bootPartitions = GetBootPartitions();
  uint32_t index = (GetPartitionIndex() + 1) % 2;
  return bootPartitions[index];
}

void WriteBootForNextPartition()
{
  uint32_t index = (GetPartitionIndex() + 1) % 2;
  WriteBootSector(index);
}

uint32_t bootloaded_vtor = 0;


extern "C" {


void __attribute__((naked)) GJ_IRQForwarder()
{
    __asm__ volatile (
            " ldr r0,=bootloaded_vtor\n" 
            " ldr r0,[r0]\n"            // Read the fake VTOR into r0
            " cmp r0, #0x00\t\n"
      
            " bne vtor_ok\n\r"
            " b . \n\r"                 // infinite loop

            " vtor_ok:  \t\n"
            " ldr r1,=0xE000ED04\n"     // Prepare to read the ICSR
            " ldr r1,[r1]\n"            // Load the ICSR
            " mov r2,#63\n"             // Prepare to mask SCB_ICSC_VECTACTIVE (6 bits, Cortex-M0)
            " and r1, r2\n"             // Mask the ICSR, r1 now contains the vector number
            " lsl r1, #2\n"             // Multiply vector number by sizeof(function pointer)
            " add r0, r1\n"             // Apply the offset to the table base
            " ldr r0,[r0]\n"            // Read the function pointer value
            " bx r0\n"                  // Aaaannd branch!
            );
}

}

void BootIntoPartition()
{
  const BootPartition *partitions = GetBootPartitions();
  const uint32_t partitionCount = GetBootPartitionCount();

  uint32_t partitionIndex = 0;    //boot into partition 0 if not boot info found

#if defined(GJ_BOOT_APPEND_ONLY_FILE)
  AppendOnlyFile file("/boot");

  auto onBoot = [&](uint32_t size, const void *data)
  {
    partitionIndex = *(uint32_t*)data - 1; //boot info is 1 based, not 0 based
  };

  file.ForEach(onBoot);
  
#else
  const BootSelPartition& bootSelPartition = *GetBootSelPartition();
  uint32_t bootInfoAddress = bootSelPartition.m_offset;

  const BootInfo *bootInfoPtr =(BootInfo*) bootInfoAddress;
  const BootInfo &bootInfo = *bootInfoPtr;

  if (!strcmp(bootInfo.m_idString, bootIdString) && bootInfo.m_partition != 0)
  {
    //valid boot section
    partitionIndex = bootInfo.m_partition - 1; //boot info is 1 based, not 0 based
  }
#endif

  partitionIndex = Min<uint32_t>(partitionIndex, partitionCount);

  {
    //load partition
    uint32_t *appAdr = (uint32_t *)partitions[partitionIndex].m_offset;
  
    if (appAdr)
    {
      
      //uint32_t entryAdr = appAdr[1];

      bootloaded_vtor = (uint32_t)appAdr;

      nrf_bootloader_app_start_impl2((uint32_t)appAdr);
      
      // typedef void (*pfnEntry)();
      //pfnEntry entry;
      //entry = (pfnEntry)entryAdr;

      //(*entry)();
    }
  }
}

void Command_WriteBoot(const char * command)
{
  CommandInfo info;

  uint32_t partition = 0;

  if (info.m_argCount)
  {
    partition = strtol(info.m_args[0].data(), nullptr, 0);
    partition = Min<uint32_t>(partition, 1);
  }

  WriteBootSector(partition);
}


const FileSectorsDef* BeginFileSectors()
{
  extern const FileSectorsDef __gj_file_sectors_def_start__;
  return &__gj_file_sectors_def_start__;
}


const FileSectorsDef* EndFileSectors()
{
  extern const FileSectorsDef __gj_file_sectors_def_end__;
  return &__gj_file_sectors_def_end__;
}



const FileSectorsDef* GetFileSectorDef(const char *path)
{
  const FileSectorsDef * it = BeginFileSectors();
  const FileSectorsDef * itEnd = EndFileSectors();

  for (; it != itEnd ; ++it)
  {
    if (!strcmp(it->m_path, path))
      return it;
  }

  return nullptr;
}

bool GetFileSectors(const char *path, uint32_t &sec, uint32_t &secCount)
{
  const FileSectorsDef * def = GetFileSectorDef(path);

  if (!def)
    return false;
    
  sec = def->m_sector;
  secCount = def->m_sectorCount;
  return true;
}

DEFINE_COMMAND_ARGS(writeboot, Command_WriteBoot);

static uint32_t SD_IRQHandler (void)
{
  //this is called when a BLE event interrupt occurs.
  //It will wake up the processor.
  //The BLE event will be processed in the update function.
  //The BLE event is not processed while in the interrupt state
  //to allow for greater code flexibility
  
  return NRF_SUCCESS;
}


void InitSoftDevice(uint32_t centralLinks, uint32_t periphLinks)
{
  if (softdevice_handler_is_enabled())
    return;

  nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;

  // Initialize the SoftDevice handler module.
  SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, SD_IRQHandler);

  uint32_t err_code;

  ble_enable_params_t ble_enable_params;
  err_code = softdevice_enable_get_default_config(centralLinks,
                                                  periphLinks,
                                                  &ble_enable_params);
  APP_ERROR_CHECK(err_code);

  // Check the ram settings against the used number of links
  CHECK_RAM_START_ADDR(centralLinks, periphLinks);

  // Enable BLE stack.
#if (NRF_SD_BLE_API_VERSION == 3) && false
  ble_enable_params.gatt_enable_params.att_mtu = NRF_BLE_MAX_MTU_SIZE;
#endif
  err_code = softdevice_enable(&ble_enable_params);
  APP_ERROR_CHECK(err_code);

#if defined(NRF_SDK12)
  err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
  APP_ERROR_CHECK(err_code);
#endif
}




void InitFStorage()
{
  if (s_fsInit)
    return;

  s_fsInit = true;

  uint32_t pages = NRF_FLASH_SIZE / NRF_FLASH_SECTOR_SIZE - 1;

  //global var fsConfig is not initialized correctly for some reason
  //need to overwrite it here
  fs_config_t fsConfig2 = 
  {
    .p_start_addr = (uint32_t*)0,
    .p_end_addr = (uint32_t*)(NRF_FLASH_SIZE),
    .callback   = fsCallback,     // Function for event callbacks.
    .num_pages = pages,             // Number of physical flash pages required.
    .priority  = 0xFE             // Priority for flash usage.
  };
  memcpy(&fsConfig, &fsConfig2, sizeof(fsConfig));

  fs_init();  
}

void InitMultiboot()
{
  REFERENCE_COMMAND(writeboot);

  InitFStorage();
}

#endif