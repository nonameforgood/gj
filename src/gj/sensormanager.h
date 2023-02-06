#pragma once

#include "vector.h"

class Sensor;
struct SleepInfo;

class SensorManager
{
public:
    SensorManager();
    

    void AddSensor(Sensor &sensor);
    void PrepareSleep(SleepInfo const &sleep);

    template<typename T>
    void ForEachSensor(T &callable);

    void UpdateSensors();
    bool AllSensorsDetected() const;
    
private:
   Vector<Sensor*> m_sensors;
};

template<typename T>
void SensorManager::ForEachSensor(T &callable)
{
  for (Sensor* sensor : m_sensors)
      callable(*sensor);
}
