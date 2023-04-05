#include "base.h"
#include "esputils.h"
#include "metricsmanager.h"
#include "commands.h"
#include "string.h"
#include <algorithm>
#include "utils.h"

#if GJ_COMMAND_USE_SECTION == 0
uint32_t constexpr MaxCommandsNoArgs = 64;
uint32_t constexpr MaxCommandsArgs = 64;

const CommandDefInfo<void()> *s_commandsNoArgs[MaxCommandsNoArgs] = {nullptr};
const CommandDefInfo<void(const char *)> *s_commandsArgs[MaxCommandsArgs] = {nullptr};

bool RegisterConfNoArgs(const CommandDefInfo<void()> *def)
{
  for (int32_t i = 0 ; i < MaxCommandsNoArgs ; ++i)
  {
    if (s_commandsNoArgs[i] == nullptr)
    {
      s_commandsNoArgs[i] = def;
      return true;
    }
  }

  return false;
}
bool RegisterConfArgs(const CommandDefInfo<void(const char *)> *def)
{
for (int32_t i = 0 ; i < MaxCommandsArgs ; ++i)
  {
    if (s_commandsArgs[i] == nullptr)
    {
      s_commandsArgs[i] = def;
      return true;
    }
  }
  return false;
}

typedef const CommandDefInfo<void()>** CommandDefNoArgsIt;
typedef const CommandDefInfo<void(const char*)>** CommandDefArgsIt;

const CommandDefInfo<void()>* FromCommandDefInfoNoArgsIt(CommandDefNoArgsIt it)
{
  return *it;
}

const CommandDefInfo<void(const char*)>* FromCommandDefInfoArgsIt(CommandDefArgsIt it)
{
  return *it;
}

static CommandDefNoArgsIt BeginNoArgsCmds()
{
  return &s_commandsNoArgs[0];
}

static CommandDefNoArgsIt EndNoArgsCmds()
{
  for (int32_t i = 0 ; i < MaxCommandsNoArgs ; ++i)
  {
    if (s_commandsNoArgs[i] == nullptr)
      return &s_commandsNoArgs[i];
  }

  return &s_commandsNoArgs[MaxCommandsNoArgs];
}


static CommandDefArgsIt BeginArgsCmds()
{
  return &s_commandsArgs[0];
}

static CommandDefArgsIt EndArgsCmds()
{
  for (int32_t i = 0 ; i < MaxCommandsArgs ; ++i)
  {
    if (s_commandsArgs[i] == nullptr)
      return &s_commandsArgs[i];
  }

  return &s_commandsArgs[MaxCommandsArgs];
}

#else

typedef const CommandDefInfo<void()>* CommandDefNoArgsIt;
typedef const CommandDefInfo<void(const char*)>* CommandDefArgsIt;


const CommandDefInfo<void()>* FromCommandDefInfoNoArgsIt(CommandDefNoArgsIt it)
{
  return it;
}

const CommandDefInfo<void(const char*)>* FromCommandDefInfoArgsIt(CommandDefArgsIt it)
{
  return it;
}

static const CommandDefInfo<void()>* BeginNoArgsCmds()
{
  extern const CommandDefInfo<void()> __gj_commands_noargs_start__;
  return &__gj_commands_noargs_start__;
}

static const CommandDefInfo<void()>* EndNoArgsCmds()
{
  extern const CommandDefInfo<void()> __gj_commands_noargs_end__;
  return &__gj_commands_noargs_end__;
}


static const CommandDefInfo<void(const char*)>* BeginArgsCmds()
{
  extern const CommandDefInfo<void(const char*)> __gj_commands_args_start__;
  return &__gj_commands_args_start__;
}

static const CommandDefInfo<void(const char*)>* EndArgsCmds()
{
  extern const CommandDefInfo<void(const char*)> __gj_commands_args_end__;
  return &__gj_commands_args_end__;
}

#endif


#define GJ_NO_ARGS_BIT 0x1000
#define GJ_ARGS_BIT 0x2000

void InitCommands(uint32_t maxCommands)
{

}

static const char *GetCommandName(int16_t index)
{
  if (index & GJ_NO_ARGS_BIT)
  {
    return FromCommandDefInfoNoArgsIt(&BeginNoArgsCmds()[index & 0xfff])->m_command;
  }
  else 
  {
    return FromCommandDefInfoArgsIt(&BeginArgsCmds()[index & 0xfff])->m_command;
  }
};

static bool CompareCommand(uint16_t left, uint16_t right)
{
  const char *leftName = GetCommandName(left);
  const char *rightName = GetCommandName(right);
  return strcmp(leftName, rightName) < 0;
};

void GetCommandIds(Vector<uint16_t> &commands)
{
  CommandDefNoArgsIt noArgItBegin = BeginNoArgsCmds();
  CommandDefNoArgsIt noArgItEnd = EndNoArgsCmds();
  CommandDefNoArgsIt noArgIt = noArgItBegin;

  CommandDefArgsIt argItBegin = BeginArgsCmds();
  CommandDefArgsIt argItEnd = EndArgsCmds();
  CommandDefArgsIt argIt = argItBegin;
  
  uint16_t staticCount = (noArgItEnd - noArgIt) + (argItEnd - argIt);

  commands.reserve(staticCount);

  uint16_t index = GJ_NO_ARGS_BIT;

  while(noArgIt < noArgItEnd)
  {
    commands.push_back(index);

    index++;
    noArgIt++;
  }

  index = GJ_ARGS_BIT;

  while(argIt < argItEnd)
  {
    commands.push_back(index);
    index++;
    argIt++;
  }

  SortU16(&*commands.begin(), &*commands.end(), CompareCommand);

  //std::sort(commands.begin(), commands.end(), compare);
}

GJString DescribeCommand(uint16_t id)
{
  GJString str;
  if (id & GJ_NO_ARGS_BIT)
  {
    str = GetCommandName(id);
  }
  else
  {
    str = GetCommandName(id);
    str += " <args>";
  }

  return str;
}

void GetCommands(Vector<GJString> &commands)
{
  Vector<uint16_t> commandsIndices;
  GetCommandIds(commandsIndices);

  commands.reserve(commandsIndices.size());
  for (int i = 0 ; i < commandsIndices.size() ; ++i)
  {
    uint16_t id = commandsIndices[i];

    GJString str = DescribeCommand(id);

    commands.push_back(str);
  }
}

void GetCommandInfo(const char *command, CommandInfo &info)
{
  const char *it = command;
  const char *itEnd = command + strlen(command);

  info.m_argCount = 0;
  
  while(it < itEnd)
  {
    const char *dividerPos = strstr(it, " ");
    if (!dividerPos)
      break;

    if (!info.m_commandLength)
    {
      info.m_commandLength = dividerPos - command;
    }

    if (info.m_argCount > 0 && info.m_argsLength[info.m_argCount - 1] == 0)
    {
      info.m_argsLength[info.m_argCount - 1] = dividerPos - info.m_argsBegin[info.m_argCount - 1];
    }

    do
    {
      dividerPos++;
    }
    while(dividerPos[0] == ' ');

    if (info.m_argCount >= CommandInfo::MaxArgs)
    {
      SER("ERR:args overflow\n\r");
      break;
    }

    info.m_argsBegin[info.m_argCount] = dividerPos;
    info.m_argsOffset[info.m_argCount] = dividerPos - command;
    
    if (dividerPos[0] == '"')
    {
      info.m_argsBegin[info.m_argCount]++;
      info.m_argsOffset[info.m_argCount]++;
      
      const char *endDividerPos = strstr(dividerPos + 1, "\"");
      if (endDividerPos)
      {
        info.m_argsLength[info.m_argCount] = endDividerPos - dividerPos - 1;
        dividerPos = endDividerPos;
      }
    }

    if (dividerPos[0] != 0)
      info.m_argCount++;

    it = dividerPos + 1;
  }
  
  info.m_totalLength = strlen(command);
  
  if (info.m_argCount)
  {
    if (!info.m_argsLength[info.m_argCount-1])
      info.m_argsLength[info.m_argCount-1] = command + info.m_totalLength - info.m_argsBegin[info.m_argCount - 1];

    //SER("CommandInfo\n\r");
    //remove double quotes
    for (int i = 0 ; i < info.m_argCount ; ++i)
    {
      char const *argBegin = info.m_argsBegin[i];
      uint32_t length = info.m_argsLength[i];
      
      if (argBegin[0] == '"')
      {
        argBegin++;
        length--;
      }

      if (length && argBegin[length - 1] == '"')
      {
        length--;
      }

      info.m_argsBegin[i] = argBegin;
      info.m_argsOffset[i] = argBegin - command;
      info.m_argsLength[i] = length;

      info.m_args[i] = StringView(info.m_argsBegin[i], info.m_argsLength[i]);

      //SER("  %d:offset=%d length=%d arg:%s\n\r", i, info.m_argsOffset[i], info.m_argsLength[i], info.m_argsBegin[i] );
    }
  }
  else if (info.m_commandLength == 0)
  {
    info.m_commandLength = info.m_totalLength;
  }
}

void SubCommandForwarder(const char *command, const SubCommands &subCommands)
{
  CommandInfo commandInfo;
  GetCommandInfo(command, commandInfo);

  if (commandInfo.m_argCount < 1)
  {
    SER("Available commands:\n\r");
    for (int32_t i = 0 ; i < subCommands.m_count ; ++i)
    {
      SER("  %s\n\r", subCommands.m_names[i]);
    }
    return;
  }

  const char *commandPos = commandInfo.m_argsBegin[0];
  auto arg0 = commandInfo.m_args[0];

  for (int32_t i = 0 ; i < subCommands.m_count ; ++i)
  {
    if (arg0 == subCommands.m_names[i])
    {
      CommandInfo subCommandinfo;

      subCommandinfo.m_commandLength = 0;
      subCommandinfo.m_totalLength = commandInfo.m_totalLength - commandInfo.m_argsOffset[0] - arg0.size();
      subCommandinfo.m_argCount = commandInfo.m_argCount - 1;

      for (int32_t j = 0 ; j < 3 ; ++j)
      {
        subCommandinfo.m_argsBegin[j] = commandInfo.m_argsBegin[j + 1];
        subCommandinfo.m_argsOffset[j] = commandInfo.m_argsOffset[j + 1];
        subCommandinfo.m_argsLength[j] = commandInfo.m_argsLength[j + 1];
        subCommandinfo.m_args[j] = commandInfo.m_args[j + 1];
      }

      subCommands.m_funcs[i](subCommandinfo);
      return;
    }
  }

  SER("Invalid command '%.*s'\n\r", commandInfo.m_argsLength[0], commandInfo.m_argsBegin[0]);
}

void InterpretCommand(const char *commandString)
{
  GJ_PROFILE(InterpretCommand);
  
  //SER("InterpretCommand '%s'\n\r", commandString);

  //if (!g_commands)
  //  return;
    
  auto isCommand = [&commandString](const char *command)
  {
    const uint32_t len = strlen(command);
    if (strncmp(commandString, command, len))
      return false;

    return commandString[len] == ' ' || commandString[len] == 0;
  };
  
  {
    CommandInfo info;
    
    GetCommandInfo(commandString, info);
    
    bool found = false;

    CommandDefNoArgsIt noArgIt = BeginNoArgsCmds();
    CommandDefNoArgsIt noArgItEnd = EndNoArgsCmds();

    while(noArgIt < noArgItEnd)
    {
      const CommandDefInfo<void()> *def = FromCommandDefInfoNoArgsIt(noArgIt);

      if (isCommand(def->m_command))
      {
        found = true;
        def->m_cb();
      }

      noArgIt++;
    }

    CommandDefArgsIt argIt = BeginArgsCmds();
    CommandDefArgsIt argItEnd = EndArgsCmds();

    while(argIt < argItEnd)
    {
      const CommandDefInfo<void(const char *)> *def = FromCommandDefInfoArgsIt(argIt);

      if (isCommand(def->m_command))
      {
        found = true;
        def->m_cb(commandString);
      }

      argIt++;
    }

    if (found)
    {
      ResetSpawnTime();//keep esp awake
    }
    else
    {
      //don't log anything as serial input might be garbage
    }
  }
}
