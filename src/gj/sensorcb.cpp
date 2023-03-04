#include "sensorcb.h"
#include "sensor.h"
#include "eventmanager.h"

DigitalSensorCB::DigitalSensorCB(DigitalSensor *sensor)
{
    auto callForward = [this](DigitalSensor &sensor, bool updated)
    {
      this->OnSensorISR(sensor, updated);
    };

    sensor->SetPostISRCB(callForward);
}

void DigitalSensorCB::SetOnChange(TCallback cb)
{
    m_onChange = cb;
}

GJ_IRAM void DigitalSensorCB::OnSensorISR(DigitalSensor &sensor, bool updated)
{
    if (m_onChange && updated)
    {
        auto callUserFunc = [&, this]()
        {
            m_onChange(sensor, 0);
        };

        GJEventManager->Add(callUserFunc);
    }
}


DigitalSensorAutoToggleCB::DigitalSensorAutoToggleCB(DigitalSensor *sensor, int32_t pullLow, int32_t pullHigh)
: m_pullLow(pullLow)
, m_pullHigh(pullHigh)
{
    auto callForward = [this](DigitalSensor &sensor, bool updated)
    {
      this->OnSensorISR(sensor, updated);
    };

    sensor->SetPostISRCB(callForward);
}

void DigitalSensorAutoToggleCB::SetOnChange(TCallback cb)
{
    m_onChange = cb;
}

GJ_IRAM void DigitalSensorAutoToggleCB::OnSensorISR(DigitalSensor &sensor, bool updated)
{
  //DigitalSensor's GetValue implements a refresh delay and might not return the real time state
  const int32_t val = ReadPin(sensor.GetPin());

  int32_t pull;

  if (val == 0)
    pull = m_pullLow;
  else
    pull = m_pullHigh;

  sensor.SetPin(sensor.GetPin(), pull);

  //printf("Val %d Set pull to %d updated:%d\n\r", val, pull, (int)updated);

  if (m_onChange && updated)
  {
    auto callUserFunc = [&, this, val]()
    {
        m_onChange(sensor, val);
    };

    GJEventManager->Add(callUserFunc);
  }
}