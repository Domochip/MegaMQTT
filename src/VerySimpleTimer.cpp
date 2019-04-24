#include "VerySimpleTimer.h"

void VerySimpleTimer::SetOnceTimeout(uint16_t d)
{
    _once = true;
    SetTimeout(d);
};
void VerySimpleTimer::SetTimeout(uint16_t d)
{
    _timeoutDuration = d;
    Reset();
};
void VerySimpleTimer::Reset()
{
    _nextMillis = millis() + _timeoutDuration;
    _active = true;
};
void VerySimpleTimer::Stop()
{
    _active = false;
};
bool VerySimpleTimer::IsTimeoutOver()
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

bool VerySimpleTimer::IsActive()
{
    return _active;
};