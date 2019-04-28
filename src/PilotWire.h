#ifndef PilotWire_h
#define PilotWire_h

#include "HADevice.h"
#include "VerySimpleTimer.h"

/*
  PilotWire Orders :
  - 0-10 : Arrêt
  - 11-20 : Hors Gel
  - 21-30 : Eco
  - 31-40 : Confort-2   //NOT YET AVAILABLE (= Eco)
  - 41-50 : Confort-1   //NOT YET AVAILABLE (= Eco)
  - 51-99 : Confort
*/

//MQTT publish :
//  ID/state
//    0->99
//MQTT subscribe :
//  ID/command
//    0->99

class PilotWire : HADevice
{
  private:
    uint8_t _currentOrder = 51;
    uint8_t _pinPos, _pinNeg;

    void SetOrder(uint8_t order);

  public:
    PilotWire(JsonVariant config, EventManager *evtMgr);
    void Init(const char *id, uint8_t pinPos, uint8_t pinNeg, EventManager *evtMgr);
    void MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic);
    bool MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length);
    bool Run();
};

#endif