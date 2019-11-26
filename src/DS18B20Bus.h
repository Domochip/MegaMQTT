#ifndef DS18B20Bus_h
#define DS18B20Bus_h

#include "HADevice.h"
#include <OneWire.h>
#include "VerySimpleTimer.h"

//A 4.7K resistor is required between VCC and the DATA pin of the 1Wire Bus
//VCC need to be provided to sensors (3 wires connected : GND,DATA,VCC)

//MQTT publish :
//  /temperatures/{ROMCode}/temperature
//    19.25

#define PUBLISH_PERIOD 60 //seconds between each convert+publish

class DS18B20Bus : HADevice
{
  private:
    OneWire _oneWire;
    bool _convertInProgress = false;
    VerySimpleTimer _timer; //used for Convertion and Publish

    boolean readScratchPad(byte addr[], byte data[]);
    void writeScratchPad(byte addr[], byte th, byte tl, byte cfg);
    void copyScratchPad(byte addr[]);
    void setupTempSensors(); //Set sensor to 12bits resolution
    void startConvertT();
    void readAndPublishTemperatures();

  public:
    DS18B20Bus(JsonVariant config, EventManager *evtMgr);
    void init(const char *id, uint8_t pinOneWire, EventManager *evtMgr);
    void mqttSubscribe(PubSubClient &mqttClient, const char *baseTopic);
    bool mqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length);
    bool run();
};

#endif