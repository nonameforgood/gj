#pragma once

#include "base.h"
#include "platform.h"
#include <functional>

class DigitalSensor;

class DigitalSensorCB
{
public:

    DigitalSensorCB();
    DigitalSensorCB(DigitalSensor *sensor);

    typedef std::function<void(DigitalSensor &sensor, uint32_t newValue)> TCallback;

    void SetOnChange(TCallback cb);

private:

    TCallback m_onChange;
    
    GJ_IRAM void OnSensorISR(DigitalSensor &sensor);
};
