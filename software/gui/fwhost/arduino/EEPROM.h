#pragma once
// EEPROM library shim: the ATmega328P's 1024 bytes, kept in memory by arduino_shim.cpp
// (optionally loaded from / saved to a file by the host program).

#include <Arduino.h>

uint8_t hostEepromRead(int address);
void    hostEepromWrite(int address, uint8_t value);

struct EEPROMClass
{
    uint8_t  read(int address)                  { return hostEepromRead(address); }
    void     write(int address, uint8_t value)  { hostEepromWrite(address, value); }
    void     update(int address, uint8_t value) { if (read(address) != value) write(address, value); }
    uint16_t length()                           { return E2END + 1; }

    template <typename T> T &get(int address, T &t)
    {
        uint8_t *p = reinterpret_cast<uint8_t *>(&t);
        for (size_t i = 0; i < sizeof(T); ++i) p[i] = read(address + int(i));
        return t;
    }

    template <typename T> const T &put(int address, const T &t)
    {
        const uint8_t *p = reinterpret_cast<const uint8_t *>(&t);
        for (size_t i = 0; i < sizeof(T); ++i) update(address + int(i), p[i]);
        return t;
    }
};

extern EEPROMClass EEPROM;
