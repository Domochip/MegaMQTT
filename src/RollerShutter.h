#ifndef RollerShutter_h
#define RollerShutter_h

#include "HADevice.h"

#include <Bounce2.h>
#include "VerySimpleTimer.h"

//MQTT publish :
//  ID/state
//    0->100
//MQTT subscribe :
//  ID/command
//    0->100

#define DEBOUNCE_INTERVAL 25
#define LONGPRESS_THRESHOLD 1000

class RollerShutter : HADevice
{
private:
  enum Movement
  {
    No,
    Down,
    Up
  };

  Bounce _btnUp, _btnDown;
  uint8_t _pinRollerDir, _pinRollerPower;
  uint8_t _travelTime = 0;
  float _currentPosition = 0.0;

  bool _ready = false;
  unsigned long _movementStart = 0;
  Movement _isMoving = No;
  VerySimpleTimer _outputTimer;

  void GoDown();
  void GoUp();
  void Stop();

public:
  RollerShutter(JsonVariant config, EventManager *evtMgr);
  void Init(const char *id, uint8_t pinBtnUp, uint8_t pinBtnDown, uint8_t pinRollerDir, uint8_t pinRollerPower, uint8_t travelTime, EventManager *evtMgr);
  void MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic);
  bool MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length);
  bool Run();
};

#endif