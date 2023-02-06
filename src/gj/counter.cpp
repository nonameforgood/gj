#include "counter.h"
#include <string.h>

Counter::Counter(uint32_t uid, const char *desc)
: m_uid(uid)
{
  memset(m_desc, 0, sizeof(m_desc));  
  strncpy(m_desc, desc, 4);
}  

uint32_t Counter::GetUID() const
{
  return m_uid;
}

const char * Counter::GetDesc() const
{
  return m_desc;
}

bool Counter::NeedsCommit() const
{
  return false;
}

bool Counter::CanCommit() const
{
  return false;
}
