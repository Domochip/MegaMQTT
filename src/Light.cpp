#include "Light.h"

Light::Light(JsonVariant config)
{
    if (config["id"].isNull())
        return;

    if (config["pins"].isNull())
        return;

    char workingPinsList[6];
    uint8_t pinBtn, pinLight;

    //Copy pins list
    strncpy(workingPinsList, config["pins"].as<const char *>(), sizeof(workingPinsList));

    //Parse it
    char *pinStr;
    pinStr = strtok(workingPinsList, ",");
    if (!pinStr)
        return;
    pinBtn = atoi(pinStr);

    pinStr = strtok(0, ",");
    if (!pinStr)
        return;
    pinLight = atoi(pinStr);

    //call Constructor with parsed values
    Light(config["id"].as<const char *>(), pinBtn, pinLight, config["pushbutton"] | false);
}

Light::Light(const char *id, uint8_t pinBtn, uint8_t pinLight, bool pushButtonMode /* = false*/)
{
    //DEBUG
    Serial.print(F("new Light("));
    Serial.print(id);
    Serial.print(',');
    Serial.print(pinBtn);
    Serial.print(',');
    Serial.print(pinLight);
    Serial.println(')');

    //copy id pointer
    _id = id;

    //start button
    _btn.attach(pinBtn, INPUT_PULLUP);
    _btn.interval(25);

    //save pin numbers
    _pinLight = pinLight;

    //setup output
    pinMode(_pinLight, OUTPUT);
    digitalWrite(_pinLight, LOW);

    //save pushButtonMode
    _pushButtonMode = pushButtonMode;

    _initialized = true;
}
void Light::MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic)
{
    char *completeTopic = (char *)malloc(strlen(baseTopic) + 1 + strlen(_id) + 8 + 1); // /command
    strcpy(completeTopic, baseTopic);
    if (baseTopic[strlen(baseTopic) - 1] != '/')
        strcat(completeTopic, "/");
    strcat(completeTopic, _id);
    strcat_P(completeTopic, PSTR("/command"));
    mqttClient.subscribe(completeTopic);
}

bool Light::MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length)
{
    if (!strncmp(relevantPartOfTopic, _id, strlen(_id)) && relevantPartOfTopic[strlen(_id)] == '/')
    {
        //TODO handle MQTT message
        return true;
    }
    return false;
}

void Light::Run()
{
    if (!_initialized)
        return;

    //if button state changed AND (not a pushButton OR input rose)
    if (_btn.update() && (!_pushButtonMode || _btn.rose()))
        digitalWrite(_pinLight, !digitalRead(_pinLight)); //then invert output
}