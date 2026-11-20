// The GPIOcore sketch, compiled as C++ for the PC, plus the probes the host program uses.
// Like the Arduino builder: Arduino.h first, then prototypes for functions the sketch uses
// before it defines them.

#include <Arduino.h>
#include <stdint.h>

void reportOutput(uint8_t index, uint8_t state, uint8_t value);

#include "GPIOcore.ino"

#include "hostapi.h"

void fwSetup() { setup(); }
void fwLoop()  { loop(); }

uint8_t fwExecute(const uint8_t *frame, int len)
{
    Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.command = frame[0];
    for (int i = 1; i < len && i - 1 < maxLen; ++i) msg.payload[i - 1] = frame[i];
    const size_t mark = hostSerialMark();
    transceiver.nack = 0;
    messageReceived(&msg, uint8_t(len - 1));
    hostSerialDropFrom(mark);                    // replies (GET) are not for the GUI
    const uint8_t nack = transceiver.nack;
    transceiver.nack = 0;
    return nack;
}

void fwDccAccessory(uint16_t address, uint8_t direction, uint8_t power)
{
    notifyDccAccTurnoutOutput(address, direction, power);
}

void fwDccExtended(uint16_t address, uint8_t value)
{
    notifyDccSigOutputState(address, value);
}

void fwDccSpeed(uint16_t address, uint8_t speed, uint8_t forward)
{
    notifyDccSpeed(address, address > 127 ? DCC_ADDR_LONG : DCC_ADDR_SHORT, speed,
                   forward ? DCC_DIR_FWD : DCC_DIR_REV, SPEED_STEP_128);
}

void fwDccFunctions(uint16_t address, uint8_t f0f4)
{
    notifyDccFunc(address, address > 127 ? DCC_ADDR_LONG : DCC_ADDR_SHORT, FN_0_4, f0f4);
}

// <index> <pinA pinB pinC pinD relayA relayB relayC> <type> <state> <data 0-3> <servo angle> <frog relays>
// (the GET_OUTPUT_DATA layout plus two bytes)
int fwOutputState(int index, uint8_t *out)
{
    Output &o = output[index];
    const Settings &s = o.eedata;
    int n = 0;
    out[n++] = uint8_t(index);
    out[n++] = s.pinA; out[n++] = s.pinB; out[n++] = s.pinC; out[n++] = s.pinD;
    out[n++] = s.relayPinA; out[n++] = s.relayPinB; out[n++] = s.relayPinC;
    out[n++] = s.type;
    out[n++] = uint8_t(s.state | (s.invertFrog << 1));   // as GET_OUTPUT_DATA: bit1 = invertFrog
    for (int i = 0; i < 4; ++i) out[n++] = s.data[i];
    out[n++] = o.position();
    out[n++] = o.frogRelays();
    return n;
}

// <index> <addr H> <addr L> <pin byte> <type> <output> <level> <steps> <step 5 bytes ...>
int fwInputState(int index, uint8_t *out)
{
    const InputSettings &s = input[index].eedata;
    int n = 0;
    out[n++] = uint8_t(index);
    out[n++] = uint8_t(s.myAddress >> 8);
    out[n++] = uint8_t(s.myAddress & 0xFF);
    out[n++] = s.pin;
    out[n++] = s.type;
    out[n++] = s.outputID;
    out[n++] = s.level;
    out[n++] = routeCount[index];
    for (uint8_t i = 0; i < routeCount[index]; ++i)
    {
        readStep(uint8_t(index), i, out + n);
        n += STEP_BYTES;
    }
    return n;
}

// <type> <flags> <delay> <name ...>
int fwBoardState(uint8_t *out)
{
    int n = 0;
    out[n++] = board.type;
    out[n++] = board.flags;
    out[n++] = board.delay;
    for (uint8_t i = 0; i < BOARD_NAME_LEN; ++i)
    {
        const uint8_t c = EEPROM.read(boardEEAddress + sizeof(board) + i);
        if (c == 0 || c == 0xFF) break;
        out[n++] = c;
    }
    return n;
}

int fwGlobals(uint8_t *out)
{
    out[0] = configSession;
    out[1] = serialIsIO;
    out[2] = configMode;
    return 3;
}
