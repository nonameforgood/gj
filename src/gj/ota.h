#ifdef GJ_ARDUINO_OTA

void UpdateOTA();

#include "clone/GJArduinoOTA.h"
#include "base.h"
#include "datetime.h"

inline void InitOTA(const char *hostName, ArduinoOTAClass::THandlerFunction endCB = {})
{
  {
    // otherwise hostname defaults to esp3232-[MAC]
     GJArduinoOTA.setHostname(hostName);

     
  
    GJArduinoOTA
      .onStart([]() {
        String type;
        if (GJArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";
  
        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        char date[20];
        ConvertEpoch(GetUnixtime(), date);
    
        LOG("OTA Start updating %s, %s\n\r", type.c_str(), date);
        FlushLog();
        //metricsManager.PrepareSleep();
      })
      .onEnd([=]() {
        char date[20];
        ConvertEpoch(GetUnixtime(), date);
    
        LOG("OTA End:%s\n\r", date);
        FlushLog();
        endCB();
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        static unsigned int s_ratio = 0;
        unsigned int ratio = (progress / (total / 100));
        if (ratio != s_ratio)
        {
            s_ratio = ratio;
            LOG("OTA Progress: %u%%\n\r", ratio);
        }      
      })
      .onError([](ota_error_t error) {
        GJ_ERROR("OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) { GJ_ERROR("Auth Failed\n\r"); }
        else if (error == OTA_BEGIN_ERROR) { GJ_ERROR("Begin Failed\n\r"); }
        else if (error == OTA_CONNECT_ERROR) { GJ_ERROR("Connect Failed\n\r"); }
        else if (error == OTA_RECEIVE_ERROR) { GJ_ERROR("Receive Failed\n\r"); }
        else if (error == OTA_END_ERROR) { GJ_ERROR("End Failed\n\r"); }
        GJ_ERROR("OTA failed, rebooting module\n\r");
        FlushLog();
        //failed OTA sometimes breaks wifi
        Reboot();
      });
  
    GJArduinoOTA.begin();
  }
}

inline void UpdateOTA()
{
  GJ_PROFILE(UpdateOTA);


  if (!IsWifiAPEnabled() && !IsWifiSTAEnabled())
    return;

  GJArduinoOTA.handleAsync();
}

#endif //GJ_ARDUINO_OTA