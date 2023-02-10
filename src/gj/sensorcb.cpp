#include "sensorcb.h"

DigitalSensorCB::DigitalSensorCB(DigitalSensor *sensor)
{
    DigitalSensor::TCallback cb;

    cb = std::bind(&DigitalSensorCB::OnSensorChange, this);

    sensor->OnChange(cb);
}

void DigitalSensorCB::OnSensorChange(DigitalSensor &sensor, uint32_t newValue)
{
    if (m_onChange)
    {
        EventManager::Function func;

        func = std::bind(m_onChange, sensor, newValue);
        GJEventManager->Add(func)
    }
}