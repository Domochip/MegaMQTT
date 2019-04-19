#include "RollerShutter.h"

RollerShutter::RollerShutter(JsonVariant config)
{
    if (config["id"].isNull())
        return;

    if (config["pins"].isNull())
        return;

    char workingPinsList[12];
    uint8_t pinBtnUp, pinBtnDown, pinRollerDir, pinRollerPower;

    //Copy pins list
    strncpy(workingPinsList, config["pins"].as<const char *>(), sizeof(workingPinsList));

    //Parse it
    char *pinStr;
    pinStr = strtok(workingPinsList, ",");
    if (!pinStr)
        return;
    pinBtnUp = atoi(pinStr);

    pinStr = strtok(0, ",");
    if (!pinStr)
        return;
    pinBtnDown = atoi(pinStr);
    pinStr = strtok(0, ",");
    if (!pinStr)
        return;
    pinRollerDir = atoi(pinStr);
    pinStr = strtok(0, ",");
    if (!pinStr)
        return;
    pinRollerPower = atoi(pinStr);

    //call Constructor with parsed values
    RollerShutter(config["id"].as<const char *>(), pinBtnUp, pinBtnDown, pinRollerDir, pinRollerPower);
}

RollerShutter::RollerShutter(const char *id, uint8_t pinBtnUp, uint8_t pinBtnDown, uint8_t pinRollerDir, uint8_t pinRollerPower)
{
    //DEBUG
    Serial.print(F("new RollerShutter("));
    Serial.print(id);
    Serial.print(',');
    Serial.print(pinBtnUp);
    Serial.print(',');
    Serial.print(pinBtnDown);
    Serial.print(',');
    Serial.print(pinRollerDir);
    Serial.print(',');
    Serial.print(pinRollerPower);
    Serial.println(')');

    //copy id pointer
    _id = id;

    //start buttons
    _btnUp.attach(pinBtnUp, INPUT_PULLUP);
    _btnUp.interval(25);
    _btnDown.attach(pinBtnDown, INPUT_PULLUP);
    _btnDown.interval(25);

    //save pin numbers
    _pinRollerDir = pinRollerDir;
    _pinRollerPower = pinRollerPower;

    //setup outputs
    pinMode(_pinRollerDir, OUTPUT);
    pinMode(_pinRollerPower, OUTPUT);

    _initialized = true;
}

void RollerShutter::Run()
{
    if (!_initialized)
        return;

    if (_btnUp.update())
    {
        if (_btnUp.read() == LOW)
        {
            digitalWrite(_pinRollerDir, HIGH);
            digitalWrite(_pinRollerPower, HIGH);
        }
    }
    if (_btnDown.update())
    {
        if (_btnDown.read() == LOW)
        {
            digitalWrite(_pinRollerDir, LOW);
            digitalWrite(_pinRollerPower, HIGH);
        }
    }
};