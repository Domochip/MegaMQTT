#ifndef Light_h
#define Light_h

#include "HADevice.h"

#include <Bounce2.h>

//Light publish :
//  - /status
//Light subscribe :
//  - /command

class Light : HADevice
{
  private:
    Bounce _btn;
    uint8_t _pinLight;
    bool _pushButtonMode = false;

    void On();
    void Off();
    void Toggle();

  public:
    Light(JsonVariant config, EventManager *evtMgr);
    void Init(const char *id, uint8_t pinBtn, uint8_t pinLight, bool pushButtonMode, EventManager *evtMgr);
    void MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic);
    bool MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length);
    bool Run();
};

#endif