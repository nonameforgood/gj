#include "../base.h"
#include "../sensor.h"
#include "../eventmanager.h"

//must connect TEST_PIN_A to TEST_PIN_B

#if defined(ESP32)
    #define TEST_PIN_A 23
    #define TEST_PIN_B 18

    #define TEST_PIN_C 23
    #define TEST_PIN_D 18
#elif defined(NRF51)
    #define TEST_PIN_A 11
    #define TEST_PIN_B 12

    #define TEST_PIN_C 1
    #define TEST_PIN_D 25
#elif defined(NRF52)
    #define TEST_PIN_A 16
    #define TEST_PIN_B 17

    #define TEST_PIN_C 4
    #define TEST_PIN_D 29
#endif

static int32_t s_testPinEventCount = 0;

GJ_IRAM void OnTestPinA(DigitalSensor &sensor)
{
    s_testPinEventCount++;
}

#define TEST_CASE(name, cond) SER("Test '%s': %s %s:%d\n\r",(cond) ? "SUCCEEDED" : "   FAILED", name, __FILE__, __LINE__)

void TestInputPins()
{
    int32_t pullDown = -1;
    int32_t pullUp = 1;

    DigitalSensor pinA;
    pinA.SetPin(TEST_PIN_A, pullDown);
    pinA.SetPostISRCB(OnTestPinA);
    pinA.EnableInterrupts(true);

    bool testPinBOutput = false;
    SetupPin(TEST_PIN_B, testPinBOutput, 0);

    WritePin(TEST_PIN_B, 0);
    Delay(100);
    TEST_CASE("Pin Input Pull down, LOW", s_testPinEventCount == 0);
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
  AnalogSensor sensor(10);

  sensor.SetPin(GJ_ADC_VDD_PIN);
  sensor.SetOnReady(OnVDDSensorReady);
  sensor.Sample();
  
  WaitForAdc(sensor);

  TEST_CASE("VDD Sensor CB called", OnVDDSensorCalled);
  TEST_CASE("VDD Sensor value is [250, 350]", sensor.GetValue() >= 250 && sensor.GetValue() <= 350);
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
  TEST_CASE("HIGH Adc Sensor value is [250, 350]", sensor.GetValue() >= 250 && sensor.GetValue() <= 350);

  WritePin(TEST_PIN_D, 0);

  sensor.Sample();
  WaitForAdc(sensor);
  TEST_CASE("LOW Adc Sensor ready", sensor.IsReady());
  TEST_CASE("LOW Adc Sensor value is [0, 50]", sensor.GetValue() >= 0 && sensor.GetValue() <= 50);
}


void TestPins()
{
  TestInputPins();
  TestVDDAdc();
  TestAdc();
}