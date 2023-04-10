#pragma once
#include "vector.h"
#include "string.h"
#include "stringview.h"

void InterpretCommand(const char *command);

#define HANDLE_MULTI_EXE_CMD(count)                                                         \
  {                                                                                         \
    static uint32_t activeSteps = 0;                                                        \
    uint32_t const stepCount = (count);                                                     \
    activeSteps++;                                                                          \
    if (activeSteps < stepCount)                                                            \
    {                                                                                       \
      SER("Command must execute again %d times to activate\n\r", stepCount - activeSteps);  \
      return;                                                                               \
    }                                                                                       \
    activeSteps = 0;                                                                        \
  }

struct CommandInfo
{
  static const uint32_t MaxArgs = 4;
  uint32_t m_commandLength = 0;
  uint32_t m_totalLength = 0;
  const char *m_argsBegin[MaxArgs] = {};
  uint16_t m_argsOffset[MaxArgs] = {};
  uint16_t m_argsLength[MaxArgs] = {};
  uint32_t m_argCount = 0;

  StringView m_args[MaxArgs] = {};
};

template <typename T>
struct CommandDefInfo
{
  const char * const m_command;
  const T *m_cb;
};

#if defined(ESP32)
  #define GJ_COMMAND_USE_SECTION 0
#elif defined(NRF)
  #define GJ_COMMAND_USE_SECTION 1
#endif

#if GJ_COMMAND_USE_SECTION
  #define GJ_COMMAND_SECTION_NOARGS ".gj_commands_noargs"
  #define GJ_COMMAND_SECTION_ARGS ".gj_commands_args"
  #define GJ_COMMAND_SECTION_NOARGS_ATTR __attribute__ ((section(GJ_COMMAND_SECTION_NOARGS)))
  #define GJ_COMMAND_SECTION_ARGS_ATTR __attribute__ ((section(GJ_COMMAND_SECTION_ARGS)))
  #define GJ_REGISTER_COMMAND_NOARGS(name)
  #define GJ_REGISTER_COMMAND_ARGS(name)
  #define GJ_COMMAND_VOLATILE volatile
#else
  #define GJ_COMMAND_SECTION_NOARGS_ATTR
  #define GJ_COMMAND_SECTION_ARGS_ATTR
  #define GJ_REGISTER_COMMAND_NOARGS(name) const bool cmdfef_init_##name = RegisterConfNoArgs(&cmddef_##name)
  #define GJ_REGISTER_COMMAND_ARGS(name) const bool cmddef_init_##name = RegisterConfArgs(&cmddef_##name)
  #define GJ_COMMAND_VOLATILE

  bool RegisterConfNoArgs(const CommandDefInfo<void()> *def);
  bool RegisterConfArgs(const CommandDefInfo<void(const char *)> *def);
#endif


#define DEFINE_COMMAND_NO_ARGS(name, cb) static const CommandDefInfo<void()>  GJ_COMMAND_SECTION_NOARGS_ATTR cmddef_##name = {#name, &cb}; GJ_REGISTER_COMMAND_NOARGS(name)
#define DEFINE_COMMAND_ARGS(name, cb) static const CommandDefInfo<void(const char *)>  GJ_COMMAND_SECTION_ARGS_ATTR cmddef_##name = {#name, &cb}; GJ_REGISTER_COMMAND_ARGS(name)
  
bool ForceLinkSymbol(const void *p);
#define REFERENCE_COMMAND(name) ForceLinkSymbol((void*)&cmddef_##name)

#define GJ_NO_ARGS_BIT 0x1000
#define GJ_ARGS_BIT 0x2000

void GetCommandInfo(const char *command, CommandInfo &info);

void GetCommands(Vector<GJString> &commands);

void GetCommandIds(Vector<uint16_t> &commands);
void GetCommandCount(uint16_t &argCount, uint16_t &noArgCount);
GJString DescribeCommand(uint16_t id);

struct SubCommands
{
  uint32_t m_count;
  const char * const *m_names;
  void (* const * m_funcs)(const CommandInfo &commandInfo);
};

void SubCommandForwarder(const char *command, const SubCommands &subCommands);

class CommandIterator
{
public:
  CommandIterator();
  uint16_t Get() const;
  bool End() const;
  void Next();

private:

  uint16_t m_index = 0;
  uint16_t m_argCount = 0;
  uint16_t m_noArgCount = 0;
};

void InitCommands(uint32_t maxArgsCommands);