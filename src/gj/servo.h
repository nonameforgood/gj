#pragma once

#include "servo/servo.h"

class GJServo
{
    public:

    void Init(uint32_t pin);
    void Term();
    void Set(uint16_t deg);
    uint16_t Get() const;
    
private:

    Servo m_servo;

    uint16_t m_angle = -1;
};