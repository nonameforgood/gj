#include "server.h"

#ifdef GJ_SERVER

#include "base.h"
#include "file.h"
#include "serial.h"
#include <WebServer.h>
#include "commands.h"
#include "esputils.h"
#include "gjonline.h"

//#include <ESPmDNS.h>


extern const char *g_serverBuildDate;
extern GJServerArgs s_serverArgs;

void handleNotFound(WebServer &server);


void handleSTARoot(GJServerArgs const &args, WebServer &server)
{
  const char *message = nullptr;


  const char *rootPart1 =
  
  "<!DOCTYPE html>"
  "<html lang=\"en\">"
  "<head> "
  "</head>"
  "  <body>";

  const char *rootPart2 =
  "     <a href=\"/command/\">Commands</a><br>"
  "     <a href=\"/command/?command=dumplog\">Log</a><br>"
  "     <a href=\"/command/?command=dumprecentlog\">Dump recent logs</a><br>"
  "     <br>"
  "  </body>"
  "</html>";


  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", rootPart1 );

  char hostNameBuffer[64];
  char const *hostName = args.m_hostName;
  if (!hostName || !hostName[0])
  {
    hostName = hostNameBuffer;
    uint8_t mac[6] = {};
    esp_efuse_mac_get_default(mac);

    sprintf(hostNameBuffer, "DefaultHostName-%02X%02X%02X%02X%02X%02X", 
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }
  char buffer[64];
  sprintf(buffer, "<p>%s<p><br><br>", hostName);
  server.sendContent_P(buffer);

  sprintf(buffer, "<p>Build date:%s<p><br><br>", g_serverBuildDate);
  server.sendContent_P(buffer);

  if (message)
    server.sendContent_P(message);

  server.sendContent_P(rootPart2);
}

void handleCommand(GJServerArgs const &args, WebServer &server)
{
  const char *message = nullptr;
  const char *rootPart1a =
    "<!DOCTYPE html>"
    "<html lang=\"en\">"
    "<head> "
    "</head>"
    "  <body>";

  const char *rootPart1b =
    
    "<table style='width:100%'>"
    "  <tr style='font2-size:24px;'>"
    "    <td style='width:100px'>" 
    "      <a href='/'>Home</a><br>";
  const char *rootPart1c =

    "      <form action='/command/' method='post'> "
    "        <label for='command'>command:</label><br>               "
    "        <input type='text' id='command' name='command'><br>     "
    "        <input type='submit' value='Submit'>"
    "      </form>                                           ";

                            

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", rootPart1a );
  server.sendContent_P(rootPart1b );
  server.sendContent_P(rootPart1c );

  Vector<const char*> commands;
  GetCommands(commands);

  char commandBuffer[128];
  for (const char *command : commands)
  {
    sprintf(commandBuffer, "     <a href='/command/?command=%s'>Run:%s</a><br>", command, command);
    server.sendContent_P(commandBuffer);
  }

  const char *rootPart2 =
    "    </td>"
    "    <td style='height:100%'>"
    "      <textarea autocomplete='off' autocorrect='off' autocapitalize='off' spellcheck='false' id='w3review' name='w3review' rows='50' style='width: 100%;'>";

  server.sendContent_P(rootPart2 );

  if (server.hasArg("command"))
  {
    String command = server.arg("command");
    
    String adjustStorage;
    
    auto netTerminal = [&](const char *text)
    {
      text = RemoveNewLineLineFeed(text, adjustStorage);
      if (text[0])
      {
        server.sendContent_P(text);
      }
    };
    
    uint32_t const terminalIndex = AddTerminalHandler(netTerminal);
    InterpretCommand(command.c_str());
    RemoveTerminalHandler(terminalIndex);
  }

  const char *rootPart3 =
    "      </textarea>"
    "    </td>"
    "  </tr>"
    "</table>"
    "  </body>"
    "</html>";
  server.sendContent_P(rootPart3);
}


void StartGJStation(WebServer &server, GJServerArgs const &args)
{
  //if (!MDNS.begin("esp8266")) {
  //  SER("ERROR:MDNS responder not started\n\r");
  //}

  auto onRoot = [=, onRequest = args.m_onRequest, server = &server]()
  {
    handleSTARoot(s_serverArgs, *server);
    onRequest();
  };

  
  auto onCommand = [=, args, onRequest = args.m_onRequest, server = &server]()
  {
    handleCommand(args, *server);
    onRequest();
  };

  auto onNotFound = [=, onRequest = args.m_onRequest, server = &server]()
  {
    handleNotFound(*server);
    onRequest();
  };

  server.on("/command/", onCommand);
  
  server.onNotFound(onNotFound);

  {
    server.on("/", onRoot);
    server.begin();
  }
  //SER("HTTP server started\n\r");
}

void UpdateSTAServer(WebServer &server)
{
  {
    GJ_PROFILE(server.handleClient);
    server.handleClient();
  }
}

#endif //GJ_SERVER