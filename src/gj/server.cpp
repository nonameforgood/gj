#include "server.h"

#ifdef GJ_SERVER

#include "base.h"

#include "datetime.h"
#include "wwwdatetime.h"
#include "crypt.h"
#include "esputils.h"
#include "millis.h"
#include "commands.h"
#include "file.h"
#include "wificonfig.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>

#include <HTTPClient.h>

GJServerArgs s_serverArgs;


class WebServer2 : public WebServer
{
public:
  WebServer2(int port = 80);
  virtual void handleClient();
};
WebServer2::WebServer2(int port)
: WebServer(port)
{
  
}
void WebServer2::handleClient()
{
if (_currentStatus == HC_NONE) {
    GJ_PROFILE(WebServer2::handleClient::None);

    WiFiClient client = _server.available();
    if (!client) {
      return;
    }

    log_v("New client");

    _currentClient = client;
    _currentStatus = HC_WAIT_READ;
    _statusChange = millis();
  }

  bool keepCurrentClient = false;
  bool callYield = false;

  if (_currentClient.connected()) {
    GJ_PROFILE(WebServer2::handleClient::connected);

    switch (_currentStatus) {
    case HC_NONE:
      // No-op to avoid C++ compiler warning
      break;
    case HC_WAIT_READ:
      // Wait for data from client to become available
      if (_currentClient.available()) {
        if (_parseRequest(_currentClient)) {
          // because HTTP_MAX_SEND_WAIT is expressed in milliseconds,
          // it must be divided by 1000
          _currentClient.setTimeout(HTTP_MAX_SEND_WAIT / 1000);
          _contentLength = CONTENT_LENGTH_NOT_SET;
          _handleRequest();

          if (_currentClient.connected()) {
            _currentStatus = HC_WAIT_CLOSE;
            _statusChange = millis();
            keepCurrentClient = true;
          }
        }
      } else { // !_currentClient.available()
        if (millis() - _statusChange <= HTTP_MAX_DATA_WAIT) {
          keepCurrentClient = true;
        }
        callYield = true;
      }
      break;
    case HC_WAIT_CLOSE:
      // Wait for client to close the connection
      if (millis() - _statusChange <= HTTP_MAX_CLOSE_WAIT) {
        keepCurrentClient = true;
        callYield = true;
      }
    }
  }

  if (!keepCurrentClient) {
    GJ_PROFILE(WebServer2::handleClient::notkeepCurrentClient);

    _currentClient = WiFiClient();
    _currentStatus = HC_NONE;
    _currentUpload.reset();
  }

  if (callYield) {
    GJ_PROFILE(WebServer2::handleClient::yield);

    yield();
  }
}

WebServer2 *server = nullptr;

void StartGJAccessPoint(WebServer &server, GJServerArgs const &args);
void StartGJStation(WebServer &server, GJServerArgs const &args);
void UpdateAPServer(WebServer &server);
void UpdateSTAServer(WebServer &server);

void handleNotFound(WebServer &server) 
{
  //digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


bool IsWifiAPEnabled();

void StartServer(GJServerArgs const &args)
{
  if (!server)
    server = new WebServer2(80);

  LOG("Starting server\n\r");

  if (IsWifiAPEnabled())
  {
    LOG("  access point mode...\n\r");
    StartGJAccessPoint(*server, args);
  }
  else
  {
    LOG("  station mode...\n\r");
    StartGJStation(*server, args);
  }
  
  bool const onlineDateNeeded = IsOnlineDateNeeded();
  if (onlineDateNeeded && args.m_onlineTimeUpdate)
      (*args.m_onlineTimeUpdate)();
}


void InitServer(GJServerArgs const &args)
{
  s_serverArgs = args;

  auto onRequest = [=,ur=args.m_onRequest]()
  {
    //restart runtime to keep ESP alive while it's webpage is accessed
    ResetSpawnTime();

    ur();
  };

  StartServer(args);
}


void UpdateServer()
{
  GJ_PROFILE(UpdateOnline);

  if (!server)
    return;
  
  bool const wifiConnected = IsWifiConnected();
  if (wifiConnected)
  {
    UpdateSTAServer(*server);
  }
  else if (IsWifiAPEnabled())
  {
    UpdateAPServer(*server);
  }
}

#endif //GJ_SERVER
