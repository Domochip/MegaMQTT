#ifndef HADevice_h
#define HADevice_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "EventManager.h"

class HADevice
{
private:
  static bool _usedPins[54];

protected:
  bool _initialized = false;
  char _id[17] = {0};
  EventManager *_evtMgr = NULL;

  bool isPinAvailable(uint8_t pinNumber);

public:
  virtual void mqttSubscribe(PubSubClient &mqttClient, const char *baseTopic) = 0;
  virtual bool mqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length) = 0;
  virtual bool run() = 0;
};

#endif