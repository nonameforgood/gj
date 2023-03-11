#include "../base.h"
#include "../commands.h"
#include "tests.h"

void TestPinCommands()
{
  char lastTerminal[64];

  auto onTerminal = [&lastTerminal](const char *str)
  {
    strcpy(lastTerminal, str);
  };

  uint32_t termHandle = AddTerminalHandler(onTerminal);

  char command[64];
  char expected[64];

  sprintf(command, "pin setup %d 0", TEST_PIN_B);
  InterpretCommand(command);
  sprintf(expected, "Pin %02d set to output pull=0\n\r", TEST_PIN_B);
  TEST_CASE("Pin Command_Setup Output", strcmp(lastTerminal, expected) == 0);

  sprintf(command, "pin setup %d 1 0", TEST_PIN_A);
  InterpretCommand(command);
  sprintf(expected, "Pin %02d set to input pull=0\n\r", TEST_PIN_A);
  TEST_CASE("Pin Command_Setup Input", strcmp(lastTerminal, expected) == 0);

  sprintf(command, "pin write %d 0", TEST_PIN_B);
  InterpretCommand(command);
  sprintf(expected, "Pin %02d set to value 0\n\r", TEST_PIN_B);
  TEST_CASE("Pin Command_WritePin LOW", strcmp(lastTerminal, expected) == 0);

  sprintf(command, "pin read %d", TEST_PIN_A);
  InterpretCommand(command);
  sprintf(expected, "Pin %d value=0\n\r", TEST_PIN_A);
  TEST_CASE("Pin Command_ReadPin, LOW", strcmp(lastTerminal, expected) == 0);
  TEST_CASE_VALUE_INT32("Pin is LOW ", ReadPin(TEST_PIN_A), 0, 0);

  sprintf(command, "pin write %d 1", TEST_PIN_B);
  InterpretCommand(command);
  sprintf(expected, "Pin %02d set to value 1\n\r", TEST_PIN_B);
  TEST_CASE("Pin Command_WritePin HIGH", strcmp(lastTerminal, expected) == 0);
  
  sprintf(command, "pin read %d", TEST_PIN_A);
  InterpretCommand(command);
  sprintf(expected, "Pin %d value=1\n\r", TEST_PIN_A);
  TEST_CASE("Pin Command_Read, HIGH", strcmp(lastTerminal, expected) == 0);
  TEST_CASE_VALUE_INT32("Pin is HIGH ", ReadPin(TEST_PIN_A), 1, 1);

  WritePin(TEST_PIN_B, 0);

  RemoveTerminalHandler(termHandle);
}