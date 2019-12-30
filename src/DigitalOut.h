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
    bool _invertOutput = false;

    void on();
    void off();

  public:
    DigitalOut(JsonVariant config, EventManager *evtMgr);
    void init(const char *id, uint8_t pinOut, bool invertOutput, EventManager *evtMgr);
    void mqttSubscribe(PubSubClient &mqttClient, const char *baseTopic) override;
    bool mqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length) override;
    bool run() override;
};

#endif