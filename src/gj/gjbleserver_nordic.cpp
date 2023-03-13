#include "gjbleserver.h"

#ifdef GJ_NORDIC_BLE


#include "gj/esputils.h"
#include "gj/serial.h"
#include "gj/commands.h"
#include "gj/config.h"
#include "gj/datetime.h"
#include "gj/eventmanager.h"
#include "gj/gjota.h"
#include "gj/nrf51utils.h"

#include <cctype>  //isprint

#if defined(NRF_SDK12)
  #include <fstorage.h>
  #include "softdevice_handler.h"
#elif defined(NRF_SDK17)
  #include <nrf_fstorage.h>
  #include <nrf_sdh.h>
  #include <nrf_sdh_ble.h>
#endif



#include <boards.h> 

#include <ble.h>
#include <ble_hci.h>
#include <ble_srv_common.h>
#include <ble_advdata.h>
#include <ble_advertising.h>
#include <ble_conn_params.h>
#include <ble_hci.h>
#include <ble_advdata.h>
#include <ble_advertising.h>
#include <ble_conn_state.h>


#include <app_util_platform.h>

extern "C" {
  int       SEGGER_RTT_printf(unsigned BufferIndex, const char * sFormat, ...);
  unsigned  SEGGER_RTT_Write(unsigned BufferIndex, const void* pBuffer, unsigned NumBytes);
  int       SEGGER_RTT_vprintf(unsigned BufferIndex, const char * sFormat, va_list * pParamList);
}

#if defined(NRF_SDK17)
  BLE_ADVERTISING_DEF(m_advertising);                                             /**< Advertising module instance. */
#endif

uint8_t addl_adv_manuf_data[] = {0x06, 0xFF, 0x11, 0x12, 0x13, 0x13, 0x13};   

struct EventString
{
  const uint32_t code;
  const char * const m_cat;
  const char * const m_string;
};

#define NRF_EVENT_STRING(cat, id) {cat##_##id, #cat, #id}

//only keeping the ones used
const EventString g_eventStrings[] =
{
  //NRF_EVENT_STRING(BLE_EVT, TX_COMPLETE),   
  NRF_EVENT_STRING(BLE_EVT, USER_MEM_REQUEST),   
  //NRF_EVENT_STRING(BLE_EVT, USER_MEM_RELEASE),   

  NRF_EVENT_STRING(BLE_GAP_EVT, CONNECTED ),                       
  NRF_EVENT_STRING(BLE_GAP_EVT, DISCONNECTED),                     
  NRF_EVENT_STRING(BLE_GAP_EVT, CONN_PARAM_UPDATE),                
  //NRF_EVENT_STRING(BLE_GAP_EVT, SEC_PARAMS_REQUEST),               
  //NRF_EVENT_STRING(BLE_GAP_EVT, SEC_INFO_REQUEST),                 
  //NRF_EVENT_STRING(BLE_GAP_EVT, PASSKEY_DISPLAY),                  
  //NRF_EVENT_STRING(BLE_GAP_EVT, KEY_PRESSED),                      
  //NRF_EVENT_STRING(BLE_GAP_EVT, AUTH_KEY_REQUEST),                 
  //NRF_EVENT_STRING(BLE_GAP_EVT, LESC_DHKEY_REQUEST),               
  //NRF_EVENT_STRING(BLE_GAP_EVT, AUTH_STATUS),                      
  //NRF_EVENT_STRING(BLE_GAP_EVT, CONN_SEC_UPDATE),                  
  NRF_EVENT_STRING(BLE_GAP_EVT, TIMEOUT),                          
  //NRF_EVENT_STRING(BLE_GAP_EVT, RSSI_CHANGED),                     
  //NRF_EVENT_STRING(BLE_GAP_EVT, ADV_REPORT),                       
  //NRF_EVENT_STRING(BLE_GAP_EVT, SEC_REQUEST),                      
  NRF_EVENT_STRING(BLE_GAP_EVT, CONN_PARAM_UPDATE_REQUEST),        
  //NRF_EVENT_STRING(BLE_GAP_EVT, SCAN_REQ_REPORT),

  NRF_EVENT_STRING(BLE_GATTC_EVT, TIMEOUT                    ),
  
  NRF_EVENT_STRING(BLE_GATTS_EVT, WRITE                    ),
#if (NRF_SD_BLE_API_VERSION >= 3)
  NRF_EVENT_STRING(BLE_GATTS_EVT, EXCHANGE_MTU_REQUEST                    ),
#endif
  NRF_EVENT_STRING(BLE_GATTS_EVT, RW_AUTHORIZE_REQUEST),
  NRF_EVENT_STRING(BLE_GATTS_EVT, SYS_ATTR_MISSING),   
  NRF_EVENT_STRING(BLE_GATTS_EVT, HVC),                
  //NRF_EVENT_STRING(BLE_GATTS_EVT, SC_CONFIRM),         
  NRF_EVENT_STRING(BLE_GATTS_EVT, TIMEOUT)             
};

const char *GetNrfEventString(uint32_t id)
{
  for ( const EventString &s : g_eventStrings)
  {
    if (id == s.code)
      return s.m_string;
  }

  return "Unknown";
}

bool PrintOnError(uint32_t err)
{
  if (IsError(err)) 
  { 
    SEGGER_RTT_printf(0, "ERROR:%s(0x%x)\n\r", ErrorToName(err), err); 
    return true;
  }
  return false;
}

void ResetOnError(int32_t errCode)
{
  if (PrintOnError(errCode)) 
  {
    APP_ERROR_CHECK_BOOL(false);;
  }
}

#define GJ_CHECK_ERROR(userErr) ResetOnError(userErr)

DEFINE_CONFIG_INT32(ble.minint, ble_minint, 0);
DEFINE_CONFIG_INT32(ble.fastadv, ble_fastadv, 150);         //fast adv interval, milliseconds
DEFINE_CONFIG_INT32(ble.slowadv, ble_slowadv, 750);         //slow adv interval, milliseconds
DEFINE_CONFIG_INT32(ble.fasttimeout, ble_fasttimeout, 10);  //seconds

void SendRecentLog(tl::function_ref<void(const char *)> cb);

#define BLE_DBG
  
#if defined(BLE_DBG)
  DEFINE_CONFIG_INT32(ble.dbglvl, ble_dbglvl, 1);

  void BLE_EVT_SER(uint32_t level, const char * sFormat, ...)
  {
    if (GJ_CONF_INT32_VALUE(ble_dbglvl) < (level))
      return;

    va_list ParamList;

    va_start(ParamList, sFormat);
    SEGGER_RTT_vprintf(0, sFormat, &ParamList);
    va_end(ParamList);
  }

  bool CanPrintBLESerial(uint32_t level)
  {
    return GJ_CONF_INT32_VALUE(ble_dbglvl) >= (level);
  }


  #define BLE_EVT_SER_LEN(lvl, s, l) { if ( CanPrintBLESerial(lvl)) SEGGER_RTT_Write(0, s, l); }
  #define BLE_DBG_PRINT(lvl, ...) { if ( CanPrintBLESerial(lvl)) SER(__VA_ARGS__); }

#else
  #define BLE_EVT_SER(level, sFormat, ...)
  #define BLE_EVT_SER_LEN(lvl, s, l)
  #define BLE_DBG_PRINT(lvl, ...)
#endif

//1: error
//2: warn
//3: info
//4: debug
//5: verbose


void SerConnParam(const ble_gap_conn_params_t &conn_params)
{
  BLE_EVT_SER(3, "  min int:%d\r\n", conn_params.min_conn_interval);
  BLE_EVT_SER(3, "  max int:%d\r\n", conn_params.max_conn_interval);
  BLE_EVT_SER(3, "  slave lat:%d\r\n", conn_params.slave_latency);
  BLE_EVT_SER(3, "  conn timout:%d\r\n", conn_params.conn_sup_timeout);
}


#define GATTS_SERVICE_UUID_TEST_A   0x00FF
#define GATTS_CHAR_UUID_TEST_A      0xFF01
#define GATTS_DESCR_UUID_TEST_A     0x3333

#define GATTS_SERVICE_UUID_TEST_B   0x00EE
#define GATTS_CHAR_UUID_TEST_B      0xEE01
#define GATTS_DESCR_UUID_TEST_B     0x2222


#define CENTRAL_LINK_COUNT              0                                           /**< Number of central links used by the application. When changing this number remember to adjust the RAM settings*/
#define PERIPHERAL_LINK_COUNT           1                                           /**< Number of peripheral links used by the application. When changing this number remember to adjust the RAM settings*/

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(8, UNIT_1_25_MS)            /**< Minimum acceptable connection interval (0.1 seconds). */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(200, UNIT_1_25_MS)            /**< Maximum acceptable connection interval (0.2 second). */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(2000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds). */


#if defined(NRF_SDK12)
  #define APP_TIMER_PRESCALER             0                                           /**< Value of the RTC1 PRESCALER register. */
  #define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)  /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
  #define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER) /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
  #define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */
#elif defined(NRF_SDK17)
  #define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)  /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
  #define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000) /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
  #define MAX_CONN_PARAMS_UPDATE_COUNT    3          
#endif

static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        //err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        //APP_ERROR_CHECK(err_code);
    }
}

static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    //err_code = ble_conn_params_init(&cp_init);
    //APP_ERROR_CHECK(err_code);
}

uint32_t bleTestIndex = 0;
uint32_t bleTestIndexEnd = 2000;
uint32_t bleTestPacketSize = 23;

void TestBLE()
{
  GJBLEServer *bleServer = GJBLEServer::GetInstance();

  char next[32];
  const bool isStart = bleTestIndex == 0;
  const bool isLineEnd = !isStart && (bleTestIndex % 128) == 0;
  sprintf(next, "%s:%d%s,", 
          isStart ? "bleteststart" : "bletest", 
          bleTestIndex, 
          isLineEnd ? "\n\r" : "");

  const uint32_t nextLen = strlen(next);

  if (!AreTerminalsReady())
  {
    SER_LARGE(next);
    bleTestIndex++;
  }

  if (bleTestIndex >= bleTestIndexEnd)
    bleTestIndex = 0;
  else
    GJEventManager->Add(TestBLE);
}

void Command_testBLE(const char *command)
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
};

DEFINE_COMMAND_ARGS(bletest, Command_testBLE);

class GJBLEServer::BLEClient
{
  public:
    BLEClient(uint16_t conn_id, ble_gap_addr_t bda);
    ~BLEClient();

    bool Indicate(uint16_t handle, const char *string);
    bool Indicate(uint16_t handle, const uint8_t *data, uint32_t len);

    void Disconnect();

    uint16_t GetConnId() const;
    uint8_t const* GetBDA() const;

    bool Is(uint16_t conn_id, ble_gap_addr_t bda) const;
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
    ble_gap_addr_t m_bda;

    volatile bool m_congested = false;
    uint32_t m_mtu = 23;
    uint32_t m_interval = 50;
    volatile uint32_t m_lastSend = 0;
    uint32_t m_dataBufferId = 0;
    uint32_t m_subscription = 0;

    uint32_t m_totalSent = 0;
};

GJBLEServer::BLEClient::BLEClient(uint16_t conn_id, ble_gap_addr_t bda)
: m_conn_id(conn_id)
{
  memcpy(&m_bda, &bda, sizeof(ble_gap_addr_t));
}

GJBLEServer::BLEClient::~BLEClient() = default;

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

bool GJBLEServer::BLEClient::Is(uint16_t conn_id, ble_gap_addr_t bda) const
{
  return m_conn_id == conn_id &&
         !memcmp(&m_bda, &bda, sizeof(ble_gap_addr_t));
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
  return m_bda.addr;
}

bool GJBLEServer::BLEClient::Indicate(uint16_t handle, const char *string)
{ 
  return Indicate(handle, (uint8_t*)string, strlen(string));
}

bool GJBLEServer::BLEClient::Indicate(uint16_t handle, const uint8_t *data, uint32_t len)
{  
  //if (!IsSubscribed(c->GetIndex()))
  //  return true;

  uint32_t interval = GJ_CONF_INT32_VALUE(ble_minint);
  
  //it's actually faster to space calls by the client interval
  interval = Max(interval, GetInterval());

  //APP_TIMER_KEEPS_RTC_ACTIVE must be defined so that the RTC doesn't stop automatically
  //this might increase power usage

  uint32_t elapsed;
  //do
  //{
    uint64_t e = GetElapsedMillis() + 1;

    

    //if (e == 0)
    //  printf(" ");
     
    elapsed = e - m_lastSend;
  //} while (elapsed < interval);
  
  //BLE_EVT_SER(3, "Elapsed:%d interval:%d\n\r", (uint32_t)elapsed, (int)interval);

  m_lastSend = GetElapsedMillis();

  while (IsCongested())
  {
    Delay(20);
  }
  
  uint32_t err_code = NRF_SUCCESS;

  //ble_gatts_value_t value_set = {len , 0, (uint8_t*)data};
  //err_code = sd_ble_gatts_value_set(m_conn_id, handle, &value_set);
  //if (err_code != 0)
  {
  //  BLE_EVT_SER(1, "sd_ble_gatts_value_set indicate failed %d\n", err_code);
  //  return false;
  }

  uint16_t localLen = (uint16_t)len;         

  ble_gatts_hvx_params_t params = {};
  params.handle = handle;
  params.type = BLE_GATT_HVX_INDICATION;
  params.offset = 0;
  params.p_len = &localLen;
  params.p_data = (uint8_t*)data;

  err_code = sd_ble_gatts_hvx(m_conn_id, &params);
  if (err_code != 0)
  {
    if (err_code == NRF_ERROR_BUSY)
      return false; //retry it

    BLE_EVT_SER(1, "sd_ble_gatts_hvx failed %s(%x)\n", ErrorToName(err_code), err_code);

    return true;//skip
  }
  
  BLE_EVT_SER(5, "Sent data:'");
  BLE_EVT_SER_LEN(5, data, len);
  BLE_EVT_SER(5, "'\n\r");

  m_totalSent += len;

  BLE_EVT_SER(5, "Total sent:%d\n\r", m_totalSent);
  
  return true;
}

void GJBLEServer::BLEClient::Disconnect()
{
  sd_ble_gap_disconnect(m_conn_id, BLE_HCI_STATUS_CODE_SUCCESS);
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
  BLE_DBG_PRINT(4, "Client %d set subst mask 0x%x result=0x%x\n", m_conn_id, indexMask, m_subscription);
}

void GJBLEServer::BLEClient::ClearSubscription(uint32_t indexMask)
{
  m_subscription &= ~indexMask;
  BLE_DBG_PRINT(4, "Client %d cleared subst mask 0x%x result=0x%x\n", m_conn_id, indexMask, m_subscription);
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

GJBLEServer* GJBLEServer::GetInstance()
{
  return ms_instance;
}

GJBLEServer::GJBLEServer() = default;

bool GJBLEServer::HasClient() const
{
  return !m_clients.empty();
}

bool GJBLEServer::IsIdle() const
{
  return m_commands.empty() &&
         m_dataBuffers.empty();  
}

bool GJBLEServer::CanSendData() const
{
  return m_dataBuffers.size() < 2;
}

void GJBLEServer::RegisterTerminalHandler()
{
  if (m_terminalIndex == -1)
  {
    auto handleTerminal = [&](const char *text)
    {
      Broadcast(text);
    };

    auto handleTerminalReady = [&]() -> bool
    {
      return this->CanSendData();
    };
    
    m_terminalIndex = AddTerminalHandler(handleTerminal, handleTerminalReady);
  }
}

GJBLEServer::BLEClient* GJBLEServer::GetClient(uint16_t conn_id, ble_gap_addr_t bda) const
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

void GJBLEServer::DeleteClient(uint16_t conn)
{
  Clients::iterator it = m_clients.begin();
  for (; it != m_clients.end() ; )
  {
    BLEClient *client = *it;

    if (client->Is(conn))
    {
      if (!client->DecRef())
      {   
        it = m_clients.erase(it);
        delete client;
        BLE_DBG_PRINT(3, "Client deleted\n");
        continue;
      }
    }

    ++it;
  }
}

void GJBLEServer::HandleBLEEvent(ble_evt_t * p_ble_evt)
{
    uint32_t err_code = NRF_SUCCESS;
    
    const ble_gatts_evt_t *gattsEvt = &p_ble_evt->evt.gatts_evt;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_DISCONNECTED:
        {
          printf("Disconnected\n\r");
          const ble_gap_evt_disconnected_t &disconnected = p_ble_evt->evt.gap_evt.params.disconnected;

          BLE_EVT_SER(3, "reason:%d\r\n", disconnected.reason);

          DeleteClient(gattsEvt->conn_handle);
          //SetupAdvertising(addl_adv_manuf_data, sizeof(addl_adv_manuf_data));
          #if defined(NRF_SDK12)
            err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
            APP_ERROR_CHECK(err_code);
          #elif defined(NRF_SDK17)
            err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
            APP_ERROR_CHECK(err_code);
          #endif

          if (m_clients.empty())
          {
            m_commands.shrink_to_fit();
            m_dataBuffers.shrink_to_fit();
          }

          break;
        }
        case BLE_GAP_EVT_CONNECTED:
        {
          printf("connected\n\r");
          const ble_gap_evt_connected_t &connected = p_ble_evt->evt.gap_evt.params.connected;
          
          BLEClient *client = new BLEClient(p_ble_evt->evt.gap_evt.conn_handle, connected.peer_addr);
          OnNewClient(client);

          client->SetInterval(connected.conn_params.max_conn_interval);

          SerConnParam(connected.conn_params);
          break;
        }
        case BLE_GAP_EVT_CONN_PARAM_UPDATE:
        {
          const ble_gap_evt_conn_param_update_t &conn_param_update = p_ble_evt->evt.gap_evt.params.conn_param_update;

          BLEClient *client = GetClient(p_ble_evt->evt.gap_evt.conn_handle);
          if (client)
          {
            SER("Connection set to %d %d\n\r", conn_param_update.conn_params.min_conn_interval, conn_param_update.conn_params.max_conn_interval);
            client->SetInterval(conn_param_update.conn_params.max_conn_interval);
          }

          SerConnParam(conn_param_update.conn_params);
          break;
        }
        case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
        {
          const ble_gap_evt_conn_param_update_request_t &conn_param_update = p_ble_evt->evt.gap_evt.params.conn_param_update_request;
          const ble_gap_conn_params_t &conn_params = conn_param_update.conn_params;
          SER("Connection request to %d %d\n\r", conn_params.min_conn_interval, conn_params.max_conn_interval);
          SerConnParam(conn_param_update.conn_params);
          break;
        }
        case BLE_GATTC_EVT_TIMEOUT:
          // Disconnect on GATT Client timeout event.
          err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                            BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
          GJ_CHECK_ERROR(err_code);
          //SetupAdvertising(addl_adv_manuf_data, sizeof(addl_adv_manuf_data));
          break; // BLE_GATTC_EVT_TIMEOUT

        case BLE_GATTS_EVT_TIMEOUT:
          // Disconnect on GATT Server timeout event.
          err_code = sd_ble_gap_disconnect(gattsEvt->conn_handle,
                                            BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
          GJ_CHECK_ERROR(err_code);
          //SetupAdvertising(addl_adv_manuf_data, sizeof(addl_adv_manuf_data));
          break; // BLE_GATTS_EVT_TIMEOUT

        case BLE_EVT_USER_MEM_REQUEST:
          err_code = sd_ble_user_mem_reply(p_ble_evt->evt.gattc_evt.conn_handle, NULL);
          GJ_CHECK_ERROR(err_code);
          break; // BLE_EVT_USER_MEM_REQUEST
        case BLE_GATTS_EVT_WRITE:
        {
          const ble_gatts_evt_write_t &writeEvt = gattsEvt->params.write;
          /*
          BLE_EVT_SER(4, "Write event\n\r");
          BLE_EVT_SER(4, " conn:0x%x\n\r", gattsEvt->conn_handle);
          BLE_EVT_SER(4, " uuid:0x%x\n\r", writeEvt.uuid.uuid);
          BLE_EVT_SER(4, " op:%d\n\r", writeEvt.op);
          BLE_EVT_SER(4, " auth:%d\n\r", writeEvt.auth_required);
          BLE_EVT_SER(4, " len:%d\n\r", writeEvt.len);
          BLE_EVT_SER(4, " data:");
          BLE_EVT_SER_LEN(4, writeEvt.data, writeEvt.len);
          BLE_EVT_SER(4, "\n\r");*/

          if (writeEvt.uuid.uuid == GATTS_CHAR_UUID_TEST_B)
          {
            BLEClient *client = GetClient(gattsEvt->conn_handle);
            Command *command = new Command({client, {(char*)writeEvt.data, writeEvt.len}});
            GJScopedLock lock(m_lock);
            m_commands.push_back(command);
          }

          break;
        }
        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
        {
            const ble_gatts_evt_rw_authorize_request_t  &req = gattsEvt->params.authorize_request;
            ble_gatts_rw_authorize_reply_params_t auth_reply = {};
  
            uint8_t valueBuffer[32];
            ble_gatts_value_t currentValue = {};
            
            if (req.type != BLE_GATTS_AUTHORIZE_TYPE_INVALID)
            {
                if ((req.request.write.op == BLE_GATTS_OP_PREP_WRITE_REQ)     ||
                    (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW) ||
                    (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL))
                {
                    const ble_gatts_evt_write_t &writeReq = req.request.write;

                    auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
                    auth_reply.params.write.gatt_status = 	BLE_GATT_STATUS_SUCCESS;
                    
                    /*
                    BLE_EVT_SER(4, "RW_AUTH req\n\r");
                    BLE_EVT_SER(4, "  attr handle:%d\n\r", writeReq.handle);
                    BLE_EVT_SER(4, "  type:%d\n\r", req.type);
                    BLE_EVT_SER(4, "  op:%d\n\r", writeReq.op);
                    BLE_EVT_SER(4, "  off:%d\n\r", writeReq.offset);
                    BLE_EVT_SER(4, "  len:%d\n\r", writeReq.len);*/

                    if (writeReq.op == BLE_GATTS_OP_PREP_WRITE_REQ) 
                    {
                      auth_reply.params.write.update = 	1;
                      auth_reply.params.write.offset = 	writeReq.offset;
                      auth_reply.params.write.len = 	writeReq.len;
                      auth_reply.params.write.p_data = writeReq.data;
                      memcpy(&m_attrValue[writeReq.offset], auth_reply.params.write.p_data, writeReq.len);

                      m_attrValueLength = Max<uint16_t>(m_attrValueLength, writeReq.offset + writeReq.len);
                    }
                    else if (writeReq.op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW)
                    {
                      ble_gatts_value_t newValue = {m_attrValueLength, 0, m_attrValue};
                      err_code = sd_ble_gatts_value_set(gattsEvt->conn_handle, m_char_handle[1].value_handle, &newValue);
                      GJ_CHECK_ERROR(err_code);

                      //if (writeReq.uuid.uuid == GATTS_CHAR_UUID_TEST_B)
                      {
                        BLEClient *client = GetClient(gattsEvt->conn_handle);
                        Command *command = new Command({client, {(char*)m_attrValue, m_attrValueLength}});
                        GJScopedLock lock(m_lock);
                        m_commands.push_back(command);
                      }

                      m_attrValueLength = 0;
                    }
                    
                    err_code = sd_ble_gatts_rw_authorize_reply(gattsEvt->conn_handle,
                                                               &auth_reply);
                    GJ_CHECK_ERROR(err_code);
                  }
                else
                {
                  m_attrValueLength = 0;
                  BLE_EVT_SER(1, "op err %d\n\r", req.request.write.op);
                }
            }
            else
            {
              m_attrValueLength = 0;
              BLE_EVT_SER(1, "req type err %d\n\r", req.type);
            }


        } break; // BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST
        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
          // No system attributes have been stored.
          err_code = sd_ble_gatts_sys_attr_set(gattsEvt->conn_handle, NULL, 0, 0);
          GJ_CHECK_ERROR(err_code);
          break; // BLE_GATTS_EVT_SYS_ATTR_MISSING
#if (NRF_SD_BLE_API_VERSION >= 3)
        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
        {
            const ble_gatts_evt_exchange_mtu_request_t  &exchange_mtu_request = gattsEvt->params.exchange_mtu_request;

            BLE_EVT_SER(1, "Exchange client mtu=%d\n\r", exchange_mtu_request.client_rx_mtu);

            err_code = sd_ble_gatts_exchange_mtu_reply(gattsEvt->conn_handle,
                                                       GATT_MTU_SIZE_DEFAULT);
            GJ_CHECK_ERROR(err_code);
            break; // BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST
        }
#endif

        default:
            // No implementation needed.
            break;
    }
}

void GJBLEServer::OnBLEEvent(ble_evt_t * p_ble_evt)
{
  BLE_EVT_SER(3, "%8d ====BLE EVENT %s(%d)\n\r", (uint32_t)GetElapsedMillis(), GetNrfEventString(p_ble_evt->header.evt_id), p_ble_evt->header.evt_id);
#if defined(NRF_SDK12)
  ble_conn_state_on_ble_evt(p_ble_evt);
#endif

#if NRF_MODULE_ENABLED(PEER_MANAGER)
  pm_on_ble_evt(p_ble_evt);
#endif
#if defined(NRF_SDK12)
  ble_conn_params_on_ble_evt(p_ble_evt);
#endif
  if(ms_instance)
    ms_instance->HandleBLEEvent(p_ble_evt);    

#if defined(NRF_SDK12)
  if (p_ble_evt->header.evt_id != BLE_GAP_EVT_DISCONNECTED)
    ble_advertising_on_ble_evt(p_ble_evt);
#endif
}

void GJBLEServer::OnNewClient(BLEClient *client)
{
  GJ_DEBUGLOC("GJBLEServer::OnNewClient");
  #ifdef ESP32
  {
    unsigned char data[256];

    for (int i = 0 ; i < 256 ; ++i)
      data[i] = (uint8_t)i;

    uint32_t crc = ComputeCrcDebug(data, 256);
    uint32_t crc2 = ComputeCrcDebug(data, 128);
    crc2 = ComputeCrcDebug(&data[128], 128, crc2);
    BLE_DBG_PRINT(4, "test crc:%u crc2:%u\n", crc, crc2);
  }
  #endif
  m_clients.push_back(client);  
}

void GJBLEServer::sys_evt_dispatch(uint32_t sys_evt)
{
#if defined(NRF_SDK12)
    // Dispatch the system event to the fstorage module, where it will be
    // dispatched to the Flash Data Storage (FDS) module.
    fs_sys_event_handler(sys_evt);


    // Dispatch to the Advertising module last, since it will check if there are any
    // pending flash operations in fstorage. Let fstorage process system events first,
    // so that it can report correctly to the Advertising module.
    ble_advertising_on_sys_evt(sys_evt);
#endif
}

void GJBLEServer::CreateChar(uint16_t service, uint16_t uuid, ble_gatts_char_handles_t &destChar)
{
  ble_gatts_char_md_t char_md;
  ble_gatts_attr_md_t cccd_md;
  ble_gatts_attr_t    attr_char_value;
  ble_uuid_t          ble_uuid;
  ble_gatts_attr_md_t attr_md;
  //ble_bps_meas_t      initial_bpm;
  //uint8_t             encoded_bpm[MAX_BPM_LEN];

  memset(&cccd_md, 0, sizeof(cccd_md));

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
  cccd_md.vlen = 1;
  cccd_md.vloc = BLE_GATTS_VLOC_STACK;
  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);

  memset(&char_md, 0, sizeof(char_md));


  char_md.char_props.write_wo_resp = 1;
  //char_md.char_props.read = 1;
  char_md.char_props.write = 1;
  //char_md.char_props.notify = 1;
  char_md.char_props.indicate = 1;
  char_md.char_user_desc_max_size = 256;
  char_md.char_user_desc_size = 256;
  char_md.p_char_user_desc    = NULL;
  char_md.p_char_pf           = NULL;
  char_md.p_user_desc_md      = NULL;
  char_md.p_cccd_md           = &cccd_md;
  char_md.p_sccd_md           = &cccd_md;

  BLE_UUID_BLE_ASSIGN(ble_uuid, uuid);
  ble_uuid.type = BLE_UUID_TYPE_BLE;


  memset(&attr_md, 0, sizeof(attr_md));

  attr_md.vloc       = BLE_GATTS_VLOC_STACK;
  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
  attr_md.rd_auth    = 0;
  attr_md.wr_auth    = 0;
  attr_md.vlen       = 1;

  memset(&attr_char_value, 0, sizeof(attr_char_value));
  //memset(&initial_bpm, 0, sizeof(initial_bpm));

  attr_char_value.p_uuid       = &ble_uuid;
  attr_char_value.p_attr_md    = &attr_md;
  attr_char_value.init_len     = 0;
  attr_char_value.init_offs    = 0;
  attr_char_value.max_len      = 256;
  //attr_char_value.p_value      = (uint8_t*)malloc(256);

  uint32_t err_code;

  err_code = sd_ble_gatts_characteristic_add(service,
                                        &char_md,
                                        &attr_char_value,
                                        &destChar);

  GJ_CHECK_ERROR(err_code);
}



static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            BLE_EVT_SER(3,"Fast advertising\r\n");
            //err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            //GJ_CHECK_ERROR(err_code);
            break;

        case BLE_ADV_EVT_SLOW:
            BLE_EVT_SER(3,"Slow advertising\r\n");
            //err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            //GJ_CHECK_ERROR(err_code);
            break;

        case BLE_ADV_EVT_IDLE:
            BLE_EVT_SER(3,"Idle advertising\r\n");
            #if defined(NRF_SDK12)
              ble_advertising_start(BLE_ADV_MODE_SLOW);
            #elif defined(NRF_SDK17)
              ble_advertising_start(&m_advertising, BLE_ADV_MODE_SLOW);
            #endif
            break;

        default:
            break;
    }
}

void GJBLEServer::SetAdvManufData(const void *data, uint32_t size)
{
  if (!m_init)
    return;
    
  uint8_t localData[31] = {(uint8_t)size + 1, 0xFF};
  memcpy(&localData[2], data, size);

  SetupAdvertising((uint8_t*)localData, size ? size + 2 : 0);
}

void GJBLEServer::SetupAdvertising(const uint8_t* manufUserData, uint32_t manufSize)
{
  int32_t err_code;

  BLE_EVT_SER(3, "SetupAdvertising\n\r");

#if defined(NRF_SDK12)

  ble_advdata_t          advdata;
  ble_adv_modes_config_t options;
  ble_advdata_manuf_data_t manufData;

  manufData.company_identifier = 0xffff;
  manufData.data.size = manufSize;
  manufData.data.p_data = (uint8_t*)manufUserData;
  

  // Build advertising data struct to pass into @ref ble_advertising_init.
  memset(&advdata, 0, sizeof(advdata));

  advdata.name_type               = BLE_ADVDATA_FULL_NAME;
  advdata.include_appearance      = false;
  advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
  advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
  advdata.uuids_complete.p_uuids  = m_adv_uuids;
  advdata.p_manuf_specific_data   = &manufData;

  memset(&options, 0, sizeof(options));
  options.ble_adv_fast_enabled  = true;
  options.ble_adv_fast_interval = GJ_CONF_INT32_VALUE(ble_fastadv) * 1000 / 625;
  options.ble_adv_fast_timeout  = GJ_CONF_INT32_VALUE(ble_fasttimeout);
  options.ble_adv_slow_enabled  = true;
  options.ble_adv_slow_interval = GJ_CONF_INT32_VALUE(ble_slowadv) * 1000 / 625;
  options.ble_adv_slow_timeout  = 60;

  //uint8_t p_encoded_data[48];
  //uint16_t p_len = 48;
  //adv_data_encode(&advdata, p_encoded_data, &p_len);

  err_code = ble_advertising_init(&advdata, NULL, &options, on_adv_evt, NULL);
  GJ_CHECK_ERROR(err_code);
#elif defined(NRF_SDK17)

  ble_advertising_init_t init;  
  memset(&init, 0, sizeof(init));

  ble_advdata_manuf_data_t manufData;
  memset(&manufData, 0, sizeof(manufData));

  manufData.company_identifier = 0xffff;
  manufData.data.size = manufSize;
  manufData.data.p_data = (uint8_t*)manufUserData;

  init.advdata.name_type               = BLE_ADVDATA_FULL_NAME;
  init.advdata.include_appearance      = false;
  init.advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
  init.advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
  init.advdata.uuids_complete.p_uuids  = m_adv_uuids;
  init.advdata.p_manuf_specific_data   = &manufData;

  init.config.ble_adv_fast_enabled  = true;
  init.config.ble_adv_fast_interval = GJ_CONF_INT32_VALUE(ble_fastadv) * 1000 / 625;
  init.config.ble_adv_fast_timeout  = GJ_CONF_INT32_VALUE(ble_fasttimeout);
  init.config.ble_adv_slow_enabled  = true;
  init.config.ble_adv_slow_interval = GJ_CONF_INT32_VALUE(ble_slowadv) * 1000 / 625;
  init.config.ble_adv_slow_timeout  = 60;

  init.evt_handler = on_adv_evt;

  //uint8_t p_encoded_data[48];
  //uint16_t p_len = 48;
  //adv_data_encode(&advdata, p_encoded_data, &p_len);

  err_code = ble_advertising_init(&m_advertising, &init);
  GJ_CHECK_ERROR(err_code);
  #endif
}

bool GJBLEServer::Init(const char *hostname, GJOTA *ota)
{
  m_hostname = hostname;
  m_ota = ota;

  return Init();
}

void GJBLEServer::Command_ble(const char *command)
{
  static constexpr const char * const s_argsName[] = {
    "on",
    "off",
    "dbg",
    "int"
  };

  static void (*const s_argsFuncs[])(const CommandInfo &commandInfo){
    Command_bleon,
    Command_bleoff,
    Command_bledbg,
    Command_bleint
    };

  SubCommands subCommands = {4, s_argsName, s_argsFuncs};

  SubCommandForwarder(command, subCommands);
}

void GJBLEServer::Command_bledbg(const CommandInfo &info)
{
  const uint32_t clientCount = GetInstance()->m_clients.size();
  const uint32_t dataCount = GetInstance()->m_dataBuffers.size();
  const uint32_t dataCap = GetInstance()->m_dataBuffers.capacity();
  const uint32_t cmdCount = GetInstance()->m_commands.size();
  const uint32_t cmdCap = GetInstance()->m_commands.capacity();
  SER("BLE:\n\r");      
  SER("  hostname:%s\n\r", GetInstance()->m_hostname.c_str());
  SER("  clients:%d\n\r", clientCount);
  for (BLEClient const *client : GetInstance()->m_clients)
  {
    SER("    conn:%02d mtu:%03d int:%02dms bda:%02x:%02x:%02x:%02x:%02x:%02x ref:%02d\n\r", 
      client->GetConnId(),
      client->GetMTU(),
      (uint32_t)(client->GetInterval() * 1.25f),
      client->GetBDA()[0], client->GetBDA()[1], client->GetBDA()[2], client->GetBDA()[3], client->GetBDA()[4], client->GetBDA()[5],
      client->GetRef()
      );
  }
  SER("  data buffers:\n\r");
  SER("    Pending:%d\n\r", dataCount);
  SER("    Capacity:%d\n\r", dataCap);
  SER("  Commands:\n\r");
  SER("    Pending:%d\n\r", cmdCount);
  SER("    Capacity:%d\n\r", cmdCap);
  //SER("  Out characteristic sub count:%d\n\r", m_outChar->GetChar()->getSubscribedCount());
}

void GJBLEServer::Command_bleon(const CommandInfo &commandInfo)
{
  if (ms_instance)
    ms_instance->Init();
}

void GJBLEServer::Command_bleoff(const CommandInfo &commandInfo)
{
  if (ms_instance)
    ms_instance->Term();
}

void GJBLEServer::Command_bleint(const CommandInfo &info)
{
  if (ms_instance)
  {
    BLEClient *client = ms_instance->m_clients.front();

    ble_gap_conn_params_t gap_conn_params;

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = 50;
    gap_conn_params.max_conn_interval = 100;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    if (info.m_argCount >= 1)
      gap_conn_params.min_conn_interval = strtol(info.m_args[0].data(), nullptr, 0);

    if (info.m_argCount >= 2)
      gap_conn_params.max_conn_interval = strtol(info.m_args[1].data(), nullptr, 0);

    int32_t err_code = sd_ble_gap_conn_param_update(client->GetConnId(), &gap_conn_params);
    if (err_code != NRF_SUCCESS)
    {
      SER("Cannot set connection params, err %s(%d)\n\r", ErrorToName(err_code), err_code);
    }
  }
}

DEFINE_COMMAND_ARGS(ble, GJBLEServer::Command_ble);

#if defined(NRF_SDK12)
extern sys_evt_handler_t g_sys_evt_handler;
#endif

void GJBLEServer::OnExit()
{
  if (ms_instance)
        ms_instance->Term();
}

bool GJBLEServer::Init()
{
  RegisterTerminalHandler();

  REFERENCE_COMMAND(ble);
  
  if (!m_oneTimeInit)
  {
    m_oneTimeInit = true;

    /*auto exitCB = [this]()
    {
      if (ms_instance)
        ms_instance->Term();
    };*/

    RegisterExitCallback(OnExit);
/*
    auto broadWSLog = [=](const char *buffer)
    {
      this->Broadcast(buffer);
    };

    auto sendWBLog = [=]()
    {
      //SendRecentLog(broadWSLog);
    };
*/
    //REFERENCE_COMMAND("blesendlog", sendWBLog);
  }


  if (!m_init)
  {
    uint32_t err_code;

    InitSoftDevice();
    
    ble_enable_params_t ble_enable_params;
    err_code = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
                                                    PERIPHERAL_LINK_COUNT,
                                                    &ble_enable_params);
    GJ_CHECK_ERROR(err_code);

    // Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT, PERIPHERAL_LINK_COUNT);

    // Enable BLE stack.
#if (NRF_SD_BLE_API_VERSION == 3) && false
    ble_enable_params.gatt_enable_params.att_mtu = NRF_BLE_MAX_MTU_SIZE;
#endif
    err_code = softdevice_enable(&ble_enable_params);
    GJ_CHECK_ERROR(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(OnBLEEvent);
    GJ_CHECK_ERROR(err_code);

#if defined(NRF_SDK12)
    // Overwrite existing sys handler if any
    g_sys_evt_handler = sys_evt_dispatch;
    err_code = softdevice_sys_evt_handler_set(g_sys_evt_handler);
    GJ_CHECK_ERROR(err_code);
#endif

    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)m_hostname.c_str(),
                                          m_hostname.size());
    GJ_CHECK_ERROR(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    GJ_CHECK_ERROR(err_code);

    m_adv_uuids[0] = {GATTS_SERVICE_UUID_TEST_A, BLE_UUID_TYPE_BLE};
    m_adv_uuids[1] = {GATTS_SERVICE_UUID_TEST_B, BLE_UUID_TYPE_BLE}; 

    //uint8_t addl_adv_manuf_data[] = {0x02, 0x01, 0x06, 0x02, 0x0a, 0x03, 0x08, 0xFF, 0x11, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05};   
    
    //SetAdvManufData(addl_adv_manuf_data, sizeof(addl_adv_manuf_data));

    
    
    SetupAdvertising(addl_adv_manuf_data, sizeof(addl_adv_manuf_data));
    SetAdvManufData(addl_adv_manuf_data, sizeof(addl_adv_manuf_data));

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &m_adv_uuids[0], &m_service_handle[0]);
    GJ_CHECK_ERROR(err_code);

    CreateChar(m_service_handle[0], GATTS_CHAR_UUID_TEST_A, m_char_handle[0]);

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &m_adv_uuids[1], &m_service_handle[1]);
    GJ_CHECK_ERROR(err_code);
   
    CreateChar(m_service_handle[1], GATTS_CHAR_UUID_TEST_B, m_char_handle[1]);

    conn_params_init();

    
#if defined(NRF_SDK12)
    err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
#elif defined(NRF_SDK17)
    err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
#endif

    //uint8_t addl_adv_manuf_data[] = {0x04, 0xFF, 0x11, 0x12, 0x13};   
    //SetAdvManufData(addl_adv_manuf_data, sizeof(addl_adv_manuf_data));

    ms_instance = this;
    
    m_init = true;
  }

  BLE_DBG_PRINT(3, "BLE Server initialized\n");

  ble_gap_addr_t localAddr;
  #if defined(S130)
    sd_ble_gap_address_get(&localAddr);
  #elif defined(S132)
    sd_ble_gap_addr_get(&localAddr);
  #endif

  SER("BLE addr %02X:%02X:%02X:%02X:%02X:%02X\n\r", localAddr.addr[0], localAddr.addr[1], localAddr.addr[2],
    localAddr.addr[3], localAddr.addr[4], localAddr.addr[5]);

  return m_init;
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
    client->Disconnect();
    delete client;
  }
  
  while(!m_dataBuffers.empty())
  {
    DataBuffer *dataBuffer = m_dataBuffers.front();
    m_dataBuffers.erase(m_dataBuffers.begin());
    delete dataBuffer;
  }

  m_commands.shrink_to_fit();

  if (m_init)
  {    
    sd_ble_gap_adv_stop();

  
    softdevice_handler_sd_disable();

    m_init = false;
  }

  LOG("BLE server terminated\n\r");
}

void GJBLEServer::InterpretCommand(const char *cmd, uint32_t len, BLEClient *client)
{
  GJ_DEBUGLOC("GJBLEServer::InterpretCommand");

  if (!strncmp(cmd, "gjcommand:", 10))
  {
    const char *subCmd = cmd + 10;
    if (!strncmp(subCmd, "blesendavailcommands", 20))
    {
      for (BLEClient *client2 : m_clients)
        SendHelp(*client2, 0, 0);     
    }
    else
    {
      uint32_t cmdLen = len - (subCmd - cmd);
      ::InterpretCommand((char*)subCmd);
    }
  }
  else if (len >= 10 && !strncmp(cmd, "time:", 5))
  {
    const char *subCmd = cmd + 5;
    int32_t time = atoi(subCmd);
    SetUnixtime(time);
  }
  else if (len >= 3 && !strncmp(cmd, "ota", 3))
  {
    if (!m_ota)
    {
      BLE_DBG_PRINT(1, "OTA ERROR:ota not setup in ble server");
    }
    else if (!client)
    {
      BLE_DBG_PRINT(1, "OTA ERROR:client is null");
    }
    else if (!GJEventManager)
    {
      BLE_DBG_PRINT(1, "OTA ERROR:GJEventManager is not setup");
    }
    else
    {
      Vector<uint8_t> cmdString;
      cmdString.resize(len+1);
      memcpy(cmdString.data(), cmd, len);
      cmdString.data()[len] = 0;//GJOTA requires null terminated strings

      GJString response;
      m_ota->HandleMessage((char*)cmdString.data(), len, response);

      if (response.length())
      {
        Broadcast(response.c_str());
        //DataBuffer *dataBuffer = new DataBuffer(response.c_str());
        //dataBuffer->m_clientId = client->GetConnId();
        //AddDataBuffer(dataBuffer);
      }
    }
  }
}  

class CommandIterator
{
  public:
  CommandIterator();
    uint16_t Get() const;
    bool End() const;
    void Next();

  private:

  uint16_t m_index = 0;
  Vector<uint16_t> m_commands;
};

CommandIterator::CommandIterator()
{
    GetCommandIds(m_commands);
}

uint16_t CommandIterator::Get() const
{
  if (m_index < m_commands.size())
    return m_commands[m_index];

  return 0;
}

bool CommandIterator::End() const
{
  return m_index >= m_commands.size();
}


void CommandIterator::Next()
{
  m_index++;
}


struct GJBLEServer::SendHelpCommand
{
  uint16_t offset = 0xffff; 
  BLEClient *client = nullptr;

  CommandIterator m_commands;
};

void GJBLEServer::SendNextHelpCommand(SendHelpCommand *helpCommand)
{
  if (!GetInstance()->CanSendData())
  {
    //wait for server to send pending data buffers
    EventManager::Function func = std::bind(SendNextHelpCommand, helpCommand);
    GJEventManager->Add(func);
    return;
  }

  DataBuffer *dataBuffer = nullptr;
  uint16_t clientId = helpCommand->client->GetConnId();

  if (helpCommand->offset == 0xffff)
  {
    dataBuffer = new DataBuffer("availcmds2:");
    helpCommand->offset = 0;
  }
  else if (!helpCommand->m_commands.End())
  {
    const uint16_t id = helpCommand->m_commands.Get();

    const GJString desc = DescribeCommand(id);
    const uint32_t len = desc.size();

    if (helpCommand->offset < len)
    {
      char cmdBuffer[32];

      if (helpCommand->offset == 0)
        strcpy(cmdBuffer, "sbb:");
      else
        strcpy(cmdBuffer, "sb:");

      const uint32_t batch = Min<uint32_t>(16, len);
      
      strncat(cmdBuffer, desc.c_str() + helpCommand->offset, batch);
      helpCommand->offset += batch;
      dataBuffer = new DataBuffer(cmdBuffer);
    }
    else
    {
      dataBuffer = new DataBuffer("sbe:");
      
      //goto next command
      helpCommand->offset = 0;
      helpCommand->m_commands.Next();
    }
  }
  else
  {
    delete helpCommand;
    helpCommand = nullptr;
    BLE_EVT_SER(3, "ram:%d\n\r", GetAvailableRam());
    dataBuffer = new DataBuffer("endavailcmds2:");
  }

  if (dataBuffer)
  {
    dataBuffer->m_clientId = clientId;
    ms_instance->AddDataBuffer(dataBuffer);
  }

  //reschedule event
  if (helpCommand)
  {
    EventManager::Function func = std::bind(SendNextHelpCommand, helpCommand);
    GJEventManager->Add(func);
  }
};
  
uint32_t GetPartitionIndex();

void GJBLEServer::SendHelp(BLEClient &client, uint16_t gatts_if, uint16_t descr_handle)
{
  LogRam();

  SER("Running partition %d\n\r", GetPartitionIndex());

  SendHelpCommand *helpCommand = new SendHelpCommand;

  helpCommand->client = &client;
  EventManager::Function func = std::bind(SendNextHelpCommand, helpCommand);
  GJEventManager->Add(func);
}

GJString GJBLEServer::CleanupText(const char *text)
{
  GJ_PROFILE(GJBLEServer::CleanupText);

  GJ_DEBUGLOC("GJBLEServer::CleanupText");

  GJString feedStorage;
  text = RemoveNewLineLineFeed(text, feedStorage);

  GJString output = text;

  for (int i = 0 ; i < output.size() ; ++i)
  {
    char c = output[i];

    if (!isprint(c) && c != '\n' && c != '\r')
      output[i] = '.';
  }

  return output;
}

void GJBLEServer::AddDataBuffer(DataBuffer *db)
{
  //GJ_DBG_PRINT("bef add DB : %d s:%d c:%d\n\r", GetAvailableRam(), m_dataBuffers.size(), m_dataBuffers.capacity());
  GJScopedLock lock(m_lock);
  m_dataBuffers.push_back(db);
  //GJ_DBG_PRINT("after add DB : %d s:%d c:%d\n\r", GetAvailableRam(), m_dataBuffers.size(), m_dataBuffers.capacity());
}

bool GJBLEServer::Broadcast(const char *text)
{
  //don't buffer pre connection broadcasts, takes too much ram
  if (m_clients.empty())
    return false;

  GJ_DEBUGLOC("GJBLEServer::Broadcast");

  GJ_PROFILE(GJBLEServer::Broadcast);

  //GJString string(text);

  //string = CleanupText(string.c_str());

  uint32_t stringSize = strlen(text);

  bool written(false);

  uint32_t mtu = 18;

  for (BLEClient *client : m_clients)
  {
    mtu = Min(mtu, client->GetMTU() - 5);
  }

  mtu = Max<uint32_t>(mtu, 16);

  uint32_t offset = 0;
  uint32_t maxSize = mtu;
  while(offset < stringSize)
  {
    uint32_t len = Min<uint32_t>(stringSize - offset, maxSize);

    char lastChar = text[offset + len - 1];
    char nextChar = text[offset + len];

    if (lastChar == '\n' && nextChar == '\r' && len)
      len--;

    DataBuffer *dataBuffer = new DataBuffer(text + offset, len);

    ReplaceLFCR((char*)dataBuffer->m_data.data(), len, ' ');
    ReplaceNonPrint((char*)dataBuffer->m_data.data(), len, ' ');

    AddDataBuffer(dataBuffer);

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
        //GJ_DBG_PRINT("Before BD \n\r");
        //GJ_DBG_PRINT("dataBuffer:'%s'\n\r", dataBuffer->m_data.data());
        bool ret = client->Indicate(m_char_handle[1].value_handle, dataBuffer->m_data.data(), dataBuffer->m_data.size());
        //GJ_DBG_PRINT("after BD \n\r");
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

void GJBLEServer::StopAdvertising()
{
  int32_t err_code = sd_ble_gap_adv_stop();
  APP_ERROR_CHECK(err_code);
}

void GJBLEServer::RestartAdvertising()
{
  if (!HasClient())
  {
    //SetupAdvertising(addl_adv_manuf_data, sizeof(addl_adv_manuf_data));
    int32_t err_code = ble_advertising_start(BLE_ADV_MODE_SLOW);
    APP_ERROR_CHECK(err_code);
  }
}

void GJBLEServer::Update()
{
  GJ_PROFILE(GJBLEServer::Update);

  GJ_DEBUGLOC("BLE Update");

  //the BLE events are not processed during the IRQ but deferred to here
  intern_softdevice_events_execute();

  if (HasClient())
  {
    //printf(" ");
  }

  while(!m_dataBuffers.empty())
  {
    DataBuffer *dataBuffer;

    {
      GJScopedLock lock(m_lock);

      if (m_dataBuffers.empty())
        break;

      dataBuffer = m_dataBuffers.front();
    }

    bool ret = BroadcastData(dataBuffer);
    if (ret)
    {
      delete dataBuffer;

      GJScopedLock lock(m_lock);
      m_dataBuffers.erase(m_dataBuffers.begin());
      
      if (m_dataBuffers.empty())
        m_dataBuffers.shrink_to_fit();
    }
    else
      break;
  }

  while(!m_commands.empty())
  {
    Command *command;

    {
      GJScopedLock lock(m_lock);
      if (m_commands.empty())
        break;

      command = m_commands.front();
      m_commands.erase(m_commands.begin());
      if (m_commands.empty())
        m_commands.shrink_to_fit();
    }

    InterpretCommand(command->m_string.c_str(),command->m_string.size(), command->m_client); 
    delete command;
  }
}

#endif //GJ_NORDIC_BLE