#ifndef RollerShutter_h
#define RollerShutter_h

#include "HADevice.h"

#include <Bounce2.h>

class RollerShutter : HADevice
{
private:
  Bounce _btnUp, _btnDown;
  uint8_t _pinRollerDir, _pinRollerPower;

public:
  RollerShutter(JsonVariant config, EventManager *evtMgr);
  void Init(const char *id, uint8_t pinBtnUp, uint8_t pinBtnDown, uint8_t pinRollerDir, uint8_t pinRollerPower, EventManager *evtMgr);
  void MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic);
  bool MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length);
  bool Run();
};

#endif