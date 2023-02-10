#pragma once

#include <functional>

class DigitalSensor;

class DigitalSensorCB
{
public:

    DigitalSensorCB();
    DigitalSensorCB(DigitalSensor *sensor);

    typedef std::function<void(DigitalSensor &sensor, uint32_t newValue)> TCallback;

    void OnChange(TCallback cb);

private:

    TCallback m_onChange;
    
    void OnSensorChange(DigitalSensor &sensor);
};
