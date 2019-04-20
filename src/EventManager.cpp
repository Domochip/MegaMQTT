#include "EventManager.h"

EventManager::EventManager()
{
    for (byte i = 0; i < NUMBER_OF_EVENTS; i++)
    {
        _eventsList[i].topic[0] = 0;
        _eventsList[i].payload[0] = 0;
        _eventsList[i].sent = true;
        _eventsList[i].retryLeft = 0;
    }
}

void EventManager::AddEvent(const char *topic, const char *payload)
{
    strcpy(_eventsList[_nextEventPos].topic, topic);
    strcpy(_eventsList[_nextEventPos].payload, payload);
    _eventsList[_nextEventPos].sent = false;
    _eventsList[_nextEventPos].retryLeft = MAX_RETRY_NUMBER;

    _nextEventPos = (_nextEventPos + 1) % NUMBER_OF_EVENTS;
}

EventManager::Event *EventManager::Available()
{
    //for each events in the list starting by nextEventPos
    for (byte evPos = _nextEventPos, counter = 0; counter < NUMBER_OF_EVENTS; counter++, evPos = (evPos + 1) % NUMBER_OF_EVENTS)
    {
        //if event not yet sent and some retries left
        if (!_eventsList[evPos].sent && _eventsList[evPos].retryLeft)
            return &(_eventsList[evPos]); //return it
    }

    return NULL;
}