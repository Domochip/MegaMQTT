#include "RollerShutter.h"

void RollerShutter::GoDown()
{
    if (!_initialized)
        return;

    _isMoving = Down;

    if (!_veluxType) //if normal Roller Shutter
    {
        digitalWrite(_pinRollerDir, LOW);
        _movementStart = millis();
        digitalWrite(_pinRollerPower, HIGH);
    }
    else //if Velux Roller Shutter : RollerDir=RollerUp; RollerPower=RollerDown
    {
        digitalWrite(_pinRollerDir, LOW);
        _movementStart = millis();
        digitalWrite(_pinRollerPower, HIGH);
    }
}

void RollerShutter::GoUp()
{
    if (!_initialized)
        return;

    _isMoving = Up;

    if (!_veluxType) //if normal Roller Shutter
    {
        digitalWrite(_pinRollerDir, HIGH);
        _movementStart = millis();
        digitalWrite(_pinRollerPower, HIGH);
    }
    else //if Velux Roller Shutter : RollerDir=RollerUp; RollerPower=RollerDown
    {
        digitalWrite(_pinRollerPower, LOW);
        _movementStart = millis();
        digitalWrite(_pinRollerDir, HIGH);
    }
}

void RollerShutter::Stop()
{
    if (!_initialized)
        return;

    if (_isMoving == No)
        return;

    //Stop movement
    if (!_veluxType) //if normal Roller Shutter
        digitalWrite(_pinRollerPower, LOW);
    else //if Velux Roller Shutter : RollerDir=RollerUp; RollerPower=RollerDown
    {
        digitalWrite(_pinRollerPower, LOW);
        digitalWrite(_pinRollerDir, LOW);
    }

    switch (_isMoving)
    {
    case Up:
        _currentPosition += ((float)(millis() - _movementStart)) / _travelTime / 10;
        break;
    case Down:
        _currentPosition -= ((float)(millis() - _movementStart)) / _travelTime / 10;
        break;
    case No: //never occurs
        break;
    }

    if (_currentPosition < 0.0)
        _currentPosition = 0.0;
    if (_currentPosition > 100.0)
        _currentPosition = 100.0;

    _isMoving = No;

    Serial.print(F("[RollerShutter] "));
    Serial.print(_id);
    Serial.print(F(" is now at "));
    Serial.print(round(_currentPosition));
    Serial.println('%');

    //Send new position through MQTT
    _evtMgr->AddEvent((String(_id) + F("/state")).c_str(), String(round(_currentPosition)).c_str());
}

RollerShutter::RollerShutter(JsonVariant config, EventManager *evtMgr)
{
    if (config["id"].isNull())
        return;

    if (config["pins"].isNull())
        return;

    if (config["travelTime"].isNull() || config["travelTime"].as<uint8_t>() == 0)
        return;

    if (config["pins"][0].isNull() || config["pins"][1].isNull() || config["pins"][2].isNull() || config["pins"][3].isNull())
        return;

    if (!config["pins"][0].as<uint8_t>() || !config["pins"][1].as<uint8_t>() || !config["pins"][2].as<uint8_t>() || !config["pins"][3].as<uint8_t>())
        return;

    //call Init with parsed values
    Init(config["id"].as<const char *>(), config["pins"][0].as<uint8_t>(), config["pins"][1].as<uint8_t>(), config["pins"][2].as<uint8_t>(), config["pins"][3].as<uint8_t>(), config["travelTime"].as<uint8_t>(), config["velux"].as<bool>(), evtMgr);
}

void RollerShutter::Init(const char *id, uint8_t pinBtnUp, uint8_t pinBtnDown, uint8_t pinRollerDir, uint8_t pinRollerPower, uint8_t travelTime, bool veluxType, EventManager *evtMgr)
{
    //DEBUG
    Serial.print(F("[RollerShutter] Init("));
    Serial.print(id);
    Serial.print(',');
    Serial.print(pinBtnUp);
    Serial.print(',');
    Serial.print(pinBtnDown);
    Serial.print(',');
    Serial.print(pinRollerDir);
    Serial.print(',');
    Serial.print(pinRollerPower);
    Serial.print(',');
    if (!veluxType)
        Serial.print(F("normal"));
    else
        Serial.print(F("velux"));
    Serial.println(F(") -> Closing Shutter to get it ready for operation"));

    //Check if pins are available
    if (!IsPinAvailable(pinBtnUp))
        return;
    if (!IsPinAvailable(pinBtnDown))
        return;
    if (!IsPinAvailable(pinRollerDir))
        return;
    if (!IsPinAvailable(pinRollerPower))
        return;

    //save EventManager
    _evtMgr = evtMgr;

    //copy id pointer
    _id = id;

    //start buttons
    _btnUp.attach(pinBtnUp, INPUT_PULLUP);
    _btnUp.interval(DEBOUNCE_INTERVAL);
    _btnDown.attach(pinBtnDown, INPUT_PULLUP);
    _btnDown.interval(DEBOUNCE_INTERVAL);

    //save pin numbers
    _pinRollerDir = pinRollerDir;
    _pinRollerPower = pinRollerPower;

    //save veluxType
    _veluxType = veluxType;

    //setup outputs
    pinMode(_pinRollerDir, OUTPUT);
    pinMode(_pinRollerPower, OUTPUT);

    //save travel time
    _travelTime = travelTime;

    _initialized = true;

    //Close completely the Roller to initialize position
    //Go Down
    GoDown();
    //during full travelTime, then we are Ready
    _outputTimer.SetOnceTimeout(1000L * _travelTime);
}

void RollerShutter::MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic)
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

bool RollerShutter::MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length)
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
            if (length > 2 && (payload[2] < '0' || payload[2] > '9'))
                return true;
            if (length > 3)
                return true;

            //Convert requested position
            uint16_t requestedPosition = payload[0] - '0';
            if (length > 1)
                requestedPosition = requestedPosition * 10 + (payload[1] - '0');
            if (length > 2)
                requestedPosition = requestedPosition * 10 + (payload[2] - '0');

            //fix too wide value
            if (requestedPosition > 100)
                requestedPosition = 100;

            //if roller is moving, then stop it (and then current Position will be refreshed)
            if (_isMoving != No)
            {
                Stop();
                _outputTimer.Stop();
            }

            //if requested is higher than current
            if (requestedPosition > _currentPosition)
            {
                //Go Up for the right duration
                GoUp();
                _outputTimer.SetTimeout(((uint16_t)_travelTime) * 10 * (((float)requestedPosition) - _currentPosition));
            }
            else
            {
                //else Go Down for the right duration
                GoDown();
                _outputTimer.SetTimeout(((uint16_t)_travelTime) * 10 * (_currentPosition - requestedPosition));
            }

            Serial.print(F("[RollerShutter] "));
            Serial.print(_id);
            Serial.print(F(" is going to "));
            Serial.print(requestedPosition);
            Serial.println('%');
        }

        return true;
    }
    return false;
}

bool RollerShutter::Run()
{
    if (!_initialized)
        return false;

    if (!_ready)
    {
        if (_outputTimer.IsTimeoutOver())
        {
            Stop();
            _ready = true;
            Serial.print(F("[RollerShutter] "));
            Serial.print(_id);
            Serial.println(F(" is ready"));
        }
        else
            return false; //While RollerShutter going to be ready, we don't need TimeCritical Operation
    }

    //_btnUp state changed
    if (_btnUp.update())
    {
        //btnUp just pressed
        if (_btnUp.fell())
        {
            //no movement is in progress
            if (_isMoving == No)
            {
                //Start movement
                GoUp();
                //Start full travel time timer (even if we already are at 70%) //TODO maybe improved at a later time
                _outputTimer.SetOnceTimeout(((uint16_t)_travelTime) * 1000);
            }
            else //movement already in progress
            {
                Stop();
                _outputTimer.Stop();
            }
        }
        //btnUp just released AND press where longer than threshold
        if (_btnUp.rose() && _btnUp.previousDuration() > LONGPRESS_THRESHOLD)
        {
            //then stop
            Stop();
            _outputTimer.Stop();
        }
    }

    //_btnDown state changed
    if (_btnDown.update())
    {
        //_btnDown just pressed
        if (_btnDown.fell())
        {
            //no movement is in progress
            if (_isMoving == No)
            {
                //Start movement
                GoDown();
                //Start full travel time timer (even if we already are at 70%) //TODO maybe improved at a later time
                _outputTimer.SetOnceTimeout(((uint16_t)_travelTime) * 1000);
            }
            else //movement already in progress
            {
                Stop();
                _outputTimer.Stop();
            }
        }
        //_btnDown just released AND press where longer than threshold
        if (_btnDown.rose() && _btnDown.previousDuration() > LONGPRESS_THRESHOLD)
        {
            //then stop
            Stop();
            _outputTimer.Stop();
        }
    }

    //if timer is over then Stop
    if (_outputTimer.IsTimeoutOver())
        Stop();

    //if roller is moving, we need to watch closely for buttons and timer end
    return _isMoving != No;
};