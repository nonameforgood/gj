#pragma once

#include "base.h"
#include "enums.h"

//#include "function_ref.hpp"
#include <functional>

#ifdef ESP32
  #include <atomic>
  #include <SoftwareSerial.h>
#elif defined(NRF)
  #include <nrf_gpiote.h>
  #include <nrf_drv_gpiote.h>
#endif

class Sensor
{
public:

    enum class Flags
    {
      None = 0,
      WakeSource_Auto = 1,
      WakeSource_Low = 2,
      WakeSource_High = 4
    };

    void SetFlags(Flags flags);
    Flags GetFlags() const;
    bool IsFlagSet(Flags flags) const;
    
    uint16_t GetPin() const { return m_pin; }
    virtual void SetPin(uint16_t pin);
    virtual uint32_t GetValue() const = 0;
    virtual bool IsDetected() const { return m_detected; }
    virtual void Update() = 0;
  
protected:
  Flags m_flags = Flags::None;
  bool m_detected = false;
private:
  uint16_t m_pin = 0;
};

ENABLE_BITMASK_OPERATORS(Sensor::Flags);

class DigitalSensor : public Sensor
{
public:
  DigitalSensor(U16 refresh = 10);

  void SetPin(uint16_t pin, int32_t pull = -1);
  virtual void Update();
  virtual uint32_t GetValue() const { return m_value; }

  void EnableInterrupts(bool enable);

  typedef std::function<void(DigitalSensor const &sensor, uint32_t count)> ConstCallback;
  typedef std::function<void(DigitalSensor &sensor, uint32_t count)> TCallback;
  
  void OnChangeConst(ConstCallback cb);
  void OnChange(TCallback cb);
  void OnFall(ConstCallback cb);
  
private:
  uint16_t const m_refresh;
  uint32_t m_lastChange = 0;
  uint32_t m_value = 0;
  int32_t m_pull = -1;
  bool m_enableInterrupts = false;
  
  bool m_wakeHandled = false;
  
#ifdef ESP32
  std::atomic<uint32_t> m_fallCount;
  std::atomic<uint32_t> m_changeCount;
#elif defined(NRF)
  uint32_t m_fallCount = 0;
  uint32_t m_changeCount = 0;
#endif

  ConstCallback m_onConstChange;
  TCallback m_onChange;
  ConstCallback m_onFall;

  void GJ_IRAM UpdateValue();
  void GJ_IRAM OnChange();
#ifdef ESP32
  static void GJ_IRAM InterruptHandler(void *param);
#elif defined(NRF)
  static void InterruptHandler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action);

  static constexpr uint32_t MaxRemaps = 4;
  static DigitalSensor* s_sensors[MaxRemaps]; 
  static uint8_t s_remap[32]; 
#endif
};

//automatically switch pull-down or up based on the pin value
class AutoToggleSensor : protected DigitalSensor
{
public:
  AutoToggleSensor(U16 refresh = 10);
  void SetPin(uint16_t pin);

  virtual uint32_t GetValue() const { return DigitalSensor::GetValue(); }

  typedef std::function<void(AutoToggleSensor &sensor, uint32_t count)> TCallback;
  void SetOnChange(TCallback cb);

private:

  TCallback m_onChange;

  static void OnDigitalChange(DigitalSensor &sensor, uint32_t count);
};

class AnalogSensor : public Sensor
{
public:
  AnalogSensor(uint32_t reads);

  void SetPin(uint16_t pin);
  virtual void Update();
  virtual uint32_t GetValue() const;
  uint32_t GetRawValue() const;
  uint32_t GetDetailedRawValue(uint32_t *min, uint32_t *max) const;

  void SetOffset(uint32_t offset);

private:
  uint32_t m_reads = 1;
  uint32_t m_offset = 0;
  bool m_isADC1 = true;
  uint32_t m_channel = 0;
};

class VoltageSensor : public AnalogSensor
{
public:
  VoltageSensor();

  void SetPin(uint16_t pin);
  virtual void Update();
  virtual uint32_t GetValue() const;

 private:
 
  
};

class BuiltInTemperatureSensor : public Sensor
{
public:
  void SetPin(uint16_t pin);
  virtual void Update();
  virtual uint32_t GetValue() const;
};

class RFIDReader : public Sensor
{
public:
  RFIDReader(uint32_t retention = 10000);

  void Init(uint16_t rx, uint16_t tx);

  void Update();

  uint32_t GetValue() const;
  
  typedef std::function<void(RFIDReader const &sensor, uint32_t tag)> TCallback;
  
  void OnChange(TCallback cb);

  void EnableDebug(bool enable);
private:

  static const int BUFFER_SIZE = 14; // RFID DATA FRAME FORMAT: 1byte head (value: 2), 10byte data (2byte version + 8byte tag), 2byte checksum, 1byte tail (value: 3)

#ifdef ESP32
  SoftwareSerial m_comm; 
#endif
  uint8_t m_buffer[BUFFER_SIZE]; // used to store an incoming data frame 
  int m_bufferIndex = 0;
  bool m_debug = false;

  uint32_t m_retention = 0;
  uint32_t m_timeStamp = 0;
  uint32_t m_tag = 0;

  TCallback m_onChange;

  void Read();
  void ExtractTag();
  void SetTag(uint32_t tag);
};