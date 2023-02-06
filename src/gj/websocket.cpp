#include "websocket.h"

#if 1

#include "esputils.h"
#include "serial.h"
#include "commands.h"
#include "gjwifi.h"
#include "gjwificlient.h"
#include "eventmanager.h"
#include "ws.h"
#include "datetime.h"
#include "lwip/sockets.h"

#define WS_LOG_ON_ERR(r, ...) if (r < 0) {LOG(__VA_ARGS__);}

void SendRecentLog(tl::function_ref<void(const char *)> cb);

class GJWebSocketServer::WSClient : public GJWifiClient
{
  public:
    WSClient(int conn, struct sockaddr_storage &source_addr);

    int status() const;

    void getip(GJString &string, int *port = nullptr) const;

  private:
  struct sockaddr_storage m_source_addr;
};

GJWebSocketServer::WSClient::WSClient(int conn, struct sockaddr_storage &source_addr)
: GJWifiClient(conn)
, m_source_addr(source_addr)
{

}

int GJWebSocketServer::WSClient::status() const
{
  if (m_conn <= 0)
    return -1;

  int error_code = 0;
  socklen_t error_code_size = sizeof(error_code);
  getsockopt(m_conn, SOL_SOCKET, SO_ERROR, &error_code, &error_code_size);

  return error_code;
}

void GJWebSocketServer::WSClient::getip(GJString &string, int *port) const
{
  if (m_conn <= 0)
    return;

  char myIP[16];

  sockaddr_in *my_addr = (sockaddr_in*)&m_source_addr;

  inet_ntop(AF_INET, &my_addr->sin_addr, myIP, sizeof(myIP));
  if (port)
    *port = ntohs(my_addr->sin_port);

  string = myIP;
}

struct GJWebSocketServer::SPendingSerial
  {
    uint32_t m_length = 0;
    static const uint32_t MaxLength = 8092;
    char m_buffer[MaxLength];
  };

static bool sendPing(false);

GJWebSocketServer::GJWebSocketServer() = default;

GJWebSocketServer::GJWebSocketServer(uint32_t port)
{
  Init(port);
}

bool GJWebSocketServer::HasClient() const
{
  return !m_clients.empty();
}

void GJWebSocketServer::RegisterTerminalHandler()
{
  if (m_terminalIndex == -1)
  {
    auto handleTerminal = [&](const char *text)
    {
      if (!m_pendingSerial)
        m_pendingSerial = new SPendingSerial;

      uint32_t length = strlen(text);

      if (m_pendingSerial->m_length + length < SPendingSerial::MaxLength)
      {
        strcpy(m_pendingSerial->m_buffer + m_pendingSerial->m_length, text);
        m_pendingSerial->m_length += length;
      }

      //if (HasClient())
      //{
      //  if (BroadcastText(m_pendingSerial->m_buffer))
      //    m_pendingSerial->m_length = 0;
      //}
    };
    
    m_terminalIndex = AddTerminalHandler(handleTerminal);
  }
}

static void command_wsping()
{
  SER("Sendping ping\n\r");
  sendPing = true;
};


DEFINE_COMMAND_NO_ARGS(wsping, command_wsping);

GJWebSocketServer *GJWebSocketServer::ms_instance = nullptr;

bool GJWebSocketServer::Init(uint32_t port)
{
  DEFINE_COMMAND_NO_ARGS(wssendlog, Command_wssendlog);
  DEFINE_COMMAND_NO_ARGS(wsdbg, Command_wsdbg);

  
  if (!m_init)
  {
    m_init = true;

    ms_instance = this;

    //netconn_thread_init();
    struct sockaddr_storage dest_addr;
    {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(80);
    }

    int err = 0;

    m_server = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    WS_LOG_ON_ERR(m_server, "GJWebSocketServer::Init:socket: err:%d\n", (int)err);

    int opt = 1;
    setsockopt(m_server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    err = bind(m_server, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    WS_LOG_ON_ERR(m_server, "GJWebSocketServer::Init:bind: err:%d\n", (int)err);

    err = listen(m_server, 1);
    WS_LOG_ON_ERR(err, "GJWebSocketServer::Init:listen: err:%d\n", (int)err);

    int flags = fcntl(m_server, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(m_server, F_SETFL, flags);
  }

  RegisterTerminalHandler();

  if (!m_wifiCBSet)
  {
    m_wifiCBSet = true;
    uint32_t p = port;

    GJWebSocketServer *s = this;

    auto onWifiStart = [=]()
    {
      bool res = s->Init(p);
      LOG("WebSocket initialize(port:%d):%s\n\r", p, res ? "success" : "failed");
    };
    RegisterWifiStartCallback(onWifiStart);

    auto onWifiStop = [&]()
    {
      this->Term();
      LOG("WebSocket term\n\r");
    };
    RegisterWifiStopCallback(onWifiStop);
  }

  REFERENCE_COMMAND(wssendlog);
  REFERENCE_COMMAND(wsdbg);
  REFERENCE_COMMAND(wsping);

  //tests
  {
    uint8_t buf[] = {0x81,0x91,0x59,0x36,0x39,0x35,0x3e,0x5c,0x5a,0x5a,0x34,0x5b,0x58,0x5b,0x3d,0x0c,0x4d,0x47,0x38,0x55,0x52,0x52,0x31};
    WSFrame frame = DecodeWSFrame(buf, sizeof(buf));
    SER_COND(frame.FIN != true, "Unexpected decoded frame: FIN != true\n\r");
    SER_COND(frame.RSV1 != false, "Unexpected decoded frame: RSV1 != false\n\r");
    SER_COND(frame.RSV2 != false, "Unexpected decoded frame: RSV2 != false\n\r");
    SER_COND(frame.RSV3 != false, "Unexpected decoded frame: RSV3 != false\n\r");
    SER_COND(frame.OpCode != WSOpCode::TextFrame, "Unexpected decoded frame: OpCode != WSOpCode::TextFrame\n\r");
    SER_COND(frame.Payload != 17, "Unexpected decoded frame: Payload != 17\n\r");
    SER_COND(frame.Mask != true, "Unexpected decoded frame: Mask != true\n\r");
  }

  return m_server >= 0;
}

void GJWebSocketServer::Command_wssendlog()
{
  auto broadWSLog = [=](const char *buffer)
  {
    ms_instance->BroadcastText(buffer);
  };
  SendRecentLog(broadWSLog);
}

void GJWebSocketServer::Command_wsdbg()
{
  SER("Client count: %d\n\r", ms_instance->m_clients.size());
  for (WSClient *client : ms_instance->m_clients)
  {
    GJString ip;
    int port(0);

    client->getip(ip, &port);
    SER("  %s:%d Connected:%d Status:%d avail:%d\n\r", ip.c_str(), port, client->connected(), client->status(), client->available());
  }
}

void GJWebSocketServer::Term()
{
  if (m_terminalIndex != -1)
  {
    RemoveTerminalHandler(m_terminalIndex);
    m_terminalIndex = -1;
    LOG("WSS terminal handler removed\n\r");
  }

  for (WSClient *client : m_clients)
  {
    delete client;
  }

  m_clients.clear();

  if (m_init)
  {
    int err = 0;
    err = close(m_server);
    WS_LOG_ON_ERR(err, "GJWebSocketServer::Term:close: err:%d\n", (int)err);
    m_server = -1;  
    m_init = false;
  }

  LOG("WSS terminated\n\r");
}

void GJWebSocketServer::OnNewClient(int conn, struct sockaddr_storage &source_addr)
{
  LOG("RAM avail:%dKB\n\r", heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024);

  WSClient *client = new WSClient(conn, source_addr);

  //wait for headers
  SetDebugLoc("Waiting for headers\n\r");
  while(!client->available())
  {
    delay(100);
  }

  uint32_t dataSize = client->available();
  Vector<uint8_t> data;
  data.resize(dataSize);
  client->read(data.data(), dataSize);
  
  char handshake[256];
  prepare_response((char*)data.data(),dataSize,handshake,nullptr);
  client->write((uint8_t*)handshake,strlen(handshake));

  Vector<GJString> commands;
  GetCommands(commands);

  GJString output;
  output += "ws_availablecommands:";
  for (GJString &str : commands)
  {
    output += str;
    output += ";";
  }

  WSFrame frame(output.c_str());
  WSData wsData;
  EncodeWSFrame(frame, wsData);

  client->write(wsData.m_buffer,wsData.m_size);

  m_clients.push_back(client);
}

GJString GJWebSocketServer::CleanupText(const char *text)
{
  GJString feedStorage;
  text = RemoveNewLineLineFeed(text, feedStorage);

  GJString output = text;

  for (int i = 0 ; i < output.length() ; ++i)
  {
    char c = output[i];

    if (!isprint(c) && c != '\n' && c != '\r')
    {
      output[i] = '.';
    }
  }

  return output;
}

bool GJWebSocketServer::BroadcastText(const char *text)
{
  GJString string("sendtext:");
  string += text;

  string = CleanupText(string.c_str());

  return Broadcast(string.c_str());
}

bool GJWebSocketServer::Broadcast(const char *text)
{
  if (m_clients.empty())
    return false;

  WSFrame frame(text);
  WSData data;
  EncodeWSFrame(frame, data);

  //printf("-----\n\r");
  //WSFrame frame2 = DecodeWSFrame(&data);
  //PrintWSFrame(frame);
  //PrintWSFrame(frame2);
  //printf("-----\n\r");
  
  bool written(false);

  Clients::iterator it = m_clients.begin();
  for (; it != m_clients.end() ; )
  {
    Client *client = *it;

    int writtenSize = client->write(data.m_buffer, data.m_size);
    written |= writtenSize != 0;

    if (!client->connected() || !writtenSize)
    {
      it = m_clients.erase(it);
      delete client;
      LOG("WS:Write failed, client removed\n\r");
    }
    else
    {
      ++it;
    }
  }

  return written;
}

void GJWebSocketServer::Update()
{
  if (!IsWifiAPEnabled() && !IsWifiSTAEnabled())
    return;

  SetDebugLoc("WSS accept");

  struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
  socklen_t addr_len = sizeof(source_addr);
  int sock = accept(m_server, (struct sockaddr *)&source_addr, &addr_len);
  if (sock < 0) 
  {
    LOG_COND(errno != EAGAIN && errno != EWOULDBLOCK, "Unable to accept connection: errno %d\n\r", errno);
  }
  else
  {
    LOG("New websocket client\n\r");
    LogTime();
    OnNewClient(sock, source_addr);
  }
  
  if (m_pendingSerial && m_pendingSerial->m_length && HasClient())
  {
    if (BroadcastText(m_pendingSerial->m_buffer))
      m_pendingSerial->m_length = 0;
  }
  
  SetDebugLoc("WSS clients");
  Clients::iterator it = m_clients.begin();
  for (; it != m_clients.end() ; )
  {
    Client *client = *it;

    if (!client->connected())
    {
      it = m_clients.erase(it);
      delete client;
      continue;
    }

    if (sendPing)
    {
      WSFrame frame;
      frame.FIN = true;
      frame.OpCode = WSOpCode::Ping;

      WSData data;
      EncodeWSFrame(frame, data);
      client->write(data.m_buffer, data.m_size);
    }

    WSData *wsData = nullptr;
    uint32_t dataSize = client->available(); 
    if (!dataSize)
    {
      ++it;
      continue;
    }

    wsData = new WSData(dataSize);
    client->read(wsData->m_buffer, dataSize);

    WSFrame frame = DecodeWSFrame(wsData);
    
    if (frame.OpCode == WSOpCode::TextFrame)
    {
      if (!strncmp((char*)frame.data, "gjcommand:", 10))
      {
        GJString command((char*)frame.data + 10, frame.Payload - 10);
        m_commands.push_back(command);
      }
      else
      {
        LOG("GJWebSocketServer::Update: received unknown text data\n\r");
      }
    }
    else if (frame.OpCode == WSOpCode::Ping)
    {
      frame.OpCode = WSOpCode::Pong;

      WSData data;
      EncodeWSFrame(frame, data);
      client->write(data.m_buffer, data.m_size);
    }
    else if (frame.OpCode == WSOpCode::Pong)
    {
      LOG("WS PONG\n\r");
    }

    if (frame.OpCode == WSOpCode::CloseConnection)
    {
      it = m_clients.erase(it);
      delete client;
    }
    else
    {
      ++it;
    }

    delete wsData;
  }

  sendPing = false;

  for (GJString const &command : m_commands)
  {
    InterpretCommand(command.c_str()); 
  }

  m_commands.clear();
}

#endif