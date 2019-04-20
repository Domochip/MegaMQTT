#ifndef RollerShutter_h
#define RollerShutter_h

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Bounce2.h>

class RollerShutter
{
  private:
    bool _initialized = false;
    const char *_id;
    Bounce _btnUp, _btnDown;
    uint8_t _pinRollerDir, _pinRollerPower;

  public:
    RollerShutter(){};
    RollerShutter(JsonVariant config);
    RollerShutter(const char *id, uint8_t pinBtnUp, uint8_t pinBtnDown, uint8_t pinRollerDir, uint8_t pinRollerPower);
    void MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic);
    bool MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length);
    void Run();
};

#endif