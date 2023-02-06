#include "server.h"

#ifdef GJ_SERVER

#include "base.h"
#include "wificonfig.h"
#include "esputils.h"
#include <DNSServer.h>
#include <WebServer.h>

extern DNSServer dnsServer;

void handleAPRoot(WebServer &server) 
{
  
  const char *pageSource =
  
  "<!DOCTYPE html>"
  "<html lang=\"en\">"
  "<head> "
  "</head>"
  "  <body>"
  "<form action=\"/setconfig/\" method='post'> "
  "<label for=\"ssis\">ssid:</label><br>               "
  "<input type=\"text\" id=\"ssis\" name=\"ssis\"><br>     "
  "<label for=\"password\">password:</label><br>       "
  "<input type=\"text\" id=\"password\" name=\"password\"> "
  "<input type=\"submit\" value=\"Submit\">"
  "</form>                                           "
  "  </body>"
  "</html>";
  
  
  const char *redirect =
  
  "<html>                                                                                                       "
  " <head>                                                                                                      "
  "    <title>Redirecting to Captive Portal</title>                                                             "
  "    <meta http-equiv='refresh' content='0; url=/portal'>                                                  "
  " </head>                                                                                                     "
  " <body>                                                                                                      "
  "    <p>Please wait, refreshing.  If page does not refresh, click <a href='/portal'>here</a> to login.</p> "
  " </body>                                                                                                     "
  "</html>                                                                                                      ";

  static bool sent = false;
  
  if ( !sent)
  {
    sent = true;
    //server.sendHeader("location", "/portal");
    //server.send(200, "text/html", pageSource );
    server.send(200, "text/html", redirect );
  }
  else
  {
    server.send(200, "text/html", pageSource );
  }
  
  SER("AP root\n\r");
  SER("   Uri:%s\n\r", server.uri().c_str());
}

void handleAPPortal(WebServer &server) 
{
  const char *pageSource =
  
  "<!DOCTYPE html>"
  "<html lang=\"en\">"
  "<head> "
  "</head>"
  "  <body>"
  "     <form action=\"/setconfig/\" method='post'> "
  "      <label for=\"ssis\">ssid:</label><br>               "
  "        <input type=\"text\" id=\"ssis\" name=\"ssis\"><br>     "
  "      <label for=\"password\">password:</label><br>       "
  "        <input type=\"text\" id=\"password\" name=\"password\"> "
  "        <input type=\"submit\" value=\"Submit\">"
  "     </form>                                           "
  "  </body>"
  "</html>";
  
  server.send(200, "text", pageSource );
  
  SER("AP portal\n\r");
  SER("   Uri:%s\n\r", server.uri().c_str());
}



bool SetWifiConfig(const char *ssid, const char *pass);


void StartGJAccessPoint(WebServer &server, GJServerArgs const &args)
{
  auto onRoot = [=, onRequest = args.m_onRequest, server = &server]()
  {
    handleAPRoot(*server);

    onRequest();
  };
  
  auto onPortalRequest = [=, onRequest = args.m_onRequest, server = &server]()
  {
    //server.sendHeader("location", "/portal");
    //server.send(200, "text", "/portal" );
    
    const char *pageSource =  "<!DOCTYPE html>"  "<html lang=\"en\">"  "<head> "  "</head>"  "  <body>"  "<form action=\"/setconfig/\" method='post'> "  "<label for=\"ssis\">ssid:</label><br>               "  "<input type=\"text\" id=\"ssis\" name=\"ssis\"><br>     "  "<label for=\"password\">password:</label><br>       "  "<input type=\"text\" id=\"password\" name=\"password\"> "  "<input type=\"submit\" value=\"Submit\">"  "</form>                                           "  "  </body>"  "</html>";
    server->send(200, "text/html", pageSource );
    
    SER("AP portal request\n\r");
    SER("   Uri:%s\n\r", server->uri().c_str());
  };
  
  auto onPortal = [=, onRequest = args.m_onRequest, server = &server]()
  {
    handleAPPortal(*server);
    onRequest();
  };
  
  auto onSetConfig = [=, server = &server]()
  {
    int c = server->args();
    c = std::min<int>(c,2);
    String httpArgs[2];
    for ( int i = 0 ; i < c ; ++i)
    {
      httpArgs[i] = std::move(server->arg(i));
    }
    
    String const &ssid = httpArgs[0];
    String const &pass = httpArgs[1];

    SetWifiConfig(ssid.c_str(), pass.c_str());
  };
  
  server.on("/", onRoot);
  server.on("/portal", onPortal);
  server.on("/mobile/status.php", onPortalRequest);
  server.on("/setconfig/", onSetConfig);
  server.onNotFound(onRoot);
  server.begin();
}

void UpdateAPServer(WebServer &server)
{
  dnsServer.processNextRequest();
    server.handleClient();
}

#endif //GJ_SERVER