#pragma once

#if defined(NRF)
#define GJ_NORDIC_BLE
#endif

#ifdef GJ_NORDIC_BLE

#include <gj/base.h>
#include <gj/vector.h>
#include <gj/string.h>
#include <gj/gjlock.h>

#include "ble.h"

class GJOTA;
struct CommandInfo;

struct ble_addr_t
{
  uint8_t val[6];
};

class GJBLEServer
{
public:
  GJBLEServer();

  void RegisterTerminalHandler();
  bool Init(const char *hostname, GJOTA *ota = nullptr);
  void Term();
  bool IsInit() const;

  void Update();
  bool Broadcast(const char *text);
  bool HasClient() const;

  void StopAdvertising();
  void RestartAdvertising();
  void SetAdvManufData(const void *data, uint32_t size);

  bool IsIdle() const;

  static GJBLEServer* GetInstance();

  static void Command_ble(const char *command);
  static void Command_bledbg(const CommandInfo &commandInfo);
  static void Command_bleon(const CommandInfo &commandInfo);
  static void Command_bleoff(const CommandInfo &commandInfo);
  static void Command_bleint(const CommandInfo &commandInfo);

protected:
  class BLEClient;

  void OnNewClient(BLEClient *client);
  
private:

  struct DataBuffer;

  bool m_init = false;
  bool m_bleInit = false;
  uint32_t m_terminalIndex = -1;
  GJOTA *m_ota = nullptr;
  GJString m_hostname;
  bool m_oneTimeInit = false;

  bool Init();
  bool CanSendData() const;
  void AddDataBuffer(DataBuffer *db);
  
  GJLock m_lock;
  typedef Vector<BLEClient*> Clients; 
  Clients m_clients;

  Vector<DataBuffer*> m_dataBuffers;

  struct Command
  {
    BLEClient *m_client = nullptr;
    GJString m_string;
  };
  Vector<Command*> m_commands;
  
  ble_uuid_t m_adv_uuids[2];
  uint16_t m_service_handle[2]; 
  ble_gatts_char_handles_t m_char_handle[2]; 


  uint8_t m_attrValue[256];
  uint16_t m_attrValueLength = 0;

  static GJBLEServer *ms_instance;

  static GJString CleanupText(const char *text);

  static void SerialOutput(const char *text);
  static bool IsOutputReady();

  bool BroadcastData(DataBuffer *dataBuffer);


  BLEClient* GetClient(uint16_t conn_id, ble_gap_addr_t bda) const;
  BLEClient* GetClient(uint16_t conn_id) const;
  void DeleteClient(uint16_t conn);


  void InterpretCommand(const char *cmd, uint32_t len, BLEClient *client);
  void SendHelp(BLEClient &client, uint16_t gatts_if, uint16_t descr_handle);
  void SetupAdvertising(const uint8_t* manufData, uint32_t manufSize);

  
  static void OnBLEEvent(ble_evt_t * p_ble_evt);
  static void sys_evt_dispatch(uint32_t sys_evt);

  void HandleBLEEvent(ble_evt_t * p_ble_evt);

  struct SendHelpCommand;
  static void SendNextHelpCommand(SendHelpCommand *helpCommand);

  static void Command_advint(const char *command);
  static void CreateChar(uint16_t service, uint16_t uuid, ble_gatts_char_handles_t &destChar);

  static void OnExit();
};

#endif //GJ_NORDIC_BLE