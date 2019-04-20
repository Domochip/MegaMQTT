#include "Light.h"

Light::Light(JsonVariant config, EventManager *evtMgr)
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
    Light(config["id"].as<const char *>(), pinBtn, pinLight, config["pushbutton"] | false, evtMgr);
}

Light::Light(const char *id, uint8_t pinBtn, uint8_t pinLight, bool pushButtonMode, EventManager *evtMgr)
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

    //save pointer to Eventmanager
    _evtMgr = evtMgr;

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
    //if relevantPartOfTopic starts with id of this Light ending with '/'
    if (!strncmp(relevantPartOfTopic, _id, strlen(_id)) && relevantPartOfTopic[strlen(_id)] == '/')
    {
        //if topic finishes by '/command'
        if (!strcmp_P(relevantPartOfTopic + strlen(relevantPartOfTopic) - 8, PSTR("/command")))
        {
            switch (payload[0])
            {
            //OFF requested
            case '0':
                digitalWrite(_pinLight, LOW);
                break;
            //ON requested
            case '1':
                digitalWrite(_pinLight, HIGH);
                break;
            //TOGGLE requested
            case 't':
            case 'T':
                digitalWrite(_pinLight, !digitalRead(_pinLight));
                break;
            }
        }

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