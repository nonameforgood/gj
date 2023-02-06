#include "config.h"
#include "esputils.h"
#include "commands.h"
#include "eventmanager.h"
#include "utils.h"

#if !defined(GJ_CONF_USE_SECTION)

typedef const ConfigDef ** ConfigDefIt;

static const ConfigDef* FromConfigDefIt(ConfigDefIt it)
{
  return *it;
}

uint32_t constexpr MaxBools = 64;
uint32_t constexpr MaxInt32s = 64;

const ConfigDef *s_configBools[MaxBools] = {nullptr};
const ConfigDef *s_configInt32s[MaxInt32s] = {nullptr};

bool RegisterConfBool(const ConfigDef *def)
{
  for (int32_t i = 0 ; i < MaxBools ; ++i)
  {
    if (s_configBools[i] == nullptr)
    {
      s_configBools[i] = def;
      return true;
    }
  }

  return false;
}
bool RegisterConfInt32(const ConfigDef *def)
{
for (int32_t i = 0 ; i < MaxInt32s ; ++i)
  {
    if (s_configInt32s[i] == nullptr)
    {
      s_configInt32s[i] = def;
      return true;
    }
  }
  return false;
}
#else
typedef const ConfigDef * ConfigDefIt;

static const ConfigDef* FromConfigDefIt(ConfigDefIt it)
{
  return it;
}

#endif

#if defined(NRF)
  #define GJ_CONFIG_CUMUL_FILE
#endif

#if defined(GJ_CONFIG_CUMUL_FILE)
  #include "appendonlyfile.h"
#else
  #include "file.h"
#endif

#if 0
  #define DEBUG_CONFIG_SER(s, ...) SER(s, ##__VA_ARGS__ )
#else
  #define DEBUG_CONFIG_SER(...)
#endif

GJString g_configFilepath = "/config";

typedef uint32_t DirtyType; //use uint64_t if 32 is not enough
static constexpr uint16_t MaxDirtyCount = sizeof(DirtyType) * 8;
static DirtyType s_dirtyBoolConfigs = 0; 
static DirtyType s_dirtyInt32Configs = 0;

class ConfigVar
{
  public:

    GJString m_name;
    GJString m_value;

    bool IsValueTrue() const;
    int32_t GetIntValue() const;
};

bool IsValueTrue(GJString const &str)
{
  bool isOn = str == "yes" ||
              str == "1" ||
              str == "true" ||
              str == "on";

  return isOn;
}

bool ConfigVar::IsValueTrue() const
{
  return ::IsValueTrue(m_value);
}

template <typename T>
ConfigDefIt BeginDefs();

template <typename T>
ConfigDefIt EndDefs();


#if defined(GJ_CONF_USE_SECTION)
template <>
ConfigDefIt BeginDefs<bool>()
{
  extern const ConfigDef __gj_configs_bool_start__;
  return &__gj_configs_bool_start__;
}


template <>
ConfigDefIt EndDefs<bool>()
{
  extern const ConfigDef __gj_configs_bool_end__;
  return &__gj_configs_bool_end__;
}



template <>
ConfigDefIt BeginDefs<int32_t>()
{
  extern const ConfigDef __gj_configs_int32_start__;
  return &__gj_configs_int32_start__;
}


template <>
ConfigDefIt EndDefs<int32_t>()
{
  extern const ConfigDef __gj_configs_int32_end__;
  return &__gj_configs_int32_end__;
}

#else //GJ_CONF_USE_SECTION

template <>
ConfigDefIt BeginDefs<bool>()
{
  return &s_configBools[0];
}


template <>
ConfigDefIt EndDefs<bool>()
{
  for (int32_t i = 0 ; i < MaxBools ; ++i)
  {
    if (s_configBools[i] == nullptr)
      return &s_configBools[i];
  }

  return &s_configBools[MaxBools];
}

template <>
ConfigDefIt BeginDefs<int32_t>()
{
  return &s_configInt32s[0];
}


template <>
ConfigDefIt EndDefs<int32_t>()
{
  for (int32_t i = 0 ; i < MaxInt32s ; ++i)
  {
    if (s_configInt32s[i] == nullptr)
      return &s_configInt32s[i];
  }

  return &s_configInt32s[MaxInt32s];
}

#endif

static bool SetConfigVal(ConfigDefIt defItBegin, ConfigDefIt defItEnd, uint32_t crc, int32_t value, DirtyType *dirtyStorage, bool serial)
{
  ConfigDefIt defIt = defItBegin;
  DirtyType dirtyBit = 1;
  for (; defIt < defItEnd;++defIt)
  {
    const ConfigDef *def = FromConfigDefIt(defIt);

    uint32_t confCrc = ComputeCrc(def->m_display, strlen(def->m_display));
    
    if (crc == confCrc)
    {
      if (dirtyStorage)
      {
        *dirtyStorage |= (def->m_value->m_value != value) ? dirtyBit : 0;
      }
      
      def->m_value->m_value = value;
      
      if (serial)
      {
        SER("Conf '%s' set to %d\n\r", def->m_display, (int32_t)value);
      }

      return true;
    }

    dirtyBit = dirtyBit << 1;
  }

  return false;
}

bool SetConfig(uint32_t crc, uint32_t value, bool updateDirty, bool serial)
{
  ConfigDefIt boolIt = BeginDefs<bool>();
  ConfigDefIt boolItEnd = EndDefs<bool>();
  ConfigDefIt int32It = BeginDefs<int32_t>();
  ConfigDefIt int32ItEnd = EndDefs<int32_t>();

  if (SetConfigVal(boolIt, boolItEnd, crc, value, updateDirty ? &s_dirtyBoolConfigs : nullptr, serial))
    return true;
  if (SetConfigVal(int32It, int32ItEnd, crc, value, updateDirty ? &s_dirtyInt32Configs : nullptr, serial))
    return true;

  return false;
}


#if defined(GJ_CONFIG_CUMUL_FILE)

static_assert(sizeof(ConfigDef) == 12);
static_assert(sizeof(ConfigValue) == 4);

uint32_t GetDirtyCount(DirtyType storage)
{
  uint32_t count = 0;
  constexpr DirtyType MaxBit = (1 << (MaxDirtyCount - 1));
  for (DirtyType i = 1 ; i < MaxBit ; i = i << 1)
  {
    count += (storage & i) != 0;
  }

   return count;
}

void WriteValues(AppendOnlyFile &file, DirtyType dirtyStorage, ConfigDefIt it,  ConfigDefIt itEnd)
{
  DirtyType dirtyBit = 1;
  for (;it < itEnd ; ++it)
  {
    if (dirtyStorage & dirtyBit)
    {
      const ConfigDef *def = FromConfigDefIt(it);

      uint32_t crc = ComputeCrc(def->m_display, strlen(def->m_display));
      uint32_t value = def->m_value->m_value;

      file.Write(&crc, 4);
      file.Write(&value, 4);
    }

    dirtyBit = dirtyBit << 1;
  }
}

static bool WriteConfigFile()
{
  ConfigDefIt boolIt = BeginDefs<bool>();
  ConfigDefIt boolItEnd = EndDefs<bool>();
  ConfigDefIt int32It = BeginDefs<int32_t>();
  ConfigDefIt int32ItEnd = EndDefs<int32_t>();

  uint32_t dirtyCount = 0;
  bool writeAll = false;

  dirtyCount = GetDirtyCount(s_dirtyBoolConfigs);
  dirtyCount += GetDirtyCount(s_dirtyInt32Configs);

/*
  for (;boolIt < boolItEnd ; ++boolIt)
  {
    dirtyCount += boolIt->m_value->m_dirty ? 1 : 0;
  }

  for (;int32It < int32ItEnd ; ++int32It)
  {
    dirtyCount += int32It->m_value->m_dirty ? 1 : 0;
  }*/

  if (dirtyCount == 0)
  {
    return false;
  }

  //boolIt = BeginDefs<bool>();
  //int32It = BeginDefs<int32_t>();

  AppendOnlyFile file(g_configFilepath.c_str());

  if (!file.BeginWrite(dirtyCount * 8))
  {
    file.Erase();

    dirtyCount = (boolItEnd - boolIt) + (int32ItEnd - int32It);

    s_dirtyBoolConfigs = ~0;
    s_dirtyInt32Configs = ~0;

    bool begin = file.BeginWrite(dirtyCount * 8);
    APP_ERROR_CHECK_BOOL(begin);
  }

  WriteValues(file, s_dirtyBoolConfigs, boolIt, boolItEnd);
  WriteValues(file, s_dirtyInt32Configs, int32It, int32ItEnd);

  file.EndWrite();
  file.Flush();

  s_dirtyBoolConfigs = 0;
  s_dirtyInt32Configs = 0;

  return true;
}

void ReadConfigData(uint32_t size, const void *data)
{
  const uint32_t *dataIt = (uint32_t*)data;
  const uint32_t *dataItEnd = dataIt + (size / 4); 

  for (; dataIt < dataItEnd ; dataIt += 2)
  {
    uint32_t crc = dataIt[0];
    uint32_t value = dataIt[1];

    const bool updateDirty = false;
    const bool serial = false;
    SetConfig(crc, value, updateDirty, serial);
  }
}

void ReadConfigFile()
{
  AppendOnlyFile file(g_configFilepath.c_str());

  auto onBlock = [&](uint32_t size, const void *data)
  {
    ReadConfigData(size, data);
  };

  file.ForEach(onBlock);
}

#else
bool WriteConfigFile()
{
  const char *filename = g_configFilepath.c_str();

 if (GJFile::Exists(filename))
  {
    GJFile::Delete(filename);
  }

  GJFile file(filename, GJFile::Mode::Write);

  ConfigDefIt boolIt = BeginDefs<bool>();
  ConfigDefIt boolItEnd = EndDefs<bool>();

  for (;boolIt < boolItEnd ; ++boolIt)
  {
    const ConfigDef *def = FromConfigDefIt(boolIt);

    uint32_t crc = ComputeCrc(def->m_display, strlen(def->m_display));
    uint32_t value = def->m_value->m_value;

    file.Write(&crc, 4);
    file.Write(&value, 4);
  }

  ConfigDefIt int32It = BeginDefs<int32_t>();
  ConfigDefIt int32ItEnd = EndDefs<int32_t>();

  for (;int32It < int32ItEnd ; ++int32It)
  {
    const ConfigDef *def = FromConfigDefIt(int32It);

    uint32_t crc = ComputeCrc(def->m_display, strlen(def->m_display));
    uint32_t value = def->m_value->m_value;

    file.Write(&crc, 4);
    file.Write(&value, 4);
  }

  file.Close();

  s_dirtyBoolConfigs = 0;
  s_dirtyInt32Configs = 0;

  SER("%s written\n\r", filename);

  return true;
}

void ReadConfigFile(const char *filename, Vector<ConfigVar> &vars)
{
  if (!GJFile::Exists(filename))
    return;

  Vector<char> fileContents;

  {
    GJFile file(filename, GJFile::Mode::Read);
    fileContents.resize(file.Size());
    file.Read(fileContents.data(), fileContents.size());
  }

  char *moduleDef = fileContents.data();

  Vector<GJString> tokens = Tokenize(moduleDef, ';');

  for (GJString &token : tokens)
  {
    DEBUG_CONFIG_SER("token:'%s'\n\r", token.c_str());

    EraseChar(token, '\n');
    EraseChar(token, '\r');

    Vector<GJString> varTokens = Tokenize(token.c_str(), '=');

    if (varTokens.size() <= 1)
      continue;

    ConfigVar var;
    var.m_name = varTokens[0];
    var.m_value = varTokens[1];

    for (int i = 2 ; i < varTokens.size() ; ++i)
    {
      var.m_value += "=";
      var.m_value += varTokens[i];
    }

    DEBUG_CONFIG_SER("Read:'%s' '%s'\n\r", var.m_name.c_str(), var.m_value.c_str());

    vars.push_back(var);
  }
}

void ReadConfigFile()
{
  Vector<ConfigVar> vars;

  ReadConfigFile(g_configFilepath.c_str(), vars);
}

#endif



#define GJ_CONF_BOOL 0x4000
#define GJ_CONF_INT32 0x8000


static const char* GetConfName(uint16_t id)
{
  const char * name = nullptr;

  if (id & GJ_CONF_BOOL)
    name = FromConfigDefIt(BeginDefs<bool>() + (id & 0xfff))->m_display;
  else
    name = FromConfigDefIt(BeginDefs<int32_t>() + (id & 0xfff))->m_display;

  return name;
};


static bool CompareConfig(const uint16_t left, const uint16_t right)
{
  const char * lefID = GetConfName(left);
  const char * rightID = GetConfName(right);

  return strcmp(lefID, rightID) < 0;
};

uint32_t currentPrintConfig = 0;

void FillIndices(Vector<uint16_t> &indices, ConfigDefIt it, ConfigDefIt itEnd, uint16_t index, uint32_t &maxLength)
{
  for (;it < itEnd ; ++it)
  {
    const char * name = GetConfName(index);
    maxLength = Max<uint32_t>(maxLength, strlen(name));
    indices.push_back(index++);
  }
}

bool PrintNextConfig()
{
  ConfigDefIt boolIt = BeginDefs<bool>();
  ConfigDefIt boolItEnd = EndDefs<bool>();
  ConfigDefIt int32It = BeginDefs<int32_t>();
  ConfigDefIt int32ItEnd = EndDefs<int32_t>();

  Vector<uint16_t> indices;
  indices.reserve((boolItEnd - boolIt) + (int32ItEnd - int32It));

  uint32_t maxLength = 0;

  FillIndices(indices, boolIt, boolItEnd, GJ_CONF_BOOL, maxLength);
  FillIndices(indices, int32It, int32ItEnd, GJ_CONF_INT32, maxLength);

  SortU16(&*indices.begin(), &*indices.end(), CompareConfig);
  
  if (currentPrintConfig == 0)
  {
    SER("Available configs:\n\r");
  }

  //for (uint16_t i : indices)
  if (currentPrintConfig < indices.size())
  {
    uint32_t i = currentPrintConfig;

    i = indices[i];

    const char * name = GetConfName(i);
    
    {
      uint16_t id2 = i & 0xfff;

      if (i & (GJ_CONF_BOOL))
      {
        volatile const ConfigDef *def = FromConfigDefIt(&boolIt[id2]);

        char format[16];
        sprintf(format, "  %%-%ds:%%s\n\r", maxLength);


        SER(format, name, def->m_value->m_value ? "true" : "false");
      }
      else 
      {
        volatile const ConfigDef *def = FromConfigDefIt(&int32It[id2]);

        char format[16];
        sprintf(format, "  %%-%ds:%%d\n\r", maxLength);


        SER(format, name, def->m_value->m_value);
      }
    }
  }

  currentPrintConfig++;

  return currentPrintConfig >= indices.size();
}

//print one config per frame
void PrintConfigHandler()
{
  if (!AreTerminalsReady())
  {
    //some terminal cannot send data at this time, postpone to later
    GJEventManager->Add(PrintConfigHandler);
    return;
  }

  if (PrintNextConfig())
  {
    currentPrintConfig = 0;//end
  }
  else
  {
    GJEventManager->Add(PrintConfigHandler);
  }
}

void PrintConfig()
{
  while(!PrintNextConfig())
  {

  }

  currentPrintConfig = 0;//end
}

static void Command_ReadConfig() 
{
  ReadConfigFile();
  SER("Config file '%s' read\n\r", g_configFilepath.c_str());
}

void Command_WriteConfig() 
{
  if (WriteConfigFile())
  {
    SER("Config file '%s' written\n\r", g_configFilepath.c_str());
  }
  else
  {
    SER("No dirty conf\n\r");
  }
}

void Command_Config(const char*command) 
{
  CommandInfo info;
  GetCommandInfo(command, info);
  if (info.m_argCount == 0)
  {
    PrintConfigHandler();
    return;
  }
  else if (info.m_argCount < 2)
  {
    SER("usage:conf <varname> <value>\n\r");
    return;
  }

  const bool updateDirty = true;
  const bool serial = true;
  const uint32_t crc = ComputeCrc(info.m_argsBegin[0], info.m_argsLength[0]);
  if (SetConfig(crc, atoi(info.m_argsBegin[1]), updateDirty, serial))
    return;

  SER("ERROR:conf not found\n\r");
} 

DEFINE_COMMAND_NO_ARGS(confread, Command_ReadConfig);
DEFINE_COMMAND_NO_ARGS(confwrite,Command_WriteConfig);
DEFINE_COMMAND_ARGS(conf,Command_Config);

void InitConfig(const char *filename)
{
  //if (filename)
#if !defined(GJ_CONFIG_CUMUL_FILE)
  g_configFilepath = GJString(FormatString("/%sconfig", GetAppFolder()));
#endif

  if (!IsSleepWakeUp())
  {
    ReadConfigFile();
  }

  REFERENCE_COMMAND(confread);
  REFERENCE_COMMAND(confwrite);
  REFERENCE_COMMAND(conf); 
}

void OnConfigSleep()
{
#if defined(ESP32)
  bool dirty = s_dirtyBoolConfigs || s_dirtyInt32Configs;

  if (dirty)
  {
    //WriteConfigFile();
  }
#endif
}