#ifndef HADevice_h
#define HADevice_h

#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "EventManager.h"

class HADevice
{
  private:
    static bool _usedPins[54];

  protected:
    bool _initialized = false;
    const char *_id = NULL;
    EventManager *_evtMgr = NULL;

    bool IsPinAvailable(uint8_t pinNumber);

  public:
    virtual void MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic) = 0;
    virtual bool MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length) = 0;
    virtual bool Run() = 0;
};

#endif