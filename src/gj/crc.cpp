#include "crc.h"
#include <string.h>
#include "string.h"

uint32_t ComputeCrc(const char* data, uint32_t len, uint32_t crc)
{
    return crcdetail::compute(data, len, crc);
}

uint32_t ComputeCrc(const unsigned char* data, uint32_t len, uint32_t crc)
{
  if (!len)
    return crc ^ 0xFFFFFFFFU;

  uint32_t newCrc = crcdetail::table[*data ^ (crc & 0xFF)] ^ (crc >> 8);

    //printf("sub crc:%u\n", newCrc);
    return ComputeCrc(data + 1, len - 1, newCrc);
}


uint32_t ComputeCrcDebug(const unsigned char* data, uint32_t len, uint32_t crc)
{
  crc = crc ^ 0xFFFFFFFFU;

  if (!len)
    return crc ^ 0xFFFFFFFFU;

  unsigned char d = *data;
  unsigned int c = crc & 0xFF;

  unsigned int i = d ^ c;

  unsigned int s = crc >> 8;

  uint32_t newCrc = crcdetail::table[i] ^ s;
  newCrc = newCrc ^ 0xFFFFFFFFU;

  //printf("sub crc d:%u c:%u i:%u s:%u new:%u\n", d, c, i, s, newCrc);
  return ComputeCrcDebug(data + 1, len - 1, newCrc);
}

StringID::StringID() = default;

StringID::StringID(const char *string, uint32_t id)
{
  m_string = string;
  m_id = id;
}


StringID::StringID(const char *string)
{
  m_string = string;
  m_id = ComputeCrc(string, strlen(string));
}

StringID::StringID(GJString const &string)
: StringID(string.c_str())
{
  
}

StringID::StringID(StringIDData const &data)
: StringID(data.m_string, data.m_id)
{
  
}

StringID& StringID::operator=(StringID const &other)
{
  m_string = other.m_string;
  m_id = other.m_id;

  return *this;
}

bool StringID::IsValid() const
{
  return m_string != nullptr;
}

const char *StringID::GetString() const
{
  return m_string;
}

uint32_t StringID::GetID() const
{
  return m_id;
}

void StringID::GetData(StringIDData &dest) const
{
  dest.m_string = m_string;
  dest.m_id = m_id;
}

StringID::operator uint32_t() const
{
  return m_id;
}
