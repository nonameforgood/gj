#include "sensormanager.h"
#include "sensor.h"
#include "sleepmanager.h"
#include "gjulp_main.h"

SensorManager::SensorManager()
{
  
} 

void SensorManager::AddSensor(Sensor &sensor)
{
  m_sensors.push_back(&sensor);
}

void SensorManager::PrepareSleep(SleepInfo const &sleep)
{
  if (sleep.m_eventType != SleepInfo::EventType::Prepare)
    return;

  LOG("SensM::PrepSlp\n\r");
  
  if (sleep.m_runUlp)
    return;
  
  uint64_t ext1MaskLow = 0;
  uint64_t ext1MaskHigh = 0;
  
  auto onSensor = [&](Sensor &sensor)
  {
    Sensor::Flags const wakeFlags = Sensor::Flags::WakeSource_Auto | Sensor::Flags::WakeSource_Low | Sensor::Flags::WakeSource_High;

    if (!sensor.IsFlagSet(wakeFlags))
      return;

    gpio_num_t const gpio = (gpio_num_t)sensor.GetPin();
    uint64_t const sensorPin = (uint64_t)gpio;

    if (sensor.IsFlagSet(Sensor::Flags::WakeSource_Auto))
    {
      //select wake mode based on the current sensor value
      uint32_t const value = sensor.GetValue();
      if (value != 0)
        ext1MaskLow |= uint64_t(1) << uint64_t(sensorPin);
      else
        ext1MaskHigh |= uint64_t(1) << uint64_t(sensorPin);
    }
    else
    {
      bool isSensorDetected = sensor.IsDetected();
      
      if (isSensorDetected)
      {
        //something is connected to the sensor pin, wake up when state changes
        if (sensor.IsFlagSet(Sensor::Flags::WakeSource_Low))
          ext1MaskLow |= uint64_t(1) << uint64_t(sensorPin);
        else
          ext1MaskHigh |= uint64_t(1) << uint64_t(sensorPin);
      }
    }
  };
  
  ForEachSensor(onSensor);
  
  if (ext1MaskLow)
  {
    if (ext1MaskHigh)
      LOG("  WARNING:wake from any high (0x%08x%08x) not applied, all low is already used\n\r",
        (uint32_t)(ext1MaskHigh >> 32), (uint32_t)(ext1MaskHigh & 0xffffffff));

    SER("  EXT1 on wake on all low to 0x%08x%08x\n\r", 
      (uint32_t)(ext1MaskLow >> 32), (uint32_t)(ext1MaskLow & 0xffffffff));
    sleep.m_sleepManager.SetWakeFromAllLowEXT1(ext1MaskLow);
  }
  else if (ext1MaskHigh)
  {
    SER("  setting wake on any high to 0x%08x%08x\n\r", 
      (uint32_t)(ext1MaskHigh >> 32), (uint32_t)(ext1MaskHigh & 0xffffffff));
    sleep.m_sleepManager.SetWakeFromAnyHighEXT1(ext1MaskHigh);
  }

  //bool const allSensorDetected = AllSensorsDetected();
  //if (!allSensorDetected)
  //{
  //  //if no sensor is detected, force wake up after a while
  //  
  //  sleep.m_sleepManager.SetWakeFromTimer(60 * 10); //10 minutes
  //  LOG("  Set wake up from timer cause:sensor not all detected\n\r");
  //}
}

void SensorManager::UpdateSensors()
{
  GJ_PROFILE(SensorManager::UpdateSensors);
  
  for (Sensor* sensor : m_sensors)
      sensor->Update();

  if (ulp_sensor_events)
    ulp_sensor_events->Reset(0);
}

bool SensorManager::AllSensorsDetected() const
{
  for (Sensor* sensor : m_sensors)
  {
    if (!sensor->IsDetected())
      return false;
  }
  return true;
}