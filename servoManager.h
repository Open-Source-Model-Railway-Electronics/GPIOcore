#pragma once

#include <Servo.h>

class ServoManager
{
public:
    static const uint8_t MAX_MANAGED_SERVOS  = 6;

    ServoManager();

    bool attach(uint8_t pin);
    void detach(uint8_t pin);
    void setServo(uint8_t pin, uint8_t pos);

private:
    Servo servos[MAX_MANAGED_SERVOS ];
    int8_t pinMap[MAX_MANAGED_SERVOS ]; // -1 = free, anders: pin number

    int8_t findIndexByPin(uint8_t pin);
    int8_t findFreeIndex();
};

extern bool callbackAttach(uint8_t pin) __attribute__((weak));
extern void callbackDetach(uint8_t pin) __attribute__((weak));
extern void callbackSetServo(uint8_t pin, uint8_t pos) __attribute__((weak));