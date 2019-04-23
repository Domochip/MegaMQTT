#include "HADevice.h"

bool HADevice::_usedPins[54] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};

//function used to check if a pin is available and mark it used for other checks
bool HADevice::IsPinAvailable(uint8_t pinNumber)
{
    if (pinNumber > 53)
    {
        Serial.print(F("[HADevice][ERROR]Incorrect pin number : "));
        Serial.println(pinNumber);
    }
    if (!_usedPins[pinNumber])
    {
        _usedPins[pinNumber] = true;
        return true;
    }
    else
    {
        Serial.print(F("[HADevice][ERROR]Pin number already used : "));
        Serial.println(pinNumber);
    }

    return false;
};