#include "PilotWire.h"

void PilotWire::setOrder(uint8_t order)
{
    _currentOrder = order;
    if (_currentOrder <= 10) //0-10 : Arrêt
    {
        //Positive half only
        digitalWrite(_pinPos, (_invertOutput ? LOW : HIGH));
        digitalWrite(_pinNeg, (_invertOutput ? HIGH : LOW));
    }
    else if (_currentOrder <= 20) //11-20 : Hors Gel
    {
        //Negative half only
        digitalWrite(_pinPos, (_invertOutput ? HIGH : LOW));
        digitalWrite(_pinNeg, (_invertOutput ? LOW : HIGH));
    }
    else if (_currentOrder <= 50) //21-30(31-40;41-50) : Eco (Confort-2; Confort-1)
    {
        //Full wave
        digitalWrite(_pinPos, (_invertOutput ? LOW : HIGH));
        digitalWrite(_pinNeg, (_invertOutput ? LOW : HIGH));
    }
    else //51-99 : Confort
    {
        //Nothing on PilotWire
        digitalWrite(_pinPos, (_invertOutput ? HIGH : LOW));
        digitalWrite(_pinNeg, (_invertOutput ? HIGH : LOW));
    }

    Serial.print(F("[PilotWire] "));
    Serial.print(_id);
    Serial.print(F(" received new order : "));
    Serial.println(_currentOrder);

    //Publish new Order back
    _evtMgr->addEvent((String(_id) + F("/state")).c_str(), String(_currentOrder).c_str());
};

PilotWire::PilotWire(JsonVariant config, EventManager *evtMgr)
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
    init(config["id"].as<const char *>(), config["pins"][0].as<uint8_t>(), config["pins"][1].as<uint8_t>(), config["invert"].as<bool>(), evtMgr);
};
void PilotWire::init(const char *id, uint8_t pinPos, uint8_t pinNeg, bool invertOutput, EventManager *evtMgr)
{
    Serial.print(F("[PilotWire] Init("));
    Serial.print(id);
    Serial.print(',');
    Serial.print(pinPos);
    Serial.print(',');
    Serial.print(pinNeg);
    Serial.println(')');

    //Check if pins are available
    if (!isPinAvailable(pinPos))
        return;
    if (!isPinAvailable(pinNeg))
        return;

    //save pointer to Eventmanager
    _evtMgr = evtMgr;

    //copy id
    strcpy(_id, id);

    //save pin numbers
    _pinPos = pinPos;
    _pinNeg = pinNeg;

    //save invert output
    _invertOutput = invertOutput;

    //setup output
    pinMode(_pinPos, OUTPUT);
    digitalWrite(_pinPos, (_invertOutput ? HIGH : LOW));
    pinMode(_pinNeg, OUTPUT);
    digitalWrite(_pinNeg, (_invertOutput ? HIGH : LOW));

    _initialized = true;

    //Initialization publish
    _evtMgr->addEvent((String(_id) + F("/state")).c_str(), "51");
};
void PilotWire::mqttSubscribe(PubSubClient &mqttClient, const char *baseTopic)
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
bool PilotWire::mqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length)
{
    //if relevantPartOfTopic starts with id of this device ending with '/'
    if (!strncmp(relevantPartOfTopic, _id, strlen(_id)) && relevantPartOfTopic[strlen(_id)] == '/')
    {
        //if topic finishes by '/command'
        if (!strcmp_P(relevantPartOfTopic + strlen(relevantPartOfTopic) - 8, PSTR("/command")))
        {

            //Check Payload
            if (length == 0)
                return true;
            if (length > 0 && (payload[0] < '0' || payload[0] > '9'))
                return true;
            if (length > 1 && (payload[1] < '0' || payload[1] > '9'))
                return true;
            if (length > 2)
                return true;

            //convert to number
            uint8_t newOrder = payload[0] - '0';
            if (length > 1)
                newOrder = newOrder * 10 + (payload[1] - '0');

            setOrder(newOrder);
        }

        return true;
    }
    return false;
};
bool PilotWire::run()
{
    return false;
};