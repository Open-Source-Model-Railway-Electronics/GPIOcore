#pragma once
// Servo library shim: records the angle per pin, the host program reports it.

#include <Arduino.h>

class Servo
{
public:
    uint8_t attach(int pin);
    uint8_t attach(int pin, int, int) { return attach(pin); }
    void    detach();
    void    write(int angle);
    int     read() const { return m_angle; }
    bool    attached() const { return m_pin >= 0; }

private:
    int m_pin   = -1;
    int m_angle = 90;
};
