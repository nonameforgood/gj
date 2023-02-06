#include "base.h"

class Photoresistor
{
public:

  Photoresistor(uint16_t pin);

  void Update();
  uint32_t GetValue() const { return m_value; }
  
private:
  uint16_t const m_pin;
  uint32_t m_real = 0;
  uint32_t m_lastChange = 0;
  uint32_t m_value = 0;
};