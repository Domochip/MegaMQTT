#include "PilotWire.h"

void PilotWire::SetOrder(uint8_t order)
{
    _currentOrder = order;
    if (_currentOrder <= 10) //0-10 : Arrêt
    {
        //Positive half only
        digitalWrite(_pinPos, HIGH);
        digitalWrite(_pinNeg, LOW);
    }
    else if (_currentOrder <= 20) //11-20 : Hors Gel
    {
        //Negative half only
        digitalWrite(_pinPos, LOW);
        digitalWrite(_pinNeg, HIGH);
    }
    else if (_currentOrder <= 50) //21-30(31-40;41-50) : Eco (Confort-2; Confort-1)
    {
        //Full wave
        digitalWrite(_pinPos, HIGH);
        digitalWrite(_pinNeg, HIGH);
    }
    else //51-99 : Confort
    {
        //Nothing on PilotWire
        digitalWrite(_pinPos, LOW);
        digitalWrite(_pinNeg, LOW);
    }

    //Publish new Order back
    _evtMgr->AddEvent((String(_id) + F("/state")).c_str(), String(_currentOrder).c_str());
};

PilotWire::PilotWire(JsonVariant config, EventManager *evtMgr)
{
    if (config["id"].isNull())
        return;

    if (config["pins"].isNull())
        return;

    char workingPinsList[6];
    uint8_t pinPos, pinNeg;

    //Copy pins list
    strncpy(workingPinsList, config["pins"].as<const char *>(), sizeof(workingPinsList));

    //Parse it
    char *pinStr;
    pinStr = strtok(workingPinsList, ",");
    if (!pinStr)
        return;
    pinPos = atoi(pinStr);

    pinStr = strtok(0, ",");
    if (!pinStr)
        return;
    pinNeg = atoi(pinStr);

    //call Init with parsed values
    Init(config["id"].as<const char *>(), pinPos, pinNeg, evtMgr);
};
void PilotWire::Init(const char *id, uint8_t pinPos, uint8_t pinNeg, EventManager *evtMgr)
{
    Serial.print(F("[PilotWire] Init("));
    Serial.print(id);
    Serial.print(',');
    Serial.print(pinPos);
    Serial.print(',');
    Serial.print(pinNeg);
    Serial.println(')');

    //Check if pins are available
    if (!IsPinAvailable(pinPos))
        return;
    if (!IsPinAvailable(pinNeg))
        return;

    //save pointer to Eventmanager
    _evtMgr = evtMgr;

    //copy id pointer
    _id = id;

    //save pin numbers
    _pinPos = pinPos;
    _pinNeg = pinNeg;

    //setup output
    pinMode(_pinPos, OUTPUT);
    digitalWrite(_pinPos, LOW);
    pinMode(_pinNeg, OUTPUT);
    digitalWrite(_pinNeg, LOW);

    _initialized = true;

    //Initialization publish
    _evtMgr->AddEvent((String(_id) + F("/state")).c_str(), "51");
};
void PilotWire::MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic)
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
bool PilotWire::MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length)
{
    //if relevantPartOfTopic starts with id of this device ending with '/'
    if (!strncmp(relevantPartOfTopic, _id, strlen(_id)) && relevantPartOfTopic[strlen(_id)] == '/')
    {
        //if topic finishes by '/command'
        if (!strcmp_P(relevantPartOfTopic + strlen(relevantPartOfTopic) - 8, PSTR("/command")))
        {
            uint8_t newOrder = 51; //default is Confort
            if (length > 0 && payload[0] >= '0' && payload[0] <= '9')
                newOrder = payload[0] - '0';
            if (length > 1 && payload[1] >= '0' && payload[1] <= '9')
                newOrder = newOrder * 10 + (payload[1] - '0');
            SetOrder(newOrder);
        }

        return true;
    }
    return false;
};
bool PilotWire::Run()
{
    return false;
};