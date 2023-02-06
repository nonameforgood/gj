#pragma once

class GJFile;

class GJLog
{
  public:

  GJLog(GJFile &file);

  uint32_t Write( const char *string );
  uint32_t Write( GJString const &string );

  uint32_t Write( int32_t value );
  uint32_t Write( uint32_t value );

private:
  GJFile &m_file;
  GJString m_pending;
  
  uint32_t PrintNumber(unsigned long n, uint8_t base);
};
