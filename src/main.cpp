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

#define JSON_BUFFER_MAX_SIZE 1024
#define JSON_DOCUMENT_MAX_SIZE 1024

//CONFIG variable
StaticJsonDocument<JSON_DOCUMENT_MAX_SIZE> jsonDoc;

EventManager eventManager;

//HADevice variables
uint8_t nbHADevices = 0;
HADevice **haDevices = NULL;

//ETHERNET variables
byte mac[6];
IPAddress ip(192, 168, 1, 177);

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

void ConfigCreateHADevices()
{
  //if HADevices is in JSON and not empty
  if (!jsonDoc[F("HADevices")].isNull() && jsonDoc[F("HADevices")].size())
  {
    //take number of HADevices to create
    nbHADevices = jsonDoc[F("HADevices")].size();

    //create array of pointer
    haDevices = new HADevice *[nbHADevices];
    //initialize pointers to NULL
    memset(haDevices, 0, nbHADevices * sizeof(HADevice *));

    //for each HADevices
    for (uint8_t i = 0; i < nbHADevices; i++)
    {
      //if device type is Light
      if (!strcmp_P(jsonDoc[F("HADevices")][i][F("type")].as<const char *>(), PSTR("Light")))
        haDevices[i] = (HADevice *)new Light(jsonDoc[F("HADevices")][i].as<JsonVariant>(), &eventManager); //create a Light
      //if device type is RollerShutter
      else if (!strcmp_P(jsonDoc[F("HADevices")][i][F("type")].as<const char *>(), PSTR("RollerShutter")))
        haDevices[i] = (HADevice *)new RollerShutter(jsonDoc[F("HADevices")][i].as<JsonVariant>(), &eventManager); //create a RollerShutter
      //if device type is DS18B20Bus
      else if (!strcmp_P(jsonDoc[F("HADevices")][i][F("type")].as<const char *>(), PSTR("DS18B20Bus")))
        haDevices[i] = (HADevice *)new DS18B20Bus(jsonDoc[F("HADevices")][i].as<JsonVariant>(), &eventManager); //create a DS18B20Bus
    }
  }
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
    Serial.println(F("[EthernetConnect]Ethernet shield not found!!! Restart..."));
    SoftwareReset();
  }

  if (Ethernet.linkStatus() == LinkOFF)
    Serial.println(F("[EthernetConnect]Ethernet cable is not connected."));

  return Ethernet.linkStatus() != LinkOFF;
}

//---------WEBSERVER---------
void WebServerCallback(EthernetClient &webClient, bool isPOSTRequest, const char *requestURI, bool isFileContentReceived, const char *fileContent)
{
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
    if (!strcmp_P(requestURI, PSTR("/conf")))
    {
      //Try to deserialize it
      DynamicJsonDocument dynJsonDoc(JSON_DOCUMENT_MAX_SIZE); //Caution : Heap already has fileContent and will get this JSON too...
      DeserializationError jsonError = deserializeJson(dynJsonDoc, fileContent);
      //if deserialization succeed
      if (!jsonError)
      {
        Serial.println(F("Save JSON to EEPROM"));

        //Save JSON to EEPROM
        ConfigSaveJson(fileContent);

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
    for (uint8_t i = 0; i < nbHADevices; i++)
      if (haDevices[i])
        haDevices[i]->MqttSubscribe(mqttClient, jsonDoc[F("MQTT")][F("baseTopic")].as<const char *>());
  }

  return mqttClient.connected();
}

void MqttCallback(char *topic, uint8_t *payload, unsigned int length)
{
  const char *baseTopic = jsonDoc[F("MQTT")][F("baseTopic")].as<const char *>();
  char *relevantPartOfTopic = topic + strlen(baseTopic);
  if (baseTopic[strlen(baseTopic) - 1] != '/')
    relevantPartOfTopic++;

  bool messageHandled = false;

  for (uint8_t i = 0; i < nbHADevices && !messageHandled; i++)
    if (haDevices[i])
      messageHandled = haDevices[i]->MqttCallback(relevantPartOfTopic, payload, length);
}

bool MqttStart()
{
  //setup MQTT client (PubSubClient)
  mqttClient.setClient(mqttEthClient).setServer(jsonDoc[F("MQTT")][F("hostname")].as<const char *>(), jsonDoc[F("MQTT")][F("port")]).setCallback(MqttCallback);

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

  //Load JSON from EEPROM
  Serial.println(F("[setup]Config JSON"));
  char *jsonBuffer = (char *)malloc(JSON_BUFFER_MAX_SIZE + 1);
  ConfigReadJson(jsonBuffer);
  if (ConfigParse(jsonBuffer))
    Serial.println(F("[setup]Config JSON : OK\n"));
  else
    Serial.println(F("[setup]Config JSON : FAILED\n"));
  free(jsonBuffer);

  //Create Home Automation Objects
  Serial.println(F("[setup]HADevices"));
  ConfigCreateHADevices();
  Serial.println(F("[setup]HADevices : Done\n"));

  //Start Ethernet
  Serial.println(F("[setup]Ethernet"));
  if (EthernetConnect())
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
  String completeTopic;
  completeTopic.reserve(16 + 1 + sizeof(EventManager::Event::topic)); //baseTopic(16)+/+Event.topic size
  //while MQTTconnected and publish works and there is an event to send
  while (mqttClient.connected() && publishSucceeded && (evtToSend = eventManager.Available()))
  {
    //build complete topic : baseTopic(with ending /) + topic in the event
    completeTopic = jsonDoc[F("MQTT")][F("baseTopic")].as<const char *>();
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