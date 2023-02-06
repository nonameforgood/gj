#pragma once

/*
    Put your ULP globals here you want visibility
    for your sketch. Add "ulp_" to the beginning
    of the variable name and must be size 'uint32_t'
*/
#include "Arduino.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");
extern uint32_t ulp_entry;

class UlpAccessor
{
  public:
    UlpAccessor(uint32_t &ulpVar);
    
    operator uint32_t() const;
    void operator =(uint32_t v);

private:
  UlpAccessor(UlpAccessor const &other) = delete;
  UlpAccessor(UlpAccessor &&other) = delete;
  uint32_t &m_ulpVar;
};

class UlpArrayAccessor
{
  public:
    UlpArrayAccessor(uint32_t *ulpVar, uint32_t arraySize);
    
    const uint16_t& operator [](std::size_t index) const;
    uint16_t& operator [](std::size_t index);

    void Reset(uint16_t value = 0);
    
    uint32_t Size() const;
    
private:
  UlpArrayAccessor(UlpArrayAccessor const &other) = delete;
  UlpArrayAccessor(UlpArrayAccessor &&other) = delete;
  uint32_t * const m_ulpVar;
  uint32_t const m_size;
};

enum UlpCountType
{
  UlpCount_Change = 0,
  UlpCount_Rise = 1,
  UlpCount_Fall = 2,
  UlpCount_Count = 3,
};

extern UlpArrayAccessor *ulp_sensor_events;
