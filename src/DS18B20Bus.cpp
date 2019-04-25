#include "DS18B20Bus.h"

//-----------------------------------------------------------------------
// DS18X20 Read ScratchPad command
boolean DS18B20Bus::ReadScratchPad(byte addr[], byte data[])
{

    boolean crcScratchPadOK = false;

    //read scratchpad (if 3 failures occurs, then return the error
    for (byte i = 0; i < 3; i++)
    {
        // read scratchpad of the current device
        _oneWire.reset();
        _oneWire.select(addr);
        _oneWire.write(0xBE); // Read ScratchPad
        for (byte j = 0; j < 9; j++)
        { // read 9 bytes
            data[j] = _oneWire.read();
        }
        if (_oneWire.crc8(data, 8) == data[8])
        {
            crcScratchPadOK = true;
            i = 3; //end for loop
        }
    }

    return crcScratchPadOK;
}
//------------------------------------------
// DS18X20 Write ScratchPad command
void DS18B20Bus::WriteScratchPad(byte addr[], byte th, byte tl, byte cfg)
{

    _oneWire.reset();
    _oneWire.select(addr);
    _oneWire.write(0x4E); // Write ScratchPad
    _oneWire.write(th);   //Th 80°C
    _oneWire.write(tl);   //Tl 0°C
    _oneWire.write(cfg);  //Config
}
//------------------------------------------
// DS18X20 Copy ScratchPad command
void DS18B20Bus::CopyScratchPad(byte addr[])
{

    _oneWire.reset();
    _oneWire.select(addr);
    _oneWire.write(0x48); //Copy ScratchPad
}
//------------------------------------------
// Function to initialize DS18X20 sensors
void DS18B20Bus::SetupTempSensors()
{
    byte addr[8];
    byte data[9];
    boolean scratchPadReaded;

    //while we find some devices
    while (_oneWire.search(addr))
    {
        //if ROM received is incorrect or not a DS1822 or DS18B20 THEN continue to next device
        if ((_oneWire.crc8(addr, 7) != addr[7]) || (addr[0] != 0x22 && addr[0] != 0x28))
            continue;

        scratchPadReaded = ReadScratchPad(addr, data);
        //if scratchPad read failed then continue to next 1-Wire device
        if (!scratchPadReaded)
            continue;

        //if config is not correct
        if (data[2] != 0x50 || data[3] != 0x00 || data[4] != 0x7F)
        {

            //write ScratchPad with Th=80°C, Tl=0°C, Config 12bits resolution
            WriteScratchPad(addr, 0x50, 0x00, 0x7F);

            scratchPadReaded = ReadScratchPad(addr, data);
            //if scratchPad read failed then continue to next 1-Wire device
            if (!scratchPadReaded)
                continue;

            //so we finally can copy scratchpad to memory
            CopyScratchPad(addr);
        }
    }
}
//------------------------------------------
// DS18X20 Start Temperature conversion
void DS18B20Bus::StartConvertT()
{
    _oneWire.reset();
    _oneWire.skip();
    _oneWire.write(0x44); // start conversion
}
//------------------------------------------
// DS18X20 Read and Publish Temperatures from all sensors
void DS18B20Bus::ReadAndPublishTemperatures()
{
    uint8_t nbROMCode = 0;
    byte(*romCodes)[8] = NULL;

    uint8_t romCode[8];

    //list all romCodes
    _oneWire.reset_search();
    while (_oneWire.search(romCode))
    {
        //if ROM received is incorrect or not a Temperature sensor THEN continue to next device
        if ((_oneWire.crc8(romCode, 7) != romCode[7]) || (romCode[0] != 0x10 && romCode[0] != 0x22 && romCode[0] != 0x28))
            continue;

        //allocate memory
        if (!romCodes)
            romCodes = (byte(*)[8])malloc(++nbROMCode * 8 * sizeof(byte));
        else
            romCodes = (byte(*)[8])realloc(romCodes, ++nbROMCode * 8 * sizeof(byte));

        //copy the romCode
        for (byte i = 0; i < 8; i++)
        {
            romCodes[nbROMCode - 1][i] = romCode[i];
        }
    }

    byte data[12];           //buffer that receive scratchpad
    char romCodeA[17] = {0}; //to convert ROMCode to char*
    //now read all temperatures
    for (byte i = 0; i < nbROMCode; i++)
    {
        //if read of scratchpad (3 try inside function)
        if (!ReadScratchPad(romCodes[i], data))
        {
            // Convert the data to actual temperature
            // because the result is a 16 bit signed integer, it should
            // be stored to an "int16_t" type, which is always 16 bits
            // even when compiled on a 32 bit processor.
            int16_t raw = (data[1] << 8) | data[0];
            if (romCodes[i][0] == 0x10)
            {                   //type S temp Sensor
                raw = raw << 3; // 9 bit resolution default
                if (data[7] == 0x10)
                {
                    // "count remain" gives full 12 bit resolution
                    raw = (raw & 0xFFF0) + 12 - data[6];
                }
            }
            else
            {
                byte cfg = (data[4] & 0x60);
                // at lower res, the low bits are undefined, so let's zero them
                if (cfg == 0x00)
                    raw = raw & ~7; // 9 bit resolution, 93.75 ms
                else if (cfg == 0x20)
                    raw = raw & ~3; // 10 bit res, 187.5 ms
                else if (cfg == 0x40)
                    raw = raw & ~1; // 11 bit res, 375 ms
                                    // default is 12 bit resolution, 750 ms conversion time
            }

            //convert ROMCode to char*
            sprintf_P(romCodeA, PSTR("%02x%02x%02x%02x%02x%02x%02x%02x"), romCodes[i][0], romCodes[i][1], romCodes[i][2], romCodes[i][3], romCodes[i][4], romCodes[i][5], romCodes[i][6], romCodes[i][7]);

            //Send temperature through MQTT (final temperature is raw/16)
            _evtMgr->AddEvent((String(_id) + F("/temperatures/") + romCodeA + F("/temperature")).c_str(), String((float)raw / 16.0, 2).c_str());
        }
    }

    //now we need to free memory of romCodes

    if (romCodes)
        free(romCodes);
}

DS18B20Bus::DS18B20Bus(JsonVariant config, EventManager *evtMgr) : _oneWire(-1)
{
    if (config["id"].isNull())
        return;

    if (config["pin"].isNull())
        return;

    //call Init
    Init(config["id"].as<const char *>(), config["pin"].as<uint8_t>(), evtMgr);
};

void DS18B20Bus::Init(const char *id, uint8_t pinOneWire, EventManager *evtMgr)
{
    //DEBUG
    Serial.print(F("[DS18B20Bus] Init("));
    Serial.print(id);
    Serial.print(',');
    Serial.print(pinOneWire);
    Serial.println(')');

    //Check if pin is available
    if (!IsPinAvailable(pinOneWire))
        return;

    //save EventManager
    _evtMgr = evtMgr;

    //copy id pointer
    _id = id;

    //Configure OneWire
    _oneWire.begin(pinOneWire);

    //Initialize temperature sensors
    SetupTempSensors();

    _initialized = true;

    //start convert of temperature
    StartConvertT();
    _timer.SetOnceTimeout(800);
};

void DS18B20Bus::MqttSubscribe(PubSubClient &mqttClient, const char *baseTopic){};
bool DS18B20Bus::MqttCallback(char *relevantPartOfTopic, uint8_t *payload, unsigned int length)
{
    return false;
};
bool DS18B20Bus::Run()
{
    if (_timer.IsTimeoutOver())
    {
        Serial.print(F("[DS18B20Bus] "));
        Serial.print(_id);
        if (_convertInProgress)
        {
            Serial.println(F(" is publishing"));
            _convertInProgress = false;
            ReadAndPublishTemperatures();
            _timer.SetOnceTimeout((uint16_t)PUBLISH_PERIOD * 1000 - 800);
        }
        else
        {
            Serial.println(F(" is converting"));
            _convertInProgress = true;
            StartConvertT();
            _timer.SetOnceTimeout(800);
        }
    }
    return false;
};