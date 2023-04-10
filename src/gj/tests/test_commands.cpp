#include "../base.h"
#include "../commands.h"
#include "tests.h"


static bool SubCommand_TestA_Called = false;
static bool SubCommand_TestB_Called = false;
static bool SubCommand_TestC_Called = false;

static void SubCommand_TestA(const CommandInfo &info)
{
  SubCommand_TestA_Called = true;

  TEST_CASE_VALUE_BOOL("SubCommand_TestA arg a", info.m_args[0] == "argA", true);
  TEST_CASE_VALUE_BOOL("SubCommand_TestA arg b", info.m_args[1] == "argB", true);
  TEST_CASE_VALUE_BOOL("SubCommand_TestA arg c", info.m_args[2] == "argC", true);
}

static void SubCommand_TestB(const CommandInfo &info)
{
  SubCommand_TestB_Called = true;
  TEST_CASE_VALUE_BOOL("SubCommand_TestB arg a", info.m_args[0] == "argD", true);
  TEST_CASE_VALUE_BOOL("SubCommand_TestB arg b", info.m_args[1] == "argE", true);
  TEST_CASE_VALUE_BOOL("SubCommand_TestB arg c", info.m_args[2] == "argF", true);
}

static void SubCommand_TestC(const CommandInfo &info)
{
  SubCommand_TestC_Called = true;
  TEST_CASE_VALUE_INT32("SubCommand_TestC no args", info.m_argCount, 0, 0);
}


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

  BEGIN_TEST(SubCommands)
  { 
    static constexpr const char * const names[] = {
      "testa",
      "testb",
      "testc"
    };

    static void (*const funcs[])(const CommandInfo &commandInfo){
      SubCommand_TestA,
      SubCommand_TestB,
      SubCommand_TestC
      };

    const SubCommands subCommands = {3, names, funcs};

    {
      const char *command = "somecmd testa argA argB argC";
      SubCommandForwarder(command, subCommands);
      TEST_CASE_VALUE_BOOL("SubCommand TestA Called", SubCommand_TestA_Called, true);
    }

    {
      const char *command = "somecmd testb argD argE argF";
      SubCommandForwarder(command, subCommands);
      TEST_CASE_VALUE_BOOL("SubCommand TestB Called", SubCommand_TestB_Called, true);
    }

    {
      const char *command = "somecmd testc";
      SubCommandForwarder(command, subCommands);
      TEST_CASE_VALUE_BOOL("SubCommand TestC Called", SubCommand_TestC_Called, true);
    }

    SubCommand_TestA_Called = false;
    SubCommand_TestB_Called = false;
    SubCommand_TestC_Called = false;

    //test unknown command
    {
      const char *command = "somecmd testi arg";
      SubCommandForwarder(command, subCommands);
      TEST_CASE_VALUE_BOOL("SubCommand not listed, TestA not called", SubCommand_TestA_Called, false);
      TEST_CASE_VALUE_BOOL("SubCommand not listed, TestB not called", SubCommand_TestB_Called, false);
      TEST_CASE_VALUE_BOOL("SubCommand not listed, TestC not called", SubCommand_TestC_Called, false);
    }

    
    SubCommand_TestA_Called = false;
    SubCommand_TestB_Called = false;
    SubCommand_TestC_Called = false;

    //test no command
    {
      const char *command = "somecmd";
      SubCommandForwarder(command, subCommands);
      TEST_CASE_VALUE_BOOL("SubCommand none, TestA not called", SubCommand_TestA_Called, false);
      TEST_CASE_VALUE_BOOL("SubCommand none, TestB not called", SubCommand_TestB_Called, false);
      TEST_CASE_VALUE_BOOL("SubCommand none, TestC not called", SubCommand_TestC_Called, false);
    }
  }

  BEGIN_TEST(MultiSpaces)
  {
    //trailing space
    {
      const char *command = "somecmd ";
      CommandInfo info;
      GetCommandInfo(command, info);

      TEST_CASE_VALUE_INT32("MultiSpaces, trailing, command length", info.m_commandLength, 7, 7);
      TEST_CASE_VALUE_INT32("MultiSpaces, trailing, command total length", info.m_totalLength, 8, 8);
      TEST_CASE_VALUE_INT32("MultiSpaces, trailing, command arg count", info.m_argCount, 0, 0);
    }

    //extra space before arg0
    {
      const char *command = "somecmd  argA";
      CommandInfo info;
      GetCommandInfo(command, info);

      TEST_CASE_VALUE_INT32("MultiSpaces, before arg0, command length", info.m_commandLength, 7, 7);
      TEST_CASE_VALUE_INT32("MultiSpaces, before arg0, command total length", info.m_totalLength, 13, 13);
      TEST_CASE_VALUE_INT32("MultiSpaces, before arg0, command arg count", info.m_argCount, 1, 1);
      TEST_CASE_VALUE_BOOL( "MultiSpaces, before arg0, arg0 value", info.m_args[0] == "argA", true);
    }


    //extra space after arg0
    {
      const char *command = "somecmd  argA ";
      CommandInfo info;
      GetCommandInfo(command, info);

      TEST_CASE_VALUE_INT32("MultiSpaces, before arg0, command length", info.m_commandLength, 7, 7);
      TEST_CASE_VALUE_INT32("MultiSpaces, before arg0, command total length", info.m_totalLength, 14, 14);
      TEST_CASE_VALUE_INT32("MultiSpaces, before arg0, command arg count", info.m_argCount, 1, 1);
      TEST_CASE_VALUE_BOOL( "MultiSpaces, before arg0, arg0 value", info.m_args[0] == "argA", true);
    }

    //extra space between all args
    {
      const char *command = "somecmd  argA  argB  argC  argD";
      CommandInfo info;
      GetCommandInfo(command, info);

      TEST_CASE_VALUE_INT32("MultiSpaces, between all, command length", info.m_commandLength, 7, 7);
      TEST_CASE_VALUE_INT32("MultiSpaces, between all, command total length", info.m_totalLength, 31, 31);
      TEST_CASE_VALUE_INT32("MultiSpaces, between all, command arg count", info.m_argCount, 4, 4);
      TEST_CASE_VALUE_BOOL( "MultiSpaces, between all, arg0 value", info.m_args[0] == "argA", true);
      TEST_CASE_VALUE_BOOL( "MultiSpaces, between all, arg1 value", info.m_args[1] == "argB", true);
      TEST_CASE_VALUE_BOOL( "MultiSpaces, between all, arg2 value", info.m_args[2] == "argC", true);
      TEST_CASE_VALUE_BOOL( "MultiSpaces, between all, arg3 value", info.m_args[3] == "argD", true);
    }


    //extra space between all args and after last arg
    {
      const char *command = "somecmd  argA  argB  argC  argD  ";
      CommandInfo info;
      GetCommandInfo(command, info);

      TEST_CASE_VALUE_INT32("MultiSpaces, between all and after, command length", info.m_commandLength, 7, 7);
      TEST_CASE_VALUE_INT32("MultiSpaces, between all and after, command total length", info.m_totalLength, 33, 33);
      TEST_CASE_VALUE_INT32("MultiSpaces, between all and after, command arg count", info.m_argCount, 4, 4);
      TEST_CASE_VALUE_BOOL( "MultiSpaces, between all and after, arg0 value", info.m_args[0] == "argA", true);
      TEST_CASE_VALUE_BOOL( "MultiSpaces, between all and after, arg1 value", info.m_args[1] == "argB", true);
      TEST_CASE_VALUE_BOOL( "MultiSpaces, between all and after, arg2 value", info.m_args[2] == "argC", true);
      TEST_CASE_VALUE_BOOL( "MultiSpaces, between all and after, arg3 value", info.m_args[3] == "argD", true);
    }
  }

  BEGIN_TEST(CommandIterator)
  {
    CommandIterator iterator;

    uint16_t commandCount = 0;

    while(!iterator.End())
    {
      commandCount++;
      const uint16_t id = iterator.Get();
      const GJString desc = DescribeCommand(id);
      Delay(15);
      SER("%s\n\r", desc.c_str());
      iterator.Next();
    }

    TEST_CASE_VALUE_INT32("CommandIterator, command count", commandCount, 9, 9);
  }
}