#include <Arduino.h>
#include <avr/boot.h>
#include <avr/wdt.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EEPROM.h>

//Ethernet variables
byte mac[6];
IPAddress ip(192, 168, 1, 177);
//WebServer
EthernetServer webServer(80);

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

void saveJSONtoEEPROM(const char *json)
{
  for (uint16_t i = 0; i < strlen(json); i++)
    EEPROM[i] = json[i];
  EEPROM[strlen(json)] = 0;
}

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
}

void loop()
{
  //take and check if a webClient is there
  EthernetClient webClient = webServer.available();
  if (webClient)
  {
    //if it's connected
    if (webClient.connected())
    {
      //receive and parse his request
      bool isRequestHeaderFinished = false;
      bool isPOSTRequest = false;
      String requestURI; //URL requested
      String requestBoundary;
      String fileContent;

      while (webClient.available() && !isRequestHeaderFinished)
      {
        String reqLine = webClient.readStringUntil('\n');

        //DEBUG
        Serial.print(F("->"));
        Serial.println(reqLine);
        Serial.print(F("--+ Length : "));
        Serial.println(reqLine.length());

        //Parse method and URI
        if (reqLine.endsWith(F(" HTTP/1.1\r")))
    {
          if (reqLine.startsWith(F("GET ")))
            requestURI = reqLine.substring(4, reqLine.length() - 10);
          if (reqLine.startsWith(F("POST ")))
          {
            isPOSTRequest = true;
            requestURI = reqLine.substring(4, reqLine.length() - 10);
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
}