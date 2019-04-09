#include <Arduino.h>
#include <avr/boot.h>
#include <avr/wdt.h>
#include <SPI.h>
#include <Ethernet.h>

//Ethernet variables
byte mac[6];
IPAddress ip(192, 168, 1, 177);
bool _needEthernetReconnection = false;
EthernetServer webServer(80);

void softwareReset()
{
  wdt_enable(WDTO_15MS);
  while (1)
  {
  }
}

bool ethernetConnection()
{
  Ethernet.begin(mac, ip);

  if (Ethernet.hardwareStatus() == EthernetNoHardware)
  {
    Serial.println(F("Ethernet shield not found!!! Restart..."));
    softwareReset();
  }

  if (Ethernet.linkStatus() == LinkOFF)
    Serial.println(F("Ethernet cable is not connected."));

  if (Ethernet.linkStatus() == LinkON)
  {
    // If webServer not yet started then start it
    if (!webServer)
      webServer.begin();
  }

  return Ethernet.linkStatus() == LinkON;
}

void setup()
{
  //build MAC address based on hidden ATMega2560 serial number
  mac[0] = boot_signature_byte_get(0x11);
  mac[1] = boot_signature_byte_get(0x12);
  mac[2] = boot_signature_byte_get(0x13);
  mac[3] = boot_signature_byte_get(0x15);
  mac[4] = boot_signature_byte_get(0x16);
  mac[5] = boot_signature_byte_get(0x17);

  //Start serial
  Serial.begin(115200);

  if (!ethernetConnection())
    _needEthernetReconnection = true;
}

void loop()
{
  EthernetClient webClient = webServer.available();
  if (webClient)
  {
    while (webClient.connected())
    {
      if (webClient.available())
        webClient.read();
    }
    webClient.stop();
  }
}