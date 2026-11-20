// Arduino core / library emulation for gpiocore_fw_host (see arduino/Arduino.h).
// Single threaded: only the host program's main loop calls into here.

#include <Arduino.h>
#include <EEPROM.h>
#include <Servo.h>
#include "src/NmraDcc.h"
#include "hostapi.h"

// ── Clock ─────────────────────────────────────────────────────────────────────
unsigned long millis()                 { return hostMillis(); }
unsigned long micros()                 { return hostMicros(); }
void delay(unsigned long ms)           { hostDelay(uint32_t(ms)); }
void delayMicroseconds(unsigned int)   {}

// ── Pins ──────────────────────────────────────────────────────────────────────
// level[]: the voltage from outside, 0-1023. 1023 = nothing pressed (pull-up, also the external
// pull-ups on A6/A7). A digital read is HIGH from 512 up.
static uint8_t  pinModes[HostPins];
static uint8_t  written[HostPins];
static uint8_t  pwm[HostPins];
static uint16_t level[HostPins] = { 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
                                    1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023 };
static uint8_t  servoAngle[HostPins] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

void pinMode(uint8_t pin, uint8_t mode)
{
    if (pin < HostPins) pinModes[pin] = mode;
}

void digitalWrite(uint8_t pin, uint8_t value)
{
    if (pin >= HostPins) return;
    written[pin] = value ? HIGH : LOW;
    pwm[pin]     = value ? 255 : 0;
}

int digitalRead(uint8_t pin)
{
    if (pin >= HostPins) return LOW;
    if (pinModes[pin] == OUTPUT) return written[pin];
    return level[pin] >= 512 ? HIGH : LOW;
}

int analogRead(uint8_t pin)
{
    if (pin >= 14) pin -= 14;                 // A0-A7 or channel 0-7
    return level[14 + (pin & 7)];
}

void analogWrite(uint8_t pin, int value)
{
    if (pin >= HostPins) return;
    pwm[pin]     = uint8_t(value < 0 ? 0 : value > 255 ? 255 : value);
    written[pin] = pwm[pin] >= 128 ? HIGH : LOW;
}

void hostSetLevel(int pin, int value)
{
    if (pin >= 0 && pin < HostPins) level[pin] = uint16_t(value < 0 ? 0 : value > 1023 ? 1023 : value);
}

int hostLevel(int pin)
{
    return pin >= 0 && pin < HostPins ? level[pin] : 0;
}

void hostPinState(uint8_t out[HostPins * 4])
{
    for (int p = 0; p < HostPins; ++p)
    {
        out[p * 4]     = pinModes[p];
        out[p * 4 + 1] = written[p];
        out[p * 4 + 2] = pwm[p];
        out[p * 4 + 3] = servoAngle[p];
    }
}

// ── Random numbers (the AVR libc generator: values differ, the use does not) ──
static unsigned long randomState = 1;

static long nextRandom()
{
    randomState = randomState * 1103515245UL + 12345UL;
    return long((randomState >> 1) & 0x7FFFFFFF);
}

long random(long howBig)                 { return howBig <= 0 ? 0 : nextRandom() % howBig; }
long random(long howSmall, long howBig)  { return howSmall >= howBig ? howSmall : howSmall + random(howBig - howSmall); }
void randomSeed(unsigned long seed)      { if (seed) randomState = seed; }

long map(long x, long inMin, long inMax, long outMin, long outMax)
{
    return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

// ── Serial (UART) ─────────────────────────────────────────────────────────────
// RX: what the GUI sent. TX: what the firmware sends; forwarded as it is at 115200 baud (the GUI's
// line). At 9600 baud it is the board link (and a DFPlayer): kept apart for hostLinkTake(). At any other
// baud rate (Bluetooth set-up) the GUI gets noise: one 0xF0 per byte, never a valid frame.
volatile uint8_t UCSR0B;
HardwareSerial   Serial;

static const size_t RxSize = 8192;
static uint8_t  rx[RxSize];
static size_t   rxHead, rxTail;               // ring buffer
static uint8_t  tx[16384];
static size_t   txLen;
static uint8_t  link[4096];                   // TX at 9600 baud: the board link / DFPlayer line
static size_t   linkLen;
static unsigned long baud;

// Fake HC-05 (AT mode): hears the firmware's TX at its baud rate, answers "OK\r\n" on RX (D0)
static unsigned long hc05Baud;
static char          hc05Line[64];
static size_t        hc05Len;
static char          hc05Heard[8][64];        // AT commands received, for the GUI's tests
static size_t        hc05Count;

static void hc05Hear(uint8_t b)
{
    if (b == '\r') return;
    if (b != '\n')
    {
        if (hc05Len < sizeof(hc05Line) - 1) hc05Line[hc05Len++] = char(b);
        return;
    }
    hc05Line[hc05Len] = 0;
    if (hc05Len >= 2 && hc05Line[0] == 'A' && hc05Line[1] == 'T')
    {
        if (hc05Count < 8) memcpy(hc05Heard[hc05Count++], hc05Line, hc05Len + 1);
        static const uint8_t ok[] = { 'O', 'K', '\r', '\n' };
        hostSerialPush(ok, sizeof(ok));
    }
    hc05Len = 0;
}

void hostHc05(uint32_t b) { hc05Baud = b; hc05Len = 0; }

bool hostHc05Take(char *line, size_t max)
{
    if (!hc05Count || !max) return false;
    strncpy(line, hc05Heard[0], max - 1);
    line[max - 1] = 0;
    memmove(hc05Heard[0], hc05Heard[1], sizeof(hc05Heard[0]) * --hc05Count);
    return true;
}

void HardwareSerial::begin(unsigned long b)
{
    baud    = b;
    UCSR0B |= _BV(RXEN0) | _BV(TXEN0);
}

void HardwareSerial::end()
{
    baud    = 0;
    UCSR0B &= ~(_BV(RXEN0) | _BV(TXEN0));
    rxHead  = rxTail = 0;
}

int HardwareSerial::available()
{
    return int((rxHead + RxSize - rxTail) % RxSize);
}

int HardwareSerial::read()
{
    if (rxHead == rxTail) return -1;
    const uint8_t b = rx[rxTail];
    rxTail = (rxTail + 1) % RxSize;
    return b;
}

int HardwareSerial::peek()
{
    return rxHead == rxTail ? -1 : rx[rxTail];
}

int HardwareSerial::availableForWrite()
{
    return 63;                                // the AVR core's TX buffer: never full here
}

size_t HardwareSerial::write(uint8_t b)
{
    if (!(UCSR0B & _BV(TXEN0))) return 1;
    if (hc05Baud && baud == hc05Baud) hc05Hear(b);
    if (baud == 9600) { if (linkLen < sizeof(link)) link[linkLen++] = b; return 1; }
    if (txLen < sizeof(tx)) tx[txLen++] = baud == 115200 ? b : 0xF0;
    return 1;
}

size_t HardwareSerial::write(const uint8_t *buffer, size_t size)
{
    for (size_t i = 0; i < size; ++i) write(buffer[i]);
    return size;
}

void hostSerialPush(const uint8_t *data, size_t n)
{
    if (!(UCSR0B & _BV(RXEN0))) return;      // UART receiver off: the bytes are lost, as on the board
    for (size_t i = 0; i < n; ++i)
    {
        const size_t next = (rxHead + 1) % RxSize;
        if (next == rxTail) return;           // overrun
        rx[rxHead] = data[i];
        rxHead = next;
    }
}

size_t hostSerialPending()
{
    return size_t(Serial.available());
}

size_t hostSerialTake(uint8_t *out, size_t max)
{
    const size_t n = txLen < max ? txLen : max;
    memcpy(out, tx, n);
    memmove(tx, tx + n, txLen - n);
    txLen -= n;
    return n;
}

size_t hostLinkTake(uint8_t *out, size_t max)
{
    const size_t n = linkLen < max ? linkLen : max;
    memcpy(out, link, n);
    memmove(link, link + n, linkLen - n);
    linkLen -= n;
    return n;
}

size_t hostSerialMark()               { return txLen; }
void   hostSerialDropFrom(size_t mark) { if (mark < txLen) txLen = mark; }

// ── EEPROM: blank like a new chip ────────────────────────────────────────────
EEPROMClass     EEPROM;
static uint8_t  eeprom[E2END + 1];
static uint32_t eepromWrites;
static bool     eepromBlank = (memset(eeprom, 0xFF, sizeof(eeprom)), true);

uint8_t hostEepromRead(int address)              { return eeprom[address & E2END]; }
void    hostEepromWrite(int address, uint8_t v)  { eeprom[address & E2END] = v; ++eepromWrites; }
uint8_t *hostEeprom()                            { (void)eepromBlank; return eeprom; }
uint32_t hostEepromWrites()                      { return eepromWrites; }

// ── Servo ─────────────────────────────────────────────────────────────────────
uint8_t Servo::attach(int pin)
{
    m_pin = pin;
    if (pin >= 0 && pin < HostPins) servoAngle[pin] = uint8_t(m_angle);
    return 1;
}

void Servo::detach()
{
    if (m_pin >= 0 && m_pin < HostPins) servoAngle[m_pin] = 0xFF;
    m_pin = -1;
}

void Servo::write(int angle)
{
    m_angle = angle < 0 ? 0 : angle > 180 ? 180 : angle;
    if (m_pin >= 0 && m_pin < HostPins) servoAngle[m_pin] = uint8_t(m_angle);
}

// ── NmraDcc: no DCC signal on the PC. The host program calls the firmware's notify callbacks
// directly (sketch.cpp, fwDcc*), as NmraDcc::process() does for a decoded packet.
NmraDcc::NmraDcc() {}
void    NmraDcc::pin(uint8_t, uint8_t, uint8_t) {}
void    NmraDcc::pin(uint8_t, uint8_t) {}
void    NmraDcc::init(uint8_t, uint8_t, uint8_t, uint8_t) {}
uint8_t NmraDcc::process() { return 0; }
