#include "gjbleserver.h"

#ifdef GJ_BLUEDROID

#include "gj/esputils.h"
#include "gj/serial.h"

#include "gj/commands.h"
#include "gj/config.h"
#include "gj/eventmanager.h"
#include "gj/datetime.h"
#include "gj/gjota.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_bt.h"


#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gattc_api.h"
#include "esp_gatt_common_api.h"
#include <esp32-hal-bt.h>

DEFINE_CONFIG_INT32(ble.dbglvl, ble_dbglvl, 1); //default level 1(error)

#define BS_LOG_ON_ERR(r, ...) if (r < 0) {LOG(__VA_ARGS__);}

void SendRecentLog(tl::function_ref<void(const char *)> cb);

#define BLE_SER(lvl, ...) { if ( GJ_CONF_INT32_VALUE(ble.dbglvl) >= lvl) printf(__VA_ARGS__); }

bool IsError(esp_err_t err) { return err != ESP_OK; }

#define BLE_CALL(call, ...) { auto ret = call; if (IsError(ret)) { BLE_SER(1, "'%s' failed:'%s'\n", #call, esp_err_to_name(ret)); __VA_ARGS__; } } 
//1: error
//2: warn
//3: info
//4: debug
//5: verbose


class GJBLEServer::BLEClient
{
  public:
    BLEClient(uint16_t conn_id, esp_bd_addr_t bda);
    ~BLEClient();

    bool Indicate(uint16_t gatts_if, uint16_t descr_handle, const char *string);

    
    uint16_t GetConnId() const;
    uint8_t const* GetBDA() const;

    bool Is(uint16_t conn_id, esp_bd_addr_t bda) const;
    bool Is(uint16_t conn_id) const;

    uint32_t GetRef() const;
    void IncRef();
    uint32_t DecRef();

    int status() const;

    void SetCongested(bool);
    bool IsCongested() const;

    void SetMTU(uint32_t mtu);
    uint32_t GetMTU() const;
  private:
    uint32_t m_ref = 1;
    uint16_t m_conn_id;
    esp_bd_addr_t m_bda;
    volatile bool m_congested = false;
    uint32_t m_mtu = 0;
    volatile uint32_t m_lastSend = 0;
};

GJBLEServer::BLEClient::BLEClient(uint16_t conn_id, esp_bd_addr_t bda)
: m_conn_id(conn_id)
{
  memcpy(m_bda, bda, sizeof(esp_bd_addr_t));

  esp_ble_conn_update_params_t conn_params = {0};
  memcpy(conn_params.bda, bda, sizeof(esp_bd_addr_t));
  /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
  conn_params.latency = 0;
  conn_params.max_int = 0x7;    // max_int = 0x20*1.25ms = 40ms
  conn_params.min_int = 0x6;    // min_int = 0x10*1.25ms = 20ms
  conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms

  esp_err_t ret = esp_ble_gap_update_conn_params(&conn_params);
  if (ret != ESP_OK) {
      GJ_ERROR("esp_ble_gap_update_conn_params failed: %s\n", esp_err_to_name(ret));
  }
}

GJBLEServer::BLEClient::~BLEClient()
{
  esp_err_t ret = esp_ble_gap_disconnect(m_bda);
  if (ret != ESP_OK) {
      GJ_ERROR("esp_ble_gap_disconnect failed: %s\n", esp_err_to_name(ret));
  }
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


bool GJBLEServer::BLEClient::Is(uint16_t conn_id, esp_bd_addr_t bda) const
{
  return m_conn_id == conn_id &&
         !memcmp(m_bda, bda, sizeof(esp_bd_addr_t));
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
  return m_bda;
}

bool GJBLEServer::BLEClient::Indicate(uint16_t gatts_if, uint16_t descr_handle, const char *string)
{
  esp_err_t errRc;

  uint32_t elapsed;
  do
  {
    elapsed = millis() - m_lastSend;
  } while (elapsed < 7);
  
  m_lastSend = millis();

  while (IsCongested())
  {
    //printf("Indicate:congestion\n");
    delay(20);
  }
  uint16_t num = esp_ble_get_sendable_packets_num();
  uint16_t numCur = esp_ble_get_cur_sendable_packets_num(m_conn_id);
  BLE_SER(4, "sendable:%d %d\n", num, numCur);

 // printf("indicate:'%.*s'\n", strlen(string), string);
  //errRc = esp_ble_gatts_set_attr_value(descr_handle, strlen(string), (uint8_t*)string);
  //if (errRc != ESP_OK) {
  //    BLE_SER("esp_ble_gatts_set_attr_value B failed: %s\n", esp_err_to_name(errRc));
  //    return false;
  //}
  while(true)
  {
    errRc = ::esp_ble_gatts_send_indicate(
      gatts_if,
      m_conn_id,
      descr_handle, 
      strlen(string), (uint8_t*)string, false); // The need_confirm = false makes this a notify.
    if (errRc != ESP_OK) {
        uint16_t num = esp_ble_get_sendable_packets_num();
        uint16_t numCur = esp_ble_get_cur_sendable_packets_num(m_conn_id);
        GJ_ERROR("esp_ble_gatts_send_indicate failed: %s %d %d\n", esp_err_to_name(errRc), num, numCur);

        if (errRc == ESP_ERR_INVALID_STATE)
        {
          return false;
        }

        delay(200);
        //return false;
    }
    else
    {
      break;
    }
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

struct GJBLEServer::SPendingSerial
{
  uint32_t m_length = 0;
  static const uint32_t MaxLength = 8092;
  char m_buffer[MaxLength];
};

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



#define GATTS_SERVICE_UUID_TEST_A   0x00FF
#define GATTS_CHAR_UUID_TEST_A      0xFF01
#define GATTS_DESCR_UUID_TEST_A     0x3333
#define GATTS_NUM_HANDLE_TEST_A     4

#define GATTS_SERVICE_UUID_TEST_B   0x00EE
#define GATTS_CHAR_UUID_TEST_B      0xEE01
#define GATTS_DESCR_UUID_TEST_B     0x2222
#define GATTS_NUM_HANDLE_TEST_B     4

#define TEST_MANUFACTURER_DATA_LEN  17

#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40

#define PREPARE_BUF_MAX_SIZE 1024

static uint8_t char1_str[] = {0x11,0x22,0x33};
static esp_gatt_char_prop_t a_property = 0;
static esp_gatt_char_prop_t b_property = 0;

static esp_attr_value_t gatts_demo_char1_val =
{
    .attr_max_len = GATTS_DEMO_CHAR_VAL_LEN_MAX,
    .attr_len     = sizeof(char1_str),
    .attr_value   = char1_str,
};

static uint8_t adv_config_done = 0;
#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

#ifdef CONFIG_SET_RAW_ADV_DATA
static uint8_t raw_adv_data[] = {
        0x02, 0x01, 0x06, 
        0x02, 0x0a, 0xeb, 0x03, 0x03, 0xab, 0xcd
};
static uint8_t raw_scan_rsp_data[] = {
        0x0f, 0x09, 0x45, 0x53, 0x50, 0x5f, 0x47, 0x41, 0x54, 0x54, 0x53, 0x5f, 0x44,
        0x45, 0x4d, 0x4f
};
#else

static uint8_t adv_service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xEE, 0x00, 0x00, 0x00,
    //second uuid, 32bit, [12], [13], [14], [15] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};

// The length of adv data must be less than 31 bytes
//static uint8_t test_manufacturer[TEST_MANUFACTURER_DATA_LEN] =  {0x12, 0x23, 0x45, 0x56};
//adv data
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0007, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data =  NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
// scan response data
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0007,
    .appearance = 0x00,
    .manufacturer_len = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data =  NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

#endif /* CONFIG_SET_RAW_ADV_DATA */


static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};


#define PROFILE_NUM 2
#define PROFILE_A_APP_ID 0
#define PROFILE_B_APP_ID 1


typedef struct {
    uint8_t                 *prepare_buf;
    int                     prepare_len;
} prepare_type_env_t;


struct GJBLEServer::gatts_profile_inst {
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
    uint16_t numHandle;

  prepare_type_env_t prepare_write_env;
    Vector<uint8_t> m_writtenData;
};

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */

void example_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);



void example_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param){
    esp_gatt_status_t status = ESP_GATT_OK;
    if (param->write.need_rsp){
        
        if (param->write.is_prep){
            BLE_SER(5, "Write rsp\n");

            if (prepare_write_env->prepare_buf == NULL) {
                prepare_write_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE*sizeof(uint8_t));
                prepare_write_env->prepare_len = 0;
                if (prepare_write_env->prepare_buf == NULL) {
                    BLE_SER(5, "Gatt_server prep no mem\n");
                    status = ESP_GATT_NO_RESOURCES;
                }
            } else {
                if(param->write.offset > PREPARE_BUF_MAX_SIZE) {
                    status = ESP_GATT_INVALID_OFFSET;
                } else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) {
                    status = ESP_GATT_INVALID_ATTR_LEN;
                }
            }

            esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
            gatt_rsp->attr_value.len = param->write.len;
            gatt_rsp->attr_value.handle = param->write.handle;
            gatt_rsp->attr_value.offset = param->write.offset;
            gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
            memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
            esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
            if (response_err != ESP_OK){
               GJ_ERROR("Send response error\n");
            }
            free(gatt_rsp);
            if (status != ESP_GATT_OK){
                return;
            }
            memcpy(prepare_write_env->prepare_buf + param->write.offset,
                   param->write.value,
                   param->write.len);
            prepare_write_env->prepare_len += param->write.len;

        }else{
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
        }
    }
}

void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param){
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC){
        //esp_log_buffer_hex(GATTS_TAG, prepare_write_env->prepare_buf, prepare_write_env->prepare_len);
    }else{
        BLE_SER(3, "ESP_GATT_PREP_WRITE_CANCEL\n");
    }
    if (prepare_write_env->prepare_buf) {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }
    prepare_write_env->prepare_len = 0;
}


GJBLEServer::BLEClient* GJBLEServer::GetClient(uint16_t conn_id, esp_bd_addr_t bda) const
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

void GJBLEServer::gatts_profile_event_handler(gatts_profile_inst &profile, esp_gatts_cb_event_t event, esp_ble_gatts_cb_param_t *param) 
{   
    GJ_PROFILE(gatts_profile_event_handler);

    esp_gatt_if_t gatts_if = profile.gatts_if;

    switch (event) {
    case ESP_GATTS_REG_EVT:
    {
        BLE_SER(3, "REGISTER_APP_EVT, status %d, app_id %d\n", param->reg.status, param->reg.app_id);

        esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(m_hostname.c_str());
        if (set_dev_name_ret){
            GJ_ERROR("set device name failed, error code = %x\n", set_dev_name_ret);
        }

        SER("BLE ADV\n");
        //config adv data
        esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
        if (ret){
            GJ_ERROR("config adv data failed, error code = %x\n", ret);
        }
        adv_config_done |= adv_config_flag;
        //config scan response data
        ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
        if (ret){
            GJ_ERROR("config scan response data failed, error code = %x\n", ret);
        }
        adv_config_done |= scan_rsp_config_flag;

        esp_ble_gatts_create_service(gatts_if, &profile.service_id, profile.numHandle);
        break;
    }
    case ESP_GATTS_READ_EVT: {
        BLE_SER(3, "GATT_READ_EVT, conn_id %d, trans_id %d, handle %d\n", param->read.conn_id, param->read.trans_id, param->read.handle);
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = 4;
        rsp.attr_value.value[0] = 0xde;
        rsp.attr_value.value[1] = 0xed;
        rsp.attr_value.value[2] = 0xbe;
        rsp.attr_value.value[3] = 0xef;
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                    ESP_GATT_OK, &rsp);
        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        BLE_SER(3, "GATT_WRITE_EVT, conn_id %d, trans_id %d, handle %d, len:%d, is_prep:%d need_rsp:%d\n", 
          param->write.conn_id, param->write.trans_id, param->write.handle, param->write.len, param->write.is_prep, param->write.need_rsp);

        profile.m_writtenData.insert(profile.m_writtenData.end(), param->write.value, param->write.value + param->write.len);
        
        if (!param->write.is_prep)
        {
          BLEClient *client = GetClient(param->write.conn_id, param->write.bda);
          auto gatts_if = gl_profile_tab[PROFILE_B_APP_ID].gatts_if;
          auto descr_handle = gl_profile_tab[PROFILE_B_APP_ID].descr_handle;

          InterpretCommand(
              (char*)profile.m_writtenData.data(), profile.m_writtenData.size(),
              client, gatts_if, descr_handle);
          
          profile.m_writtenData.clear();
        }
        example_write_event_env(gatts_if, &profile.prepare_write_env, param);
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
        BLE_SER(3, "ESP_GATTS_EXEC_WRITE_EVT\n");
        //SER("total BLE data:%d %.*s\n", profile.m_writtenData.size(), param->write.len, param->write.value);
        profile.m_writtenData.clear();

        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        example_exec_write_event_env(&profile.prepare_write_env, param);
        break;
    case ESP_GATTS_MTU_EVT:
    {
        SER("ESP_GATTS_MTU_EVT, MTU %d\n", param->mtu.mtu);
        BLEClient *client = GetClient(param->mtu.conn_id);
        if (client)
          client->SetMTU(param->mtu.mtu);

        break;
    }
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT:
    {
        BLE_SER(3, "CREATE_SERVICE_EVT, status %d,  service_handle %d\n", param->create.status, param->create.service_handle);
        profile.service_handle = param->create.service_handle;

        esp_ble_gatts_start_service(profile.service_handle);
        a_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE_NR | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
        esp_err_t add_char_ret = esp_ble_gatts_add_char(profile.service_handle, &profile.char_uuid,
                                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                        a_property,
                                                        &gatts_demo_char1_val, NULL);
        if (add_char_ret){
            GJ_ERROR("add char failed, error code =%x",add_char_ret);
        }
        break;
    }
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT: {
        uint16_t length = 0;
        const uint8_t *prf_char;

        profile.descr_handle = param->add_char.attr_handle;
        BLE_SER(3, "ADD_CHAR_EVT, status %d,  attr_handle %d, service_handle %d\n",
                param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
        profile.char_handle = param->add_char.attr_handle;
        profile.descr_uuid.len = ESP_UUID_LEN_16;
        profile.descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
        esp_err_t get_attr_ret = esp_ble_gatts_get_attr_value(param->add_char.attr_handle,  &length, &prf_char);
        if (get_attr_ret == ESP_FAIL){
            GJ_ERROR("ILLEGAL HANDLE");
        }

        BLE_SER(2, "the gatts demo char length = %x\n", length);
        for(int i = 0; i < length; i++){
            BLE_SER(2, "prf_char[%x] =%x\n",i,prf_char[i]);
        }
        BLE_CALL(esp_ble_gatts_add_char_descr(profile.service_handle, &profile.descr_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL));

        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        
        BLE_SER(3, "ADD_DESCR_EVT, status %d, attr_handle %d, service_handle %d\n",
                 param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        BLE_SER(3, "SERVICE_START_EVT, status %d, service_handle %d\n",
                 param->start.status, param->start.service_handle);
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT: {
        BLE_SER(3, "ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:\n",
                 param->connect.conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);

        bool found(false);
        for (BLEClient *client : m_clients)
        {
          if (client->Is(param->connect.conn_id, param->connect.remote_bda))
          {
            found = true;
            client->IncRef();
          }
        }

        esp_err_t ret;

        if (!found)
        {
          BLE_CALL(esp_ble_gattc_send_mtu_req(gatts_if, param->connect.conn_id));

          BLEClient *client = new BLEClient(param->connect.conn_id, param->connect.remote_bda);
          OnNewClient(client);
        }

        //esp32 ble auto stops adv on client conn, restart it
        BLE_CALL(esp_ble_gap_start_advertising(&adv_params));
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
    {
        BLE_SER(3, "ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%x\n\r", param->disconnect.reason);
        BLE_CALL(esp_ble_gap_start_advertising(&adv_params));

        Clients::iterator it = m_clients.begin();
        for (; it != m_clients.end() ; )
        {
          BLEClient *client = *it;

          if (client->Is(param->disconnect.conn_id, param->disconnect.remote_bda))
          {
            if (!client->DecRef())
            {   
              it = m_clients.erase(it);
              delete client;
              BLE_SER(3, "Client deleted\n");
              continue;
            }
          }

          ++it;
        }

        break;
    }
    case ESP_GATTS_CONF_EVT:
        if (param->conf.status != ESP_GATT_OK){
            BLE_SER(3, "ESP_GATTS_CONF_EVT, status %d attr_handle %d\n", param->conf.status, param->conf.handle);
            //esp_log_buffer_hex(GATTS_TAG, param->conf.value, param->conf.len);
        }
        break;
    case ESP_GATTS_CONGEST_EVT:
    {
      BLE_SER(3, "ESP_GATTS_CONGEST_EVT state:%d\n", param->congest.congested);
      BLEClient *client = GetClient(param->congest.conn_id);
      BLE_SER(3, "Client:%p\n", client);
      if (client)
        client->SetCongested(param->congest.congested);
      break;
    }
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    
      BLE_SER(3, "Other event %d\n", event);
    default:
        break;
    }
}

void GJBLEServer::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
        } else {
            GJ_ERROR("Reg app failed, app_id %04x, status %d\n",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    int idx;
    for (idx = 0; idx < PROFILE_NUM; idx++) {
        if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
            gatts_if == gl_profile_tab[idx].gatts_if) 
        {
          if (ms_instance)
            ms_instance->gatts_profile_event_handler(gl_profile_tab[idx], event, param);
        }
    }
}

void GJBLEServer::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
#ifdef CONFIG_SET_RAW_ADV_DATA
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done==0){
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done==0){
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
#else
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done == 0){
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done == 0){
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
#endif
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //advertising start complete event to indicate advertising start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            GJ_ERROR("Advertising start failed\n");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            GJ_ERROR("Advertising stop failed\n");
        } else {
            BLE_SER(3, "Stop adv successfully\n");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         LOG("BLE connection status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d\n\r",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

GJBLEServer::gatts_profile_inst* GJBLEServer::gl_profile_tab = nullptr;

bool GJBLEServer::Init(const char *hostname, GJOTA *ota)
{
  m_hostname = hostname;
  m_ota = ota;
  
  return Init();
}

bool GJBLEServer::Init()
{
  if (!m_init)
  {
    gl_profile_tab = new gatts_profile_inst[2];

    gl_profile_tab[PROFILE_A_APP_ID].gatts_if = ESP_GATT_IF_NONE;       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    gl_profile_tab[PROFILE_A_APP_ID].service_id.is_primary = true;
    gl_profile_tab[PROFILE_A_APP_ID].service_id.id.inst_id = 0x00;
    gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
    gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST_A;
    gl_profile_tab[PROFILE_A_APP_ID].char_uuid.len = ESP_UUID_LEN_16;
    gl_profile_tab[PROFILE_A_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST_A;
    gl_profile_tab[PROFILE_A_APP_ID].numHandle = GATTS_NUM_HANDLE_TEST_A;

    gl_profile_tab[PROFILE_B_APP_ID].gatts_if = ESP_GATT_IF_NONE;       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    gl_profile_tab[PROFILE_B_APP_ID].service_id.is_primary = true;
    gl_profile_tab[PROFILE_B_APP_ID].service_id.id.inst_id = 0x00;
    gl_profile_tab[PROFILE_B_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
    gl_profile_tab[PROFILE_B_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST_B;
    gl_profile_tab[PROFILE_B_APP_ID].char_uuid.len = ESP_UUID_LEN_16;
    gl_profile_tab[PROFILE_B_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST_B;
    gl_profile_tab[PROFILE_B_APP_ID].numHandle = GATTS_NUM_HANDLE_TEST_B;

    // Initialize NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    btStart();

    BLE_CALL(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL0, ESP_PWR_LVL_P9));
    BLE_CALL(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9));
    BLE_CALL(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9));
    BLE_CALL(esp_bredr_tx_power_set(ESP_PWR_LVL_P9, ESP_PWR_LVL_P9));

    BLE_CALL(esp_bluedroid_init(), return false);
    BLE_CALL(esp_bluedroid_enable(), return false);
    BLE_CALL(esp_ble_gatts_register_callback(gatts_event_handler), return false);
    BLE_CALL(esp_ble_gap_register_callback(gap_event_handler), return false);
    BLE_CALL(esp_ble_gatts_app_register(PROFILE_A_APP_ID), return false);
    BLE_CALL(esp_ble_gatts_app_register(PROFILE_B_APP_ID), return false);
    BLE_CALL(esp_ble_gatt_set_local_mtu(500));

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

    auto broadWSLog = [=](const char *buffer)
    {
      this->Broadcast(buffer);
    };

    auto sendWBLog = [=]()
    {
      SendRecentLog(broadWSLog);
    };

    REGISTER_COMMAND("blesendlog", sendWBLog);

    REGISTER_COMMAND("bledbg", [this](){
      uint32_t clientCount = m_clients.size();
      SER("BLE clients:%d\n\r", clientCount);
      for (BLEClient const *client : m_clients)
      {
        SER("  conn:%02d mtu:%03d bda:%02x:%02x:%02x:%02x:%02x:%02x ref:%02d\n\r", 
          client->GetConnId(),
          client->GetMTU(),
          client->GetBDA()[0], client->GetBDA()[1], client->GetBDA()[2], client->GetBDA()[3], client->GetBDA()[4], client->GetBDA()[5],
          client->GetRef()
          );
      }
    });


    REGISTER_COMMAND("bleon", [this](){
      if (!ms_instance)
        this->Init();
    });


    REGISTER_COMMAND("bleoff", [](){
      if (ms_instance)
        ms_instance->Term();
    });
  }

  return m_init;
}

void GJBLEServer::Term()
{
  if (m_terminalIndex != -1)
  {
    RemoveTerminalHandler(m_terminalIndex);
    m_terminalIndex = -1;
    LOG("WSS terminal handler removed\n\r");
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

    esp_err_t ret;
    
    ret = esp_ble_gatts_app_unregister(gl_profile_tab[0].gatts_if);
    if (ret != ESP_OK) {
      GJ_ERROR("esp_ble_gatts_app_unregister(0) failed: %s\n", esp_err_to_name(ret));
    }

    ret = esp_ble_gatts_app_unregister(gl_profile_tab[1].gatts_if);
    if (ret != ESP_OK) {
      GJ_ERROR("esp_ble_gatts_app_unregister(1) failed: %s\n", esp_err_to_name(ret));
    }

    ret = esp_bluedroid_disable();
    if (ret != ESP_OK) {
      GJ_ERROR("esp_bluedroid_disable failed: %s\n", esp_err_to_name(ret));
    }
    ret = esp_bluedroid_deinit();
    if (ret != ESP_OK) {
      GJ_ERROR("esp_bluedroid_deinit failed: %s\n", esp_err_to_name(ret));
    }

    if (!btStop())
    {
      GJ_ERROR("btStop failed\n");
    }

    delete [] gl_profile_tab;
    gl_profile_tab = nullptr;

    m_init = false;
  }

  LOG("WSS terminated\n\r");
}

void GJBLEServer::OnNewClient(BLEClient *client)
{
  {
    unsigned char data[256];

    for (int i = 0 ; i < 256 ; ++i)
      data[i] = (uint8_t)i;

    uint32_t crc = ComputeCrcDebug(data, 256);
    uint32_t crc2 = ComputeCrcDebug(data, 128);
    crc2 = ComputeCrcDebug(&data[128], 128, crc2);
    BLE_SER(4, "test crc:%u crc2:%u\n", crc, crc2);
  }

  BLE_SER(4, "RAM avail:%dKB\n\r", heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024);

  //wait for headers
  SetDebugLoc("Waiting for headers\n\r");
  m_clients.push_back(client);  
}

void GJBLEServer::InterpretCommand(const char *cmd, uint32_t len, 
  BLEClient *client, uint16_t gatts_if, uint16_t descr_handle)
{
  if (!strncmp(cmd, "gjcommand:", 10))
  {
    //printf("%.*s\n", param->write.len, param->write.value);

    const char *subCmd = cmd + 10;
    if (!strncmp(subCmd, "blesendavailcommands", 20))
    {
      if (client)
        SendHelp(*client, gatts_if, descr_handle);
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
    GJString response;
    m_ota->HandleMessage(cmd, len, response);

    if (response.length() && client)
    {
      client->Indicate(gatts_if, descr_handle, response.c_str());
    }
  }
}  

void GJBLEServer::SendHelp(BLEClient &client, uint16_t gatts_if, uint16_t descr_handle)
{
  Vector<GJString> commands;
  GetCommands(commands);

  String output;// = *new String;
  //output += "ws_availablecommands:";
  for (String &str : commands)
  {
    if (!output.length())
      output += "ws_availablecommands:";

    uint32_t len = str.length() + 1;

    if ((output.length() + len) > 95)
    {
      printf("avail commands len:%d '%s'\n", output.length(), output.c_str());
      client.Indicate(gatts_if, descr_handle, output.c_str());

      output = "ws_availablecommands:";
    }

    output += str;
    output += ";";
  }

  if (output.length())
  {
    printf("avail commands len:%d '%s'\n", output.length(), output.c_str());
    client.Indicate(gatts_if, descr_handle, output.c_str());
  }
}


String GJBLEServer::CleanupText(const char *text)
{
  GJ_PROFILE(GJBLEServer::CleanupText);

  String feedStorage;
  text = RemoveNewLineLineFeed(text, feedStorage);

  String output = text;

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
  if (m_clients.empty())
    return false;

  GJ_PROFILE(GJBLEServer::Broadcast);

  String string(text);

  string = CleanupText(string.c_str());

  bool written(false);

  uint32_t mtu = ESP_GATT_MAX_MTU_SIZE;

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
    //GJString subStr(FormatString("'%.*s'", len, string.c_str() + offset));

    GJString subStr(string.c_str() + offset, len);


    //printf("b-----------------\n\r");
    //printf("'%s'\n", subStr.c_str());
    //printf("e-----------------\n\r");

    //printf("%s", dbg.c_str());
    //printf("%s", subStr.c_str());

    written |= BroadcastText(subStr.c_str());

    offset += len;
  }

  return written;
}

bool GJBLEServer::BroadcastText(const char *text)
{
  GJ_PROFILE(GJBLEServer::BroadcastText);

  bool written(false);

  Clients::iterator it = m_clients.begin();
  for (; it != m_clients.end() ; )
  {
    BLEClient *client = *it;

    auto gatts_if = gl_profile_tab[PROFILE_B_APP_ID].gatts_if;
    auto descr_handle = gl_profile_tab[PROFILE_B_APP_ID].descr_handle;

    bool ret = client->Indicate(gatts_if, descr_handle, text);
    if (!ret)
    {
      GJ_ERROR("GJBLEServer::BroadcastText delete client\n");
      delete client;
      it = m_clients.erase(it);
    }
    else
    {
      written = true;
      ++it;
    }
  }

  return written;
}

void GJBLEServer::Update()
{
  GJ_PROFILE(GJBLEServer::Update);

  SetDebugLoc("BLE Update");

  if (m_pendingSerial && m_pendingSerial->m_length)
  {
    if (Broadcast(m_pendingSerial->m_buffer))
      m_pendingSerial->m_length = 0;
  }

  for (GJString const &command : m_commands)
  {
    ::InterpretCommand(command.c_str()); 
  }

  m_commands.clear();

  SetDebugLoc("");
}

#endif