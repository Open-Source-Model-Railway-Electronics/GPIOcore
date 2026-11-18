#pragma once
// Interface between the host program (main.cpp, normal C++) and the firmware side
// (arduino_shim.cpp + sketch.cpp, compiled with the firmware's packed struct layout).
// Only plain types cross it.

#include <stdint.h>
#include <stddef.h>

constexpr int HostPins = 22;   // D0-D13, A0-A7 (Nano)

// ── provided by main.cpp ──────────────────────────────────────────────────────
uint32_t hostMillis();
uint32_t hostMicros();
void     hostDelay(uint32_t ms);            // the firmware's delay(): the clock jumps ahead, no real wait

// ── provided by arduino_shim.cpp ──────────────────────────────────────────────
void     hostSerialPush(const uint8_t *data, size_t n);   // bytes from the GUI (UART RX)
size_t   hostSerialPending();                             // RX bytes the firmware has not read yet
size_t   hostSerialTake(uint8_t *out, size_t max);        // bytes the firmware sent (UART TX at 115200; noise at other rates but 9600)
size_t   hostSerialMark();                                // TX position, for hostSerialDropFrom()
void     hostSerialDropFrom(size_t mark);                 // forget what was sent after mark
// A fake HC-05 in AT mode on D0/D1: answers "OK" to every AT command it gets at this baud rate (0 = none)
void     hostHc05(uint32_t baud);
bool     hostHc05Take(char *line, size_t max);            // the next AT command it received (without CR LF)

void     hostSetLevel(int pin, int level);                // voltage on a pin, 0-1023 (1023 = pulled up)
int      hostLevel(int pin);
// per pin: mode (INPUT 0 / OUTPUT 1 / INPUT_PULLUP 2), level written, PWM value, servo angle (0xFF = no servo)
void     hostPinState(uint8_t out[HostPins * 4]);

uint8_t *hostEeprom();                                    // 1024 bytes
uint32_t hostEepromWrites();                              // counts writes (change detection)

// ── provided by sketch.cpp (the firmware itself) ──────────────────────────────
void     fwSetup();
void     fwLoop();
uint8_t  fwExecute(const uint8_t *frame, int len);       // <command> <payload>: through messageReceived(), replies dropped; returns the NACK code (0 = ACK)
void     fwDccAccessory(uint16_t address, uint8_t direction, uint8_t power);   // as NmraDcc: notifyDccAccTurnoutOutput()
void     fwDccExtended(uint16_t address, uint8_t value);                       // notifyDccSigOutputState()
void     fwDccSpeed(uint16_t address, uint8_t speed, uint8_t forward);         // notifyDccSpeed(), 128 steps
void     fwDccFunctions(uint16_t address, uint8_t f0f4);                       // notifyDccFunc(), FN_0_4
int      fwOutputState(int index, uint8_t *out);         // 16 bytes, see main.cpp
int      fwInputState(int index, uint8_t *out);          // 8 + 4 x steps bytes
int      fwBoardState(uint8_t *out);                     // 3 + name
int      fwGlobals(uint8_t *out);                        // configSession, serialIsIO, configMode
