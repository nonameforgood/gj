
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

void TestGJ();