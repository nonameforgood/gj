#pragma once

#if defined(ESP8266) || defined(ESP32) || defined(ESP_PLATFORM)
  #define GJ_NIMBLE
#endif

#ifdef GJ_NIMBLE

#include "vector.h"
#include "string.h"

//#include <nimble/ble.h>
#include <NimBLEDevice.h>

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
  
private:

  bool m_init = false;
  uint32_t m_terminalIndex = -1;
  GJOTA *m_ota = nullptr;
  GJString m_hostname;
  bool m_oneTimeInit = false;

  bool Init();
  
  typedef Vector<BLEClient*> Clients; 
  Clients m_clients;

  struct DataBuffer;

  Vector<DataBuffer*> m_dataBuffers;
  Vector<GJString> m_commands;
  
  static GJBLEServer *ms_instance;

  static GJString CleanupText(const char *text);

  bool BroadcastData(DataBuffer *dataBuffer);


  BLEClient* GetClient(uint16_t conn_id, ble_addr_t bda) const;
  BLEClient* GetClient(uint16_t conn_id) const;

  class Characteristic;
  NimBLEService *m_inService = nullptr;
  NimBLEService *m_outService = nullptr;
  Characteristic* m_inChar;
  Characteristic* m_outChar;

  class ServerCallbacks;
  class CharacteristicCallbacks;
  class DescriptorCallbacks;

  void InterpretCommand(const char *cmd, uint32_t len, BLEClient *client);
  void SendHelp(BLEClient &client, uint16_t gatts_if, uint16_t descr_handle);

  static void Command_BLETest(const char *command);
  static void Command_BLESendLogs();
  static void Command_BLEDbg();
  static void Command_BLEOn();
  static void Command_BLEOff();

};

#endif