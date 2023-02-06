#include "base.h"
#include "gjwifi.h"
#include "commands.h"
#include "wificonfig.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include "datetime.h"
#include "datetime.h"
#include "http.h"
#include <DNSServer.h>

#include "esputils.h"
#include "eventlisteners.h"
#include "eventmanager.h"
#include "config.h"
#include "gjatomic.h"

#include <esp_wifi.h>
#include <esp_private/wifi.h>


#define WIFI_AP_RUNTIME (5*60)

WifiConfig g_wifiConfig;
GJWifiArgs g_wifiArgs;
bool g_wifiInit = false;
bool onlineAccessRequested = false;
uint32_t g_connectionAttempStart = 0;
DNSServer dnsServer;
EventListeners<WifiStartCallback> s_wifiStartEvent("WifiStart");
EventListeners<WifiStopCallback> s_wifiStopEvent("WifiStop");

static wifi_event_id_t s_eventId(0);
static bool s_keepAliveSet(false); 

DEFINE_CONFIG_BOOL(wifi.db, wifi_dbg, false);
DEFINE_CONFIG_INT32(wifi.reconnectPeriod, wifi_reconnectPeriod, 60);
DEFINE_CONFIG_INT32(wifi.keepalive, wifi_keepalive, 10);

void KeepAlive();
void AddKeepAlive();
void HandleSTAConnection();
const char* GetWifiEventName(arduino_event_id_t id);
void DoTurnOffWifi();
static void StartWifi(GJWifiArgs const &args);

bool GetWifiDbg()
{
  return GJ_CONF_BOOL_VALUE(wifi_dbg);
}

#define GJ_WIFI_DBG_LOG(...) LOG_COND(GetWifiDbg(), __VA_ARGS__)

void RegisterWifiStartCallback(WifiStartCallback cb)
{
  s_wifiStartEvent.AddListener(cb);
}

void RegisterWifiStopCallback(WifiStopCallback cb)
{
  s_wifiStopEvent.AddListener(cb);
}

bool wifiIsAP = false;
bool IsWifiAP()
{
  return wifiIsAP && !IsWifiConnected();
}

bool IsWifiAPEnabled()
{
  return WiFi.getMode() == WIFI_MODE_AP || WiFi.getMode() == WIFI_MODE_APSTA;
}

bool IsWifiSTAEnabled()
{
  return WiFi.getMode() == WIFI_MODE_STA || WiFi.getMode() == WIFI_MODE_APSTA;
}

String GetWifiIP()
{
  return WiFi.localIP().toString();
}


bool IsWifiConnected()
{
  bool const wifiConnected = WiFi.status() == WL_CONNECTED;
  return wifiConnected;
}

bool IsOnline()
{
  return IsWifiConnected();
}

bool SetWifiConfig(const char *ssid, const char *pass)
{
  g_wifiConfig.m_ssid = ssid;
  g_wifiConfig.m_pass = pass;
  g_wifiArgs.m_ssid = g_wifiConfig.m_ssid.c_str();
  g_wifiArgs.m_password = g_wifiConfig.m_pass.c_str();

  SER("Wifi config set to '%s' '%s'\n", ssid, pass);

  return true;
}


void SetupSTA(GJWifiArgs const &args)
{
  //WiFi.enableSTA(true);
  WiFi.setHostname(args.m_hostName.c_str());
  WiFi.begin(args.m_ssid, args.m_password);

  g_connectionAttempStart = millis();
}


void WaitForSTAIP(GJWifiArgs const &args)
{
  uint32_t const startTime = millis();
  uint32_t const timeout = args.m_timeout;
  
  LOG("Waiting for IP");
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    LOG(".");

    GJ_WIFI_DBG_LOG("WiFi Status:%d\n\r", (int)WiFi.status());

    uint32_t const elapsed = millis() - startTime;
    if ( elapsed > timeout )
    {
      break;
    }
  }
  LOG("\n\r");

  if (WiFi.status() != WL_CONNECTED)
  {
    LOG("  Failed to connect to SSID \"%s\" after %dms\n\r", args.m_ssid, timeout );
  }
  else
  {
    LOG("  STA IP address:%s\n\r", GetWifiIP().c_str());
    LOG("  STA hostname:%s\n\r", args.m_hostName.c_str());
    LOG("  done\n\r");
  }
}

void WaitForSTAIP()
{
  WaitForSTAIP(g_wifiArgs);
}

void StartWifiStation(GJWifiArgs const &args)
{
  WiFi.setHostname(args.m_hostName.c_str());
  WiFi.begin(args.m_ssid, args.m_password);

  uint32_t const startTime = millis();
  uint32_t const timeout = args.m_timeout;

  LOG("  Connecting to SSID '%s' ", args.m_ssid);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    LOG(".");

    uint32_t const elapsed = millis() - startTime;
    if ( elapsed > timeout )
    {
      break;
    }
  }
  LOG("\n\r");

  if (WiFi.status() != WL_CONNECTED)
  {
    LOG("  Failed to connect to SSID \"%s\" after %dms\n\r", args.m_ssid, timeout );
  }
  else
  {
    LOG("  STA IP address:%s\n\r", GetWifiIP().c_str());
    LOG("  STA hostname:%s\n\r", args.m_hostName.c_str());
    LOG("  done\n\r");
  }
}


void SetupAP(GJWifiArgs const &args)
{
  char hostNameBuffer[64];
  char const *hostName = args.m_hostName.c_str();
  if (!hostName || !hostName[0])
  {
    hostName = hostNameBuffer;
    uint8_t mac[6] = {};
    esp_efuse_mac_get_default(mac);

    sprintf(hostNameBuffer, "DefaultHostName-%02X%02X%02X%02X%02X%02X", 
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

  const char *ssid = hostName;
  const char *pass = nullptr;
  
  WiFi.softAP(ssid, pass);

  IPAddress Ip(10, 0, 0, 1);
  IPAddress NMask(255, 255, 255, 0);
  WiFi.softAPConfig(Ip, Ip, NMask);
  
  const byte DNS_PORT = 53;
  dnsServer.start(DNS_PORT, "*", Ip);
  
  IPAddress myIP = WiFi.softAPIP();
  LOG("  SSID:%s\n\r", hostName);
  LOG("  AP IP address:%s\n\r", myIP.toString().c_str());

  wifiIsAP = true;
}

void StartWifiAccessPoint(GJWifiArgs const &args)
{
  char hostNameBuffer[64];
  char const *hostName = args.m_hostName.c_str();
  if (!hostName || !hostName[0])
  {
    hostName = hostNameBuffer;
    uint8_t mac[6] = {};
    esp_efuse_mac_get_default(mac);

    sprintf(hostNameBuffer, "DefaultHostName-%02X%02X%02X%02X%02X%02X", 
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

  const char *ssid = hostName;
  const char *pass = nullptr;
  
  WiFi.softAP(ssid, pass);

  uint32_t beginTime = millis();

  while(true)
  {
    int status = WiFi.getStatusBits();

    SER("Status:%x\n\r", status);

    if ((status & AP_STARTED_BIT) != 0)
      break;

    delay(10);

    uint32_t endTime = millis();
    if ((endTime - beginTime) > 2000)
      break;
  }

  IPAddress Ip(10, 0, 0, 1);
  IPAddress NMask(255, 255, 255, 0);
  WiFi.softAPConfig(Ip, Ip, NMask);
  
  const byte DNS_PORT = 53;
  dnsServer.start(DNS_PORT, "*", Ip);
  
  IPAddress myIP = WiFi.softAPIP();
  LOG("  SSID:%s\n\r", hostName);
  LOG("  AP IP address:%s\n\r", myIP.toString().c_str());

  wifiIsAP = true;
}

void StartWifiAPSTA(GJWifiArgs const &args)
{
  LOG("StartWifiAPSTA\n\r");

  char hostNameBuffer[64];
  char const *hostName = args.m_hostName.c_str();
  if (!hostName || !hostName[0])
  {
    hostName = hostNameBuffer;
    uint8_t mac[6] = {};
    esp_efuse_mac_get_default(mac);

    sprintf(hostNameBuffer, "DefaultHostName-%02X%02X%02X%02X%02X%02X", 
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

  const char *ssid = hostName;
  const char *pass = nullptr;
  
  WiFi.softAP(ssid, pass);

  uint32_t beginTime = millis();

  IPAddress Ip(10, 0, 0, 1);
  IPAddress NMask(255, 255, 255, 0);
  WiFi.softAPConfig(Ip, Ip, NMask);
  
  const byte DNS_PORT = 53;
  dnsServer.start(DNS_PORT, "*", Ip);
  
  IPAddress myIP = WiFi.softAPIP();
  LOG("  SSID:%s\n\r", hostName);
  LOG("  AP IP address:%s\n\r", myIP.toString().c_str());

  wifiIsAP = true;

  StartWifiStation(args);
}

void ReconnectWifi()
{
  DoTurnOffWifi();
  StartWifi(g_wifiArgs);
}

static void StartWifi(GJWifiArgs const &args)
{
  if (IsWifiConnected())
    return;

  LOG("Start wifi\n\r");

  if (s_eventId == 0)
  {
    auto onWifiEvent = [](arduino_event_id_t event, arduino_event_info_t info)
    {
      bool addSTAConnectionEvent(false);

      GJ_WIFI_DBG_LOG("WifiDbg:event %d(%s)\n\r", event, GetWifiEventName(event)); 

      if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
      {
        uint8_t reason = info.wifi_sta_disconnected.reason;
        char ssid[33] = {0};
        uint8_t *bssid = info.wifi_sta_disconnected.bssid;
        strncpy(ssid, (char*)info.wifi_sta_disconnected.ssid, 32);

        GJ_WIFI_DBG_LOG("WifiDbg:STA Disconnected ssid:%s bssid:%x:%x:%x:%x:%x:%x reason:%d\n\r", 
          ssid,
          bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5],
          reason);

        if (reason == WIFI_REASON_AUTH_LEAVE)
        {
          GJ_WIFI_DBG_LOG("WifiDbg:Leave case, reconnecting wifi\n\r");
          EventManager::Function f(ReconnectWifi);
          GJEventManager->Add(f);
        }
        else
          addSTAConnectionEvent = true;
      }
      else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP )
      {
        GJ_WIFI_DBG_LOG("WifiDbg:WIFI_STA_GOT_IP handled\n\r"); 

        LOG("STA IP:%s\n\r", GetWifiIP().c_str());
        addSTAConnectionEvent = true;
      }

      if (addSTAConnectionEvent)
      {
        EventManager::Function f(HandleSTAConnection);
        GJEventManager->Add(f);
        GJ_WIFI_DBG_LOG("WifiDbg:HandleSTAConnection event added\n\r");
      }
    };

    WiFiEventFuncCb eventCB(onWifiEvent);

    s_eventId = WiFi.onEvent(eventCB);

    GJ_WIFI_DBG_LOG("WifiDbg:event handler added\n\r");
  }

  //if (!s_keepAliveSet)
  //{
  //  s_keepAliveSet = true;
//
  //  AddKeepAlive();
  //}
  
  delay(200);   ///for brown out

  bool ssidIsEmpty = !args.m_ssid || !args.m_ssid[0]; 

  if (!ssidIsEmpty)
  {
    LOG("  Wifi:Starting station\n\r");
    LOG("  Connecting to SSID '%s' ", args.m_ssid);
    //WiFi.mode(WIFI_STA); 
    //StartWifiStation(args);
    SetupSTA(args);
    /*
    if (!IsWifiConnected())
    {
      LOG("Failed to connect wifi, retrying...\n\r");
      WiFi.mode(WIFI_STA); 
      esp_wifi_stop();
      WiFi.mode(WIFI_STA); 
      StartWifiStation(args);
    }*/
  }
  else
  {
    LOG("WiFi:no config set, enabling AP\n\r");
    SetupAP(args);
  }

  //only go in AP mode when module was just powered on and not when waking up
  if (!IsWifiConnected() && !IsSleepWakeUp())
  {
    //LOG("  Wifi:starting access point...\n\r");
    //WiFi.mode(WIFI_AP_STA);
    //StartWifiAccessPoint(args);
    //StartWifiAPSTA(args);
    //SetupAP(args);
  }

  //if (IsWifiAP())
  //{
  //  LOG("  Wifi is running as an access point\n\r");
  //  SetRuntime(WIFI_AP_RUNTIME);
  //  LOG("  Runtime set to %d seconds\n\r", (int)WIFI_AP_RUNTIME);
  //}
//
  //bool const onlineDateNeeded = IsOnlineDateNeeded();
  //if (onlineDateNeeded)
  //    UpdateDatetimeOnline();

  s_wifiStartEvent.CallListeners();
}

void DoTurnOffWifi()
{
  bool const wifiConnected = IsWifiConnected();

  if (wifiConnected || IsWifiAP())
    s_wifiStopEvent.CallListeners();

  if (s_eventId != 0)
  {
    WiFi.removeEvent(s_eventId);
    s_eventId = 0;
    GJ_WIFI_DBG_LOG("WifiDbg:event handler removed\n\r");
  }

  if (wifiConnected)
  {
    bool wifiOff(true);
    WiFi.disconnect(wifiOff);
    delay(200);
  }

  WiFi.mode(WIFI_MODE_NULL);
  wifiIsAP = false;
}

void TurnOffWifi()
{
  DoTurnOffWifi();
  onlineAccessRequested = false;
}

void RequestWifiAccess()
{
  onlineAccessRequested = true;

  if (g_wifiInit && WiFi.getMode() == WIFI_MODE_NULL)
  {
    StartWifi(g_wifiArgs);
  }
}

void TermWifi()
{
  TurnOffWifi();
}

void OnExitWifiCallback()
{
  //SER("OnExitWifiCallback\n\r");
  TurnOffWifi();
}

void ShowWifiNetworks()
{
  int16_t count = WiFi.scanNetworks();

  SER("Wifi networks:\n\r");

  for (int16_t i = 0 ; i < count ; ++i)
  {
    String ssid;
    uint8_t encryptionType{};
    int32_t RSSI{};
    uint8_t* BSSID{};
    int32_t channel{};

    bool res = WiFi.getNetworkInfo(i, ssid, encryptionType, RSSI, BSSID, channel);

    String BSSIDstr = WiFi.BSSIDstr(i);

    SER("  %s enc:%02d RSSI:%04d BSSID:%17s chan:%02d '%s' \n\r", 
    res ? "OK" : "FAIL", encryptionType, RSSI, BSSIDstr.c_str(), channel, ssid.c_str());
  }

  if (!count)
    SER("  None found\n\r");
}


void Command_DebugWifi()
{
  String staIP = GetWifiIP();
  IPAddress apIP = WiFi.softAPIP();
  
  uint8_t primCh = 0;
  wifi_second_chan_t secondCh = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&primCh, &secondCh);

  SER("Wifi:\n\r"
      "  Connected:%d\n\r"
      "  Wifi Requested:%d\n\r"
      "  Args Set:%d\n\r"
      "  ssid:'%s'\n\r"
      "  STA IP:%s\n\r"
      "  AP IP:%s\n\r"
      "  AP enabled:%s\n\r"
      "  STA status:%d\n\r"
      "  Prim ch:%d\n\r"
      "  Secondary ch:%d\n\r"
      ,
      WiFi.isConnected(),
      onlineAccessRequested,
      g_wifiInit,
      g_wifiArgs.m_ssid ? g_wifiArgs.m_ssid : "N/A",
      staIP.c_str(),
      apIP.toString().c_str(),
      IsWifiAPEnabled() ? "yes" : "no",
      (int32_t)WiFi.status(),
      primCh,
      (int)secondCh
      );
}

void Command_ReconnectWifi()
{
  ReconnectWifi();
}

bool TestUrl(const char *url)
{
  if (!url)
  {
    SER("TestUrl:nothing specified\n\r");
    return false;
  }

  return false;

  //String response;
  //return HttpGet(url, response);
}

void TestWifiConnection()
{
  //int listen_sock = 0;
  //close(listen_sock);

  const char *urls[] = 
  {"http://worldtimeapi.org",
   "http://worldclockapi.com",
   "http://google.com"};

  for (const char *url : urls)
  {
    if (TestUrl(url))
    {
      SER("%s test:success\n\r", url);
      break;
    }
    else
    {
      SER("%s test:FAILURE\n\r", url);
    }
  }
}

bool IsSTAEnabled()
{
  return WiFi.getMode() == WIFI_MODE_STA || WiFi.getMode() == WIFI_MODE_APSTA;
}

void KeepAlive();
void AddKeepAlive()
{
  int period = GJ_CONF_BOOL_VALUE(wifi_keepalive);
  EventManager::Function f(KeepAlive);
  GJEventManager->DelayAdd(f, period * 1000 * 1000);
}

void KeepAlive()
{
  SER("KeepAlive\n\r");

  if (IsWifiConnected() && onlineAccessRequested)
  {
    const char *url = "http://michelvachon.com";

    if (!TestUrl(url))
    {
      DoTurnOffWifi();
      StartWifi(g_wifiArgs);
      char date[20];
      ConvertEpoch(GetUnixtime(), date);
      SER("Wifi restarted at %s\n\r", date);
    }
  }

  AddKeepAlive();
}

void HandleSTAConnection()
{
  if (IsWifiConnected())
  {
    GJ_WIFI_DBG_LOG("WifiDbg:HandleLostSTA:wifi reconnected\n\r");

    esp_wifi_set_inactive_time(WIFI_IF_STA, 16000);

    bool const onlineDateNeeded = IsOnlineDateNeeded();
    if (onlineDateNeeded)
        UpdateTimeOnline();
        
    if (IsWifiAPEnabled())
    {
      GJ_WIFI_DBG_LOG("WifiDbg:Disabling AP\n\r");
      WiFi.enableAP(false);
    }
  }
  else if (onlineAccessRequested)
  {
    bool elapsedConn = (millis() - g_connectionAttempStart) > g_wifiArgs.m_timeout;
      
    if (!IsWifiAPEnabled() && elapsedConn)
    {
      WiFi.enableAP(true);
      SetupAP(g_wifiArgs);
      GJ_WIFI_DBG_LOG("WifiDbg:AP enabled\n\r");
    }

    {
      int reconnectPeriod = GJ_CONF_INT32_VALUE(wifi_reconnectPeriod);

      if (!IsSTAEnabled())
      {
        bool elapsedReconn = ((millis() - g_connectionAttempStart) / 1000) > reconnectPeriod;
        if (elapsedReconn)
        {
          GJ_WIFI_DBG_LOG("WifiDbg:reconnect period reached\n\r");
          SetupSTA(g_wifiArgs);
          GJ_WIFI_DBG_LOG("WifiDbg:STA enabled\n\r");
        }
      }
      else
      {
        bool elapsedConn = (millis() - g_connectionAttempStart) > g_wifiArgs.m_timeout;
        if (elapsedConn)
        {
          GJ_WIFI_DBG_LOG("WifiDbg:connect timeout\n\r");
          WiFi.enableSTA(false);
          GJ_WIFI_DBG_LOG("WifiDbg:STA disabled\n\r");

          EventManager::Function f(HandleSTAConnection);
          GJEventManager->DelayAdd(HandleSTAConnection, (reconnectPeriod + 2) * 1000 * 1000);
          GJ_WIFI_DBG_LOG("WifiDbg:HandleSTAConnection event delay added\n\r");
        }
      }
    }
  }
}

const char* GetWifiEventName(arduino_event_id_t id)
{
  const char *names[] = {
  "WIFI_READY",
	"WIFI_SCAN_DONE",
	"WIFI_STA_START",
	"WIFI_STA_STOP",
	"WIFI_STA_CONNECTED",
	"WIFI_STA_DISCONNECTED",
	"WIFI_STA_AUTHMODE_CHANGE",
	"WIFI_STA_GOT_IP",
	"WIFI_STA_GOT_IP6",
	"WIFI_STA_LOST_IP",
	"WIFI_AP_START",
	"WIFI_AP_STOP",
	"WIFI_AP_STACONNECTED",
	"WIFI_AP_STADISCONNECTED",
	"WIFI_AP_STAIPASSIGNED",
	"WIFI_AP_PROBEREQRECVED",
	"WIFI_AP_GOT_IP6",
	"WIFI_FTM_REPORT",
	"ETH_START",
	"ETH_STOP",
	"ETH_CONNECTED",
	"ETH_DISCONNECTED",
	"ETH_GOT_IP",
	"ETH_GOT_IP6",
	"WPS_ER_SUCCESS",
	"WPS_ER_FAILED",
	"WPS_ER_TIMEOUT",
	"WPS_ER_PIN",
	"WPS_ER_PBC_OVERLAP",
	"SC_SCAN_DONE",
	"SC_FOUND_CHANNEL",
	"SC_GOT_SSID_PSWD",
	"SC_SEND_ACK_DONE",
	"PROV_INIT",
	"PROV_DEINIT",
	"PROV_START",
	"PROV_END",
	"PROV_CRED_RECV",
	"PROV_CRED_FAIL",
	"PROV_CRED_SUCCESS"};

  return names[id];
}


static void command_wifioff()
{
    TurnOffWifi();
    SER("Online turned off\n\r");
}
static void command_wifion() 
{
    if (!IsWifiConnected())
    {
      onlineAccessRequested = true;
      StartWifi(g_wifiArgs);
      WaitForSTAIP(g_wifiArgs);
    }
    SER("Online enabled\n\r");
} 
static void command_wificonf(const char* command)
{
  CommandInfo info;
  GetCommandInfo(command, info);

  if (info.m_argCount == 2)
  {
    GJString ssid(info.m_argsBegin[0], info.m_argsLength[0]);
    GJString pass(info.m_argsBegin[1], info.m_argsLength[1]);
    SetWifiConfig(ssid.c_str(), pass.c_str());
  }
  else
  {
    SER("Invalid arg count:'%s'\n\r", command);
  }
}

static void command_wifiwriteconf()
{
    WriteWifiConfig(g_wifiArgs.m_configPath, g_wifiConfig);
}

DEFINE_COMMAND_NO_ARGS(wifioff, command_wifioff);
DEFINE_COMMAND_NO_ARGS(wifion, command_wifion);
DEFINE_COMMAND_ARGS(wificonf, command_wificonf);
DEFINE_COMMAND_NO_ARGS(wifiwriteconf, command_wifiwriteconf );

DEFINE_COMMAND_NO_ARGS(wifinets, ShowWifiNetworks );
DEFINE_COMMAND_NO_ARGS(wifireconn, Command_ReconnectWifi );
DEFINE_COMMAND_NO_ARGS(wifidbg, Command_DebugWifi );
DEFINE_COMMAND_NO_ARGS(wifitest, TestWifiConnection );


void InitWifi(GJWifiArgs const &args)
{
  if (g_wifiInit)
    return;

  REFERENCE_COMMAND(wifioff);
  REFERENCE_COMMAND(wifioff);
  REFERENCE_COMMAND(wificonf);
  REFERENCE_COMMAND(wifiwriteconf);
  REFERENCE_COMMAND(wifinets );
  REFERENCE_COMMAND(wifireconn );
  REFERENCE_COMMAND(wifidbg );
  REFERENCE_COMMAND(wifitest );

  

  if (args.m_ssid)
  {
    g_wifiConfig.m_ssid = args.m_ssid;
    if (args.m_password)
      g_wifiConfig.m_pass = args.m_password;
  }
  else
  {
    ReadWifiConfig(args.m_configPath, g_wifiConfig);
  }

  if (!g_wifiInit)
  {
    RegisterExitCallback(OnExitWifiCallback);
  }
  
  g_wifiInit = true;
  g_wifiArgs = args;
  g_wifiArgs.m_ssid = g_wifiConfig.m_ssid.c_str();
  g_wifiArgs.m_password = g_wifiConfig.m_pass.c_str();

  if (!IsSleepWakeUp())
  {
    LOG("Online enabled:not a wake up\n\r");
  }
  else if (!onlineAccessRequested)
  {
    LOG("Online not requested\n\r");
    return;
  }

  StartWifi(g_wifiArgs);
}



void UpdateWifi()
{
  //HandleLostSTA();
}