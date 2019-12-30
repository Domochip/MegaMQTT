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
  uint8_t _pinRollerDir, _pinRollerPower; //For Velux Roler Shutter : RollerDir=RollerUp; RollerPower=RollerDown
  uint8_t _travelTime = 0;
  bool _invertOutput = false;
  bool _veluxType = false;
  float _currentPosition = 0.0;

  bool _ready = false;
  unsigned long _movementStart = 0;
  Movement _isMoving = No;
  VerySimpleTimer _outputTimer;

  void goDown();
  void goUp();
  void stop();

public:
  RollerShutter(JsonVariant config, EventManager *evtMgr);
  void init(const char *id, uint8_t pinBtnUp, uint8_t pinBtnDown, uint8_t pinRollerDir, uint8_t pinRollerPower, uint8_t travelTime, bool invertOutput, bool veluxType, EventManager *evtMgr);
  void mqttSubscribe(PubSubClient &mqttClient, const char *baseTopic) override;
  bool mqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length) override;
  bool run() override;
};

#endif