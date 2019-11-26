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
    void setOnceTimeout(uint16_t d);
    void setTimeout(uint16_t d);
    void reset();
    void stop();
    bool isTimeoutOver();
    bool isActive();
};

#endif