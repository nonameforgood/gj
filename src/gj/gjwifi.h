#pragma once



struct GJWifiArgs
{
  String m_hostName;
  const char* m_ssid = nullptr;
  const char* m_password = nullptr;
  uint32_t m_timeout = 10000;
  const char *m_configPath = "/gjserverconfig.txt";
};

void RequestWifiAccess();

void InitWifi(GJWifiArgs const &args);
void WaitForSTAIP();
void UpdateWifi();
void TermWifi();

bool IsWifiAPEnabled();
bool IsWifiSTAEnabled();
bool IsWifiAP();
String GetWifiIP();

bool IsWifiConnected();
bool IsOnline();
bool TestUrl(const char *url);

typedef std::function<void()> WifiStartCallback;
void RegisterWifiStartCallback(WifiStartCallback cb);

typedef std::function<void()> WifiStopCallback;
void RegisterWifiStopCallback(WifiStopCallback cb);