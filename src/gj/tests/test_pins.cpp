#include "../base.h"
#include "../sensor.h"
#include "../eventmanager.h"

//must connect TEST_PIN_A to TEST_PIN_B

#if defined(ESP32)
    #define TEST_PIN_A 23
    #define TEST_PIN_B 18
#elif defined(NRF51)
    #define TEST_PIN_A 11
    #define TEST_PIN_B 12
#elif defined(NRF52)
    #define TEST_PIN_A 16
    #define TEST_PIN_B 17
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

void TestVDDAdc()
{
  AnalogSensor sensor(10);

  sensor.SetPin(0);
  sensor.SetOnReady(OnVDDSensorReady);
  sensor.Sample();
  
  const uint32_t begin = GetElapsedMillis();
  while(!sensor.IsReady())
  {
    GJEventManager->WaitForEvents(0);

    Delay(5);

    const uint32_t end = GetElapsedMillis();

    if ((end - begin) > 500)
      break;
  }

  TEST_CASE("VDD Sensor CB called", OnVDDSensorCalled);
  TEST_CASE("VDD Sensor value >= 250", sensor.GetValue() >= 250);
}


void TestPins()
{
  TestInputPins();
  TestVDDAdc();
}