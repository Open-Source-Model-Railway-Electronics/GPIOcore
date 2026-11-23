# Review IOcore: firmware, Python GUI and new Qt GUI

Date: 2026-11-04 · Scope: the whole `IOcore/` folder (firmware, `GUI_Python/`, `GUI/untitled/`)

## Summary

- **Firmware**: the basis is good (state machines, EEPROM model, compact protocol), but there are a few
  bugs that hurt immediately on real hardware:
  - pulse outputs never switch off (the coil stays energised, **F9**);
  - the defaults and presets put outputs on D0/D1 (Serial) and D2 (DCC input, **F7**);
  - every DCC command sets `servoMin` to 0 (**F8**);
  - the output opcode announces 13 bytes, but the payload is 14 bytes (**F1**).
- **Python GUI**: does not work with the current firmware. It crashes on the first message, receives nothing
  (the receive loop is called only once) and uses an old payload layout. See **P1–P9**.
- **Qt GUI**: new project `software/gui/`. It works with the current firmware (including the
  protocol quirks below) and also with a fixed firmware (0x2E). There are 19 automatic
  tests, which run against a firmware simulator. **Not yet tested on real hardware**.

Severity: **H** = broken/dangerous on hardware, **M** = wrong behaviour or data corruption, **L** = tidiness/robustness.

## Status 2026-11-09: fixes applied

The firmware compiles for `arduino:avr:nano` and `arduino:avr:uno` (20066 bytes flash, 1438 bytes RAM).
Before the fixes this was 19554 / 1430. It has **not** been tested on hardware yet. A backup of the old
source files is outside the project.

| ID      | Status | What was done |
|---------|--------|------------------|
| F1      | ✅ | `OPCODE_SET_OUTPUT_DATA` = 0x2E (14 bytes) |
| F2      | ✅ | removed `sendAck()` from both SET handlers |
| F3      | ✅ | `if( index >= 16 ) return ;` in all 4 data handlers |
| F4      | ✅ | preset 4: `for( i=0; ...)` and `setupInputs(8)` added |
| F5      | ✅ (different from proposed) | broadcasts **not** removed, but aligned with the GET layout: pinC/pinD (= NA) added, switchTime little-endian. Presets now also reset pinC/pinD. |
| F6      | ✅ | `sendMyVersion()` sends `version` from `src/version.h` ("v0.0.0") |
| F7      | ✅ | defaults and presets start at D3 (16 outputs = D3..A4) |
| F8      | ✅ | `setVal()` only for PWM and 4-aspect signal |
| F9      | ✅ | `if( timer.update( 1 ) )` in the pulse branch |
| F10     | ✅ | edge detection via `oldPos` (see explanation below) |
| F11     | ✅ | single steady follows the state; `exitState` (pins low) only for pulse types |
| F12     | ✅ | `!= NA` guards on relayPinC (pulse) and pinB (exit, flashing) |
| F13     | ✅ | `pinMode` for relayPinC |
| F14     | ✅ | `return 0 ;` in `setVal()`/`setState()` |
| F15     | ✅ partly | division by 0 at servoSpeed 0 caught; start position 0° and `servoPattern` unchanged |
| F16     | ✅ | route default 0xFF; `triggerOutputs()` accepts only `outID < 16` |
| F17     | ✅ partly (11-10) | presets set unused inputs to `NOT_IN_USE`, instead of latching with output 15 |
| F18     | — | unchanged (DCC lockout remains a TODO) |
| F19–F21 | ✅ | include capitalisation, `uin8t` typo, `#pragma once` |
| F22     | — | `SET/GET_*_STATE` still empty |
| **F23** | ✅ new | see below |

**F23 (H, newly found while fixing): `TIMER_ON` works only once.** `Timer::update()`
(`src/ST.cpp`) only restarts after `IN` has been 0 once. `turnOnRelays`, `turnOffRelays` and the
pulse branch only call `update( 1 )`, so from the second time on `Q` never comes and the state
machine hangs. Fix: `timer.update( 0 ) ;` in the entry of those three states.

**Behaviour after F10, good to know:**
- An output (servo, steady, pulse, relay) only does something on a **state change**.
- Writing through `SET_OUTPUT_DATA` (GUI "Write" or "Test state") always executes the state once,
  even if it has not changed.
- After start-up every output goes to the stored state once. **Pulse coils do not**, so that
  16 coils do not fire at the same time at power-up.

**Existing boards:** the new pin defaults (F7) only apply after a factory reset (DEADBEEF missing
in EEPROM) or after a preset. A board that is already configured keeps its old pins. After
flashing, check in the GUI pin map whether anything is still on D0/D1/D2.

**Qt GUI:** Demo now imitates the fixed firmware. Old firmware is still recognised
(legacy message, plus a pulse warning only for old firmware). There are 19 tests (17 core +
2 GUI), all green. These include new tests for the fixed firmware (pins from D3,
route default, preset broadcast layout, preset 4).

## Update 2026-11-11 (2): route activation simplified, help and translations

- **Route activation no longer has a choice.** A button starts the route when pressed (falling edge), a
  DCC address on every command (direction 0 and 1). `ROUTE_FALLING` (6) has been removed from `config.h` and `input.cpp`.
  The firmware compiles (20162 bytes).
- **GUI step list:**
  - at least one row is always open; for an empty route you choose the first output there;
  - deleting is done with a trash button per row;
  - bug fix: row buttons were deleted while their own click was still running (`delete` → `deleteLater`).
- **Help:** 17 extended explanation texts for beginners in `software/gui/i18n/info/en/*.html`, plus
  *Help → Getting started*.
- **Translations:**
  - UI texts through Qt Linguist (`i18n/iocore_<language>.ts`; `iocore_nl.ts` created, 269 texts,
    not yet translated);
  - explanation texts as HTML per language, with fallback to English;
  - a *View → Language* menu.

  Explanation for translators: `software/gui/i18n/README.md`.
- There are 39 tests (31 core + 8 GUI), all green.

## Update 2026-11-11: protocol v2 (GUI done, decoder still to do)

See **[PROTOCOL.md](PROTOCOL.md)** for:
- **`HELLO`**: logging in with capabilities, plus a 5 s boot window after which D0/D1 become free;
- **routes**: 16 steps of 4 bytes (output, value, raw flag, 14-bit wait time in 0.1 s);
- **`SERVO_LIVE`**;
- the **EEPROM budget**: 16 × 16 steps do not fit in the internal 1 KB, so the decoder reports its
  limits in `HELLO`;
- the DCCnext pins.

The GUI and the simulator implement v2. With the current firmware the GUI falls back to v1: routes of
8 steps, state only, 0.5 s per step. The firmware itself has **not** been changed yet; that is listed
as a task list in §9 of PROTOCOL.md.

GUI changes:
- input settings slide in from the left, output settings from the right;
- (i) explanations everywhere;
- frog interlocking: electrofrog **or** unifrog;
- the servo "pattern" (unused) and the start aspect of the signal are gone;
- live servo;
- at most one recorder;
- DCCnext profile.

There are 38 tests (31 core + 7 GUI), all green.

## Update 2026-11-10: input behaviour and new configuration view

**F24 (H, input behaviour), applied in `input.cpp` and `config.h`:**
- **Pull-up.** Input pins never got a `pinMode` and so were floating. They are now set to `INPUT_PULLUP`,
  in `loadEEPROM()` and `set()`. A button or contact goes to GND; pressing = falling edge.
- **Toggle.** `MOMENTARY` reacted to `toggled()`, so on **both** edges: it switched twice per press,
  which is net the same as absolute. Toggle now switches only on the falling edge
  (= pressing).
- **Toggle through DCC.** `setDccAddress()` first did `state = direction` and then `^= 1`. That is not a
  toggle but "reversed direction". The line `state = direction` is gone; `LATCHING` sets the
  direction itself.
- **Turnout route edge.** A route started on every edge. Now you choose per input:
  - `ROUTE` (2) = rising edge, or DCC direction 1;
  - new `ROUTE_FALLING` (6) = falling edge, or DCC direction 0.

  The 3-bit type field has room for that.
- **Callback only for direct inputs.** `callbackInput()` is now only called for
  `MOMENTARY`/`LATCHING`, also for DCC. Before, a recorder or feedback input with a
  matching DCC address could switch an output by accident.
- The firmware compiles (20200 bytes flash). It has not been tested on hardware yet.

**Note for *absolute*:** the level is read raw, so a pressed/closed contact = state 0.
If that has to become 1, `trigger.update( !digitalRead( eedata.pin ) )` is the minimal change. With that
the edges of toggle and route do flip as well.

**Qt GUI:** the *Outputs* and *Inputs* tabs have been replaced by one **Configuration** view. On the left
are the inputs, on the right the outputs, the wires in between, and the settings are in a sidebar on the right.
- You link by dragging an output onto an input.
- Every linked input has a unique colour, and an output takes over that colour.
- Direct control (toggle/absolute) and route (rising/falling, at most 8 steps with state) exclude
  each other.

There are 25 tests (21 core + 4 GUI, including real drag tests), all green.

---

## 1. Firmware: protocol (`messageParser.*`, `src/Transceiver.*`)

### F1 (H): `OPCODE_SET_OUTPUT_DATA` announces 13 bytes, the payload is 14

`messageParser.h:10`. The low nibble of the opcode is the length. The payload is
`index + pinA..pinD + relayA..C + type + state + data[4]` = **14** bytes, but 0x2D says 13.

Consequences:
- with GET, `data[3]` does not come along (servoPattern, high byte of pauseTime/switchTime);
- with SET, the firmware reads `payload[13]` anyway, and that byte then comes from the buffer of the **previous**
  message. A swing light can therefore get a pause of tens of seconds.

```cpp
// was
const int OPCODE_SET_OUTPUT_DATA   = 0x2D ;
// becomes
const int OPCODE_SET_OUTPUT_DATA   = 0x2E ;   // 14 payload bytes
```

The Qt GUI automatically recognises which variant the firmware uses: the reply to `GET_OUTPUT_DATA` is
0x2D or 0x2E. In legacy mode the GUI shows a warning.

### F2 (M): double ACK after every SET

`messageParser.cpp:102` and `:153` send `transceiver.sendAck()`. After that `Transceiver::receive()`
(`src/Transceiver.cpp:116`) sends yet another ACK. A host that expects "one ACK per request" gets out of
sync. The Qt GUI handles this with a short drain period after every transaction.

```cpp
// was (both SET handlers)
    transceiver.sendAck() ;
// becomes
    // ACK is sent by Transceiver::receive()
```

### F3 (H): no bounds check on `index`

`messageParser.cpp:82, 107, 131, 160`: `input[index]` and `output[index]` are used without checking
whether `index < 16`. One wrong byte on the serial line then causes a write outside the array
(RAM corruption).

```cpp
// add after each 'uint8_t index = msg->payload[0] ;'
    if( index >= 16 ) return ;
```

### F4 (H): preset 4 uses an uninitialised `i`

`messageParser.cpp:325`: `for( int i=0; ...)` declares a new `i` that hides the outer `uint8_t i`.
As a result `clearUnusedOutputs( i )` (`:436`) gets a random value. `setupInputs(8)` is also missing in
this case.

```cpp
// was
        for( int i=0; i<8; i++ )
// becomes
        for( i=0; i<8; i++ )
```

And after the loop, before `break ;`: `setupInputs(8) ;`

### F5 (M): preset broadcasts use the old layout, and pinC/pinD are not reset

The presets (`messageParser.cpp:218–433`) and `clearUnusedOutputs()` send 12 bytes in the **old** order:
`pinA pinB relayA relayB relayC type ...`, so without pinC/pinD. That does not match the layout that
`GET_OUTPUT_DATA` uses. In addition they leave `pinC`/`pinD` at whatever was there before.

Proposal: in the presets also set `pinC`/`pinD` to `0xFF`, and remove the broadcasts. A host reads
everything again after the ACK; the Qt GUI already does that and ignores the broadcasts.

### F6 (L): version is a placeholder

`messageParser.cpp:448`: `"12345678"` is sent, while `src/version.h` contains `"v0.0.0"` (and
is unused). Proposal: send `version`.

### Protocol notes for host implementations

These points are not bugs, but they are pitfalls for every host implementation. The Python GUI fell into
all of them.

- `ACK` (0x70) is 1 byte and `NACK` (0x71 + reason) is 2 bytes, both **without** checksum.
- A host→device message with length 0 (e.g. `REQ_VERSION` 0x50) is executed immediately. The host must
  **not** send a checksum after it, otherwise that byte counts as a new command.
- `REQ_VERSION` gets no ACK, only the version message (0x5n).
- Every GET first gets the data reply and then another ACK.

---

## 2. Firmware: outputs (`output.*`, `IOcore.ino`)

### F7 (H): defaults and presets start at pin 0 (D0/D1 = Serial, D2 = DCC input)

- `output.cpp:49` sets `eedata.pinA = index ++` → output 0..15 = D0..A1.
- `messageParser.cpp:213` sets `uint8_t pinIndex = 0;`.
- After a factory reset `loadEEPROM()` therefore does `pinMode( 2, OUTPUT )` on the **DCC input**, and then
  no DCC comes in any more. Output 0/1 do nothing (the UART has priority).

D3..A4 are exactly 16 free pins (presets 2 and 6 use 15/16 of them):

```cpp
// output.cpp:49  was
    eedata.pinA = index ++  ;
// becomes
    eedata.pinA = 3 + index ++  ;   // skip D0/D1 (Serial) and D2 (DCC)

// messageParser.cpp:213  was
    uint8_t pinIndex = 0;
// becomes
    uint8_t pinIndex = 3;
```

The Qt GUI marks D0/D1/D2 as reserved, colours conflicts red (table + pin map) and gives a
warning in the editor.

### F8 (H): every DCC/button command overwrites `data[0]`

`IOcore.ino:121`: `callbackInput()` always calls `output[index].setVal( val )`. For normal
turnout commands `val` is 0. `pwmVal` shares a union with `servoMin`, the low byte of `switchTime` and
`swing.pulseTime`. So every operation sets those to 0 and writes that to EEPROM.

```cpp
// was
    output[index].setVal(   val   ) ;
// becomes
    if( output[index].eedata.type == PWM || output[index].eedata.type == fourToneSignal ) output[index].setVal( val ) ;
```

### F9 (H): pulse outputs never switch off

`output.cpp:217`: in `moveActuator`, for `singlePulse`/`doublePulse` only `timer.Q` is read.
`timer.update()` is never called, so `Q` stays 0. The end of the pulse never comes, and a
turnout coil stays energised (risk of a burnt coil).

```cpp
// was
            if( timer.Q ) // pulse expired, -> exit
// becomes
            if( timer.update( 1 ) ) // pulse expired, -> exit
```

### F10 (H): the output state machine runs continuously, even without a state change

`output.cpp:476–478`: `idle` goes straight on to `turnOffRelays`/`moveActuator` on every `update()`.
Consequences:
- the frog relays switch off and on again every cycle of 100 ms (chatter);
- pulse outputs pulse again endlessly (after fix F9);
- servos are attached again and again.

Edge detection is needed. The existing, unused member `oldPos` (`output.h:127`) can be used for it:

```cpp
// output.cpp:476  was
            else if( eedata.relayPinA != NA
                 &&  eedata.relayPinB != NA )       { sm.nextState( turnOffRelays      ) ; }
            else {                                    sm.nextState( moveActuator        ) ; } }
// becomes
            else if( eedata.state == oldPos )       { }                                 // nothing to do
            else if( eedata.relayPinA != NA
                 &&  eedata.relayPinB != NA )       { oldPos = eedata.state ; sm.nextState( turnOffRelays ) ; }
            else {                                    oldPos = eedata.state ; sm.nextState( moveActuator  ) ; } }
```

Note: `oldPos` starts at 0. An output that has state 1 in EEPROM therefore moves at start-up; an
output with state 0 does not. Choose deliberately what you want at boot.

### F11 (M): single steady does not follow the state

`output.cpp:193` always writes `HIGH`. After that the `exitState` (`:232–236`) sets pinA/pinB `LOW` again,
so a steady output only flashes briefly.

```cpp
// output.cpp:193  was
            digitalWrite( eedata.pinA, HIGH ) ;
// becomes
            digitalWrite( eedata.pinA, eedata.state ) ;
```

And use the `exitState` only for the pulse types (`singlePulse`, `doublePulse`).

### F12 (M): `digitalWrite()` on unassigned pins (NA = 63)

`output.cpp:219` (relayPinC), `:235` (pinB for single types) and `:299/:304` (pinB for singleFlashing)
write without a `!= NA` check. On AVR, `digitalWrite(63)` reads outside the PROGMEM pin tables and sets
random port bits. Guard them as on `:333`:

```cpp
// was
        digitalWrite( eedata.pinB, LOW ) ;
// becomes
        if( eedata.pinB != NA ) digitalWrite( eedata.pinB, LOW ) ;
```

### F13 (M): unifrog relay gets no `pinMode`

`output.cpp:73`: `loadEEPROM()` sets relayPinA/B as OUTPUT, but not relayPinC. As a result
`digitalWrite` only switches the pull-up.

```cpp
// add after output.cpp:73
    if( eedata.relayPinC != NA ) pinMode( eedata.relayPinC, OUTPUT ) ;
```

### F14 (M): `setVal()`/`setState()` have no return

`output.cpp:21` and `:38` are `uint8_t` functions without a `return`; that is undefined behaviour. Minimal
fix: `return 0 ;`. Caveat: every state change writes EEPROM (one byte per toggle). With ~100k
cycles, that is something to keep an eye on for heavily used turnouts.

### F15 (L): servo details

- `output.cpp:95`: `500 / timer.PT` divides by 0 at `servoSpeed = 0`. The GUI enforces ≥ 1.
- `pos` starts at 0, so the first movement after boot swings from 0°.
- `servoPattern` is stored, but not used anywhere.

---

## 3. Firmware: inputs (`input.*`)

### F16 (M): default route steps point to output 63

`input.cpp:43` fills `linkedOutput[]` with `NA` (0x3F), but the end marker in `triggerOutputs()` is 0x7F
(`:139`). If an input is set to ROUTE without filling in the steps, then
`callbackInput( 63, ...)` follows, which is `output[63]` outside the array.

```cpp
// input.cpp:43  was
        eedata.linkedOutput[i] = NA ;
// becomes
        eedata.linkedOutput[i] = 0xFF ;   // 0x7F in bits 6-0 = end of route

// input.cpp:139  was
        if( outID != 0x7F )
// becomes
        if( outID < 16 )
```

The Qt GUI shows such steps as "⚠ output 63 (invalid)" and warns.

### F17 (L): `outputID` is 4 bits

`input.h:11`: "no output" (0xFF in `setupInputs`) becomes 15. That is harmless as long as address and pin are also
empty, but it is good to know.

### F18 (L): known TODO: repeated DCC packets

A DCC command station repeats accessory packets. With MOMENTARY the output therefore toggles several times
(already a TODO in `IOcore.ino:161`).

---

## 4. Firmware: other

| ID  | Severity | Location                 | Finding                                                                                                          |
|-----|-------|--------------------------|------------------------------------------------------------------------------------------------------------------|
| F19 | M     | `servoManager.cpp:1`     | `#include "ServoManager.h"`, but the file is called `servoManager.h`. On Linux/macOS the build fails (case sensitive). |
| F20 | L     | `src/macros.h:36`        | Typo `uin8t` in `toggleState` (macro unused).                                                                    |
| F21 | L     | `messageParser.h`        | No `#pragma once`.                                                                                               |
| F22 | L     | `messageParser.cpp:187`  | `SET/GET_*_STATE` are empty. The GUI therefore tests outputs through `SET_OUTPUT_DATA`.                          |
| —   | OK    | EEPROM                   | Outputs (~12 B) + inputs (~13 B) × 16 ≈ 400 B < 420 (recorder start); DEADBEEF at 1000. Fits, but tight.        |

---

## 5. Python GUI (`GUI_Python/`)

| ID | Location                  | Finding                                                                                                                             |
|----|---------------------------|-------------------------------------------------------------------------------------------------------------------------------------|
| P1 | `main.py:39`              | `Transceiver.OPCODE_SET_INPUT_DATA` etc. do not exist in `Transceiver.py` → `AttributeError` at the first message.                  |
| P2 | `Transceiver.py:60`       | `time.ticks_ms()` / `ticks_diff()` are MicroPython; in CPython this gives an `AttributeError`.                                      |
| P3 | `main.py:254, 268`        | `periodic_receive()` is called once and never scheduled again → nothing is ever received.                                           |
| P4 | `main.py:120–129`, `parser.py:44–58` | Old output layout (no pinC/pinD). Type list differs (Servo = 6, firmware = 11); type is always sent as 0, data always 0. |
| P5 | `parser.py:112`           | `switchTime` is read big-endian; the firmware (AVR) is little-endian.                                                               |
| P6 | `parser.py:3–6`, `main.py:104` | Only 3 of the 6 input types.                                                                                                   |
| P7 | `main.py:55–65`, `Transceiver.py:112` | NACK branch: `(str)` concatenation gives a `TypeError`, `err_msg` may be undefined. The host also sends an ACK itself for every received frame. |
| P8 | `status_icons.py:112`, `main.py:264` | `update_icons()` reschedules itself on every call and is also called every 500 ms → the number of timers grows without limit, plus a flood of debug prints. |
| P9 | `parser.py:85–116`        | Dynamic servo/PWM fields are recreated but never stored → these values can never be sent.                                           |

Conclusion: the Python GUI is not usable with the current firmware. The Qt GUI replaces it functionally.

---

## 6. New Qt GUI (`software/gui/`)

**Features**
- Serial connection (115200 8N1). Recognises Arduino/CH340/FTDI/CP210x ports automatically and waits for the
  boot message, with `REQ_VERSION` as fallback.
- **Outputs**: all 14 types. The editor adapts per type: the right pins, frog relays,
  parameters with units (e.g. "× 10 ms = 500 ms"), servo sliders, a 4-aspect signal per LED and a
  PWM slider. Buttons "Test state 0/1".
- **Inputs**: all 6 types, DCC address and/or pin, target output and a route editor (8 steps + state).
- **Pin map**: D0..A5 with who uses which pin. Conflicts and reserved pins are red, PWM pins
  are marked. Click on a card to open that output/input.
- **Tracking changes**: changed rows are bold with a "●". "Write N change(s)" writes only
  what has changed and then reads it back for verification. Revert, and Open/Save as JSON (offline
  configuration).
- **Presets** 0–7 with confirmation; afterwards the whole board is read back cleanly.
- **Serial log** with TX/RX hex and raw payload per editor (handy for firmware debugging).
- **Demo mode**: a built-in firmware simulator that imitates the behaviour of the current firmware
  (including F1, F2, F5 and the bitfield truncation).

**Building**: open `software/gui/CMakeLists.txt` in Qt Creator (kit Qt 6.10.1 MinGW). Tested with
Qt 6.10.1 / MinGW 13.1. The code uses no Qt6-only APIs and `find_package` falls back to Qt5, but
I could **not** test a Qt5 build (only Qt 6 is on this machine).

**Tests** (`ctest`): 17 unit tests (framing, model, link ↔ simulator: legacy/fixed, double ACK, NACK retry,
preset) and 2 GUI tests (edit → write → read back → clean; preset + conflict marking). All
green.

**Not tested**: real hardware. First test advice: try Demo, then connect to a board, "Read
board" and look at the serial log. Use the pulse types (F9) and relays (F10) only after the firmware fixes.

`GUI/untitled/` is the empty Qt Creator template; I left it in place and it can go.

## Update 2026-11-12 / 13: firmware v2 tested on the board

Tested on a real Nano ("Test decoder") with a script over USB and with the GUI. Found and fixed:

| Problem | Severity | Fix |
|---|---|---|
| **Outputs never left `idle`**: `idleF()` returned 0 and the `State()` macro only executes the state when ≠ 0. No pin ever switched (already so in the original firmware) | H | `idleF()` returns 1 |
| Signal: flashing used `timer.Q` (high for one call per 500 ms), the lamp lit for µs | M | own phase bit |
| Coil turnouts: frog relay not set after start-up (electrofrog dead, unifrog possibly wrong polarity) | H | relays in `loadEEPROM()` |
| F15 servo started at 0° | M | starts at the stored end position; servos one by one (500 ms) |
| F18 repeated DCC packets flip a toggle | M | 500 ms lockout time per input (toggle and route) |
| New output pin only an output after reset | M | `pinMode` in `Output::set()`, old pins released |
| GUI sent a route again endlessly (timing off → wait time 0 read back) | M | compare without wait time + never send the same twice |
| GUI timeout when writing a route (EEPROM shifting takes seconds) | L | 4 s timeout |

New: OUTPUT_EVENT (0x45) with servo degrees and frog relays, SET_INPUT_STATE (0x32), DCC_EXT (0x33),
routes on *straight only* / *curved only*, config button A7 + LED D13 + teaching through loc 9999, presets moved to the GUI,
board silent without GUI. Flash 89 %, RAM 80 %.
