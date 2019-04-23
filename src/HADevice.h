#ifndef HADevice_h
#define HADevice_h

#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "EventManager.h"

class HADevice
{
  protected:
    bool _initialized = false;
    const char *_id = NULL;
    EventManager *_evtMgr = NULL;

  public:
    virtual void MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic) = 0;
    virtual bool MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length) = 0;
    virtual bool Run() = 0;
};

#endif