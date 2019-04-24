#ifndef VerySimpleTimer_h
#define VerySimpleTimer_h

#include <Arduino.h>

class VerySimpleTimer
{
  private:
    bool _active = false;
    bool _once = false;
    uint16_t _timeoutDuration = 0;
    unsigned long _nextMillis = 0;

  public:
    void SetOnceTimeout(uint16_t d);
    void SetTimeout(uint16_t d);
    void Reset();
    void Stop();
    bool IsTimeoutOver();
    bool IsActive();
};

#endif