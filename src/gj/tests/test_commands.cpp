#include "../base.h"
#include "../commands.h"
#include "tests.h"

void TestCommands()
{

  BEGIN_TEST(CommandInfo_NoArgs)
  {
    const char *command = "commandName";

    CommandInfo info;
    GetCommandInfo(command, info);

    TEST_CASE_VALUE_INT32("command length", info.m_commandLength, 11, 11);
    TEST_CASE_VALUE_INT32("command totalLength", info.m_totalLength, 11, 11);
    TEST_CASE_VALUE_INT32("command argCount", info.m_argCount, 0, 0);
  }

  BEGIN_TEST(CommandInfo_1Arg)
  {
    const char *command = "commandName rgt";

    CommandInfo info;
    GetCommandInfo(command, info);

    TEST_CASE_VALUE_INT32("command length", info.m_commandLength, 11, 11);
    TEST_CASE_VALUE_INT32("command total length", info.m_totalLength, 15, 15);
    TEST_CASE_VALUE_INT32("command arg count", info.m_argCount, 1, 1);

    TEST_CASE_VALUE_PTR("command arg begin", info.m_argsBegin[0], &command[12], &command[12]);
    TEST_CASE_VALUE_INT32("command arg offset", info.m_argsOffset[0], 12, 12);
    TEST_CASE_VALUE_INT32("command arg length", info.m_argsLength[0], 3, 3);
  }


  BEGIN_TEST(CommandInfo_1QuotedArg)
  {
    const char *command = "commandName \"rgt\"";

    CommandInfo info;
    GetCommandInfo(command, info);

    TEST_CASE_VALUE_INT32("command length", info.m_commandLength, 11, 11);
    TEST_CASE_VALUE_INT32("command total length", info.m_totalLength, 17, 17);
    TEST_CASE_VALUE_INT32("command arg count", info.m_argCount, 1, 1);

    TEST_CASE_VALUE_PTR("command arg begin", info.m_argsBegin[0], &command[13], &command[13]);
    TEST_CASE_VALUE_INT32("command arg offset", info.m_argsOffset[0], 13, 13);
    TEST_CASE_VALUE_INT32("command arg length", info.m_argsLength[0], 3, 3);
  }

  BEGIN_TEST(CommandInfo_4Args)
  {
    const char *command = "commandName argA argAVal argB 5645";

    CommandInfo info;
    GetCommandInfo(command, info);

    TEST_CASE_VALUE_INT32("command length", info.m_commandLength, 11, 11);
    TEST_CASE_VALUE_INT32("command total length", info.m_totalLength, 34, 34);
    TEST_CASE_VALUE_INT32("command arg count", info.m_argCount, 4, 4);
    TEST_CASE_VALUE_PTR("command arg begin", info.m_argsBegin[0], &command[12], &command[12]);
    TEST_CASE_VALUE_PTR("command arg begin", info.m_argsBegin[1], &command[17], &command[17]);
    TEST_CASE_VALUE_PTR("command arg begin", info.m_argsBegin[2], &command[25], &command[25]);
    TEST_CASE_VALUE_PTR("command arg begin", info.m_argsBegin[3], &command[30], &command[30]);

    TEST_CASE_VALUE_INT32("command arg offset", info.m_argsOffset[0], 12, 12);
    TEST_CASE_VALUE_INT32("command arg offset", info.m_argsOffset[1], 17, 17);
    TEST_CASE_VALUE_INT32("command arg offset", info.m_argsOffset[2], 25, 25);
    TEST_CASE_VALUE_INT32("command arg offset", info.m_argsOffset[3], 30, 30);

    TEST_CASE_VALUE_INT32("command arg length", info.m_argsLength[0], 4, 4);
    TEST_CASE_VALUE_INT32("command arg length", info.m_argsLength[1], 7, 7);
    TEST_CASE_VALUE_INT32("command arg length", info.m_argsLength[2], 4, 4);
    TEST_CASE_VALUE_INT32("command arg length", info.m_argsLength[3], 4, 4);
  }

  BEGIN_TEST(CommandInfo_4QuotedArgs)
  {
    const char *command = "commandName2 \"argA\" \"argAVal\" \"argB\" \"5645\"";

    CommandInfo info;
    GetCommandInfo(command, info);

    TEST_CASE_VALUE_INT32("command length", info.m_commandLength, 12, 12);
    TEST_CASE_VALUE_INT32("command total length", info.m_totalLength, 43, 43);
    TEST_CASE_VALUE_INT32("command arg count", info.m_argCount, 4, 4);
    TEST_CASE_VALUE_PTR("command arg begin", info.m_argsBegin[0], &command[14], &command[14]);
    TEST_CASE_VALUE_PTR("command arg begin", info.m_argsBegin[1], &command[21], &command[21]);
    TEST_CASE_VALUE_PTR("command arg begin", info.m_argsBegin[2], &command[31], &command[31]);
    TEST_CASE_VALUE_PTR("command arg begin", info.m_argsBegin[3], &command[38], &command[38]);

    TEST_CASE_VALUE_INT32("command arg offset", info.m_argsOffset[0], 14, 14);
    TEST_CASE_VALUE_INT32("command arg offset", info.m_argsOffset[1], 21, 21);
    TEST_CASE_VALUE_INT32("command arg offset", info.m_argsOffset[2], 31, 31);
    TEST_CASE_VALUE_INT32("command arg offset", info.m_argsOffset[3], 38, 38);

    TEST_CASE_VALUE_INT32("command arg length", info.m_argsLength[0], 4, 4);
    TEST_CASE_VALUE_INT32("command arg length", info.m_argsLength[1], 7, 7);
    TEST_CASE_VALUE_INT32("command arg length", info.m_argsLength[2], 4, 4);
    TEST_CASE_VALUE_INT32("command arg length", info.m_argsLength[3], 4, 4);
  }


  BEGIN_TEST(CommandInfo_SomeQuotedArgs)
  {
    const char *command = "commandName222 argA \"some spaces here\" argB \"some more spaces\"";

    CommandInfo info;
    GetCommandInfo(command, info);

    TEST_CASE_VALUE_INT32("command length", info.m_commandLength, 14, 14);
    TEST_CASE_VALUE_INT32("command total length", info.m_totalLength, 62, 62);
    TEST_CASE_VALUE_INT32("command arg count", info.m_argCount, 4, 4);
    TEST_CASE_VALUE_PTR("command arg begin", info.m_argsBegin[0], &command[15], &command[15]);
    TEST_CASE_VALUE_PTR("command arg begin", info.m_argsBegin[1], &command[21], &command[21]);
    TEST_CASE_VALUE_PTR("command arg begin", info.m_argsBegin[2], &command[39], &command[39]);
    TEST_CASE_VALUE_PTR("command arg begin", info.m_argsBegin[3], &command[45], &command[45]);

    TEST_CASE_VALUE_INT32("command arg offset", info.m_argsOffset[0], 15, 15);
    TEST_CASE_VALUE_INT32("command arg offset", info.m_argsOffset[1], 21, 21);
    TEST_CASE_VALUE_INT32("command arg offset", info.m_argsOffset[2], 39, 39);
    TEST_CASE_VALUE_INT32("command arg offset", info.m_argsOffset[3], 45, 45);

    TEST_CASE_VALUE_INT32("command arg length", info.m_argsLength[0], 4, 4);
    TEST_CASE_VALUE_INT32("command arg length", info.m_argsLength[1], 16, 16);
    TEST_CASE_VALUE_INT32("command arg length", info.m_argsLength[2], 4, 4);
    TEST_CASE_VALUE_INT32("command arg length", info.m_argsLength[3], 16, 16);

  }


  BEGIN_TEST(CommandInfo_ConvertCommandInfo)
  {
    const char *command = "commandName222 argA \"some spaces here\" argB \"some more spaces\"";

    CommandInfo info;
    GetCommandInfo(command, info);

    TEST_CASE_VALUE_INT32("command length", info.m_commandLength, 14, 14);
    TEST_CASE_VALUE_INT32("command total length", info.m_totalLength, 62, 62);
    TEST_CASE_VALUE_INT32("command arg count", info.m_argCount, 4, 4);

    TEST_CASE("command arg", info.m_args[0] == "argA");
    TEST_CASE("command space", info.m_args[1] == "some spaces here");
    TEST_CASE("command argB", info.m_args[2] == "argB");
    TEST_CASE("command space", info.m_args[3] == "some more spaces");
  }

}