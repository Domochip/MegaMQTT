#ifndef Light_h
#define Light_h

#include <ArduinoJson.h>
#include <Bounce2.h>

class Light
{
  private:
    bool _initialized = false;
    const char *_id;
    Bounce _btn;
    uint8_t _pinLight;
    bool _pushButtonMode = false;

  public:
    Light(JsonVariant config);
    Light(const char *id, uint8_t pinBtn, uint8_t pinLight, bool pushButtonMode = false);
    void Run();
};

#endif