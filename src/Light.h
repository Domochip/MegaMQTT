#ifndef Light_h
#define Light_h

#include <Bounce2.h>

class Light
{
  private:
    bool _initialized = false;
    Bounce _btn;
    uint8_t _pinLight;

  public:
    Light(const char *pinsList);
    Light(uint8_t pinBtn, uint8_t pinLight);
    void Run();
};

#endif