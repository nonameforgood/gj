
#include "base.h"
#include "config.h"
#include "gjwificlient.h"
#include "lwip/dns.h"
#ifdef ESP8266
#include <ESP8266HTTPClient.h>
#else
#include "clone/GJHTTPClient.h"
#endif

DEFINE_CONFIG_BOOL(http.enable, http_enable, false);

void InitHttp()
{
  static bool isInit = false;
  if (isInit)
    return;

  isInit = true;
  
  dns_init();
}

bool HttpGet(const char *get, String &response)
{  

#if 1
  InitHttp();
  bool enabled = GJ_CONF_BOOL_VALUE(http_enable);
  if (!enabled)
    return false;


  

  //Client *client = nullptr;
  GJWifiClient client;
  gj::HTTPClient http;

  printf("GJWifiClient:%p\n\r", &client);
  bool ret = false;

  if (http.begin(client, get)) 
  {  
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        response = http.getString();
        ret = true;
      }
      else
      {
        LOG("http.GET failed:%d\n\r",httpCode);
        LOG("  request:%s\n\r", get);
      }
    } 
    else
    {
      LOG("http.GET failed:%d\n\r",httpCode);
      LOG("  request:%s\n\r", get);
    }

    http.end();
  } 
  else
  {
    LOG("http.begin failed\n\r");
    LOG("  request:%s\n\r", get);
  }

  return ret;
  #endif
}



bool HttpPost(const char *uri, const char *data, uint32_t dataSize, GJString &response)
{
  return false;
#if 0
  InitHttp();
  bool enabled = GJ_CONF_BOOL_VALUE(http_enable));
  if (!enabled)
    return false;
    
  Client *client;
  gj::HTTPClient http;

  bool ret = false;


  if (http.begin(*client, uri)) 
  {  
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // start connection and send HTTP header
    int httpCode = http.POST((uint8_t*)data, dataSize);

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        response = http.getString();
        ret = true;
      }
      else
      {
        LOG("http.POST failed:%d\n\r",httpCode);
        LOG("  Request:%s\n\r", uri)
      }
    } 
    else
    {
      LOG("http.POST failed:%d\n\r",httpCode);
      LOG("  Request:%s\n\r", uri)
    }

    http.end();
  } 
  else
  {
    LOG("http.POST.begin failed:%d\n\r");
    LOG("  Request:%s\n\r", uri)
  }

  return ret;

  #endif
}
