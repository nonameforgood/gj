#include "gjbleserver.h"

#ifdef GJ_NIMBLE

#include "esputils.h"
#include "serial.h"

#include "commands.h"
#include "config.h"
#include "eventmanager.h"
#include "datetime.h"
#include "gjota.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include <NimBLEDevice.h>

DEFINE_CONFIG_INT32(ble.dbglvl, ble_dbglvl, 1); //default level 1(error)
DEFINE_CONFIG_INT32(ble.minint, ble_minint, 0);


void SendRecentLog(std::function<void(const char *)> cb);

#define BLE_SER(lvl, ...) { if ( GJ_CONF_INT32_VALUE(ble_dbglvl) >= (lvl)) SER(__VA_ARGS__); }

#define BLE_CALL(call, ...) { auto ret = call; if (IsError(ret)) { BLE_SER(1, "'%s' failed:'%s'\n", #call, esp_err_to_name(ret)); __VA_ARGS__; } } 
//1: error
//2: warn
//3: info
//4: debug
//5: verbose


#define GATTS_SERVICE_UUID_TEST_A   0x00FF
#define GATTS_CHAR_UUID_TEST_A      0xFF01
#define GATTS_DESCR_UUID_TEST_A     0x3333

#define GATTS_SERVICE_UUID_TEST_B   0x00EE
#define GATTS_CHAR_UUID_TEST_B      0xEE01
#define GATTS_DESCR_UUID_TEST_B     0x2222

class GJBLEServer::Characteristic
{
  public:
    Characteristic(NimBLECharacteristic *c, uint32_t index);

    NimBLECharacteristic* GetChar() const;
    uint32_t GetIndex() const;

  private:
  NimBLECharacteristic *m_char;
  const uint32_t m_index;
};

GJBLEServer::Characteristic::Characteristic(NimBLECharacteristic *c, uint32_t index)
: m_char(c)
, m_index(index)
{

}

NimBLECharacteristic* GJBLEServer::Characteristic::GetChar() const
{
  return m_char;
}

uint32_t GJBLEServer::Characteristic::GetIndex() const
{
  return m_index;
}
  
class GJBLEServer::BLEClient
{
  public:
    BLEClient(uint16_t conn_id, ble_addr_t bda);
    ~BLEClient();

    bool Indicate(Characteristic *c, const char *string);
    bool Indicate(Characteristic *c, const uint8_t *data, uint32_t len);


    uint16_t GetConnId() const;
    uint8_t const* GetBDA() const;

    bool Is(uint16_t conn_id, ble_addr_t bda) const;
    bool Is(uint16_t conn_id) const;

    uint32_t GetRef() const;
    void IncRef();
    uint32_t DecRef();

    int status() const;

    void SetCongested(bool);
    bool IsCongested() const;

    void SetMTU(uint32_t mtu);
    uint32_t GetMTU() const;

    void SetInterval(uint32_t interval);
    uint32_t GetInterval() const;

    void SetDataBufferId(uint32_t id);
    uint32_t GetDataBufferId() const;

    void SetSubscription(uint32_t indexMask);
    void ClearSubscription(uint32_t indexMask);
    bool IsSubscribed(uint32_t index);

  private:
    uint32_t m_ref = 1;
    uint16_t m_conn_id;
    ble_addr_t m_bda;

    //::NimBLEClient *m_client = nullptr;

    volatile bool m_congested = false;
    uint32_t m_mtu = 23;
    uint32_t m_interval = 50;
    volatile uint32_t m_lastSend = 0;
    uint32_t m_dataBufferId = 0;
    uint32_t m_subscription = 0;
};

GJBLEServer::BLEClient::BLEClient(uint16_t conn_id, ble_addr_t bda)
: m_conn_id(conn_id)
{
  //m_client = NimBLEDevice::getClientByID(conn_id);
  //m_client = NimBLEDevice::createClient(bda);
  //m_client->connect();

  memcpy(&m_bda, &bda, sizeof(ble_addr_t));

/*
  esp_ble_conn_update_params_t conn_params = {0};
  memcpy(conn_params.bda, bda, sizeof(ble_addr_t));
  /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
  /*
  conn_params.latency = 0;
  conn_params.max_int = 0x7;    // max_int = 0x20*1.25ms = 40ms
  conn_params.min_int = 0x6;    // min_int = 0x10*1.25ms = 20ms
  conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms

  esp_err_t ret = esp_ble_gap_update_conn_params(&conn_params);
  if (ret != ESP_OK) {
      GJ_ERROR("esp_ble_gap_update_conn_params failed: %s\n", esp_err_to_name(ret));
  }
  */
}

GJBLEServer::BLEClient::~BLEClient()
{

}

void GJBLEServer::BLEClient::SetCongested(bool congested)
{
  m_congested = congested;
}

bool GJBLEServer::BLEClient::IsCongested() const
{
  return m_congested;
}
void GJBLEServer::BLEClient::IncRef()
{
  m_ref++;
}
uint32_t GJBLEServer::BLEClient::DecRef()
{
  m_ref--;
  return m_ref;
}
bool GJBLEServer::BLEClient::Is(uint16_t conn_id, ble_addr_t bda) const
{
  return m_conn_id == conn_id &&
         !memcmp(&m_bda, &bda, sizeof(ble_addr_t));
}

bool GJBLEServer::BLEClient::Is(uint16_t conn_id) const
{
  return m_conn_id == conn_id;
}

uint32_t GJBLEServer::BLEClient::GetRef() const
{
  return m_ref;
}

uint16_t GJBLEServer::BLEClient::GetConnId() const
{
  return m_conn_id;
}

uint8_t const * GJBLEServer::BLEClient::GetBDA() const
{
  return m_bda.val;
}

bool GJBLEServer::BLEClient::Indicate(Characteristic *c, const char *string)
{ 
  return Indicate(c, (uint8_t*)string, strlen(string));
}

bool GJBLEServer::BLEClient::Indicate(Characteristic *c, const uint8_t *data, uint32_t len)
{  
  if (!IsSubscribed(c->GetIndex()))
    return true;

  esp_err_t errRc;

  uint32_t interval = GJ_CONF_INT32_VALUE(ble_minint);
  
  //it's actually faster to space calls by the client interval
  interval = std::max(interval, GetInterval());

  uint32_t elapsed;
  do
  {
    elapsed = millis() - m_lastSend;
  } while (elapsed < interval);
  
  m_lastSend = millis();

  while (IsCongested())
  {
    delay(20);
  }

  NimBLECharacteristic *nimbleChar = c->GetChar();
  uint16_t handle = nimbleChar->getHandle();

  os_mbuf *om = ble_hs_mbuf_from_flat(data, len);

  //copied from NimBLECharacteristic::notify
  //notify not used because it cannot target a specific client
  int rc = ble_gattc_indicate_custom(m_conn_id, handle, om);
  if (rc != 0)
  {
    BLE_SER(2, "ble_gattc_indicate_custom failed %d\n", rc);
    return false;
  }
  
  return true;
}

int GJBLEServer::BLEClient::status() const
{
  return 0;
}

void GJBLEServer::BLEClient::SetMTU(uint32_t mtu)
{
  m_mtu = mtu;
}
uint32_t GJBLEServer::BLEClient::GetMTU() const
{
  return m_mtu;
}

void GJBLEServer::BLEClient::SetInterval(uint32_t value)
{
  m_interval = value;
}
uint32_t GJBLEServer::BLEClient::GetInterval() const
{
  return m_interval;
}   


void GJBLEServer::BLEClient::SetDataBufferId(uint32_t value)
{
  m_dataBufferId = value;
}
uint32_t GJBLEServer::BLEClient::GetDataBufferId() const
{
  return m_dataBufferId;
}   

void GJBLEServer::BLEClient::SetSubscription(uint32_t indexMask)
{
  m_subscription |= indexMask;
  BLE_SER(4, "Client %d set subst mask 0x%x result=0x%x\n", m_conn_id, indexMask, m_subscription);
}
void GJBLEServer::BLEClient::ClearSubscription(uint32_t indexMask)
{
  m_subscription &= ~indexMask;
  BLE_SER(4, "Client %d cleared subst mask 0x%x result=0x%x\n", m_conn_id, indexMask, m_subscription);
}

bool GJBLEServer::BLEClient::IsSubscribed(uint32_t index)
{
  return m_subscription & (1 << index);
}

struct GJBLEServer::DataBuffer
{
  static uint32_t ms_nextId;

  uint32_t m_id = ms_nextId++;

  DataBuffer();
  DataBuffer(const char *string);
  DataBuffer(const void *data, uint32_t len);

  uint16_t m_clientId = 0xffff;
  Vector<uint8_t> m_data;
};
uint32_t GJBLEServer::DataBuffer::ms_nextId = 1;

GJBLEServer::DataBuffer::DataBuffer() = default;
GJBLEServer::DataBuffer::DataBuffer(const char *string)
{
  uint32_t len = strlen(string);
  m_data.resize(len);
  memcpy(m_data.data(), string, len);
}
GJBLEServer::DataBuffer::DataBuffer(const void *data, uint32_t len)
{
  m_data.resize(len);
  memcpy(m_data.data(), data, len);
}
  
GJBLEServer *GJBLEServer::ms_instance = nullptr;

GJBLEServer::GJBLEServer() = default;

bool GJBLEServer::HasClient() const
{
  return !m_clients.empty();
}

void GJBLEServer::RegisterTerminalHandler()
{
  if (m_terminalIndex == -1)
  {
    auto handleTerminal = [&](const char *text)
    {
      Broadcast(text);
    };
    
    m_terminalIndex = AddTerminalHandler(handleTerminal);
  }
}


GJBLEServer::BLEClient* GJBLEServer::GetClient(uint16_t conn_id, ble_addr_t bda) const
{
  for (BLEClient *client : m_clients)
  {
    if (client->Is(conn_id, bda))
    {
      return client;
    }
  }

  return nullptr;
}

GJBLEServer::BLEClient* GJBLEServer::GetClient(uint16_t conn_id) const
{
  for (BLEClient *client : m_clients)
  {
    if (client->Is(conn_id))
    {
      return client;
    }
  }

  return nullptr;
}


class GJBLEServer::ServerCallbacks: public NimBLEServerCallbacks {

    GJBLEServer *m_gjServer;
public:
    ServerCallbacks(GJBLEServer *server)
    : m_gjServer(server)
    {

    }

    void onConnect(NimBLEServer* pServer) {
        BLE_SER(5, "Client connected\n"
                "Multi-connect support: start advertising\n");
        NimBLEDevice::startAdvertising();
    };

    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
        BLE_SER(4, "Connected Client address: %s\n", NimBLEAddress(desc->peer_ota_addr).toString().c_str());

        uint16_t minInterval = 6;
        uint16_t maxInterval = 7;
        uint16_t latency = 0;
        uint16_t timeout = 400;
        pServer->updateConnParams(desc->conn_handle, minInterval, maxInterval, latency, timeout);

        BLEClient *client = new BLEClient(desc->conn_handle, desc->peer_id_addr);
        client->SetInterval(maxInterval);
        
        m_gjServer->OnNewClient(client);
    };
    
    void onDisconnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
        BLE_SER(4, "Client disconnected - start advertising");
        NimBLEDevice::startAdvertising();

        Clients::iterator it = m_gjServer->m_clients.begin();
        for (; it != m_gjServer->m_clients.end() ; )
        {
          BLEClient *client = *it;

          if (client->Is(desc->conn_handle, desc->peer_id_addr))
          {
            if (!client->DecRef())
            {   
              it = m_gjServer->m_clients.erase(it);
              delete client;
              BLE_SER(3, "Client deleted\n");
              continue;
            }
          }

          ++it;
        }
    };

    void onMTUChange(uint16_t MTU, ble_gap_conn_desc* desc) {
        BLEClient *client = m_gjServer->GetClient(desc->conn_handle, desc->peer_id_addr);
        if (client)
          client->SetMTU(MTU);
        BLE_SER(5, "MTU updated: %u for connection ID: %u\n", MTU, desc->conn_handle);
    };
};


/** Handler class for characteristic actions */
class GJBLEServer::CharacteristicCallbacks: public NimBLECharacteristicCallbacks {
  GJBLEServer *m_gjServer = nullptr;
  Characteristic *m_char;

public:
  CharacteristicCallbacks(GJBLEServer *gjServer, Characteristic *c)
  : m_gjServer(gjServer)
  , m_char(c)
  {

  }

  void onRead(NimBLECharacteristic* pCharacteristic){
      BLE_SER(5, "%s: onRead(), len: ", pCharacteristic->getUUID().toString().c_str(),
              pCharacteristic->getValue().size());
  };

  void onWrite(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc) {

      std::string value = pCharacteristic->getValue();

      BLE_SER(5, "%s: onWrite(), len:%d\n", pCharacteristic->getUUID().toString().c_str(), value.size());

      BLEClient *client = m_gjServer->GetClient(desc->conn_handle, desc->peer_id_addr);
      m_gjServer->InterpretCommand(
          (char*)value.c_str(), value.size(),
          client);
          
  };

  /** The status returned in status is defined in NimBLECharacteristic.h.
   *  The value returned in code is the NimBLE host return code.
   */
  void onStatus(NimBLECharacteristic* pCharacteristic, Status status, int code) {
      bool isError = status >= NimBLECharacteristicCallbacks::ERROR_INDICATE_DISABLED;
      BLE_SER(isError ? 2 : 5, "Notification/Indication status:%d  code:%d\n", status, code);
  };

  void onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) {
      BLEClient *client = m_gjServer->GetClient(desc->conn_handle, desc->peer_id_addr);

      GJ_ASSERT(m_char, "onSubscribe:m_char is null");

      uint32_t index = m_char->GetIndex();

      if (client && subValue == 0)
        client->ClearSubscription(1 << index);
      else if (client)
        client->SetSubscription(1 << index);

      auto getSubString = [](uint16_t s)
      {
        if(s == 0) {
          return " Unsubscribed to ";
        }else if(s == 1) {
          return " Subscribed to notfications for ";
        } else if(s == 2) {
          return " Subscribed to indications for ";
        } else if(s == 3) {
          return " Subscribed to notifications and indications for ";
        }

        return "";
      };

      BLE_SER(4, "Client ID:%d Address:%s %s %s\n", 
        desc->conn_handle,
        std::string(NimBLEAddress(desc->peer_ota_addr)).c_str(),
        getSubString(subValue),
        std::string(pCharacteristic->getUUID()).c_str());
  };
};


/** Handler class for descriptor actions */    
class GJBLEServer::DescriptorCallbacks : public NimBLEDescriptorCallbacks {

    GJBLEServer *m_server;
    public:
    DescriptorCallbacks(GJBLEServer *server)
    : m_server(server)
    {
    }

    void onWrite(NimBLEDescriptor* pDescriptor) {
      BLE_SER(5, "%s Descriptor witten value len:%d\n", 
        pDescriptor->getUUID().toString().c_str(), pDescriptor->getLength());
    };

    void onRead(NimBLEDescriptor* pDescriptor) {
      BLE_SER(5, "%s Descriptor read value len:%d\n", 
        pDescriptor->getUUID().toString().c_str(), pDescriptor->getLength());
    };
};

bool GJBLEServer::Init(const char *hostname, GJOTA *ota)
{
  m_hostname = hostname;
  m_ota = ota;
  
  return Init();
}

uint32_t bleTestIndex = 0;
uint32_t bleTestIndexEnd = 2000;
uint32_t bleTestPacketSize = 23;

void TestBLE()
{
  GJString str;

  str.reserve(500);

  str = (bleTestIndex == 0) ? "bleteststart:" : "bletest:";
  for (int i = 0 ; i < 128 ; ++i)
  {
    GJString next = FormatString("%d,", bleTestIndex++);
    uint32_t nextLen = next.size();

    if ((str.size() + nextLen) > bleTestPacketSize)
    {
      SER(str.c_str());
      str.clear();
      str = "bletest:";
    }

    str += next;  

    if (bleTestIndex >= bleTestIndexEnd)
      break;
  }

  if (!str.empty())
    SER(str.c_str());

  SER("\n");
  
  if (bleTestIndex >= bleTestIndexEnd)
    bleTestIndex = 0;
  else
    GJEventManager->DelayAdd(TestBLE, 100000);
}


bool GJBLEServer::Init()
{
  DEFINE_COMMAND_ARGS(bletest,Command_BLETest);
  DEFINE_COMMAND_NO_ARGS(blesendlog, Command_BLESendLogs);
  DEFINE_COMMAND_NO_ARGS(bledbg, Command_BLEDbg);
  DEFINE_COMMAND_NO_ARGS(bleon, Command_BLEOn);
  DEFINE_COMMAND_NO_ARGS(bleoff, Command_BLEOff);

  if (!m_init)
  {

    // Initialize NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    NimBLEDevice::init(m_hostname.c_str());
    NimBLEDevice::setSecurityAuth(false, false, true); 
    NimBLEDevice::setMTU(500);

    NimBLEServer *pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks(this));

    m_inService = pServer->createService(NimBLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_A));
    NimBLECharacteristic* pInCharacteristic = m_inService->createCharacteristic(
                                               NimBLEUUID((uint16_t)GATTS_CHAR_UUID_TEST_A),
                                               NIMBLE_PROPERTY::READ |
                                               NIMBLE_PROPERTY::WRITE |
                                               NIMBLE_PROPERTY::NOTIFY
                                              );
    CharacteristicCallbacks *charDBs = new CharacteristicCallbacks(this, nullptr);
    pInCharacteristic->setValue("input");
    pInCharacteristic->setCallbacks(charDBs);
    m_inChar = new Characteristic(pInCharacteristic, 2);
    NimBLEDescriptor* pInDesc = pInCharacteristic->createDescriptor(NimBLEUUID((uint16_t)GATTS_DESCR_UUID_TEST_A)); 

    m_outService = pServer->createService(NimBLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_B));
    NimBLECharacteristic* pOutCharacteristic = m_outService->createCharacteristic(
                                               NimBLEUUID((uint16_t)GATTS_CHAR_UUID_TEST_B),
                                               NIMBLE_PROPERTY::READ |
                                               NIMBLE_PROPERTY::WRITE |
                                               NIMBLE_PROPERTY::NOTIFY
                                              );
    m_outChar = new Characteristic(pOutCharacteristic, 1);

    pOutCharacteristic->setValue("output");
    pOutCharacteristic->setCallbacks(new CharacteristicCallbacks(this, m_outChar));
    NimBLEDescriptor* pOutDesc = pOutCharacteristic->createDescriptor(NimBLEUUID((uint16_t)GATTS_DESCR_UUID_TEST_B)); 

    m_inService->start();
    m_outService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();

    pAdvertising->addServiceUUID(m_inService->getUUID());
    pAdvertising->addServiceUUID(m_outService->getUUID());
    /** If your device is battery powered you may consider setting scan response
     *  to false as it will extend battery life at the expense of less data sent.
     */
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    ms_instance = this;
    
    m_init = true;
  }

  RegisterTerminalHandler();

  if (!m_oneTimeInit)
  {
    m_oneTimeInit = true;

    auto exitCB = [this]()
    {
      if (ms_instance)
        ms_instance->Term();
    };

    RegisterExitCallback(exitCB);
  }

  return m_init;
}

void GJBLEServer::Command_BLETest(const char *command)
{
  CommandInfo info;
  GetCommandInfo(command, info);

  for (int i = 0 ; i < info.m_argCount ; ++i)
  {
    GJString value(FormatString("%.*s", info.m_argsLength[i], info.m_argsBegin[i]));

    if (i == 0)
      bleTestIndexEnd = atoi(value.c_str()); 
    else
      bleTestPacketSize = atoi(value.c_str()); 
  }

  SER("Ble test: max=%d pck:%d\n", bleTestIndexEnd, bleTestPacketSize);

  TestBLE();
}

void GJBLEServer::Command_BLESendLogs()
{
  auto broadWSLog = [=](const char *buffer)
  {
    ms_instance->Broadcast(buffer);
  };
  
  SendRecentLog(broadWSLog);
}


void GJBLEServer::Command_BLEDbg()
{
  uint32_t clientCount = ms_instance->m_clients.size();
  SER("BLE:\n\r");
  SER("  clients:%d\n\r", clientCount);
  for (BLEClient const *client : ms_instance->m_clients)
  {
    SER("    conn:%02d mtu:%03d int:%02d bda:%02x:%02x:%02x:%02x:%02x:%02x ref:%02d\n\r", 
      client->GetConnId(),
      client->GetMTU(),
      client->GetInterval(),
      client->GetBDA()[0], client->GetBDA()[1], client->GetBDA()[2], client->GetBDA()[3], client->GetBDA()[4], client->GetBDA()[5],
      client->GetRef()
      );
  }
  SER("  Pending data buffers:%d\n\r", ms_instance->m_dataBuffers.size());
  SER("  Out characteristic sub count:%d\n\r", ms_instance->m_outChar->GetChar()->getSubscribedCount());
}


void GJBLEServer::Command_BLEOn()
{
  //if (!ms_instance)
  //    this->Init();
  //}
}

void GJBLEServer::Command_BLEOff()
{
  if (ms_instance)
        ms_instance->Term();
}

void GJBLEServer::Term()
{
  if (m_terminalIndex != -1)
  {
    RemoveTerminalHandler(m_terminalIndex);
    m_terminalIndex = -1;
    LOG("BLE server terminal handler removed\n\r");
  }

  Clients clients = std::move(m_clients);
  while(!clients.empty())
  {
    BLEClient *client = clients[0];
    clients.erase(clients.begin());
    delete client;
  }

  if (m_init)
  {
    ms_instance = nullptr;
    
    delete m_inChar->GetChar()->getCallbacks();
    delete m_inChar;
    m_inChar = nullptr;
    m_inService = nullptr;

    delete m_outChar->GetChar()->getCallbacks();
    delete m_outChar;
    m_outChar = nullptr;
    m_outService = nullptr;

    bool clearAll = true;
    NimBLEDevice::deinit(clearAll);

    m_init = false;
  }

  LOG("BLE server terminated\n\r");
}

void GJBLEServer::OnNewClient(BLEClient *client)
{
  GJ_DEBUGLOC("GJBLEServer::OnNewClient");
  {
    unsigned char data[256];

    for (int i = 0 ; i < 256 ; ++i)
      data[i] = (uint8_t)i;

    uint32_t crc = ComputeCrcDebug(data, 256);
    uint32_t crc2 = ComputeCrcDebug(data, 128);
    crc2 = ComputeCrcDebug(&data[128], 128, crc2);
    BLE_SER(4, "test crc:%u crc2:%u\n", crc, crc2);
  }

  m_clients.push_back(client);  
}

void GJBLEServer::InterpretCommand(const char *cmd, uint32_t len, BLEClient *client)
{
  GJ_DEBUGLOC("GJBLEServer::InterpretCommand");

  if (!strncmp(cmd, "gjcommand:", 10))
  {
    const char *subCmd = cmd + 10;
    if (!strncmp(subCmd, "blesendavailcommands", 20))
    {
      if (client)
          SendHelp(*client, 0, 0);
    }
    else
    {
      printf("command:%.*s\n", len, cmd);

      uint32_t cmdLen = len - (subCmd - cmd);
      GJString command((char*)subCmd, cmdLen);
      m_commands.push_back(command);
    }
  }
  else if (len >= 10 && !strncmp(cmd, "time:", 5))
  {
    const char *subCmd = cmd + 5;

    int32_t time = atoi(subCmd);

    SetUnixtime(time);
  }
  else if (len >= 3 && !strncmp(cmd, "ota", 3) && m_ota)
  {
    Vector<uint8_t> cmdString;
    cmdString.resize(len);
    memcpy(cmdString.data(), cmd, len);

    auto sendResponse = [=]()
    {
      GJString response;
      m_ota->HandleMessage((char*)cmdString.data(), len, response);

      if (response.length() && client)
      {
        client->Indicate(m_outChar, response.c_str());
      }
    };

    if (GJEventManager)
    {
      GJEventManager->Add(sendResponse);
    }
  }
}  

void GJBLEServer::SendHelp(BLEClient &client, uint16_t gatts_if, uint16_t descr_handle)
{
  Vector<GJString> commands;
  GetCommands(commands);

  GJString output;
  for (GJString &str : commands)
  {
    if (!output.length())
      output += "ws_availablecommands:";

    uint32_t len = str.length() + 1;

    if ((output.length() + len) > 95)
    {
      DataBuffer *dataBuffer = new DataBuffer(output.c_str());
      dataBuffer->m_clientId = client.GetConnId();
      m_dataBuffers.push_back(dataBuffer);

      output = "ws_availablecommands:";
    }

    output += str;
    output += ";";
  }

  if (output.length())
  {
    DataBuffer *dataBuffer = new DataBuffer(output.c_str());
    dataBuffer->m_clientId = client.GetConnId();
    m_dataBuffers.push_back(dataBuffer);
  }
}

GJString GJBLEServer::CleanupText(const char *text)
{
  GJ_PROFILE(GJBLEServer::CleanupText);

  GJ_DEBUGLOC("GJBLEServer::CleanupText");

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

bool GJBLEServer::Broadcast(const char *text)
{
  if (m_clients.empty() || !m_outChar || !m_outChar->GetChar()->getSubscribedCount())
    return false;

  GJ_DEBUGLOC("GJBLEServer::Broadcast");

  GJ_PROFILE(GJBLEServer::Broadcast);

  GJString string(text);

  string = CleanupText(string.c_str());

  bool written(false);

  uint32_t mtu = 500;//ESP_GATT_MAX_MTU_SIZE;

  for (BLEClient *client : m_clients)
  {
    mtu = std::min(mtu, client->GetMTU() - 5);
  }

  mtu = std::max<uint32_t>(mtu, 23);

  uint32_t offset = 0;
  uint32_t maxSize = mtu;
  while(offset < string.length())
  {
    uint32_t len = std::min<uint32_t>(string.length() - offset, maxSize);

    DataBuffer *dataBuffer = new DataBuffer(string.c_str() + offset, len);
    m_dataBuffers.push_back(dataBuffer);

    offset += len;
  }

  return written;
}

bool GJBLEServer::BroadcastData(DataBuffer *dataBuffer)
{
  GJ_PROFILE(GJBLEServer::BroadcastData);

  Clients::iterator it = m_clients.begin();
  for (; it != m_clients.end() ; )
  {
    BLEClient *client = *it;

    if (client->GetDataBufferId() < dataBuffer->m_id)
    {
      bool canIndicate = dataBuffer->m_clientId == 0xffff || dataBuffer->m_clientId == client->GetConnId(); 

      if (canIndicate)
      {
        bool ret = client->Indicate(m_outChar, dataBuffer->m_data.data(), dataBuffer->m_data.size());
        if (!ret)
          return false;
      }

      client->SetDataBufferId(dataBuffer->m_id);
    }

    ++it;
  }

  //also returns true when data buffer client id is not found
  return true;
}

void GJBLEServer::Update()
{
  GJ_PROFILE(GJBLEServer::Update);

  GJ_DEBUGLOC("BLE Update");

  //accumulate data buffer on no clients,
  //useful to send boot logs
  while(!m_dataBuffers.empty() && HasClient())
  {
    DataBuffer *dataBuffer = m_dataBuffers.front();

    bool ret = BroadcastData(dataBuffer);

    if (ret)
    {
      m_dataBuffers.erase(m_dataBuffers.begin());
      delete dataBuffer;
    }
    else
      break;
  }

  for (GJString const &command : m_commands)
  {
    ::InterpretCommand(command.c_str()); 
  }

  m_commands.clear();
}

#endif