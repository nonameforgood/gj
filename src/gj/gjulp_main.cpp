#include "gjulp_main.h"
#include "serial.h"

UlpAccessor::UlpAccessor(uint32_t &ulpVar)
:m_ulpVar(ulpVar)
{
  
}

UlpAccessor::operator uint32_t() const 
{ 
  return m_ulpVar & 0xffff; 
}

void UlpAccessor::operator =(uint32_t v) 
{ 
  m_ulpVar = v; 
}

UlpArrayAccessor *ulp_sensor_events = nullptr;

UlpArrayAccessor::UlpArrayAccessor(uint32_t *ulpVar, uint32_t arraySize)
: m_ulpVar(ulpVar)
, m_size(arraySize)
{
}

const uint16_t& UlpArrayAccessor::operator [](std::size_t index) const
{
  if (index >= m_size)
  {
    abort();
  }
  
  return *(uint16_t*)&m_ulpVar[index];
}
uint16_t& UlpArrayAccessor::operator [](std::size_t index)
{
  if (index >= m_size)
  {
    abort();
  }
  
  return *(uint16_t*)&m_ulpVar[index];
}

uint32_t UlpArrayAccessor::Size() const
{
  return m_size;
}

void UlpArrayAccessor::Reset(uint16_t value)
{
  for (int i = 0 ; i < UlpCount_Count * 18 ; ++i)
    (*this)[i] = value;
}
    

