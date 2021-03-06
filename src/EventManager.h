#ifndef EventManager_h
#define EventManager_h

#include "Arduino.h"

//Number of Event is the number of events to retain for send
#define NUMBER_OF_EVENTS 16
//number of retry to send event to Home Automation
#define MAX_RETRY_NUMBER 3

class EventManager
{
public:
  typedef struct
  {
    char topic[16 + 1 + 5 + 1]; //id(16)+/+state(longest topic for now)+0
    char payload[7];            //-10.25 (longest payload for now) (°C)
    bool sent;                  //event sent to HA or not
    byte retryLeft;             //number of retries left to send event to Home Automation
  } Event;

private:
  Event _eventsList[NUMBER_OF_EVENTS]; //events list
  byte _nextEventPos = 0;

public:
  EventManager();
  void addEvent(const char *topic, const char *payload);
  Event *available();
};

#endif