#include "sensorcb.h"
#include "sensor.h"
#include "eventmanager.h"

DigitalSensorCB::DigitalSensorCB(DigitalSensor *sensor)
{
    auto callForward = [this](DigitalSensor &sensor)
    {
      this->OnSensorISR(sensor);
    };

    sensor->SetPostISRCB(callForward);
}

void DigitalSensorCB::SetOnChange(TCallback cb)
{
    m_onChange = cb;
}

GJ_IRAM void DigitalSensorCB::OnSensorISR(DigitalSensor &sensor)
{
    if (m_onChange)
    {
        auto callUserFunc = [&, this]()
        {
            m_onChange(sensor, 0);
        };

        GJEventManager->Add(callUserFunc);
    }
}