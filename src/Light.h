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

  public:
    Light(JsonVariant config);
    Light(const char *id, uint8_t pinBtn, uint8_t pinLight);
    void Run();
};

#endif