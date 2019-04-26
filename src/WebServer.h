#ifndef WebServer_h
#define WebServer_h

#include <Ethernet.h>
#include <ArduinoJson.h>

typedef void (*WebServerCallbackFunc)(EthernetClient &webClient, bool isPOSTRequest, const char *requestURI, bool isFileContentReceived, const char *fileContent);

class WebServer
{
private:
  EthernetServer _webServer;
  WebServerCallbackFunc _callback = NULL;

public:
  WebServer();
  void Begin(WebServerCallbackFunc callback);
  void Run();
};

#endif