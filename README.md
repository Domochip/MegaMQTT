# MegaMQTT

Use an Arduino Mega + Ethernet HAT to make Home Automation through MQTT  
You can automate : 
- Lights
- Roller Shutter
- Velux Roller Shutter
- Temperature sensor (DS18B20)
- PilotWire (French standard to "pilot" electric heater)
- Single relay

## What do you need ?

### Hardware

 - An Arduino Mega 2560 ![Mega 2560](https://raw.github.com/Domochip/MegaMQTT/master/img/mega.jpg)
 - An Ethernet HAT (W5100) ![Ethernet HAT](https://raw.github.com/Domochip/MegaMQTT/master/img/W5100%20hat.jpg)
 - Some Relay boards for 110/220V ![Ethernet HAT](https://raw.github.com/Domochip/MegaMQTT/master/img/relay%20board.jpg)
 - (Power)
 - (Ethernet cable)

### Software

 - Visual Studio Code
 - PlatformIO

## Make It

Plug Ethernet Hat on your Mega, power it and flash this sketch using VSCode/PlatformIO.

First start is configured to use DHCP (or fallback to `192.168.1.177` if DHCP communication failed)

Have a look at Serial output of your Mega and go to default webpage using IP

## Set it up

To configure your system, you need to provide it a configuration JSON file like this example : 

```json
{
    "System": {
        "name": "MegaMQTT1",
        "ip": "192.168.1.176"
    },
    "MQTT": {
        "hostname": "192.168.1.19",
        "port": 1883,
        "username": "",
        "password": "",
        "baseTopic": "MegaMQTT"
    },
    "HADevices": [
        {
            "type": "RollerShutter",
            "id": "VR0",
            "pins": [22,23,24,25],
            "travelTime": 30,
            "invert": true
        },
        {
            "type": "RollerShutter",
            "id": "VR1",
            "pins": [26,27,28,29],
            "travelTime": 35,
            "velux": true
        },
        {
            "type": "Light",
            "id": "L0",
            "pins": [2,11],
            "pushbutton": false
        },
        {
            "type": "DS18B20Bus",
            "id": "DSB0",
            "pin": 3
        },
        {
            "type": "PilotWire",
            "id": "PW0",
            "pins": [30,31]
        },
        {
            "type": "DigitalOut",
            "id": "D0",
            "pin": 13
        }
    ]
}
```

## System

|ID|Type/Size|Description|
|--|--|--|
|name|16 char|name of the mega to identify it only|
|ip|Text|(optional) fix IP configuration (DHCP if empty or non existent)|

## MQTT

|ID|Type/Size|Description|
|--|--|--|
|hostname|32 char|ip or dns name of the MQTT broker|
|port|integer|TCP port of the MQTT server (standard default is 1883)|
|username|integer|(optional) MQTT username if required by broker|
|password|integer|(optional) MQTT password if required by broker|
|baseTopic|16 char|prefix used in all MQTT subscribe/publish|

## HADevices

HADevices are "logical devices" like a Roller Shutter or a Light

You can configure multiple devices of each type in the configuration JSON.

### Light

JSON requirements :  

|ID|Type/Size|Description|
|--|--|--|
|type|fixed value|Light|
|id|16 char|unique identifier of this HADevice|
|pins|2 integers|array of pin numbers : [button,Light relay]|
|pushbutton|boolean|(optional) true for push button (button with spring)|
|invert|boolean|(optional) true to invert output|

MQTT publication :  

|topic|data|Description|
|--|--|--|
|{MQTT BaseTopic}/{HADevice ID}/state|0 \| 1|0 : light is off ; 1 : light is on|

MQTT subscribtion :  

|topic|data|Description|
|--|--|--|
|{MQTT BaseTopic}/{HADevice ID}/command|0 \| 1 \| t|0 : power off light ; 1 : power on light ; toggle light|

### RollerShutter

JSON requirements :  

|ID|Type/Size|Description|
|--|--|--|
|type|fixed value|RollerShutter|
|id|16 char|unique identifier of this HADevice|
|velux|boolean|(optional) true for Velux SSL or SML RollerShutter|
|pins|4 integers|array of pin numbers : [buttonUp,buttonDown,Direction relay,Power relay]<br>(velux type : [buttonUp,buttonDown,Roller Up relay,Roller Down relay])|
|travelTime|integer|time in seconds for your Shutter to open completely|
|invert|boolean|(optional) true to invert output|

MQTT publication :  

|topic|data|Description|
|--|--|--|
|{MQTT BaseTopic}/{HADevice ID}/state|0->100| position of the roller shutter (%)|

MQTT subscribtion :  

|topic|data|Description|
|--|--|--|
|{MQTT BaseTopic}/{HADevice ID}/command|0->100|Move the Roller Shutter to the desired position (%)|

TODO : electric diagrams for normal and velux Roller Shutter

### DS18B20Bus

JSON requirements :  

|ID|Type/Size|Description|
|--|--|--|
|type|fixed value|Light|
|id|16 char|unique identifier of this HADevice|
|pin|1 integer|pin number of the OneWire bus|

MQTT publication :  

|topic|data|Description|
|--|--|--|
|{MQTT BaseTopic}/temperatures/{ROMCode}/temperature|-55.00->125.00|temperature (°C)|

If multiple sensors are on the Bus, all temperatures are published.  

### PilotWire

JSON requirements :  

|ID|Type/Size|Description|
|--|--|--|
|type|fixed value|Light|
|id|16 char|unique identifier of this HADevice|
|pins|2 integers|array of pin numbers : [Positive relay,Negative relay]|
|invert|boolean|(optional) true to invert output|

MQTT publication :  

|topic|data|Description|
|--|--|--|
|{MQTT BaseTopic}/{HADevice ID}/state|0->99|current Order value (0-10 : Arrêt ; 11-20 : Hors Gel ; 21-50 : Eco ; 51-99 : Confor)|

MQTT subscribtion :  

|topic|data|Description|
|--|--|--|
|{MQTT BaseTopic}/{HADevice ID}/command|0->99|requested PiloteWire Order (0-10 : Arrêt ; 11-20 : Hors Gel ; 21-50 : Eco ; 51-99 : Confor)|

### DigitalOut

JSON requirements :  

|ID|Type/Size|Description|
|--|--|--|
|type|fixed value|Light|
|id|16 char|unique identifier of this HADevice|
|pin|1 integer|pin number of the output to control|
|invert|boolean|(optional) true to invert output|

MQTT publication :  

|topic|data|Description|
|--|--|--|
|{MQTT BaseTopic}/{HADevice ID}/state|0 \| 1|0 : output is off ; 1 : output is on|

MQTT subscribtion :  

|topic|data|Description|
|--|--|--|
|{MQTT BaseTopic}/{HADevice ID}/command|0 \| 1|0 : power off output ; 1 : power on output|