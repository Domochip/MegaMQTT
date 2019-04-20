#ifndef RollerShutter_h
#define RollerShutter_h

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Bounce2.h>

#include "EventManager.h"

class RollerShutter
{
private:
  bool _initialized = false;
  const char *_id;
  Bounce _btnUp, _btnDown;
  uint8_t _pinRollerDir, _pinRollerPower;
  EventManager *_evtMgr = NULL;

public:
  RollerShutter(JsonVariant config, EventManager *evtMgr);
  void Init(const char *id, uint8_t pinBtnUp, uint8_t pinBtnDown, uint8_t pinRollerDir, uint8_t pinRollerPower, EventManager *evtMgr);
  void MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic);
  bool MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length);
  bool Run();
};

#endif