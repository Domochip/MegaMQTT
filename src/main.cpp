#include <Arduino.h>
#include <avr/boot.h>
#include <avr/wdt.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <SimpleTimer.h> //!\ MAX_TIMERS = 1 /!\.

#include "EventManager.h"

#include "Light.h"
#include "RollerShutter.h"

#define JSON_BUFFER_MAX_SIZE 1024
#define JSON_DOCUMENT_MAX_SIZE 1024

//CONFIG variable
StaticJsonDocument<JSON_DOCUMENT_MAX_SIZE> jsonDoc;

EventManager eventManager;

//HA variables
uint8_t nbLights = 0;
Light **lights = NULL;
uint8_t nbRollerShutters = 0;
RollerShutter **rollerShutters = NULL;

//ETHERNET variables
byte mac[6];
IPAddress ip(192, 168, 1, 177);

//WEBSERVER variables
EthernetServer webServer(80);

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
void ConfigReadJson(char *jsonBuffer)
{
  uint16_t i = 0;
  while (EEPROM[i])
  {
    jsonBuffer[i] = EEPROM[i];
    i++;
  }
  EEPROM[i] = 0;
}

void ConfigSaveJson(const char *json)
{
  for (uint16_t i = 0; i < strlen(json); i++)
    EEPROM[i] = json[i];
  EEPROM[strlen(json)] = 0;
}

bool ConfigParse(const char *jsonBuffer)
{
  DeserializationError jsonError = deserializeJson(jsonDoc, jsonBuffer);
  if (jsonError)
    Serial.println(F("JSON parsing failed"));
  return !jsonError;
}

void ConfigCreateHAObjects()
{
  //if Light is in JSON and not empty
  if (!jsonDoc[F("Light")].isNull() && jsonDoc[F("Light")].size())
  {
    nbLights = jsonDoc[F("Light")].size();
    lights = new Light *[nbLights];
    for (byte i = 0; i < nbLights; i++)
    {
      lights[i] = new Light(jsonDoc[F("Light")][i].as<JsonVariant>(), &eventManager);
    }
  }

  //if RollerShutter is in JSON and not empty
  if (!jsonDoc[F("RollerShutter")].isNull() && jsonDoc[F("RollerShutter")].size())
  {
    nbRollerShutters = jsonDoc[F("RollerShutter")].size();
    rollerShutters = new RollerShutter *[nbRollerShutters];
    for (byte i = 0; i < nbRollerShutters; i++)
    {
      rollerShutters[i] = new RollerShutter(jsonDoc[F("RollerShutter")][i].as<JsonVariant>(), &eventManager);
    }
  }
}

//---------WEBSERVER---------
void WebServerStart()
{
  // If webServer not yet started then start it
  if (!webServer)
    webServer.begin();
}

void WebServerRun()
{
  //take and check if a webClient is there
  EthernetClient webClient = webServer.available();
  if (!webClient)
    return;

  //if it's connected
  if (webClient.connected())
  {
    //-------------------Receive and parse his request-------------------
    bool isRequestHeaderFinished = false;
    bool isPOSTRequest = false;
    String requestURI; //URL requested
    String requestBoundary;
    String fileContent;

    while (webClient.available() && !isRequestHeaderFinished)
    {
      String reqLine = webClient.readStringUntil('\n');

      //DEBUG
      // Serial.print(F("->"));
      // Serial.println(reqLine);
      // Serial.print(F("--+ Length : "));
      // Serial.println(reqLine.length());

      //Parse method and URI
      if (reqLine.endsWith(F(" HTTP/1.1\r")))
      {
        if (reqLine.startsWith(F("GET ")))
          requestURI = reqLine.substring(4, reqLine.length() - 10);
        if (reqLine.startsWith(F("POST ")))
        {
          isPOSTRequest = true;
          requestURI = reqLine.substring(5, reqLine.length() - 10);
        }
      }

      //Parse boundary (if file is POSTed)
      if (reqLine.startsWith(F("Content-Type: multipart/form-data; boundary=")))
      {
        requestBoundary = reqLine.substring(44, reqLine.length() - 1);
      }

      //Does request Header is finished
      if (reqLine.length() == 1 && reqLine[0] == '\r')
        isRequestHeaderFinished = true;
    }

    //Try to receive file content (only for POST request)
    bool isFileContentReceived = false;
    if (isPOSTRequest)
    {
      //look for boundary
      if (webClient.find((char *)requestBoundary.c_str()))
      {
        //then go to next empty line (so skip it)
        webClient.find((char *)"\r\n\r\n");

        //complete requestBoundary to find its end
        requestBoundary = "--" + requestBoundary + '\r';

        //and finally read file content until boundary end string is found
        while (webClient.available() && !isFileContentReceived)
        {
          //read content line
          String dataLine = webClient.readStringUntil('\n');
          //if it doesn't match end of boundary
          if (dataLine.compareTo(requestBoundary))
            fileContent += dataLine + '\n'; //add it to fileContent with removed \n
          else
            isFileContentReceived = true; //else end of file content is there
        }
      }
    }

    //-------------------Answer to the request-------------------
    //if GET request
    if (!isPOSTRequest)
    {
      webClient.println(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nNothing Here Yet ;-)"));
      delay(1);         //give webClient time to receive the data
      webClient.stop(); //close the connection
    }
    else //else that is a POST request
    {
      //if JSON Config file POSTed
      if (requestURI == F("/conf"))
      {
        //Try to deserialize it
        DynamicJsonDocument dynJsonDoc(JSON_DOCUMENT_MAX_SIZE); //Caution : Heap already has fileContent and will get this JSON too...
        DeserializationError jsonError = deserializeJson(dynJsonDoc, fileContent);
        //if deserialization succeed
        if (!jsonError)
        {
          Serial.println(F("Save JSON to EEPROM"));

          //Save JSON to EEPROM
          ConfigSaveJson(fileContent.c_str());

          //Answer to the webClient
          webClient.println(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nJSON Config file saved"));

          delay(1);         //give webClient time to receive the data
          webClient.stop(); //close the connection

          Serial.println(F("Reboot"));
          SoftwareReset();
        }
        else
          webClient.println(F("HTTP/1.1 400 Bad Request\r\n\r\nIncorrect JSON Config file"));

        //Clean Dynamic JSON Doc
        dynJsonDoc.clear();
      }
    }

    //DEBUG
    Serial.print(F("Request Method : "));
    Serial.println(isPOSTRequest ? F("POST") : F("GET"));
    Serial.print(F("Request URI : "));
    Serial.println(requestURI);
    Serial.print(F("Request Boundary : "));
    Serial.println(requestBoundary);
    Serial.print(F("Request File received : "));
    Serial.println(isFileContentReceived ? F("YES") : F("NO"));
    Serial.print(F("Request File content : "));
    Serial.println(fileContent);
  }
  webClient.stop();
}

//---------ETHERNET---------
bool EthernetConnect()
{
  //build MAC address based on hidden ATMega2560 serial number
  mac[0] = 0xDE;
  mac[1] = 0xAD;
  mac[2] = 0xBE;
  mac[3] = 0xEF;
  mac[4] = boot_signature_byte_get(0x16);
  mac[5] = boot_signature_byte_get(0x17);

  Ethernet.begin(mac, ip);

  if (Ethernet.hardwareStatus() == EthernetNoHardware)
  {
    Serial.println(F("Ethernet shield not found!!! Restart..."));
    SoftwareReset();
  }

  if (Ethernet.linkStatus() == LinkOFF)
    Serial.println(F("Ethernet cable is not connected."));

  return Ethernet.linkStatus() != LinkOFF;
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
  if (!jsonDoc[F("MQTT")][F("username")].as<const char *>()[0])
    mqttClient.connect(clientID);
  else
    mqttClient.connect(clientID, jsonDoc[F("MQTT")][F("username")].as<const char *>(), jsonDoc[F("MQTT")][F("password")].as<const char *>());

  //Subscribe to needed topic
  if (mqttClient.connected())
  {
    if (nbLights)
      for (uint8_t i = 0; i < nbLights; i++)
        lights[i]->MqttSubscribe(mqttClient, jsonDoc[F("baseTopic")].as<const char *>());

    if (nbRollerShutters)
    {
      for (uint8_t i = 0; i < nbRollerShutters; i++)
        rollerShutters[i]->MqttSubscribe(mqttClient, jsonDoc[F("baseTopic")].as<const char *>());
    }
  }

  return mqttClient.connected();
}

void MqttCallback(char *topic, uint8_t *payload, unsigned int length)
{
  bool messageHandled = false;

  const char *baseTopic = jsonDoc[F("baseTopic")].as<const char *>();
  char *relevantPartOfTopic = topic + strlen(baseTopic);
  if (baseTopic[strlen(baseTopic) - 1] != '/')
    relevantPartOfTopic++;

  if (nbLights)
    for (uint8_t i = 0; i < nbLights && !messageHandled; i++)
      messageHandled = lights[i]->MqttCallback(relevantPartOfTopic, payload, length);

  if (nbRollerShutters)
    for (uint8_t i = 0; i < nbRollerShutters && !messageHandled; i++)
      messageHandled = rollerShutters[i]->MqttCallback(relevantPartOfTopic, payload, length);
}

void MqttStart()
{
  //setup MQTT client (PubSubClient)
  mqttClient.setClient(mqttEthClient).setServer(jsonDoc[F("MQTT")][F("hostname")].as<const char *>(), jsonDoc[F("MQTT")][F("port")]).setCallback(MqttCallback);

  //Then connect
  if (MqttConnect())
    Serial.println(F("MQTT connected"));
  else
    Serial.println(F("MQTT not Connected"));
}

void MqttRun()
{
  //If MQTT need to be reconnected
  if (needMqttReconnect)
  {
    needMqttReconnect = false;
    Serial.print(F("MQTT Reconnection : "));
    if (MqttConnect())
      Serial.println(F("OK"));
    else
      Serial.println(F("Failed"));
  }

  //if MQTT not connected and reconnect timer not started
  if (!mqttClient.connected() && !mqttReconnectTimer.isEnabled(0))
  {
    Serial.println(F("MQTT Disconnected"));
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

  //Load JSON from EEPROM
  char *jsonBuffer = (char *)malloc(JSON_BUFFER_MAX_SIZE + 1);
  ConfigReadJson(jsonBuffer);
  ConfigParse(jsonBuffer);
  free(jsonBuffer);

  //Create Home Automation Objects
  ConfigCreateHAObjects();

  //Start Ethernet
  EthernetConnect();

  //Start WebServer
  WebServerStart();

  //Start MQTT
  MqttStart();
}

//---------LOOP---------
void loop()
{
  bool timeCriticalOperationInProgress = false;

  //------------------------HOME AUTOMATION------------------------
  if (nbLights)
    for (uint8_t i = 0; i < nbLights; i++)
      timeCriticalOperationInProgress |= lights[i]->Run();

  if (nbRollerShutters)
  {
    for (uint8_t i = 0; i < nbRollerShutters; i++)
      timeCriticalOperationInProgress |= rollerShutters[i]->Run();
  }

  //if time critical operation is in progress, so skip rest of code
  if (timeCriticalOperationInProgress)
    return;

  //------------------------WEBSERVER------------------------
  WebServerRun();

  //------------------------MQTT------------------------
  MqttRun();
  //publish Events
  EventManager::Event *evtToSend;
  bool publishSucceeded = true;
  String completeTopic;
  completeTopic.reserve(16 + 1 + sizeof(EventManager::Event::topic)); //baseTopic(16)+/+Event.topic size
  //while publish works and there is an event to send
  while (publishSucceeded && (evtToSend = eventManager.Available()))
  {
    //build complete topic : baseTopic(with ending /) + topic in the event
    completeTopic = jsonDoc[F("baseTopic")].as<const char *>();
    if (completeTopic[completeTopic.length() - 1] != '/')
      completeTopic += '/';
    completeTopic += evtToSend->topic;
    //publish
    if ((publishSucceeded = mqttClient.publish(completeTopic.c_str(), evtToSend->payload)))
      evtToSend->sent = true; //if that works, then tag event as sent
    else
      evtToSend->retryLeft--; //else decrease retry count
  }
}