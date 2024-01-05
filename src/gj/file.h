#pragma once

#include "function_ref.hpp"
#include "base.h"

#if !defined(ARDUINO_ESP32_DEV) && defined(CONFIG_IDF_CMAKE) || defined(GJ_IDF)
  #define GJ_FILE_SPIFFS() 0
  #define GJ_FILE_LITTLEFS() 0
  #define GJ_FILE_SPIFFS_IDF() 1
  #define GJ_FILE_BLOCKFS() 0
#elif defined(NRF)
  #define GJ_FILE_SPIFFS() 0
  #define GJ_FILE_LITTLEFS() 0
  #define GJ_FILE_SPIFFS_IDF() 0
  #define GJ_FILE_BLOCKFS() 1
#else
  //#define GJ_FILE_SPIFFS() 1
  #define GJ_FILE_SPIFFS() 0
  #define GJ_FILE_LITTLEFS() 1
  #define GJ_FILE_SPIFFS_IDF() 0
  #define GJ_FILE_BLOCKFS() 0
#endif

#if GJ_FILE_SPIFFS_IDF()
  class File
  {

  };
#elif GJ_FILE_LITTLEFS()
  #include "LittleFS.h"
#elif GJ_FILE_SPIFFS()
  #include "SPIFFS.h"
#else
enum SeekMode {
    SeekSet = 0,
    SeekCur = 1,
    SeekEnd = 2
};
class File
{
public:
    inline File() {
        
    }

    inline size_t write(uint8_t) { return {}; }
    inline size_t write(const uint8_t *buf, size_t size) { return {}; }
    inline int available() { return {}; }
    inline int read()  { return {}; }
    inline int peek() { return {}; }
    inline void flush() {}
    inline size_t read(uint8_t* buf, size_t size) { return {}; }
    inline size_t readBytes(char *buffer, size_t length)
    {
        return read((uint8_t*)buffer, length);
    }

    inline bool seek(uint32_t pos, SeekMode mode) {return false;}
    inline bool seek(uint32_t pos)
    {
        return seek(pos, SeekSet);
    }
    inline size_t position() const { return {}; }
    inline size_t size() const { return {}; }
    inline void close() {}
    inline operator bool() const { return false; }
    inline uint32_t getLastWrite() { return {}; }
    inline const char* path() const { return {}; }
    inline const char* name() const { return {}; }

    inline bool isDirectory(void) { return {}; }
    inline File openNextFile(const char* mode = "") { return {}; }
    inline void rewindDirectory(void) {}

protected:

};
#endif

class GJString;

class GJFile
{
public:

  enum class Mode
  {
    Read = 1,
    Write = 2,
    Append = 3
  };

  GJFile();
  GJFile( const char *path, Mode mode );
  ~GJFile();

  operator bool() const;  

  bool Open( const char *path, Mode mode );
  bool Close();
  void Flush();
  uint32_t Size() const; 

  void Seek(uint32_t offset, uint32_t origin);
  uint32_t Tell() const;
  bool Eof() const;

  uint32_t Read( void *buffer, uint32_t size );
  uint32_t Write( void const *buffer, uint32_t size );
  uint32_t Write( const char *string );
  uint32_t Write( GJString const &string );

  uint32_t Write( int32_t value );
  uint32_t Write( uint32_t value );

  uint32_t Read( int32_t &value );
  uint32_t Read( uint32_t &value );

  static bool Exists(const char *path);
  static bool Delete(const char *path);
  
private:

  uint32_t PrintNumber(unsigned long n, uint8_t base);

#if GJ_FILE_SPIFFS_IDF()
  
#elif GJ_FILE_LITTLEFS() || GJ_FILE_SPIFFS()
  mutable File m_file = {};

#elif GJ_FILE_BLOCKFS()

  uint32_t m_offset = 0;
  uint32_t m_offsetEnd = 0;
  uint32_t m_pos = 0;
#endif
};

class FileSystem
{
  public:

  static uint32_t Capacity();
  static uint32_t Used();

  static bool Init();
  static bool Exists(const char *path);
  static bool Delete(const char *path);
  static bool Rename(const char *oldName, const char *newName);
  static void ListDir(const char * dirname, uint8_t levels, std::function<void(File &, uint32_t)> cb);

private:
  static void ListDir(const char * dirname, uint8_t levels, uint32_t depth, std::function<void(File &, uint32_t)> cb);
};

void InitFileSystem(const char *appFolder);
const char *GetAppFolder();
void ShowFSInfo();