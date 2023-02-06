#include "base.h"
#include "profiling.h"
#include "commands.h"
#include <algorithm>

static bool s_continousProfile = false;
static uint32_t s_lastProfile = 0;



void BeginProfile()
{
  GJProfileBase::Enable(true);
  s_lastProfile = GetElapsedMillis();
}
void Command_profile() {
  BeginProfile();
  SER("Profiling started\n\r");
}
void Command_beginprofile() {
  s_continousProfile = true;
  BeginProfile();
  SER("Profiling started\n\r");
} 
void Command_endprofile() {
  s_continousProfile = false;
  GJProfileBase::Enable(false);
  SER("Profiling stopped\n\r");
}

DEFINE_COMMAND_NO_ARGS(profile, Command_profile);
DEFINE_COMMAND_NO_ARGS(beginprofile, Command_beginprofile);
DEFINE_COMMAND_NO_ARGS(endprofile, Command_endprofile);

void InitProfiling()
{
  REFERENCE_COMMAND(profile);
  REFERENCE_COMMAND(beginprofile);
  REFERENCE_COMMAND(endprofile);
}

void UpdateProfile()
{
  if (s_continousProfile)
  {
    if ((GetElapsedMillis() - s_lastProfile) >= 2000)
    {
      GJProfileBase::Enable(false);
      GJProfileBase::Enable(true);
      s_lastProfile = GetElapsedMillis();
    }
  }
  else
  {
    GJProfileBase::Enable(false);
  }
}

bool GJProfileBase::ms_enable = false;
GJProfileBase::Instance* GJProfileBase::Instance::ms_first = nullptr;

void GJProfileBase::Cumul::Add(int64_t begin)
{
  int64_t const end = GetProfileTime();
  int64_t const duration = end - begin;
      
  m_total += duration;
  m_count += 1;
  m_max = std::max(m_max, duration);
}

GJProfileBase::Instance::Instance()
{
  Instance **it = &ms_first;

  while(*it)
  {
    it = &(*it)->m_next;
  }
  
  *it = this;
}

bool GJProfileBase::Enable(bool enable)
{
  bool const wasEnabled = ms_enable;
  ms_enable = enable;
  
  if (wasEnabled && !enable)
  {
    Report();
  }

  return true;
}

bool GJProfileBase::Report()
{
  SER("Profile (times in microseconds)\n\r");
  SER("                                                   Calls    Cumul     Mean      Max\n\r");
        
  Instance *instance = Instance::ms_first;
  while(instance)
  {
    Cumul &cumul = instance->m_cumul;
    
    if (instance->m_name)
    {
      SER("%-50s %8d %8d %8d %8d\n\r", 
        instance->m_name ? instance->m_name : "*N/A*", 
        (uint32_t)cumul.m_count,
        (uint32_t)cumul.m_total, 
        cumul.m_count ? ((uint32_t)cumul.m_total / (uint32_t)cumul.m_count ) : (uint32_t)0,
        (uint32_t)cumul.m_max
        );
    }
    
    cumul = {};
    
    instance = instance->m_next;
  }

  return true;
}
