#include "DigitalOut.h"

void DigitalOut::On()
{

    digitalWrite(_pinOut, HIGH);
    Serial.print(F("[DigitalOut] "));
    Serial.print(_id);
    Serial.println(F(" : ON"));
    _evtMgr->AddEvent((String(_id) + F("/state")).c_str(), "1");
}
void DigitalOut::Off()
{
    digitalWrite(_pinOut, LOW);
    Serial.print(F("[DigitalOut] "));
    Serial.print(_id);
    Serial.println(F(" : OFF"));
    _evtMgr->AddEvent((String(_id) + F("/state")).c_str(), "0");
}

DigitalOut::DigitalOut(JsonVariant config, EventManager *evtMgr)
{
    if (config["id"].isNull())
        return;

    if (config["pin"].isNull())
        return;

    //call Init
    Init(config["id"].as<const char *>(), config["pin"].as<uint8_t>(), evtMgr);
};

void DigitalOut::Init(const char *id, uint8_t pinOut, EventManager *evtMgr)
{
    //DEBUG
    Serial.print(F("[DigitalOut] Init("));
    Serial.print(id);
    Serial.print(',');
    Serial.print(pinOut);
    Serial.println(')');

    //Check if pin is available
    if (!IsPinAvailable(pinOut))
        return;

    //save EventManager
    _evtMgr = evtMgr;

    //copy id pointer
    _id = id;

    //save pin numbers
    _pinOut = pinOut;

    //setup output
    pinMode(_pinOut, OUTPUT);
    digitalWrite(_pinOut, LOW);

    _initialized = true;

    //Initialization publish
    _evtMgr->AddEvent((String(_id) + F("/state")).c_str(), "0");
};

void DigitalOut::MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic)
{
    char *completeTopic = new char[strlen(baseTopic) + 1 + strlen(_id) + 8 + 1]; // /command
    strcpy(completeTopic, baseTopic);
    if (baseTopic[strlen(baseTopic) - 1] != '/')
        strcat(completeTopic, "/");
    strcat(completeTopic, _id);
    strcat_P(completeTopic, PSTR("/command"));
    mqttClient.subscribe(completeTopic);
    delete[] completeTopic;
};

bool DigitalOut::MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length)
{
    //if relevantPartOfTopic starts with id of this device ending with '/'
    if (!strncmp(relevantPartOfTopic, _id, strlen(_id)) && relevantPartOfTopic[strlen(_id)] == '/')
    {
        //if topic finishes by '/command'
        if (!strcmp_P(relevantPartOfTopic + strlen(relevantPartOfTopic) - 8, PSTR("/command")))
        {
            if (length == 1)
            {
                switch (payload[0])
                {
                //0 requested
                case '0':
                    Off();
                    break;
                //1 requested
                case '1':
                    On();
                    break;
                }
            }
        }

        return true;
    }
    return false;
};

bool DigitalOut::Run()
{
    return false;
};