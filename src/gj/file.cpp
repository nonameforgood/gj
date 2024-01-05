#include "file.h"

#include "string.h"
#include "base.h"
#include "esputils.h"
#include "printmem.h"
#include "datetime.h"
#include "commands.h"
#include "config.h"
#include "eventmanager.h"


#if defined(NRF)
#include "littlefs/lfs.h"
#include "nrf51utils.h"

#if defined(NRF_SDK12)
  #include "softdevice_handler.h"
#elif defined(NRF_SDK17)
  #include <nrf_sdh.h>
#endif

#endif

#if GJ_FILE_SPIFFS_IDF()
  auto &CurrentFS = SPIFFS;
#elif GJ_FILE_LITTLEFS()
  auto &CurrentFS = LittleFS;
#elif GJ_FILE_SPIFFS()
  auto &CurrentFS = SPIFFS;
#endif

void DumpFile(GJString path, uint32_t offset, bool hex);

void Command_info(const CommandInfo &commandInfo ) 
{
  ShowFSInfo();
}

void Command_formatfs(const CommandInfo &commandInfo) 
{
  HANDLE_MULTI_EXE_CMD(3);
  
  SER("Formatting file system...");

  bool ret = false;

  #if GJ_FILE_SPIFFS_IDF()
  
  #elif GJ_FILE_LITTLEFS() || GJ_FILE_SPIFFS()
    ret = CurrentFS.format();
  #endif

  if (ret)
  {
    SER("done\n\r");
  }
  else
  {
    SER("Failed!\n\r");
  }
  ShowFSInfo();
}

#ifdef ESP32
void Command_rename(const CommandInfo &info) 
{
  if (info.m_argCount != 2)
  {
    SER("  arg err");
    return;
  }
  
  const char *newName = info.m_argsBegin[1];
  
  char oldName[64];
  uint32_t const firstParamLength = info.m_argsBegin[1] - info.m_argsBegin[0] - 1;
  
  if (firstParamLength >= sizeof(oldName))
  {
    SER("  err len");
    return;
  }
  
  strncpy(oldName, info.m_argsBegin[0], firstParamLength);
  oldName[firstParamLength] = 0;
  
  bool const ret = FileSystem::Rename(oldName, newName);
  
  SER("  Renamed from '%s' to '%s':%s\n\r", oldName, newName, ret ? "SUCCESS" : "FAILED");
}

void Command_delete(const CommandInfo &info) {
  if (info.m_argCount != 1)
  {
    SER("  arg err");
    return;
  }
  
  bool const ret = FileSystem::Delete(info.m_argsBegin[0]);
  
  SER("  Deleted '%s':%s\n\r", info.m_argsBegin[0], ret ? "SUCCESS" : "FAILED");
} 
#endif

void Command_dumpfile(const CommandInfo &info, bool hex) {
  if (info.m_argCount != 1)
  {
    SER("  arg err");
    return;
  }

  const char *filepath = info.m_argsBegin[0];

  if (!GJFile::Exists(filepath))
  {
    SER("dumpfile:file '%s' not found\n\r", filepath);
    return;
  }

  GJString s = filepath;

  SER("Dumping file :'%s' ----------\n\r", filepath);
  DumpFile(s, 0, hex);
} 

void Command_dumpfile(const CommandInfo &info) 
{
  const bool hex = false;
  Command_dumpfile(info, hex);
}

void Command_dumpfilehex(const CommandInfo &info) 
{
  const bool hex = true;
  Command_dumpfile(info, hex);
}

static char s_appFolder[32] = "";

void SetAppFolder(const char *appFolder)
{
  if (strlen(appFolder) >= sizeof(s_appFolder))
  {
    GJ_ERROR("App folder too long, max is %d\n", sizeof(s_appFolder) - 1);
    return;
  }

  strcpy(s_appFolder, appFolder);

  if (s_appFolder[0] == '/')
    strcpy(s_appFolder, s_appFolder + 1);

  uint32_t len = strlen(s_appFolder);
  if (len && s_appFolder[len - 1] != '/')
  {
    s_appFolder[len] = '/';
    s_appFolder[len + 1] = 0;
  }

  len = strlen(s_appFolder);
  if (!len)
  {
    GJ_ERROR("App folder is empty\n");
    return;
  }

  char buf[64];
  sprintf(buf, "/%.*s", strlen(s_appFolder) - 1, s_appFolder);

  printf("Creating app folder '%s'\n", buf);

#if GJ_FILE_SPIFFS() || GJ_FILE_LITTLEFS()
  bool ret = CurrentFS.mkdir(buf);
  if (!ret)
  {
    GJ_ERROR("Cannot create app folder '%s'\n", buf);
  }
#endif
}

const char *GetAppFolder()
{
  return s_appFolder;
}


void Command_fs(const char *command)
{
  static constexpr const char * const s_argsName[] = {
    "info",
    "format",
#ifdef ESP32
    "rename",
    "delete",
#endif
    "dump",
    "dumphex"
  };

  static void (*const s_argsFuncs[])(const CommandInfo &commandInfo){
    Command_info,
    Command_formatfs,
#ifdef ESP32
    Command_rename,
    Command_delete,
#endif
    Command_dumpfile,
    Command_dumpfilehex,
    
    };

  uint32_t argCount = sizeof(s_argsName)/sizeof(s_argsName[0]);

  const SubCommands subCommands = {argCount, s_argsName, s_argsFuncs};

  SubCommandForwarder(command, subCommands);
}

DEFINE_COMMAND_ARGS(fs,Command_fs);

DEFINE_CONFIG_BOOL(file.dbg, file_dbg, false);
DEFINE_CONFIG_BOOL(file.autoflush, file_autoflush, false);

void InitFileSystem(const char *appFolder)
{
#ifdef ESP32
  SetAppFolder(init.appFolder);
#endif

  FileSystem::Init();

  REFERENCE_COMMAND(fs);
}

GJFile::GJFile() = default;

GJFile::GJFile( const char *path, Mode mode )
{
  Open(path, mode);
}

GJFile::~GJFile()
{
  Close();
}


bool GJFile::Open( const char *path, Mode mode )
{
  if ((bool)*this)
  {
    //file already open
    return true;
  }

#if GJ_FILE_SPIFFS_IDF()
  
#elif GJ_FILE_SPIFFS() || GJ_FILE_LITTLEFS()
  if (!FileSystem::Init())
    return false;

  const char *openMode = "";
  
  if ( mode == Mode::Read )
  {
    openMode = "r";
  }
  else if ( mode == Mode::Write )
  {
    bool const exists = CurrentFS.exists(path);
    openMode = exists ? "r+" : "w";
  }
  else if ( mode == Mode::Append )
  {
    openMode = "a";
  }

  m_file = CurrentFS.open(path, openMode);

  bool fileDbg = GJ_CONF_BOOL_VALUE(file_dbg);
  SER_COND(fileDbg, "Open file \"%s\"(mode:%s):%s\n\r", path, openMode, m_file ? "\":OK" : "\":KOK");

  return m_file;
#elif GJ_FILE_BLOCKFS()

  if (mode != Mode::Read)
    return false;

  const FileSectorsDef* def = GetFileSectorDef(path);
  if (!def)
    return false;

  m_offset = def->m_sector;
  m_offsetEnd = def->m_sector + (def->m_sectorCount * 1024);
  m_pos = 0;

  return true;
#else
  return false;
#endif
}

bool GJFile::Close()
{
#if GJ_FILE_SPIFFS() || GJ_FILE_LITTLEFS()
  if (m_file)
  {
    m_file.flush(); //probably not needed
    m_file.close();
    m_file = {};
  }

  return true;

#elif GJ_FILE_BLOCKFS()

  m_offset = 0;
  m_offsetEnd = 0;
  m_pos = 0;

  return true;
#else
  return false;
#endif
}

void GJFile::Flush()
{
#if GJ_FILE_SPIFFS() || GJ_FILE_LITTLEFS()
  if (m_file)
  {
    m_file.flush();
  }
#endif
}

uint32_t GJFile::Size() const
{
#if GJ_FILE_SPIFFS() || GJ_FILE_LITTLEFS()
  if (m_file)
  {
    return (uint32_t)m_file.size();
  }
#elif GJ_FILE_BLOCKFS()

  return m_offsetEnd - m_offset;
#endif
  return 0;
}


GJFile::operator bool() const
{
#if GJ_FILE_SPIFFS() || GJ_FILE_LITTLEFS()
  return (bool)m_file;
#elif GJ_FILE_BLOCKFS()

  return m_offsetEnd != 0;

#else
  return false;
#endif
}  

void GJFile::Seek(uint32_t offset, uint32_t origin)
{
#if GJ_FILE_SPIFFS() || GJ_FILE_LITTLEFS()
  m_file.seek(offset, (fs::SeekMode)origin);

#elif GJ_FILE_BLOCKFS()

  
  if (origin == 0)    //set
  {
    uint32_t maxSeek = m_offsetEnd - m_offset;
    m_pos = Min(maxSeek, offset);
  }
  else if (origin == 1)   //cur
  {
    uint32_t maxSeek = m_offsetEnd - m_pos;
    m_pos += Min(maxSeek, offset);
  }
  else  //2
  {
    m_pos = m_offsetEnd - m_offset;
  }
#endif
}

uint32_t GJFile::Tell() const
{
#if GJ_FILE_SPIFFS() || GJ_FILE_LITTLEFS()
  return (uint32_t)m_file.position();
#elif GJ_FILE_BLOCKFS()

  return m_pos;
#else
  return 0;
#endif
}
  
bool GJFile::Eof() const
{
#if GJ_FILE_SPIFFS() || GJ_FILE_LITTLEFS()
  if (!m_file)
  {
    LOG("ERROR:Eof:File not open\n\r");
    return true;
  }

  return !m_file.available();
#elif GJ_FILE_BLOCKFS()
  uint32_t maxSeek = m_offsetEnd - m_offset;
  return m_pos == maxSeek;
#else
  return true;
#endif
}

uint32_t GJFile::Read( void *buffer, uint32_t size )
{
#if GJ_FILE_SPIFFS() || GJ_FILE_LITTLEFS()
  if (!m_file)
  {
    LOG("ERROR:Read:File not open\n\r");
    return 0;
  }

  return (uint32_t)m_file.read( (uint8_t*)buffer, size );

#elif GJ_FILE_BLOCKFS()
  #if defined(NRF)
    void *fileData = (char*)(m_offset + m_pos);
    uint32_t maxLeft = m_offsetEnd - (m_offset + m_pos);
    uint32_t readSize = Min(size, maxLeft);

    memcpy(buffer, fileData, size);

    m_pos += readSize;
    
    return readSize;
  #endif
#else
  return 0;
#endif
}

uint32_t GJFile::Read( int32_t &value )
{
  return Read(&value, sizeof(value));
}

uint32_t GJFile::Read( uint32_t &value )
{
  return Read(&value, sizeof(value));
}

uint32_t GJFile::Write( void const *buffer, uint32_t size )
{
#if GJ_FILE_SPIFFS() || GJ_FILE_LITTLEFS()
  if (!m_file)
  {
    LOG("ERROR:Write::File not open\n\r");
    return 0;
  }

  bool autoFlush = GJ_CONF_BOOL_VALUE(file_autoflush);
  uint32_t sizeWritten = (uint32_t)m_file.write( (uint8_t*)buffer, size );
  if (autoFlush)
    m_file.flush();

  if (sizeWritten != size)
  {
    LOG("WARNING:GJFile::Write:%d/%d written\n\r", sizeWritten, size);
  }
  return sizeWritten;
#else
  return 0;
#endif
}


uint32_t GJFile::Write( const char *string )
{
  return Write( string, strlen(string) );
}

uint32_t GJFile::Write( GJString const &string )
{
  return Write(string.c_str());
}


uint32_t GJFile::Write( int32_t value )
{
  uint32_t size = 0;
  if(value < 0) {
      Write("-");
      size++;
      value = -value;
  }
  
  return PrintNumber( value, 10 ) + size;
}

uint32_t GJFile::Write( uint32_t value )
{ 
  return PrintNumber( value, 10 );
}


uint32_t GJFile::PrintNumber(unsigned long n, uint8_t base) {
    char buf[8 * sizeof(long) + 1]; // Assumes 8-bit chars plus zero byte.
    char *str = &buf[sizeof(buf) - 1];

    *str = '\0';

    // prevent crash if called with base == 1
    if(base < 2)
        base = 10;

    do {
        unsigned long m = n;
        n /= base;
        char c = m - base * n;
        *--str = c < 10 ? c + '0' : c + 'A' - 10;
    } while(n);

    return Write(str);
}

bool GJFile::Exists(const char *path)
{
  return FileSystem::Exists(path);
}

bool GJFile::Delete(const char *path)
{
  return FileSystem::Delete(path);
}
  
uint32_t FileSystem::Capacity()
{
  Init();
#if GJ_FILE_SPIFFS_IDF()
  
#elif GJ_FILE_LITTLEFS() || GJ_FILE_SPIFFS()
  return CurrentFS.totalBytes();
#endif

  return 0;
}

uint32_t FileSystem::Used()
{
  Init();
#if GJ_FILE_SPIFFS_IDF()
  
#elif GJ_FILE_LITTLEFS() || GJ_FILE_SPIFFS()
  return CurrentFS.usedBytes();
#endif

  return 0;
}
//-

bool FileSystem::Init()
{
#if GJ_FILE_SPIFFS_IDF()
  
#elif GJ_FILE_LITTLEFS() || GJ_FILE_SPIFFS()
  static bool s_init = false;
  if (s_init == false)
  {
    bool formatIfFail = false;
    if(!CurrentFS.begin(formatIfFail))
    {
      SER("ERROR:FS Begin Failed, formatting...should take about 1 minute\n\r");
      bool const formatRet = CurrentFS.format();
      LOG("FS Format result:%d\n\r", (int)formatRet);
      if (!formatRet)
      {
        SER("ERROR:FS Format failed, file system not mounted\n\r");
        return false;
      }

      LOG("FS Format success:total size:%d\n\r", CurrentFS.totalBytes());
        
      if(!CurrentFS.begin(false))
      {
        SER("ERROR:FS begin failed, file system not mounted\n\r");
        return false;
      }
    }

    s_init = true;
  }

  return true;

#elif GJ_FILE_BLOCKFS()

    InitFStorage();

#endif

  return false;
}

bool FileSystem::Exists(const char *path)
{
  Init();
#if GJ_FILE_SPIFFS_IDF()
  
#elif GJ_FILE_LITTLEFS() || GJ_FILE_SPIFFS()
  bool const ret = CurrentFS.exists(path);
  return ret;
#elif GJ_FILE_BLOCKFS()
  GJFile f(path, GJFile::Mode::Read);
  return (bool)f;
#endif

  return false;
}

bool FileSystem::Delete(const char *path) 
{
  Init();
#if GJ_FILE_SPIFFS_IDF()
  
#elif GJ_FILE_LITTLEFS() || GJ_FILE_SPIFFS()
  bool const ret = CurrentFS.remove(path);
  if (!ret)
  {
    LOG("Delete of file \"%s\" failed\n\r", path);
  }
  return ret;
#endif

  return false;
}

bool FileSystem::Rename(const char *oldName, const char *newName)
{
  Init();
#if GJ_FILE_SPIFFS_IDF()
  
#elif GJ_FILE_LITTLEFS() || GJ_FILE_SPIFFS()
  return CurrentFS.rename(oldName, newName);
#endif
  return false;
}

void FileSystem::ListDir(const char * dirname, uint8_t levels, std::function<void(File &, uint32_t)> cb)
{
  ListDir(dirname, levels, 0, cb);
}

void FileSystem::ListDir(const char * dirname, uint8_t levels, uint32_t depth, std::function<void(File &, uint32_t)> cb)
{
  Init();
#if GJ_FILE_SPIFFS_IDF()
  
#elif GJ_FILE_LITTLEFS() || GJ_FILE_SPIFFS()
  fs::FS &fs = CurrentFS;

  //Serial.printf("  Listing directory: %s\r\n\r", dirname);

  File root = fs.open(dirname);
  if(!root){
      GJ_ERROR("- failed to open directory\n\r");
      return;
  }
  if(!root.isDirectory()){
      GJ_ERROR(" - not a directory");
      return;
  }

  File file = root.openNextFile();
  
  while(file){
      cb(file, depth);
      if(file.isDirectory()){
          if(levels){
              char folder[32];
              sprintf(folder, "/%s", file.name());
              FileSystem::ListDir(folder, levels -1, depth + 1, cb);
          }
      } 
      file = root.openNextFile();
  }
#endif
}

void DumpFile(GJString path, uint32_t offset, bool hex)
{
  uint32_t batch = 0;
  
  GJFile file(path.c_str(), GJFile::Mode::Read);
  if (!file)
  {
    SER("dumpfile:cannot read file '%s'\n\r", path.c_str());
    SER("\n\rDumping file done ----------\n\r");
    return;
  }

  if (AreTerminalsReady())
  {
    file.Seek(offset, 0);

    unsigned char buffer[129];
    uint32_t maxReadSize = hex ? 32 : (sizeof(buffer) - 1);
  
    batch = maxReadSize;
  
    batch = file.Read(buffer, batch);
    buffer[batch] = 0;

    if (hex)
    {
      PrintMemLine(offset, buffer, batch, maxReadSize);
    }
    else
    {
      SER((char*)buffer);
    }
  }

  if (file.Tell() != file.Size())
  {
    EventManager::Function f = std::bind(DumpFile, path, offset + batch, hex);
    GJEventManager->Add(f);
    return;
  }

  SER("\n\rDumping file done ----------\n\r");
}

void ShowFSInfo()
{
  LOG("FS info:\n\r");
#if GJ_FILE_SPIFFS_IDF()
  
#elif GJ_FILE_LITTLEFS() || GJ_FILE_SPIFFS()
  #if GJ_FILE_LITTLEFS()
      LOG("  Type:LittleFS\n\r");
  #elif GJ_FILE_SPIFFS()
      LOG("  Type:SPIFFS\n\r");
  #endif
  LOG("  Size:%d\n\r", CurrentFS.totalBytes());
  LOG("  Used:%d\n\r", CurrentFS.usedBytes());
#endif

#if GJ_FILE_BLOCKFS()
  const FileSectorsDef* sectorIt = BeginFileSectors();
  const FileSectorsDef* sectorItEnd = EndFileSectors();

  const char *spaces = "                        ";
  
  while(sectorIt != sectorItEnd)
  {
    int spaceCount = 24 - strlen(sectorIt->m_path);
    LOG("   %s%.*s  MaxSize:%d \n\r", sectorIt->m_path, spaceCount, spaces, sectorIt->m_sectorCount * NRF_FLASH_SECTOR_SIZE);

    sectorIt++;
  }
#else
  auto cb = [](File &file, uint32_t depth)
  {
    time_t const lastWrite = file.getLastWrite();

    const char *spaces = "                        ";
    int spaceCount = 24 - strlen(file.name()) - depth * 2;

    if(file.isDirectory())
    {  
      LOG("   %.*s%s%.*s             lastWrite:%d\n\r", depth * 2, spaces, file.name(), spaceCount, spaces, lastWrite);
    } 
    else 
    {
      LOG("   %.*s%s%.*s Size:%6d lastWrite:%d\n\r", depth * 2, spaces, file.name(), spaceCount, spaces, file.size(), lastWrite);
    }
  };

  LOG("  Listing directory: %s\r\n\r", "/");
  FileSystem::ListDir("/", 3, cb);
#endif

}
