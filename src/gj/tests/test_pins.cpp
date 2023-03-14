#include "../base.h"
#include "../sensor.h"
#include "../sensorcb.h"
#include "../eventmanager.h"
#include "tests.h"


static int32_t s_testPinEventCount = 0;

GJ_IRAM void OnTestPinA(DigitalSensor &sensor, bool updated)
{
    s_testPinEventCount++;
}

void TestInputPins()
{
    int32_t pullDown = -1;
    int32_t pullUp = 1;

    bool testPinBOutput = false;
    SetupPin(TEST_PIN_B, testPinBOutput, 0);

    DigitalSensor pinA;
    pinA.SetPin(TEST_PIN_A, pullDown);
    pinA.SetPostISRCB(OnTestPinA);
    pinA.EnableInterrupts(true);

    WritePin(TEST_PIN_B, 0);
    Delay(100);
    TEST_CASE_VALUE_INT32("Pin Input Pull down, LOW", s_testPinEventCount, 0, 0);
    s_testPinEventCount = 0;

    WritePin(TEST_PIN_B, 1);
    Delay(100);
    WritePin(TEST_PIN_B, 0);
    Delay(100);
    WritePin(TEST_PIN_B, 1);
    Delay(100);
    TEST_CASE("Pin Input Pull down, HIGH, LOW, HIGH", s_testPinEventCount == 3);
    s_testPinEventCount = 0;


    WritePin(TEST_PIN_B, 1);
    pinA.SetPin(TEST_PIN_A, pullUp);
    Delay(100);
    TEST_CASE("Pin Input Pull up, HIGH", s_testPinEventCount == 0);
    s_testPinEventCount = 0;
    
    WritePin(TEST_PIN_B, 0);
    Delay(100);
    TEST_CASE("Pin Input Pull up, LOW", s_testPinEventCount == 1);
    s_testPinEventCount = 0;

    WritePin(TEST_PIN_B, 1);
    Delay(100);
    WritePin(TEST_PIN_B, 0);
    Delay(100);
    WritePin(TEST_PIN_B, 1);
    Delay(100);
    TEST_CASE("Pin Input Pull up, HIGH, LOW, HIGH", s_testPinEventCount == 3);
    s_testPinEventCount = 0;
}

void TestInputPinsRefreshRate()
{
  auto onTestPinARefresh = [](DigitalSensor &sensor, bool updated)
  {
    if (updated)
      s_testPinEventCount++;
  };


  int32_t pullDown = -1;
  int32_t pullUp = 1;

  uint32_t refreshRate(10);
  DigitalSensor pinA(refreshRate);
  pinA.SetPin(TEST_PIN_A, pullDown);
  pinA.SetPostISRCB(onTestPinARefresh);
  pinA.EnableInterrupts(true);

  s_testPinEventCount = 0;

  bool testPinBOutput = false;
  SetupPin(TEST_PIN_B, testPinBOutput, 0);

  for (int i = 0 ; i < 10 ; ++i)
  {
    Delay(3);
    WritePin(TEST_PIN_B, i % 2);
  }
  
  Delay(refreshRate);

  TEST_CASE_VALUE_INT32("Pin Input Refresh rate", s_testPinEventCount, 1, 9);
}

void TestDigitalSensorAutoToggleCB()
{
  uint32_t testPinEventCount = 0;

  auto onTestPinARefresh = [&](DigitalSensor &sensor, uint32_t value)
  {
      testPinEventCount++;
  };

  int32_t pullDown = -1;
  int32_t pullUp = 1;

  uint32_t refreshRate(10);
  DigitalSensor pinA(refreshRate);
  DigitalSensorAutoToggleCB sensorCB(&pinA, 0, -1);
  pinA.SetPin(TEST_PIN_A, pullDown);
  pinA.EnableInterrupts(true);

  sensorCB.SetOnChange(onTestPinARefresh);

  bool testPinBIsInput = false;
  SetupPin(TEST_PIN_B, testPinBIsInput, 0);

  auto refreshEventManager = []()
  {
    GJEventManager->WaitForEvents(0);
  };

  for (int i = 0 ; i < 10 ; ++i)
  {
    Delay(refreshRate);
    WritePin(TEST_PIN_B, i % 2);

    refreshEventManager();
  }
  
  TEST_CASE_VALUE_INT32("Pin Input AutoToggleCB", testPinEventCount, 5, 6);
}


static bool OnVDDSensorCalled = false;
static void OnVDDSensorReady(AnalogSensor &sensor)
{
  OnVDDSensorCalled = true;
}

static void WaitForAdc(AnalogSensor &sensor)
{
  const uint32_t begin = GetElapsedMillis();
  while(!sensor.IsReady())
  {
    GJEventManager->WaitForEvents(0);

    Delay(5);

    const uint32_t end = GetElapsedMillis();

    if ((end - begin) > 500)
      break;
  }
}

void TestVDDAdc()
{
  //ESP32 cannot read input voltage
#if !defined(ESP32)
  AnalogSensor sensor(10);

  sensor.SetPin(GJ_ADC_VDD_PIN);
  sensor.SetOnReady(OnVDDSensorReady);
  sensor.Sample();
  
  WaitForAdc(sensor);

  TEST_CASE("VDD Sensor CB called", OnVDDSensorCalled);
  TEST_CASE_VALUE_INT32("VDD Sensor", sensor.GetValue(), 250, 350);
#endif
}


void TestAdc()
{
  SetupPin(TEST_PIN_D, 0, 0);
  WritePin(TEST_PIN_D, 1);

  AnalogSensor sensor(10);
  sensor.SetPin(TEST_PIN_C);

  sensor.Sample();
  WaitForAdc(sensor);
  TEST_CASE("HIGH Adc Sensor ready", sensor.IsReady());
  TEST_CASE_VALUE_INT32("HIGH Adc Sensor value", sensor.GetValue(), 250, 350);

  WritePin(TEST_PIN_D, 0);

  sensor.Sample();
  WaitForAdc(sensor);
  TEST_CASE("LOW Adc Sensor ready", sensor.IsReady());
  TEST_CASE_VALUE_INT32("LOW Adc Sensor value", sensor.GetValue(), 0, 50);
}

void TestDigitalSensorToggle()
{
  int32_t pullDown = -1;
  int32_t pullUp = 1;
  int32_t isrCount = 0;

  bool testPinBIsInput = false;
  SetupPin(TEST_PIN_B, testPinBIsInput, 0);
  WritePin(TEST_PIN_B, 0);
  Delay(1);

  auto onPinISR = [&isrCount](DigitalSensor &sensor, bool updated)
  {
    ++isrCount;
  };

  uint32_t refreshRate(10);
  DigitalSensor pinA(refreshRate);
  pinA.m_polarity = DigitalSensor::Toggle;
  pinA.SetPostISRCB(onPinISR);
  pinA.SetPin(TEST_PIN_A, pullDown);
  pinA.EnableInterrupts(true);

  Delay(1);

  int count = 10;
  for (int i = 0 ; i < count ; ++i)
  {
    WritePin(TEST_PIN_B, 1);
    Delay(1);
    WritePin(TEST_PIN_B, 0);
    Delay(1);
  };
  
  TEST_CASE_VALUE_INT32("DigitalSensor, Toggle", isrCount, count * 2, count * 2);
}


void TestDigitalSensorRise()
{
  int32_t pullDown = -1;
  int32_t pullUp = 1;
  int32_t isrCount = 0;

  bool testPinBIsInput = false;
  SetupPin(TEST_PIN_B, testPinBIsInput, 0);
  WritePin(TEST_PIN_B, 0);
  Delay(1);

  auto onPinISR = [&isrCount](DigitalSensor &sensor, bool updated)
  {
    ++isrCount;
  };

  uint32_t refreshRate(10);
  DigitalSensor pinA(refreshRate);
  pinA.m_polarity = DigitalSensor::Rise;
  pinA.SetPostISRCB(onPinISR);
  pinA.SetPin(TEST_PIN_A, pullDown);
  pinA.EnableInterrupts(true);

  Delay(1);

  int count = 10;
  for (int i = 0 ; i < count ; ++i)
  {
    WritePin(TEST_PIN_B, 1);
    Delay(1);
    WritePin(TEST_PIN_B, 0);
    Delay(1);
  };
  
  TEST_CASE_VALUE_INT32("DigitalSensor, Rise", isrCount, count, count);
}


void TestDigitalSensorFall()
{
  int32_t pullDown = -1;
  int32_t pullUp = 1;
  int32_t isrCount = 0;

  bool testPinBIsInput = false;
  SetupPin(TEST_PIN_B, testPinBIsInput, 0);
  WritePin(TEST_PIN_B, 0);
  Delay(1);

  auto onPinISR = [&isrCount](DigitalSensor &sensor, bool updated)
  {
    ++isrCount;
  };

  uint32_t refreshRate(10);
  DigitalSensor pinA(refreshRate);
  pinA.m_polarity = DigitalSensor::Fall;
  pinA.SetPostISRCB(onPinISR);
  pinA.SetPin(TEST_PIN_A, pullDown);
  pinA.EnableInterrupts(true);

  Delay(1);

  int count = 10;
  for (int i = 0 ; i < count ; ++i)
  {
    WritePin(TEST_PIN_B, 1);
    Delay(1);
    WritePin(TEST_PIN_B, 0);
    Delay(1);
  };
  
  TEST_CASE_VALUE_INT32("DigitalSensor, Fall", isrCount, count, count);
}

void TestPins()
{
  TestInputPins();
  TestInputPinsRefreshRate();
  TestDigitalSensorAutoToggleCB();
  TestVDDAdc();
  TestAdc();
  TestDigitalSensorToggle();
  TestDigitalSensorRise();
  TestDigitalSensorFall();
}