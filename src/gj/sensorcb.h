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
    
    GJ_IRAM void OnSensorISR(DigitalSensor &sensor, bool updated);
};



class DigitalSensorAutoToggleCB
{
public:

    DigitalSensorAutoToggleCB(DigitalSensor *sensor, int32_t pullLow, int32_t pullHigh);

    typedef std::function<void(DigitalSensor &sensor, uint32_t newValue)> TCallback;

    void SetOnChange(TCallback cb);

private:

    const int32_t m_pullLow;
    const int32_t m_pullHigh;

    TCallback m_onChange;
    
    GJ_IRAM void OnSensorISR(DigitalSensor &sensor, bool updated);
};
