#include <Arduino.h>
#include <avr/boot.h>
#include <avr/wdt.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <SimpleTimer.h> //!\ MAX_TIMERS = 1 /!\.

#define JSON_BUFFER_MAX_SIZE 1024
#define JSON_DOCUMENT_MAX_SIZE 1024

//Ethernet variables
byte mac[6];
IPAddress ip(192, 168, 1, 177);
//WebServer
EthernetServer webServer(80);

//JSON variable
StaticJsonDocument<JSON_DOCUMENT_MAX_SIZE> jsonDoc;

//MQTT variables
EthernetClient mqttEthClient;
PubSubClient mqttClient;
bool needMqttReconnect = false;
SimpleTimer mqttReconnectTimer;

void SoftwareReset()
{
  wdt_enable(WDTO_15MS);
  while (1)
  {
  }
}

bool EthernetConnection()
{
  Ethernet.begin(mac, ip);

  if (Ethernet.hardwareStatus() == EthernetNoHardware)
  {
    Serial.println(F("Ethernet shield not found!!! Restart..."));
    SoftwareReset();
  }

  if (Ethernet.linkStatus() == LinkOFF)
    Serial.println(F("Ethernet cable is not connected."));

  // If webServer not yet started then start it
  if (!webServer)
    webServer.begin();

  return Ethernet.linkStatus() != LinkOFF;
}

void ReadJSONFromEEPROM(char *jsonBuffer)
{
  uint16_t i = 0;
  while (EEPROM[i])
  {
    jsonBuffer[i] = EEPROM[i];
    i++;
  }
  EEPROM[i] = 0;
}

void SaveJSONToEEPROM(const char *json)
{
  for (uint16_t i = 0; i < strlen(json); i++)
    EEPROM[i] = json[i];
  EEPROM[strlen(json)] = 0;
}

bool ParseJSON(const char *jsonBuffer)
{
  DeserializationError jsonError = deserializeJson(jsonDoc, jsonBuffer);
  if (jsonError)
    Serial.println(F("JSON parsing failed"));
  return !jsonError;
}

void HandleWebClient(EthernetClient &webClient)
{
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
          requestURI = reqLine.substring(5, reqLine.length() - 10);
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
    }
    else //else that is a POST request
    {
      //if JSON Config file POSTed
      if (requestURI == F("/conf"))
      {
        //Deserialize it into jsonDoc
        DeserializationError jsonError = deserializeJson(jsonDoc, fileContent);
        //if deserialization succeed
        if (!jsonError)
        {
          Serial.println(F("Save JSON to EEPROM"));

          //Save JSON to EEPROM
          SaveJSONToEEPROM(fileContent.c_str());

          //Answer to the webClient
          webClient.println(F("HTTP/1.1 200 OK"));
          webClient.println(F("Content-Type: text/html"));
          webClient.println();
          webClient.println(F("JSON Config file saved"));

          delay(1);         // give webClient time to receive the data
          webClient.stop(); // close the connection

          Serial.println(F("Reboot"));
          SoftwareReset();
        }
        else
        {
          webClient.println(F("HTTP/1.1 400 Bad Request"));
          webClient.println();
          webClient.println(F("Incorrect JSON Config file"));

          //Reload JSON from EEPROM
          //TODO
        }
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
//------------------------------------------
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
    //Subscribe to needed topic
  }

  return mqttClient.connected();
}

void MqttCallback(char *topic, uint8_t *payload, unsigned int length) {}

void setup()
{
  //build MAC address based on hidden ATMega2560 serial number
  mac[0] = 0xDE;
  mac[1] = 0xAD;
  mac[2] = 0xBE;
  mac[3] = 0xEF;
  mac[4] = boot_signature_byte_get(0x16);
  mac[5] = boot_signature_byte_get(0x17);

  //Start serial
  Serial.begin(115200);

  //Start Ethernet
  EthernetConnection();

  //Load JSON from EEPROM
  char *jsonBuffer = (char *)malloc(JSON_BUFFER_MAX_SIZE + 1);
  ReadJSONFromEEPROM(jsonBuffer);
  ParseJSON(jsonBuffer);
  free(jsonBuffer);

  //setup MQTT client (PubSubClient)
  mqttClient.setClient(mqttEthClient).setServer(jsonDoc[F("MQTT")][F("hostname")].as<const char *>(), jsonDoc[F("MQTT")][F("port")]).setCallback(MqttCallback);

  //Then connect
  if (MqttConnect())
    Serial.println(F("MQTT connected"));
  else
    Serial.println(F("MQTT not Connected"));
}

void loop()
{
  //------------------------HOME AUTOMATION------------------------
  
  //------------------------WEBSERVER------------------------
  //take and check if a webClient is there
  EthernetClient webClient = webServer.available();
  if (webClient)
    HandleWebClient(webClient);

  //------------------------MQTT------------------------
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