#include "Light.h"

void Light::On()
{
    if (digitalRead(_pinLight) == LOW)
    {
        digitalWrite(_pinLight, HIGH);
        _evtMgr->AddEvent((String(_id) + F("/state")).c_str(), "1");
    }
}
void Light::Off()
{
    if (digitalRead(_pinLight) == HIGH)
    {
        digitalWrite(_pinLight, LOW);
        _evtMgr->AddEvent((String(_id) + F("/state")).c_str(), "0");
    }
}
void Light::Toggle()
{
    digitalWrite(_pinLight, !digitalRead(_pinLight));
    _evtMgr->AddEvent((String(_id) + F("/state")).c_str(), (digitalRead(_pinLight) == LOW) ? "0" : "1");
}

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

    //call Init with parsed values
    Init(config["id"].as<const char *>(), pinBtn, pinLight, config["pushbutton"] | false, evtMgr);
}

void Light::Init(const char *id, uint8_t pinBtn, uint8_t pinLight, bool pushButtonMode, EventManager *evtMgr)
{
    Serial.print(F("[Light] Init("));
    Serial.print(id);
    Serial.print(',');
    Serial.print(pinBtn);
    Serial.print(',');
    Serial.print(pinLight);
    if (pushButtonMode)
    {
        Serial.print(',');
        Serial.print(F("pushButtonMode"));
    }
    Serial.println(')');

    //Check if pins are available
    if (!IsPinAvailable(pinBtn))
        return;
    if (!IsPinAvailable(pinLight))
        return;

    //save pointer to Eventmanager
    _evtMgr = evtMgr;

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
    _evtMgr->AddEvent((String(_id) + F("/state")).c_str(), "0");

    //save pushButtonMode
    _pushButtonMode = pushButtonMode;

    _initialized = true;
}
void Light::MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic)
{
    char *completeTopic = new char[strlen(baseTopic) + 1 + strlen(_id) + 8 + 1]; // /command
    strcpy(completeTopic, baseTopic);
    if (baseTopic[strlen(baseTopic) - 1] != '/')
        strcat(completeTopic, "/");
    strcat(completeTopic, _id);
    strcat_P(completeTopic, PSTR("/command"));
    mqttClient.subscribe(completeTopic);
    delete[] completeTopic;
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
                Off();
                break;
            //ON requested
            case '1':
                On();
                break;
            //Toggle requested
            case 't':
            case 'T':
                Toggle();
                break;
            }
        }

        return true;
    }
    return false;
}

bool Light::Run()
{
    if (!_initialized)
        return false;

    //if button state changed AND (not a pushButton OR input rose)
    if (_btn.update() && (!_pushButtonMode || _btn.rose()))
        Toggle(); //then invert output

    //no time critical state, so always false is returned
    return false;
}