
#include "string.h"

struct WifiConfig
{
  String m_ssid;
  String m_pass;
};


bool ReadWifiConfig(const char *filepath, WifiConfig &config);
bool WriteWifiConfig(const char *filepath, WifiConfig const &config);