#pragma once

//#define GJ_BLUEDROID

#ifdef GJ_BLUEDROID

#include <gj/vector.h>
#include <gj/string.h>

#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"

class GJOTA;

class GJBLEServer
{
public:
  GJBLEServer();

  void RegisterTerminalHandler();
  bool Init(const char *hostname, GJOTA *ota = nullptr);
  void Term();

  void Update();
  bool Broadcast(const char *text);
  bool HasClient() const;

protected:
  class BLEClient;

  void OnNewClient(BLEClient *client);
  bool BroadcastText(const char *text);

private:

  bool m_init = false;
  uint32_t m_terminalIndex = -1;
  GJOTA *m_ota = nullptr;
  GJString m_hostname;
  bool m_oneTimeInit = false;

  
  bool Init();
  
  typedef Vector<BLEClient*> Clients; 
  Clients m_clients;

  Vector<GJString> m_commands;
  
  struct SPendingSerial;
  SPendingSerial *m_pendingSerial = nullptr;
  
  static String CleanupText(const char *text);

  struct gatts_profile_inst;
  
  static gatts_profile_inst *gl_profile_tab;

  static GJBLEServer *ms_instance;

  BLEClient* GetClient(uint16_t conn_id, esp_bd_addr_t bda) const;
  BLEClient* GetClient(uint16_t conn_id) const;


  static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
  static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  void gatts_profile_event_handler(gatts_profile_inst &profile, esp_gatts_cb_event_t event, esp_ble_gatts_cb_param_t *param);
  
  void InterpretCommand(const char *cmd, uint32_t len, BLEClient *client, uint16_t gatts_if, uint16_t descr_handle);
  void SendHelp(BLEClient &client, uint16_t gatts_if, uint16_t descr_handle);
};

#else 

#include "gjbleserver_nimble.h"
#include "gjbleserver_nordic.h"
#endif