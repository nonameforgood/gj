#pragma once

#include "vector.h"
#include "string.h"

class Client;

class GJWebSocketServer
{
public:
  GJWebSocketServer();
  GJWebSocketServer(uint32_t port);

  void RegisterTerminalHandler();
  bool Init(uint32_t port);
  void Term();

  void Update();
  bool Broadcast(const char *text);
  bool HasClient() const;

protected:

  void OnNewClient(int new_conn, struct sockaddr_storage &source_addr);
  bool BroadcastText(const char *text);

private:

  bool m_init = false;
  bool m_wifiCBSet = false;
  uint32_t m_terminalIndex = -1;
  int m_server = -1;

  class WSClient;

  typedef Vector<WSClient*> Clients; 
  Clients m_clients;

  Vector<GJString> m_commands;
  
  struct SPendingSerial;
  SPendingSerial *m_pendingSerial = nullptr;
  
  static GJString CleanupText(const char *text);

  static GJWebSocketServer *ms_instance;
  static void Command_wssendlog();
  static void Command_wsdbg();
};