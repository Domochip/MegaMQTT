#include "Light.h"

Light::Light(const char *pinsList)
{
    char workingPinsList[6];
    uint8_t pinBtn, pinLight;

    strncpy(workingPinsList, pinsList, sizeof(workingPinsList));

    char *pinStr;
    pinStr = strtok(workingPinsList, ",");
    if (!pinStr)
        return;
    pinBtn = atoi(pinStr);

    pinStr = strtok(0, ",");
    if (!pinStr)
        return;
    pinLight = atoi(pinStr);

    Light(pinBtn, pinLight);
}
Light::Light(uint8_t pinBtn, uint8_t pinLight)
{
    //DEBUG
    Serial.print(F("new Light("));
    Serial.print(pinBtn);
    Serial.print(',');
    Serial.print(pinLight);
    Serial.println(')');

    _btn.attach(pinBtn, INPUT_PULLUP);
    _btn.interval(25);

    _pinLight = pinLight;
    pinMode(_pinLight, OUTPUT);
    digitalWrite(_pinLight, LOW);

    _initialized = true;
}
void Light::Run()
{
    //TODO handle pushbutton
    //if button state changed, then invert output
    if (_btn.update())
        digitalWrite(_pinLight, !digitalRead(_pinLight));
}