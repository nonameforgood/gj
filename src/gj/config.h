#pragma once

#include "base.h"
#include "vector.h"
#include "crc.h"
#include "string.h"

void InitConfig(const char *file = nullptr);
void OnConfigSleep();

struct ConfigVarDesc
{
  bool m_onDisk = true;
};

struct ConfigValue
{
  int32_t m_value;
};

struct ConfigDef
{
  const char* m_display;  //using StringID prevents RTC_DATA_ATTR from working
  ConfigValue * m_value;
  ConfigVarDesc m_desc;
};

#if defined(ESP32)
#elif defined(NRF)
  #define GJ_CONF_USE_SECTION
#endif

#ifdef GJ_CONF_USE_SECTION
  #define GJ_CONF_SECTION_BOOL ".gj_configs_bool"
  #define GJ_CONF_SECTION_INT32 ".gj_configs_int32"
  #define GJ_CONF_SECTION_BOOL_ATTR __attribute__ ((section(GJ_CONF_SECTION_BOOL)))
  #define GJ_CONF_SECTION_INT32_ATTR __attribute__ ((section(GJ_CONF_SECTION_INT32)))
  #define GJ_REGISTER_CONF_BOOL(conf)
  #define GJ_REGISTER_CONF_INT32(conf)
  #define GJ_CONF_VOLATILE volatile
#else
  #define GJ_CONF_SECTION_BOOL_ATTR
  #define GJ_CONF_SECTION_INT32_ATTR
  #define GJ_REGISTER_CONF_BOOL(name) const bool confdef_init_##name = RegisterConfBool(&confdef_##name)
  #define GJ_REGISTER_CONF_INT32(name) const bool confdef_init_##name = RegisterConfInt32(&confdef_##name)
   #define GJ_CONF_VOLATILE

  bool RegisterConfBool(const ConfigDef *def);
  bool RegisterConfInt32(const ConfigDef *def);
#endif

#define DEFINE_CONFIG_BOOL(display, name, def) ConfigValue confValue_##name = {def}; \
                GJ_CONF_VOLATILE const ConfigDef GJ_CONF_SECTION_BOOL_ATTR confdef_##name = {#display, &confValue_##name, {}}; \
                GJ_REGISTER_CONF_BOOL(name);
 
#define DEFINE_CONFIG_INT32(display, name, def) ConfigValue confValue_##name = {def}; \
                GJ_CONF_VOLATILE const ConfigDef GJ_CONF_SECTION_INT32_ATTR confdef_##name = {#display, &confValue_##name, {}};\
                 GJ_REGISTER_CONF_INT32(name);
 
#define GJ_CONF_BOOL_VALUE(name) ((bool)confdef_##name.m_value->m_value)
#define GJ_CONF_INT32_VALUE(name) confdef_##name.m_value->m_value


void PrintConfig();