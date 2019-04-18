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
      String requestedWebpage; //URL requested
      while (webClient.available())
      {
        String reqLine = webClient.readStringUntil('\n');
        if (reqLine.endsWith(F(" HTTP/1.1\r")))
    {
          if (reqLine.startsWith(F("GET ")))
            requestedWebpage = reqLine.substring(4, reqLine.length() - 10);
        }
      }
      Serial.print(F("URL requested : "));
      Serial.println(requestedWebpage);
    }
    webClient.stop();
  }
}