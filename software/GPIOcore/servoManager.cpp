#include "servoManager.h"

ServoManager::ServoManager()
{
    for (uint8_t i = 0; i < MAX_MANAGED_SERVOS ; i++)
    {
        pinMap[i] = -1;
    }
}

int8_t ServoManager::findIndexByPin(uint8_t pin)
{
    for (uint8_t i = 0; i < MAX_MANAGED_SERVOS ; i++)
    {
        if (pinMap[i] == pin)
            return i;
    }
    return -1;
}

int8_t ServoManager::findFreeIndex()
{
    for (uint8_t i = 0; i < MAX_MANAGED_SERVOS ; i++)
    {
        if (pinMap[i] == -1)
            return i;
    }
    return -1;
}

bool ServoManager::attach(uint8_t pin)
{
    if (findIndexByPin(pin) != -1)
        return true; // already attached

    int8_t idx = findFreeIndex();
    if (idx == -1)
        return false; // no free servos

    servos[idx].attach(pin);
    pinMap[idx] = pin;
    return true;
}

void ServoManager::detach(uint8_t pin)
{
    int8_t idx = findIndexByPin(pin);
    if (idx == -1)
        return;

    servos[idx].detach();
    pinMap[idx] = -1;
}

void ServoManager::setServo(uint8_t pin, uint8_t pos)
{
    int8_t idx = findIndexByPin(pin);
    if (idx == -1)
        return;

    servos[idx].write(pos);
}
