#include "commands.h"
#include "base.h"

#define GJ_WIFI_SNIFFER

#ifdef GJ_WIFI_SNIFFER

#include <esp_wifi.h>


const char *wifiPrefix = "";
//#define PROMIS_SER(fmt, ...) SER("%s" fmt, wifiPrefix, #__VA_ARGS__ )
#define PROMIS_SER(fmt, ...) SER(fmt, #__VA_ARGS__ )

struct FrameDebug
{
  uint8_t secs;
  uint8_t type;
  uint8_t subType;
};

void resetSnif();

FrameDebug *g_debugFrames = nullptr;
uint32_t g_debugFrameIndex = 0;
uint32_t g_maxDebugFrameIndex = 0;
uint64_t g_debugFrameStart = 0;

void PrintPromiscuousPacket(wifi_promiscuous_pkt_t *packet)
{
  wifi_pkt_rx_ctrl_t *rx_ctrl = &packet->rx_ctrl;

  SER("%schan:%d ",wifiPrefix,  (int)rx_ctrl->channel);
  SER("%sschan:%d ", wifiPrefix, (int)rx_ctrl->secondary_channel);
  SER("%ssig_len:%d ", wifiPrefix, (int)rx_ctrl->sig_len);
  SER("%srx_state:%d ", wifiPrefix, (int)rx_ctrl->rx_state);
}

typedef struct { // or this
  uint8_t mac[6];
} __attribute__((packed)) MacAddr;

typedef struct 
{
  unsigned int ProtoVersion : 2;
  unsigned int Type : 2;
  unsigned int SubType : 4;
  unsigned int ToDS : 1;
  unsigned int FromDS : 1;
  unsigned int MoreFrags : 1;
  unsigned int Retry : 1;
  unsigned int Power : 1;
  unsigned int MoreData : 1;
  unsigned int Protect : 1;
  unsigned int HTC : 1;
} FrameControl;

typedef struct { // still dont know much about this
  FrameControl fctl;
  int16_t duration;
  MacAddr da;
  MacAddr sa;
  MacAddr bssid;
  int16_t seqctl;
  unsigned char payload[];
} __attribute__((packed)) FrameHeader;

const char *GetMGMTSubType(int subType)
{
  const char *text[] = {
    "Association Request",
    "Association Response",
    "Reassociation Request",
    "Reassociation Response",
    "Probe Request",
    "Probe Response",
    "Timing Advertisement",
    "Reserved",
    "Beacon",
    "ATIM",
    "Disassociation",
    "Authentication",
    "Deauthentication",
    "Action",
    "Action No Ack (NACK)",
    "Reserved"
  };

  return text[subType];
}


const char *GetCTRLSubType(int subType)
{
  const char *text[] = {
    "Reserved",
    "Reserved",
    "Trigger",
    "TACK",
    "Beamforming Report Poll",
    "VHT/HE NDP Announcement",
    "Control Frame Extension",
    "Control Wrapper",
    "Block Ack Request",
    "Block Ack",
    "PS-Poll",
    "RTS",
    "CTS",
    "ACK",
    "CF-End",
    "CF-End + CF-ACK",
    ""
  };

  return text[subType];
}


const char *GetDataSubType(int subType)
{
  const char *text[] = {
    "Data",
    "Reserved",
    "Reserved",
    "Reserved",
    "Null (no data)",
    "Reserved",
    "Reserved",
    "Reserved",
    "QoS Data",
    "QoS Data + CF-ACK",
    "QoS Data + CF-Poll",
    "QoS Data + CF-ACK + CF-Poll",
    "QoS Null (no data)",
    "Reserved",
    "QoS CF-Poll (no data)",
    "QoS CF-ACK + CF-Poll (no data)"
  };

  return text[subType];
}


void PrintMGMTPacket(FrameHeader *frame)
{
  

  FrameControl fc = frame->fctl;

  char buffer[128];

  sprintf(buffer, "MGMT sub:%s\n\r", GetMGMTSubType(fc.SubType));
  SER_LARGE(buffer);

  //SER("%stype:%d sub:%d ", wifiPrefix, fc.Type, fc.SubType);
  //SER("%ssub type:%s ", wifiPrefix, subType[fc.SubType]);
  //SER("%sda:%02x:%02x:%02x:%02x:%02x:%02x ", wifiPrefix, mgmt->da.mac[0], mgmt->da.mac[1], mgmt->da.mac[2], mgmt->da.mac[3], mgmt->da.mac[4], mgmt->da.mac[5]);
  //SER("%ssa:%02x:%02x:%02x:%02x:%02x:%02x\n", wifiPrefix, mgmt->sa.mac[0], mgmt->sa.mac[1], mgmt->sa.mac[2], mgmt->sa.mac[3], mgmt->sa.mac[4], mgmt->sa.mac[5]);
  
}


void PrintCTRLPacket(FrameHeader *frame)
{
  FrameControl fc = frame->fctl;
  char buffer[128];

  sprintf(buffer, "CTRL sub:%s\n\r", GetCTRLSubType(fc.SubType));
  SER_LARGE(buffer);

  //SER("%stype:%d sub:%d ", wifiPrefix, fc.Type, fc.SubType);
  //SER("%ssub type:%s ", wifiPrefix, subType[fc.SubType]);
  //SER("%sda:%02x:%02x:%02x:%02x:%02x:%02x ", wifiPrefix, mgmt->da.mac[0], mgmt->da.mac[1], mgmt->da.mac[2], mgmt->da.mac[3], mgmt->da.mac[4], mgmt->da.mac[5]);
  //SER("%ssa:%02x:%02x:%02x:%02x:%02x:%02x\n", wifiPrefix, mgmt->sa.mac[0], mgmt->sa.mac[1], mgmt->sa.mac[2], mgmt->sa.mac[3], mgmt->sa.mac[4], mgmt->sa.mac[5]);
  
}


void PrintDATAPacket(FrameHeader *frame)
{
  FrameControl fc = frame->fctl;

  char buffer[128];

  sprintf(buffer, "DATA sub:%s\n\r", GetDataSubType(fc.SubType));
  SER_LARGE(buffer);

  //SER("%stype:%d sub:%d ", wifiPrefix, fc.Type, fc.SubType);
  //SER("%ssub type:%s ", wifiPrefix, subType[fc.SubType]);
  //SER("%sda:%02x:%02x:%02x:%02x:%02x:%02x ", wifiPrefix, mgmt->da.mac[0], mgmt->da.mac[1], mgmt->da.mac[2], mgmt->da.mac[3], mgmt->da.mac[4], mgmt->da.mac[5]);
  //SER("%ssa:%02x:%02x:%02x:%02x:%02x:%02x\n", wifiPrefix, mgmt->sa.mac[0], mgmt->sa.mac[1], mgmt->sa.mac[2], mgmt->sa.mac[3], mgmt->sa.mac[4], mgmt->sa.mac[5]);
  
}


void PrintMISCPacket(FrameHeader *frame)
{

  FrameControl fc = frame->fctl;

  char buffer[128];

  sprintf(buffer, "MISC sub:%d\n\r", fc.SubType);
  SER_LARGE(buffer);

}

bool storeFrames = true;

void ResetSnif()
{
  SER("Current snif %d\n\r", g_debugFrameIndex);

  if (g_debugFrameIndex != g_maxDebugFrameIndex)
  {
    g_debugFrameIndex = 0;
    g_debugFrameStart = millis();
  }
}

void PrintStoredFrames()
{
  for (uint32_t i = 0 ; i < g_debugFrameIndex ; ++i)
  {
    FrameDebug *fd = &g_debugFrames[i];

    if (fd->type == 0)
    {
      SER("MGMT t:%d sub:%s\n\r", fd->secs, GetMGMTSubType(fd->subType));
    }
    else if (fd->type == 1)
    {
      SER("CTRL t:%d sub:%s\n\r", fd->secs, GetCTRLSubType(fd->subType));
    }
    else if (fd->type == 2)
    {
      SER("DATA t:%d sub:%s\n\r", fd->secs, GetDataSubType(fd->subType));
    }
    else
    {
      SER("MISC t:%d sub:%d\n\r", fd->secs, fd->subType);
    }
  }

  SER("Total snif %d\n\r", g_debugFrameIndex);
}


void OnPromiscuous(void *buf, wifi_promiscuous_pkt_type_t type)
{
  if (storeFrames)
  {
    if (!g_debugFrames)
    {
      g_maxDebugFrameIndex = 1024 * 2;
      g_debugFrames = new FrameDebug[g_maxDebugFrameIndex];
      g_debugFrameIndex = 0;
      g_debugFrameStart = millis();
    }

    if (g_debugFrameIndex < g_maxDebugFrameIndex)
    {
      FrameDebug *fd = &g_debugFrames[g_debugFrameIndex++];

      wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t*)buf; 
      FrameHeader *header = (FrameHeader*)packet->payload;

      fd->secs = (millis() - g_debugFrameStart) / 1000;
      fd->type = (int)type;
      fd->subType = header->fctl.SubType;
    }

    return;
  }

  const char *typeString[] = {
    "MGMT",
    "CTRL",
    "DATA",
    "MISC"
  };

  const char *pad[] = {
    "",
    "                      ",
    "                                            ",
    "                                                                  "
  };

  //wifiPrefix = pad[(int)type];

  wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t*)buf; 

  //SER("PCKT %s ", typeString[(int)type]);

  //PrintPromiscuousPacket(packet);

  if (type == 0)
  {
    PrintMGMTPacket((FrameHeader*)packet->payload);
  }
  else if (type == 1)
  {
    PrintCTRLPacket((FrameHeader*)packet->payload);
  }
  else if (type == 2)
  {
    PrintCTRLPacket((FrameHeader*)packet->payload);
  }
  else
  {
    PrintMISCPacket((FrameHeader*)packet->payload);
  }
}

bool IsSnifferEnabled()
{
  bool enabled = false;
  ESP_ERROR_CHECK(esp_wifi_get_promiscuous(&enabled));

  return enabled;
}

void EnableSniffer()
{
  if (IsSnifferEnabled())
    return;

  wifi_promiscuous_filter_t promisFilter = {WIFI_PROMIS_FILTER_MASK_MGMT|WIFI_PROMIS_FILTER_MASK_CTRL};
  wifi_promiscuous_filter_t promisCtrlFilter = {WIFI_PROMIS_CTRL_FILTER_MASK_ALL};
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&promisFilter));
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous_ctrl_filter(&promisCtrlFilter));
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(&OnPromiscuous));
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
  
  SER("Wifi promiscuous mode enabled\n\r");
}

void DisableSniffer()
{
  if (!IsSnifferEnabled())
    return;
    
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
}

void ToggleWifiSniffer()
{
  if (IsSnifferEnabled())
    DisableSniffer();
  else
    EnableSniffer();
}



DEFINE_COMMAND_NO_ARGS(wifisnif, ToggleWifiSniffer );
DEFINE_COMMAND_NO_ARGS(wifisnifprint, PrintStoredFrames );

#endif //GJ_WIFI_SNIFFER

void InitSniffer()
{
#ifdef GJ_WIFI_SNIFFER
  REFERENCE_COMMAND(wifisnif);
  REFERENCE_COMMAND(wifisnifprint);
#endif
}