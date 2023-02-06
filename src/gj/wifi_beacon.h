#if 0

#include <stdint.h>
#include <nvs_flash.h>
#include <esp_wifi.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

#define __force
#define __bitwise
#define STRUCT_PACKED __attribute__ ((packed))
#define ETH_ALEN 6

typedef u16 __bitwise be16;
typedef u16 __bitwise le16;
typedef u32 __bitwise be32;
typedef u32 __bitwise le32;
typedef u64 __bitwise be64;
typedef u64 __bitwise le64;

extern "C"{ 

  #include <wpa/ieee802_11_defs.h>
}


namespace sniffer
{
  #define ESP_ERROR_CHECK2(x) ({                                         \
        esp_err_t __err_rc = (x);                                                   \
        if (__err_rc != ESP_OK) {                                                   \
            _esp_error_check_failed_without_abort(__err_rc, __FILE__, __LINE__,     \
                                    __ASSERT_FUNC, #x);                             \
        }                                                                           \
        __err_rc;                                                                   \
    })

typedef struct {
    ieee80211_hdr hdr;
    uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;


const char *
wifi_sniffer_packet_type2str(wifi_promiscuous_pkt_type_t type)
{
	switch(type) {
	case WIFI_PKT_MGMT: return "MGMT";
	case WIFI_PKT_DATA: return "DATA";
	default:	
	case WIFI_PKT_MISC: return "MISC";
	}
}


esp_err_t
event_handler(void *ctx, system_event_t *event)
{
	
	return ESP_OK;
}

struct FrameControl
{
  uint16_t ProtocolVersion : 2;
  uint16_t Type : 2;
  uint16_t Subtype : 4;
  uint16_t ToDS : 1;
  uint16_t FromDS : 1;

  uint16_t Pad : 6;
};

bool IsSameMac( uint8_t const *left, uint8_t const *right )
{
  return memcmp(left, right, 6) == 0;
}

struct Beacon
{
  uint8_t mac[6];
  uint64_t timestamp;
  uint32_t lastUpdate;
};

RTC_DATA_ATTR Beacon g_previousBeacons[8] = {};
RTC_DATA_ATTR Beacon g_beacons[8] = {};

void
wifi_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type)
{

	if (type != WIFI_PKT_MGMT)
		return;

	const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
	const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
	const ieee80211_hdr *hdr = &ipkt->hdr;
  const ieee80211_mgmt *mgmt = (ieee80211_mgmt*)&ipkt->hdr;
  FrameControl *frameControl = (FrameControl*)&mgmt->frame_control;



	SER("PACKET TYPE=%s, CHAN=%02d, RSSI=%02d"
		//" ADDR1=%02x:%02x:%02x:%02x:%02x:%02x,"
		" ADDR2=%02x:%02x:%02x:%02x:%02x:%02x ",
		//" ADDR3=%02x:%02x:%02x:%02x:%02x:%02x",
		wifi_sniffer_packet_type2str(type),
		ppkt->rx_ctrl.channel,
		ppkt->rx_ctrl.rssi,
		/* ADDR1 */
		//hdr->addr1[0],hdr->addr1[1],hdr->addr1[2],
		//hdr->addr1[3],hdr->addr1[4],hdr->addr1[5],
		/* ADDR2 */
		hdr->addr2[0],hdr->addr2[1],hdr->addr2[2],
		hdr->addr2[3],hdr->addr2[4],hdr->addr2[5]
		/* ADDR3 */
		//hdr->addr3[0],hdr->addr3[1],hdr->addr3[2],
		//hdr->addr3[3],hdr->addr3[4],hdr->addr3[5]
	);


  SER(" mgmt type:%d sub:%d", frameControl->Type, frameControl->Subtype);

  if (frameControl->Subtype == WLAN_FC_STYPE_BEACON)
  {
    u8 const *packetTS = mgmt->u.beacon.timestamp;
    
    uint64_t ts;
    memcpy(&ts, packetTS, 8);

    SER(" BEACON: 0x%08x%08x  ",
      (uint32_t)(ts >> 32), (uint32_t)(ts & 0xffffFFFF) );

    uint8_t const emptyMac[6] = {}; 

    //find beacon
    for ( uint32_t i = 0 ; i < 8 ; ++i )
    {
      Beacon &beacon = g_beacons[i];

      if (IsSameMac(beacon.mac, hdr->addr2))
      {
        SER_COND(ts < beacon.timestamp, 
          " timestamp unexpected: before:0x%08x%08x after:0x%08x%08x ",
          (uint32_t)(beacon.timestamp >> 32), (uint32_t)(beacon.timestamp & 0xffffFFFF),
          (uint32_t)(ts >> 32), (uint32_t)(ts & 0xffffFFFF) );

        beacon.timestamp = ts;
        beacon.lastUpdate = millis();
        SER(" Updated slot %d", i);
        break;
      }

      if (IsSameMac(beacon.mac, emptyMac))
      {
        memcpy(beacon.mac, hdr->addr2, 6);
        beacon.timestamp = ts;
        beacon.lastUpdate = millis();
        SER("  Init slot %d", i);
        break;
      }
    }
    
  }

  SER("\n\r");
}

void
wifi_sniffer_init(void)
{
  static wifi_country_t wifi_country = {"CN", 1, 13, 0, WIFI_COUNTRY_POLICY_AUTO};

  //for ( uint32_t i = 0 ; i < 8 ; ++i )
  //{
  //  Beacon &beacon = g_beacons[i];
//
	//  SER("Beacon %d ADDR1=%02x:%02x:%02x:%02x:%02x:%02x\n\r",
  //    i,
  //    beacon.mac[0],beacon.mac[1],beacon.mac[2],
  //    beacon.mac[3],beacon.mac[4],beacon.mac[5] );
//
  //}


	nvs_flash_init();
    	tcpip_adapter_init();
    	ESP_ERROR_CHECK2( esp_event_loop_init(event_handler, NULL) );
    	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK2( esp_wifi_init(&cfg) );
	ESP_ERROR_CHECK2( esp_wifi_set_country(&wifi_country) ); /* set country for channel range [1, 13] */
	ESP_ERROR_CHECK2( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    	ESP_ERROR_CHECK2( esp_wifi_set_mode(WIFI_MODE_NULL) );
    	ESP_ERROR_CHECK2( esp_wifi_start() );
	esp_wifi_set_promiscuous(true);
	esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler);
}

} //namespace sniffer

#endif