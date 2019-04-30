#include <Arduino.h>
#include <avr/boot.h>
#include <avr/wdt.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <SimpleTimer.h> //!\ MAX_TIMERS = 1 /!\.

#include "WebServer.h"
#include "EventManager.h"

#include "HADevice.h"
#include "Light.h"
#include "RollerShutter.h"
#include "DS18B20Bus.h"
#include "PilotWire.h"
#include "DigitalOut.h"

//Web Resources
#include "data\pure-min.css.gz.h"
#include "data\side-menu.css.gz.h"
#include "data\side-menu.js.gz.h"
#include "data\index.html.gz.h"
#include "data\status0.html.gz.h"
#include "data\config0.html.gz.h"

#define VERSION "1.0"

#define DEFAULT_IP "192.168.1.177"

//this size is used to dimension 2 static memory allocation : (so consumption is 2 * GLOBAL_BUFFER_AND_JSONDOC_SIZE)
// - allocation of a globalBuffer for general purpose (including building HTTP answer packet to send)
// - allocation of a StaticJsonDocument that contains configuration loaded during setup()
#define GLOBAL_BUFFER_AND_JSONDOC_SIZE 1024 //minimum size is 1024 (web answer)

//GLOBAL USAGE
char globalBuffer[GLOBAL_BUFFER_AND_JSONDOC_SIZE];

//CONFIG variables
//config contains infos that need to be retained for Run
struct
{
  struct
  {
    char name[16 + 1] = {0}; //system name (max 16 char)
    IPAddress ip = (uint32_t)0;
  } system;
  struct
  {
    char hostname[32 + 1] = {0};
    uint32_t port = 1883;
    char username[16 + 1] = {0};
    char password[16 + 1] = {0};
    char baseTopic[16 + 1] = {0};
  } mqtt;
} config;

//eventManager store events to send to MQTT
EventManager eventManager;

//HADevice variables
uint8_t nbHADevices = 0;
HADevice **haDevices = NULL;

//ETHERNET variables
byte mac[6];
IPAddress ip;

//WebServer variable
WebServer webServer;

//MQTT variables
EthernetClient mqttEthClient;
PubSubClient mqttClient;
bool needMqttReconnect = false;
SimpleTimer mqttReconnectTimer;

//---------UTILS---------
void SoftwareReset()
{
  wdt_enable(WDTO_15MS);
  while (1)
  {
  }
}

//---------CONFIG---------
bool ConfigReadAndParseFromEEPROM(DynamicJsonDocument &configJSON, char *jsonBuffer, uint16_t jsonBufferSize)
{
  uint16_t i = 0;
  while (EEPROM[i] && i < jsonBufferSize)
  {
    jsonBuffer[i] = EEPROM[i];
    i++;
  }
  EEPROM[i] = 0;

  DeserializationError jsonError = deserializeJson(configJSON, jsonBuffer);
  if (jsonError)
    Serial.println(F("JSON parsing failed"));
  return !jsonError;
}

void ConfigReadSystemAndMQTT(DynamicJsonDocument &configJSON)
{
  //read System/name
  if (!configJSON[F("System")][F("name")].isNull() && strlen(configJSON[F("System")][F("name")].as<const char *>()) < sizeof(config.system.name))
    strcpy(config.system.name, configJSON[F("System")][F("name")].as<const char *>());
  else
    strcpy_P(config.system.name, PSTR("MegaMQTT"));

  Serial.print(F("[setup][Config] System/name="));
  Serial.println(config.system.name);

  //read System/ip
  if (!configJSON[F("System")][F("ip")].isNull())
  {
    IPAddress tmpIP;
    if (tmpIP.fromString(configJSON[F("System")][F("ip")].as<const char *>()))
      config.system.ip = tmpIP;
  }

  Serial.print(F("[setup][Config] System/ip="));
  config.system.ip.printTo(Serial);
  Serial.println();

  //read MQTT/hostname
  if (!configJSON[F("MQTT")][F("hostname")].isNull() && strlen(configJSON[F("MQTT")][F("hostname")].as<const char *>()) < sizeof(config.mqtt.hostname))
    strcpy(config.mqtt.hostname, configJSON[F("MQTT")][F("hostname")].as<const char *>());

  Serial.print(F("[setup][Config] MQTT/hostname="));
  Serial.println(config.mqtt.hostname);

  //read MQTT/port
  config.mqtt.port = configJSON[F("MQTT")][F("port")] | config.mqtt.port;

  Serial.print(F("[setup][Config] MQTT/port="));
  Serial.println(config.mqtt.port);

  //read MQTT/username
  if (!configJSON[F("MQTT")][F("username")].isNull() && strlen(configJSON[F("MQTT")][F("username")].as<const char *>()) < sizeof(config.mqtt.username))
    strcpy(config.mqtt.username, configJSON[F("MQTT")][F("username")].as<const char *>());

  Serial.print(F("[setup][Config] MQTT/username="));
  Serial.println(config.mqtt.username);

  //read MQTT/password
  if (!configJSON[F("MQTT")][F("password")].isNull() && strlen(configJSON[F("MQTT")][F("password")].as<const char *>()) < sizeof(config.mqtt.password))
    strcpy(config.mqtt.password, configJSON[F("MQTT")][F("password")].as<const char *>());

  Serial.print(F("[setup][Config] MQTT/password="));
  if (strlen(config.mqtt.password))
    Serial.println(F("***"));
  else
    Serial.println();

  //read MQTT/baseTopic
  if (!configJSON[F("MQTT")][F("baseTopic")].isNull() && strlen(configJSON[F("MQTT")][F("baseTopic")].as<const char *>()) < sizeof(config.mqtt.baseTopic))
    strcpy(config.mqtt.baseTopic, configJSON[F("MQTT")][F("baseTopic")].as<const char *>());
  Serial.print(F("[setup][Config] MQTT/baseTopic="));
  Serial.println(config.mqtt.baseTopic);
}

void ConfigSaveJsonToEEPROM(const char *json)
{
  for (uint16_t i = 0; i < strlen(json); i++)
    EEPROM[i] = json[i];
  EEPROM[strlen(json)] = 0;
}

void ConfigCreateHADevices(DynamicJsonDocument &configJSON)
{
  //if HADevices is in JSON and not empty
  if (!configJSON[F("HADevices")].isNull() && configJSON[F("HADevices")].size())
  {
    //take number of HADevices to create
    nbHADevices = configJSON[F("HADevices")].size();

    //create array of pointer
    haDevices = new HADevice *[nbHADevices];
    //initialize pointers to NULL
    memset(haDevices, 0, nbHADevices * sizeof(HADevice *));

    //for each HADevices
    for (uint8_t i = 0; i < nbHADevices; i++)
    {
      //if device type is Light
      if (!strcmp_P(configJSON[F("HADevices")][i][F("type")].as<const char *>(), PSTR("Light")))
        haDevices[i] = (HADevice *)new Light(configJSON[F("HADevices")][i].as<JsonVariant>(), &eventManager); //create a Light
      //if device type is RollerShutter
      else if (!strcmp_P(configJSON[F("HADevices")][i][F("type")].as<const char *>(), PSTR("RollerShutter")))
        haDevices[i] = (HADevice *)new RollerShutter(configJSON[F("HADevices")][i].as<JsonVariant>(), &eventManager); //create a RollerShutter
      //if device type is DS18B20Bus
      else if (!strcmp_P(configJSON[F("HADevices")][i][F("type")].as<const char *>(), PSTR("DS18B20Bus")))
        haDevices[i] = (HADevice *)new DS18B20Bus(configJSON[F("HADevices")][i].as<JsonVariant>(), &eventManager); //create a DS18B20Bus
      //if device type is PilotWire
      else if (!strcmp_P(configJSON[F("HADevices")][i][F("type")].as<const char *>(), PSTR("PilotWire")))
        haDevices[i] = (HADevice *)new PilotWire(configJSON[F("HADevices")][i].as<JsonVariant>(), &eventManager); //create a PilotWire
      //if device type is DigitalOut
      else if (!strcmp_P(configJSON[F("HADevices")][i][F("type")].as<const char *>(), PSTR("DigitalOut")))
        haDevices[i] = (HADevice *)new DigitalOut(configJSON[F("HADevices")][i].as<JsonVariant>(), &eventManager); //create a DigitalOut
    }
  }
}

//---------ETHERNET---------
bool EthernetConnect(uint8_t *mac, IPAddress &requestedIP)
{
  Serial.print(F("[EthernetConnect] Starting with MAC="));
  sprintf_P(globalBuffer, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print(globalBuffer);

  //if no requestedIP configured
  if (!requestedIP[0] && !requestedIP[1] && !requestedIP[2] && !requestedIP[3])
    // {
    //   Serial.println(F(" and DHCP"));
    //   //try DHCP
    //   if (Ethernet.begin(mac) == 0)
    //   {
    //     //if DHCP failed, use default IP
    requestedIP.fromString(DEFAULT_IP);
  //     Serial.print(F("[EthernetConnect] DHCP failed, switch to default IP ("));
  //     requestedIP.printTo(Serial);
  //     Serial.println(')');
  //     Ethernet.begin(mac, requestedIP);
  //   }
  // }
  // else
  // {
  Serial.print(F(" and ip="));
  requestedIP.printTo(Serial);
  Serial.println();
  Ethernet.begin(mac, requestedIP);
  //}

  if (Ethernet.hardwareStatus() == EthernetNoHardware)
  {
    Serial.println(F("[EthernetConnect] Ethernet shield not found!!! Restart..."));
    SoftwareReset();
  }

  if (Ethernet.linkStatus() == LinkOFF)
    Serial.println(F("[EthernetConnect] Ethernet cable is not connected."));

  return Ethernet.linkStatus() != LinkOFF;
}

//---------WEBSERVER---------
void WebServerCallback(EthernetClient &webClient, bool isPOSTRequest, const char *requestURI, bool isFileContentReceived, const char *fileContent)
{
  //if GET request
  if (!isPOSTRequest)
  {

    //look for the right URI and :
    // - put header into globalBuffer
    // - set content pointer into contentPtr
    // - set content size into contentSize
    // - do not return 404

    bool return404 = true;
    const uint8_t *contentPtr = NULL;
    uint16_t contentSize = 0;

    if (!strcmp_P(requestURI, PSTR("/pure-min.css")))
    {
      //build Header
      sprintf_P(globalBuffer, PSTR("HTTP/1.1 200 OK\r\nConnection: close\r\nAccept-Ranges: none\r\nCache-Control: max-age=604800, public\r\nContent-Type: text/css\r\nContent-Encoding: gzip\r\nContent-Length: %d\r\n\r\n"), (uint16_t)sizeof(puremincssgz));
      contentPtr = puremincssgz;
      contentSize = sizeof(puremincssgz);
      return404 = false;
    }
    else if (!strcmp_P(requestURI, PSTR("/side-menu.css")))
    {
      //build Header
      sprintf_P(globalBuffer, PSTR("HTTP/1.1 200 OK\r\nConnection: close\r\nAccept-Ranges: none\r\nCache-Control: max-age=604800, public\r\nContent-Type: text/css\r\nContent-Encoding: gzip\r\nContent-Length: %d\r\n\r\n"), (uint16_t)sizeof(sidemenucssgz));
      contentPtr = sidemenucssgz;
      contentSize = sizeof(sidemenucssgz);
      return404 = false;
    }
    else if (!strcmp_P(requestURI, PSTR("/side-menu.js")))
    {
      //build Header
      sprintf_P(globalBuffer, PSTR("HTTP/1.1 200 OK\r\nConnection: close\r\nAccept-Ranges: none\r\nCache-Control: max-age=604800, public\r\nContent-Type: text/javascript\r\nContent-Encoding: gzip\r\nContent-Length: %d\r\n\r\n"), (uint16_t)sizeof(sidemenujsgz));
      contentPtr = sidemenujsgz;
      contentSize = sizeof(sidemenujsgz);
      return404 = false;
    }
    else if (!strcmp_P(requestURI, PSTR("/")))
    {
      //build Header
      sprintf_P(globalBuffer, PSTR("HTTP/1.1 200 OK\r\nConnection: close\r\nAccept-Ranges: none\r\nContent-Type: text/html\r\nContent-Encoding: gzip\r\nContent-Length: %d\r\n\r\n"), (uint16_t)sizeof(indexhtmlgz));
      contentPtr = indexhtmlgz;
      contentSize = sizeof(indexhtmlgz);
      return404 = false;
    }
    else if (!strcmp_P(requestURI, PSTR("/status0.html")))
    {
      //build Header
      sprintf_P(globalBuffer, PSTR("HTTP/1.1 200 OK\r\nConnection: close\r\nAccept-Ranges: none\r\nContent-Type: text/html\r\nContent-Encoding: gzip\r\nContent-Length: %d\r\n\r\n"), (uint16_t)sizeof(status0htmlgz));
      contentPtr = status0htmlgz;
      contentSize = sizeof(status0htmlgz);
      return404 = false;
    }
    else if (!strcmp_P(requestURI, PSTR("/config0.html")))
    {
      //build Header
      sprintf_P(globalBuffer, PSTR("HTTP/1.1 200 OK\r\nConnection: close\r\nAccept-Ranges: none\r\nContent-Type: text/html\r\nContent-Encoding: gzip\r\nContent-Length: %d\r\n\r\n"), (uint16_t)sizeof(config0htmlgz));
      contentPtr = config0htmlgz;
      contentSize = sizeof(config0htmlgz);
      return404 = false;
    }
    else if (!strcmp_P(requestURI, PSTR("/gs0")))
    {
      unsigned long minutes = millis() / 60000;
      uint16_t jsonSize;

      //define size of resultantJSON
      sprintf_P(globalBuffer, PSTR("{\"n\":\"%s\",\"b\":\"%s\",\"u\":\"%dd%02dh%02dm\"}"), config.system.name, VERSION, (minutes / 1440), (minutes / 60 % 24), (minutes % 60));
      jsonSize = strlen(globalBuffer);
      //build Header+Content
      sprintf_P(globalBuffer, PSTR("HTTP/1.1 200 OK\r\nConnection: close\r\nAccept-Ranges: none\r\nCache-Control: no-cache\r\nContent-Type: text/json\r\nContent-Length: %d\r\n\r\n"), jsonSize);
      sprintf_P(globalBuffer + strlen(globalBuffer), PSTR("{\"n\":\"%s\",\"b\":\"%s\",\"u\":\"%dd%02dh%02dm\"}"), config.system.name, VERSION, (minutes / 1440), (minutes / 60 % 24), (minutes % 60));
      return404 = false;
    }

    //Answer to the client
    if (!return404)
    {
      //Send Header
      webClient.write(globalBuffer, strlen(globalBuffer));

      //Then Send Content
      for (uint16_t pos = 0; pos < contentSize; pos += 1024)
      {
        memcpy_P(globalBuffer, contentPtr + pos, ((pos + 1024) < contentSize) ? 1024 : (contentSize - pos));
        webClient.write(globalBuffer, ((pos + 1024) < contentSize) ? 1024 : (contentSize - pos));
      }
    }
    else
    {
      //build 404 answer
      strcpy_P(globalBuffer, PSTR("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\nAccept-Ranges: none\r\n\r\n"));
      //Send it
      webClient.write(globalBuffer, strlen(globalBuffer));
    }

    delay(1);         //give webClient time to receive the data
    webClient.stop(); //close the connection
  }
  else //else that is a POST request
  {
    //if JSON Config file POSTed
    if (!strcmp_P(requestURI, PSTR("/conf")))
    {
      struct GlobalBufferAllocator
      {
        void *allocate(size_t n)
        {
          if (n > GLOBAL_BUFFER_AND_JSONDOC_SIZE)
          {
            Serial.println(F("GlobalBufferAllocator can't go over globalBuffer size"));
            SoftwareReset();
          }
          return (void *)globalBuffer;
        }
        void deallocate(void *p) {}
      };
      typedef BasicJsonDocument<GlobalBufferAllocator> JsonDocumentInGlobalBuffer;

      //Try to deserialize it
      JsonDocumentInGlobalBuffer configJSONInGlobalBuffer(GLOBAL_BUFFER_AND_JSONDOC_SIZE); //Warning : Heap already has fileContent and will get this JSON too...
      DeserializationError jsonError = deserializeJson(configJSONInGlobalBuffer, fileContent);
      //if deserialization succeed
      if (!jsonError)
      {
        if (configJSONInGlobalBuffer[F("System")][F("name")].isNull())
        {
          Serial.println(F("[WebServerCallback] System/name is missing from new JSON"));
          webClient.println(F("HTTP/1.1 400 Bad Request\r\n\r\nSystem/name is missing from JSON Config file"));
          return;
        }

        Serial.println(F("[WebServerCallback] Save JSON to EEPROM"));

        //Save JSON to EEPROM
        ConfigSaveJsonToEEPROM(fileContent);

        //Answer to the webClient
        webClient.println(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nJSON Config file saved"));

        delay(1);         //give webClient time to receive the data
        webClient.stop(); //close the connection

        Serial.println(F("[WebServerCallback] Reboot"));
        SoftwareReset();
      }
      else
        webClient.println(F("HTTP/1.1 400 Bad Request\r\n\r\nJSON Config file parse failed"));
    }
  }
}

//---------MQTT---------
// Connect then Subscribe to MQTT
bool MqttConnect()
{
  if (Ethernet.linkStatus() == LinkOFF)
    return false;

  //Generate CLientID
  char clientID[18];
  sprintf_P(clientID, PSTR("%02x:%02x:%02x:%02x:%02x:%02x"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  //Connect
  if (!config.mqtt.username[0])
    mqttClient.connect(clientID);
  else
    mqttClient.connect(clientID, config.mqtt.username, config.mqtt.password);

  //Subscribe to needed topic
  if (mqttClient.connected())
  {
    for (uint8_t i = 0; i < nbHADevices; i++)
      if (haDevices[i])
        haDevices[i]->MqttSubscribe(mqttClient, config.mqtt.baseTopic);
  }

  return mqttClient.connected();
}

void MqttCallback(char *topic, uint8_t *payload, unsigned int length)
{
  char *relevantPartOfTopic = topic + strlen(config.mqtt.baseTopic);
  if (config.mqtt.baseTopic[strlen(config.mqtt.baseTopic) - 1] != '/')
    relevantPartOfTopic++;

  bool messageHandled = false;

  for (uint8_t i = 0; i < nbHADevices && !messageHandled; i++)
    if (haDevices[i])
      messageHandled = haDevices[i]->MqttCallback(relevantPartOfTopic, payload, length);
}

bool MqttStart()
{
  //setup MQTT client (PubSubClient)
  mqttClient.setClient(mqttEthClient).setServer(config.mqtt.hostname, config.mqtt.port).setCallback(MqttCallback);

  //Then connect
  if (MqttConnect())
    return true;

  return false;
}

void MqttRun()
{
  //If MQTT need to be reconnected
  if (needMqttReconnect)
  {
    needMqttReconnect = false;
    Serial.println(F("[MQTTRun] Reconnection"));
    if (MqttConnect())
      Serial.println(F("[MQTTRun] Reconnection : OK"));
    else
      Serial.println(F("[MQTTRun] Reconnection : Failed"));
  }

  //if MQTT not connected and reconnect timer not started
  if (!mqttClient.connected() && !mqttReconnectTimer.isEnabled(0))
  {
    Serial.println(F("[MQTTRun] Disconnected"));
    //set Timer to reconnect after 20 or 60 sec (Eth connected or not)
    mqttReconnectTimer.setTimeout((Ethernet.linkStatus() != LinkOFF) ? 20000 : 60000, []() { needMqttReconnect = true; mqttReconnectTimer.deleteTimer(0); });
  }

  //Run mqttReconnectTimer
  mqttReconnectTimer.run();

  //Run mqttClient
  mqttClient.loop();
}

//---------SETUP---------
void setup()
{
  //Start serial
  Serial.begin(115200);

  //Create JsonDocument
  DynamicJsonDocument configJSON(GLOBAL_BUFFER_AND_JSONDOC_SIZE);

  //Load JSON from EEPROM
  Serial.println(F("[setup]Config JSON"));
  if (ConfigReadAndParseFromEEPROM(configJSON, globalBuffer, sizeof(globalBuffer)))
  {
    ConfigReadSystemAndMQTT(configJSON);
    Serial.println(F("[setup]Config JSON : OK\n"));
  }
  else
    Serial.println(F("[setup]Config JSON : FAILED\n"));

  //Create Home Automation Objects
  Serial.println(F("[setup]HADevices"));
  ConfigCreateHADevices(configJSON);
  Serial.println(F("[setup]HADevices : Done\n"));

  //Start Ethernet
  Serial.println(F("[setup]Ethernet"));

  //build MAC address based on hidden ATMega2560 serial number
  mac[0] = 0xDE;
  mac[1] = 0xAD;
  mac[2] = 0xBE;
  mac[3] = 0xEF;
  mac[4] = boot_signature_byte_get(0x16);
  mac[5] = boot_signature_byte_get(0x17);

  if (EthernetConnect(mac, config.system.ip))
    Serial.println(F("[setup]Ethernet : OK\n"));
  else
    Serial.println(F("[setup]Ethernet : FAILED\n"));

  //Start WebServer
  Serial.println(F("[setup]WebServer"));
  webServer.Begin(WebServerCallback);
  Serial.println(F("[setup]WebServer : Started\n"));

  //Start MQTT
  Serial.println(F("[setup]MQTT client"));
  if (MqttStart())
    Serial.println(F("[setup]MQTT client : OK\n"));
  else
    Serial.println(F("[setup]MQTT client : FAILED\n"));
}

//---------LOOP---------
void loop()
{
  bool timeCriticalOperationInProgress = false;

  //------------------------HOME AUTOMATION------------------------
  for (uint8_t i = 0; i < nbHADevices; i++)
    if (haDevices[i])
      timeCriticalOperationInProgress |= haDevices[i]->Run();

  //if time critical operation is in progress, then skip rest of code
  if (timeCriticalOperationInProgress)
    return;

  //------------------------WEBSERVER------------------------
  webServer.Run();

  //------------------------MQTT------------------------
  MqttRun();
  //publish Events
  EventManager::Event *evtToSend;
  bool publishSucceeded = true;
  //while MQTTconnected and publish works and there is an event to send
  while (mqttClient.connected() && publishSucceeded && (evtToSend = eventManager.Available()))
  {
    //build complete topic in globalBuffer : baseTopic(with ending /) + topic in the event
    strcpy(globalBuffer, config.mqtt.baseTopic);
    if (globalBuffer[strlen(globalBuffer) - 1] != '/')
      strcat_P(globalBuffer, PSTR("/"));
    strcat(globalBuffer, evtToSend->topic);
    //publish
    if ((publishSucceeded = mqttClient.publish(globalBuffer, evtToSend->payload)))
      evtToSend->sent = true; //if that works, then tag event as sent
    else
      evtToSend->retryLeft--; //else decrease retry count
  }
}