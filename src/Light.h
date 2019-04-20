#ifndef Light_h
#define Light_h

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Bounce2.h>

#include "EventManager.h"

//Light publish :
//  - /status
//Light subscribe :
//  - /command

class Light
{
  private:
    bool _initialized = false;
    const char *_id;
    Bounce _btn;
    uint8_t _pinLight;
    bool _pushButtonMode = false;
    EventManager *_evtMgr = NULL;

    void ON();
    void OFF();
    void TOGGLE();

  public:
    Light(JsonVariant config, EventManager *evtMgr);
    Light(const char *id, uint8_t pinBtn, uint8_t pinLight, bool pushButtonMode, EventManager *evtMgr);
    void MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic);
    bool MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length);
    void Run();
};

#endif