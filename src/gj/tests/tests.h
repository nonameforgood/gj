
#define BEGIN_TEST(name) SER("Starting test '%s'\n", #name); 

void LogTest(const char *name, bool success);

#define TEST_CASE(name, cond)                                                                           \
{                                                                                                       \
  const bool res = (cond);                                                                              \
  const auto lName = (name);                                                                            \
  LogTest(lName, res);                                                                                  \
  SER("Test '%s': %s\n\r", res ? "SUCCEEDED" : "   FAILED", lName);                                     \
}

#define TEST_CASE_VALUE_INT32(name, val, min, max)                                                      \
{                                                                                                       \
  const int32_t lVal=(val), lMin=(min), lMax=(max);                                                     \
  const bool res = (lVal >= lMin && lVal <= lMax);                                                      \
  const auto lName = (name);                                                                            \
  LogTest(lName, res);                                                                                  \
  SER("Test '%s': %s (%d <= %d <= %d)\n\r", res ? "SUCCEEDED" : "   FAILED", lName, lMin, lVal, lMax);  \
}

#define TEST_CASE_VALUE_PTR(name, val, min, max)                                                        \
{                                                                                                       \
  const void* lVal=(val), *lMin=(min), *lMax=(max);                                                     \
  const bool res = (lVal >= lMin && lVal <= lMax);                                                      \
  const auto lName = (name);                                                                            \
  LogTest(lName, res);                                                                                  \
  SER("Test '%s': %s (%p <= %p <= %p)\n\r", res ? "SUCCEEDED" : "   FAILED", lName, lMin, lVal, lMax);  \
}

#define TEST_CASE_VALUE_BOOL(name, val, expect)                                                         \
{                                                                                                       \
  const bool lVal=(val), lExpect=(expect);                                                              \
  const bool res = (lVal == lExpect);                                                                   \
  const auto lName = (name);                                                                            \
  LogTest(lName, res);                                                                                  \
  SER("Test '%s': %s (%d == %d)\n\r", res ? "SUCCEEDED" : "   FAILED", lName, lVal, lExpect);           \
}

//must connect TEST_PIN_A to TEST_PIN_B
//must connect TEST_PIN_C to TEST_PIN_D

#if defined(ESP32)
    #define TEST_PIN_A 33
    #define TEST_PIN_B 18

    #define TEST_PIN_C 33
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

void TestGJ();