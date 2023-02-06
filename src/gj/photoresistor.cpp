#include "photoresistor.h"

Photoresistor::Photoresistor(uint16_t pin)
: m_pin(pin)
{

}
void Photoresistor::Update()
{
  uint32_t current = digitalRead( m_pin );
  
  if ( current == m_real )
  {
    uint32_t const m = millis();
    uint32_t const e = m - m_lastChange;

    if ( e > 30 )
    {
      m_value = current;
    }
  }
  else
  {
    m_lastChange = millis();
  }

  m_real = current;
}