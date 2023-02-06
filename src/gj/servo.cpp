#include "servo.h"
#include "base.h"

void GJServo::Init(uint32_t pin)
{
    m_servo.attach(pin, 0);
    //uint32_t v = m_servo.read();
    //SER("Servo init:%d\n\r", v);
}
void GJServo::Term()
{
    m_servo.detach();
}

void GJServo::Set(uint16_t deg)
{
    m_angle = deg;
    m_servo.write(deg);
}

uint16_t GJServo::Get() const
{
    return m_angle;
}