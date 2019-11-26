#include "VerySimpleTimer.h"

void VerySimpleTimer::setOnceTimeout(uint16_t d)
{
    _once = true;
    setTimeout(d);
};
void VerySimpleTimer::setTimeout(uint16_t d)
{
    _timeoutDuration = d;
    reset();
};
void VerySimpleTimer::reset()
{
    _nextMillis = millis() + _timeoutDuration;
    _active = true;
};
void VerySimpleTimer::stop()
{
    _active = false;
};
bool VerySimpleTimer::isTimeoutOver()
{
    if (_active && millis() > _nextMillis)
    {
        if (_once)
            _active = false;
        else
            _nextMillis += _timeoutDuration;

        return true;
    }
    return false;
};

bool VerySimpleTimer::isActive()
{
    return _active;
};