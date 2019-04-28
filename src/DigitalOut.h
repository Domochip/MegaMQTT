#ifndef DigitalOut_h
#define DigitalOut_h

#include "HADevice.h"

//MQTT publish :
//  ID/state
//    0,1
//MQTT subscribe :
//  ID/command
//    0,1

class DigitalOut : HADevice
{
  private:
    uint8_t _pinOut;

    void On();
    void Off();

  public:
    DigitalOut(JsonVariant config, EventManager *evtMgr);
    void Init(const char *id, uint8_t pinOut, EventManager *evtMgr);
    void MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic);
    bool MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length);
    bool Run();
};

#endif