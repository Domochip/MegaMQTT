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

    boolean ReadScratchPad(byte addr[], byte data[]);
    void WriteScratchPad(byte addr[], byte th, byte tl, byte cfg);
    void CopyScratchPad(byte addr[]);
    void SetupTempSensors(); //Set sensor to 12bits resolution
    void StartConvertT();
    void ReadAndPublishTemperatures();

  public:
    DS18B20Bus(JsonVariant config, EventManager *evtMgr);
    void Init(const char *id, uint8_t pinOneWire, EventManager *evtMgr);
    void MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic);
    bool MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length);
    bool Run();
};

#endif