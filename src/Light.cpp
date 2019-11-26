#include "Light.h"

void Light::on()
{
    if (digitalRead(_pinLight) == (_invertOutput ? HIGH : LOW))
    {
        digitalWrite(_pinLight, (_invertOutput ? LOW : HIGH));
        _evtMgr->addEvent((String(_id) + F("/state")).c_str(), "1");
    }
}
void Light::off()
{
    if (digitalRead(_pinLight) == (_invertOutput ? LOW : HIGH))
    {
        digitalWrite(_pinLight, (_invertOutput ? HIGH : LOW));
        _evtMgr->addEvent((String(_id) + F("/state")).c_str(), "0");
    }
}
void Light::toggle()
{
    digitalWrite(_pinLight, !digitalRead(_pinLight));
    _evtMgr->addEvent((String(_id) + F("/state")).c_str(), (digitalRead(_pinLight) == (_invertOutput ? HIGH : LOW)) ? "0" : "1");
}

Light::Light(JsonVariant config, EventManager *evtMgr)
{
    if (config["id"].isNull())
        return;

    if (config["pins"].isNull())
        return;

    if (config["pins"][0].isNull() || config["pins"][1].isNull())
        return;

    if (!config["pins"][0].as<uint8_t>() || !config["pins"][1].as<uint8_t>())
        return;

    //call Init with parsed values
    init(config["id"].as<const char *>(), config["pins"][0].as<uint8_t>(), config["pins"][1].as<uint8_t>(), config["pushbutton"].as<bool>(), config["invert"].as<bool>(), evtMgr);
}

void Light::init(const char *id, uint8_t pinBtn, uint8_t pinLight, bool pushButtonMode, bool invertOutput, EventManager *evtMgr)
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
    if (!isPinAvailable(pinBtn))
        return;
    if (!isPinAvailable(pinLight))
        return;

    //save pointer to Eventmanager
    _evtMgr = evtMgr;

    //copy id
    strcpy(_id, id);

    //start button
    _btn.attach(pinBtn, INPUT_PULLUP);
    _btn.interval(25);

    //save pin numbers
    _pinLight = pinLight;

    //save invert output
    _invertOutput = invertOutput;

    //setup output
    pinMode(_pinLight, OUTPUT);
    digitalWrite(_pinLight, (_invertOutput ? HIGH : LOW));

    //save pushButtonMode
    _pushButtonMode = pushButtonMode;

    _initialized = true;

    //Initialization publish
    _evtMgr->addEvent((String(_id) + F("/state")).c_str(), "0");
}
void Light::mqttSubscribe(PubSubClient &mqttClient, const char *baseTopic)
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

bool Light::mqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length)
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
                //OFF requested
                case '0':
                    off();
                    break;
                //ON requested
                case '1':
                    on();
                    break;
                //Toggle requested
                case 't':
                case 'T':
                    toggle();
                    break;
                }
            }
        }

        return true;
    }
    return false;
}

bool Light::run()
{
    if (!_initialized)
        return false;

    //if button state changed AND (not a pushButton OR input rose)
    if (_btn.update() && (!_pushButtonMode || _btn.rose()))
        toggle(); //then invert output

    //no time critical state, so always false is returned
    return false;
}