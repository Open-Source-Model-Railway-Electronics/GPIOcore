#pragma once
// Arduino core shim for gpiocore_fw_host: the GPIOcore firmware (software/GPIOcore) compiled for
// the PC. Only what the firmware uses is here. Pins, Serial, EEPROM and the clock are emulated in
// arduino_shim.cpp; the host program (main.cpp) drives them through hostapi.h.
//
// The firmware is compiled with -fpack-struct -mno-ms-bitfields so its EEPROM structs have the
// AVR layout (sizeof(Settings) == 10, sizeof(InputSettings) == 4, checked by static_asserts).
// Do not include C++ standard headers here: their types would get that packing too.

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define ARDUINO_HOST_SHIM 1

typedef uint8_t  byte;
typedef bool     boolean;
typedef uint16_t word;

#define HIGH          1
#define LOW           0
#define INPUT         0
#define OUTPUT        1
#define INPUT_PULLUP  2

// Arduino Nano pin numbers
static const uint8_t A0 = 14, A1 = 15, A2 = 16, A3 = 17, A4 = 18, A5 = 19, A6 = 20, A7 = 21;
#define NUM_DIGITAL_PINS 22
#define digitalPinToInterrupt(p) ((p) == 2 ? 0 : (p) == 3 ? 1 : -1)

// AVR bits the firmware touches (GPIOcore.ino: RX off for the DFPlayer)
extern volatile uint8_t UCSR0B;
#define RXEN0  4
#define TXEN0  3
#define _BV(bit) (1 << (bit))
#define E2END  0x3FF
#define PROGMEM

// the same macros as the AVR core (after the C headers, so they do not clash with them)
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
int  digitalRead(uint8_t pin);
int  analogRead(uint8_t pin);
void analogWrite(uint8_t pin, int value);

long random(long howBig);
long random(long howSmall, long howBig);
void randomSeed(unsigned long seed);
long map(long x, long inMin, long inMax, long outMin, long outMax);

class HardwareSerial
{
public:
    void   begin(unsigned long baud);
    void   end();
    int    available();
    int    read();
    int    peek();
    void   flush() {}
    int    availableForWrite();
    size_t write(uint8_t b);
    size_t write(const uint8_t *buffer, size_t size);
    size_t write(const char *s) { return write(reinterpret_cast<const uint8_t *>(s), strlen(s)); }
    operator bool() const { return true; }
};

extern HardwareSerial Serial;
