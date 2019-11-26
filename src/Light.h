#ifndef Light_h
#define Light_h

#include "HADevice.h"

#include <Bounce2.h>

//MQTT publish :
//  ID/state
//    0,1
//MQTT subscribe :
//  ID/command
//    0,1,t,T

class Light : HADevice
{
private:
  Bounce _btn;
  uint8_t _pinLight;
  bool _pushButtonMode = false;
  bool _invertOutput = false;

  void on();
  void off();
  void toggle();

public:
  Light(JsonVariant config, EventManager *evtMgr);
  void init(const char *id, uint8_t pinBtn, uint8_t pinLight, bool pushButtonMode, bool invertOutput, EventManager *evtMgr);
  void mqttSubscribe(PubSubClient &mqttClient, const char *baseTopic);
  bool mqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length);
  bool run();
};

#endif