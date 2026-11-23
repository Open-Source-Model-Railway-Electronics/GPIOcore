# IOcore serial protocol

Date: 2026-11-18 (updated) · Status: pre-release (v0). This document describes the current protocol, as implemented by
the firmware (`software/IOcore`), the Qt GUI and the simulator (`software/gui/src/simulateddevice.cpp`).

After opening the port the GUI sends `HELLO`. A board that answers is an IOcore decoder and reports its
capabilities. If a board does not answer at all, the GUI first waits up to 30 s for the board to announce itself
(a short press on its config button, §3.1). If nothing comes, the GUI asks you to flash the IOcore firmware.

## 1. Framing

```text
<command> <payload ...> <checksum>
command  : high nibble = message group, low nibble = payload length (0..14)
           low nibble 0xF = extended length: payload[0] = length incl. itself (max 32)
checksum : XOR of command and all payload bytes
```

| Exception | Bytes | Remark |
|---|---|---|
| `ACK` | `70` | 1 byte, no checksum |
| `NACK` | `71 <reason>` | 2 bytes, no checksum (1 = checksum, 2 = length, 3 = timeout) |
| Host → device with length 0 | `<cmd>` | Executed immediately and gets **no** checksum and no ACK (`REQ_VERSION`, `HELLO`) |

The protocol is binary with full 8-bit bytes. So there is no 7-bit limit as in LocoNet. The
"bit 7 = state, bits 0–6 = id" layout is not used anywhere.

Every complete frame with a valid checksum gets one `ACK` from `Transceiver::receive()` after it has been processed.
A GET first gets the data reply and then that `ACK`.

**Timing and limits** (`src/Transceiver.cpp`):
- **Byte timeout.** At most **5 ms** may pass between two bytes of one frame. After that the
  firmware answers with `NACK 3` and starts over. So send a frame in one go.
- **Maximum length.** A payload is at most **32 bytes**; longer gives `NACK 2`.
- **Start-up.** After a reset the firmware sends **nothing**: without the GUI the board is silent. After about
  2.5 s the GUI asks for `REQ_VERSION` and then reports in with `HELLO` (within the serial window of 5 s, §3.1).
  If no `HELLO` arrives, the firmware stops Serial and D0/D1 become normal pins. Opening the port resets an
  Uno or Nano through DTR. The bootloader and `setup()` together take about 1–2 s.
- **Index ≥ 16.** An `index` of 16 or higher is ignored, but the frame still gets an `ACK`. A GET
  then gives no data reply.
- **Saving.** `SET_INPUT_DATA` and `SET_OUTPUT_DATA` go straight to EEPROM. A `SET_OUTPUT_DATA` executes the
  state once, even if it has not changed (this is how "Test state" works).

## 2. Opcodes

| Opcode | Direction | Payload |
|---|---|---|
| `1E` SET_INPUT_DATA | ↔ | `index addrH addrL pin type outputID b6 b7..b13` (14 B), see §2.2 |
| `11` GET_INPUT_DATA | → | `index` → reply `1E` |
| `2E` SET_OUTPUT_DATA | ↔ | `index pinA pinB pinC pinD relayA relayB relayC type state data[4]` (14 B), see §2.1 |
| `21` GET_OUTPUT_DATA | → | `index` → reply `2E` |
| `50` REQ_VERSION | → | – → reply `5n` + version text (`src/version.h`, now `v0.0.0`) |
| `32` SET_INPUT_STATE | → | `index direction` — acts as if a DCC command for the address of this input arrives (toggle, absolute, route). For testing and simulation |
| `31` GET_INPUT_STATE | → | `index` — **reserved**, only `ACK` |
| `42` SET_OUTPUT_STATE | → | `index state` — **reserved**, only `ACK` |
| `33` DCC_EXT | → | `addrH addrL value` — acts as if a DCC extended accessory packet (signal aspect) for that address arrives. Only direct inputs respond |
| `41` GET_OUTPUT_STATE | → | `index` → reply `45` with the current state. Signals (type 10 and 15) answer **state 2 + the current aspect**, so the GUI shows them right after connecting; a servo answers its state + the current angle |
| **`45` OUTPUT_EVENT** | ← | `output state value ms_lo ms_hi` — unsolicited, only after HELLO: every time an output switches (state 0/1, or 2 = raw value in `value`), with the lower 16 bits of `millis()`. While turning, a servo reports **every degree** (state 2, value = angle); an intermediate step is dropped if the serial buffer is full, the end position never is. A 4-aspect signal and a block signal report their aspect as state 2. **State 3** = frog relay switched, `value` bit0 relay A, bit1 relay B (electrofrog), bit2 relay C (unifrog); `GET_OUTPUT_STATE` sends this as a second message if the output has relays |
| `B1` CLEAR_OUTPUT | – | is in `messageParser.h`, but is not used |
| **`90` HELLO** | → | – → reply `95` |
| **`82` GET_ROUTE** | → | `route first` → reply `8F` |
| **`8F` SET_ROUTE** | ↔ | `len route total first step[n]` (n ≤ 7) |
| **`A2` SERVO_LIVE** | → | `output angle` |
| **`C0` GET_BOARD** | → | – → reply `CF` |
| **`CF` SET_BOARD** | ↔ | `len type flags delay name[0..16]`, see §9 |
| **`D1` GET_ANALOG** | → | `channel` (0–7 = A0–A7) → reply `D3`, then the normal `ACK`. Live measurement for setting up analog inputs; the GUI asks for it about every 250 ms while an analog input is on screen. HELLO bit4 |
| **`D3` ANALOG_VALUE** | ← | `channel valueH valueL` — `analogRead()` of A0–A7, 0–1023. HELLO bit4 |
| **`E0` BT_SETUP** | → | – (no checksum, no ACK) → after ~0.3–2 s reply `E3`. Sets up an HC-05 in AT mode on D0/D1, see §11. HELLO bit5 |
| **`E3` BT_RESULT** | ← | `status baud name_ok` — status 0 = done, 1 = no HC-05 answered, 2 = `AT+UART` refused; baud = the rate the HC-05 answered at ÷ 2400 (0 = none); name_ok 1 = `AT+NAME` answered OK. HELLO bit5 |


### 2.1 Output fields (`Settings` in `output.h`, 10 bytes in EEPROM)

| Field | Bits | Values |
|---|---|---|
| `pinA`–`pinD`, `relayA`–`relayC` | 6 | 0–13 = D0–D13, 14–19 = A0–A5, **0x3F = not used** (0xFF becomes 0x3F) |
| `type` | 4 | see the table below |
| `state` | 1 | 0 = curved / off, 1 = straight / on |
| `invertFrog` | 1 | 1 = relay pins A–C are driven inverted (relay modules that switch on LOW); the reported relay state (`OUTPUT_EVENT` state 3) stays logical |
| `data[4]` | 32 | union; the meaning depends on the type |

Frog relays: `relayA` + `relayB` = **electrofrog** (A at straight, B at curved, both off first);
`relayC` = **unifrog**. The GUI allows only one of the two.

In `SET_OUTPUT_DATA` / `GET_OUTPUT_DATA` the `state` byte carries `state` in **bit0** and `invertFrog` in **bit1**
(bits 2–7 are 0). Older firmware ignores bit1 and reads it back as 0.

| `type` | Name | Pins | `data[4]` |
|---|---|---|---|
| 0 | single steady | A | – |
| 1 | single pulse | A (+ unifrog C) | `uint32` LE switching time × 100 ms |
| 2 | single flashing | A | `uint32` LE half period × 10 ms |
| 3 | double steady | A, B | – |
| 4 | double pulse | A, B | `uint32` LE pulse time × 10 ms |
| 5 | double flashing | A, B | `uint32` LE half period × 10 ms |
| 6 | light effect | per effect | `data[3]` = effect: 0 swing light (A, B), 1 fireplace (A, B, PWM), 2 TL tube (A, PWM), 3 welder (A), 4 photographer (A). Swing: on time = `data[0] \| (data[1] & 0x0F) << 8`, pause = `data[1] >> 4 \| data[2] << 4`, 12 bits each × 10 ms; other effects: `data[0..2]` unused (0) |
| 7 | board link (TX) | A = D1 (fixed) | `data[0]` = own address (0 = none), `data[1]` bit0 = extended; see §12 |
| 8–9 | free | – | not used (EEPROM layout 1 had fireplace, TL tube and welder here) |
| 10 | 4-aspect signal | A–D | `data[0]` = aspect: bits 0–3 on, bits 4–7 flashing (set by DCC-ext or a route; the GUI leaves it as it is), `data[1]` = flashing frequency in 0.1 Hz, `data[2]`/`data[3]` unused (0) |
| 11 | servo | A (+ relays) | `data[0]` angle at state 0, `data[1]` angle at state 1, `data[2]` speed in ms/° (≥ 1), `data[3]` = 0, or semaphore arm (below) |
| 12 | PWM | A (PWM) | `data[0]` = brightness 0–255, `data[1]` = fade time in s for a full change 0→255 (0 = immediate, max. 255 s). The output takes one step towards the new brightness every `data[1] × 1000 / 255` ms (own values, route step, DCC-ext); at start-up it fades up from 0. `OUTPUT_EVENT` reports the target value immediately |
| 13 | not in use | – | – |
| 14 | sound effect | A = always **1 (D1, TX)** | – (no tracks of its own). The input chooses the track: own values (§4), a route step (raw) or DCC-ext plays its value as the track number (0 = stop). Without own values: state 1 = track 1, state 0 = stop |
| 15 | block signal | A (B–D optional) | `data[0]` = aspect when its own block is occupied, `data[1]` = aspect when the next block is occupied, `data[2]` = aspect when both are free (aspect = same format as type 10), `data[3]` = own block input (bits 0–3) + next block input (bits 4–7); next = own: no next block (below) |

**Block signal (type 15).** The firmware chooses the aspect itself from two inputs, normally sensors (input
type 4), every 50 ms (`blockSignals()` in `IOcore.ino`): `data[0]` if the own block input is occupied,
otherwise `data[1]` if there is a next block and its input is occupied, otherwise `data[2]`. "Occupied" =
the input is in use, has a pin and is pressed (LOW, or the analog condition holds); one input may serve
several signals. The current aspect is only kept in RAM (no EEPROM writes) and reported as `OUTPUT_EVENT`
state 2 when it changes; `GET_OUTPUT_STATE` answers state 2 + the current aspect. Flashing lamps flash at a
fixed 1 Hz. Commands to it are ignored: inputs (toggle / absolute, own values), route steps and DCC-ext.

| Byte | Bits | Meaning |
|---|---|---|
| `data[3]` | 0–3 | input index (0–15) of the own block detector |
| `data[3]` | 4–7 | input index of the next block detector; the same as the own block = no next block |

Example: own block = input 3, next block = input 9, occupied = lamp C (`0x04`), next occupied = lamp B
(`0x02`), free = lamp A (`0x01`): `data` = `04 02 01 93`. Without a next block (own = 5): `data[3]` = `0x55`.

Until 2026-11-16 this was a mode of the 4-aspect signal (type 10, `data[3]` bit7, with lamp numbers in
`data[2]`/`data[3]`). That mode is removed: type 10 is a manual signal again, and the GUI loads such a signal
from an old board file as a manual signal (`data[2]` = `data[3]` = 0).

**Servo as semaphore arm (type 11, `data[3]` bit7).** The servo follows input `data[3] & 0x0F`: occupied →
state 0 (curved = arm at stop), free → state 1 (straight = clear); bit6 swaps them. It moves at its own speed
through the normal state change (and reports it like any state change); checked every 50 ms.

| `data[3]` bits | Meaning |
|---|---|
| 7 | 1 = follows a detector input |
| 6 | 1 = swapped: occupied = state 1 (straight) |
| 0–3 | input index (0–15) of the detector |


**Sound effect (type 14, HELLO bit4).** A DFPlayer Mini has its RX connected to the TX pin D1 of the Nano
(through 1 kΩ). The firmware sends the DFPlayer frame (9600 baud, 10 bytes: `7E FF 06 12 00 00 track csH csL EF`:
play `/MP3/<track>.mp3` by name, e.g. `/MP3/0001.mp3`; `16` instead of `12` = stop) only if **no PC** is connected: in a configuration session D1 is the
USB line. Every time a track is started (or stopped), the output reports `OUTPUT_EVENT` with **state 2,
value = track**, even if it could not play anything. Several sound outputs may share D1 (the GUI does not count that as a
conflict); D1 together with another input or output is a conflict.

PWM pins are D3, D5, D6, D9, D10 and D11. D9/D10 do no PWM as long as a servo output exists
(Servo library, timer 1).

### 2.2 Input fields (`InputSettings` in `input.h`, 12 bytes in EEPROM)

| Field | Bits | Values |
|---|---|---|
| `address` | 12 | DCC accessory address 1–4094, **0xFFF = none** |
| `pin` | 6 | as for outputs; the firmware sets `INPUT_PULLUP` (button to GND). **Or an analog comparison**: `0x20 + 8 × mode + channel`, see below |
| `type` | 3 | see below |
| `outputID` | 4 | 0–15: the output (direct) |
| `b6` (byte 6 of `1E`) | 8 | **analog threshold / 4** for an analog `pin` code, otherwise 0xFF |
| `b7`–`b13` | 7 × 8 | unused (0xFF) |

Routes are not part of `SET_INPUT_DATA`; they only travel through `GET_ROUTE` / `SET_ROUTE` (§4). `GET_INPUT_DATA`
returns the same 14-byte layout.

**Analog inputs (HELLO bit4).** Instead of a pin number, `pin` then contains `0x20 | mode << 3 | channel`:
channel 0–7 = A0–A7 (the real pin is 14 + channel), mode 0 = **below** the threshold, 1 = **above** the threshold,
2 = **at** the threshold (one button of a resistor ladder). Mode 3 (so also `0x3F` = not used) is never
analog. The threshold (0–1016, steps of 8) is in `b6` as **threshold / 4**: `SET_INPUT_DATA` takes
it from there, `GET_INPUT_DATA` puts it back there (only for an analog input; otherwise `b6` is 0xFF). The firmware stores threshold / 8 (7 bits, `EEPROM.md`).

| Mode | Pressed when | Released again when |
|---|---|---|
| 0 below | value < threshold | value ≥ threshold + 20 |
| 1 above | value > threshold | value ≤ threshold − 20 |
| 2 at | value − threshold between −10 and +10 | outside that |

The firmware reads every 50 ms and counts a change only after two equal readings. "Pressed" works exactly
like a button to GND (LOW): toggle switches when pressed, absolute follows (pressed = 0), all three
route types start when pressed. The rules of the real pin apply to the channel
(A7 = config button, A6 only on the Nano) and an analog code as an
output gives `NACK 5`. Several analog inputs may share one channel (resistor ladder); a digital input or
an output on that pin is a conflict.

Input types:
- 0 toggle — switches when the button is pressed (falling edge) or on every DCC command;
- 1 absolute — follows the level (closed = 0) or the DCC direction;
- 2 route — starts when the button is pressed (falling edge) or on **every** DCC command for the address,
  direction 0 and 1;
- 3 not in use;
- 4 **sensor** — an occupancy detector or other sensor (digital pin or analog condition). It switches
  nothing itself and has no output, route or DCC address; it only provides its state (pressed = LOW =
  occupied) for block signals and servos used as semaphore arms (§2.1). Until 2026-11-16 type 4 was the recorder: the GUI loads
  a `"recorder"` input from an old file as 3; on a board an old recorder input now reads as a sensor (harmless,
  it switches nothing);
- 5 — reserved (was the recorder's feedback input, removed 2026-11-16); the firmware reads it as 3.

- 6 route **straight only** — starts when the button is pressed, or only on a DCC command
  'straight' (direction 1) for the address;
- 7 route **curved only** — same, only on 'curved' (direction 0).

With 6 and 7 two routes share one DCC address, for example a level crossing barrier: curved = closed, straight = open.

**Repeated DCC packets.** A command station sends a turnout command a few times. A toggle input and
a route take the first one and ignore the same address for 500 ms afterwards (`DCC_LOCKOUT_MS`). An absolute
input does not need this (same direction = same state). `SET_INPUT_STATE` (0x32) goes through the same
code, so the lockout time applies there too.

**Start-up (firmware 2026-11-18).** The first sample of an input's pin is its starting level, not an edge. An
**absolute** input applies that level once, exactly as an edge would (custom values included), but only when it
differs from its output's stored state: a switch moved while the power was off is followed, without needless
EEPROM writes. Toggle and route inputs do nothing at start-up. `SET_INPUT_DATA` starts an input again (the
next sample is a new starting level), so a new configuration gets the same rule. An input on D0/D1 starts when
those pins become I/O (`serialToIO()`).

### 2.3 Presets (in the GUI)

Since 2026-11-13 the presets are no longer in the firmware (that saved 2.7 KB of flash). The GUI calculates a
preset (`presetBoard()` in `gpioconfig.cpp`) and sends the result as normal `SET_OUTPUT_DATA` /
`SET_INPUT_DATA` (saved live). So this also works offline and in the simulation.

Pins start at D3 and skip **D13** (config LED): D3–D12, A0–A5. The inputs that are used become
absolute, DCC address 1, 2, …, input *n* → output *n*; the rest become *not in use*.

| Preset | Set-up |
|---|---|
| 0 | 16 servos |
| 1 | 8 servos + unifrog relay |
| 2 | 5 servos + electrofrog relay |
| 3 | 16 × single steady |
| 4 | 8 × double pulse |
| 5 | 5 × double pulse + unifrog relay |
| 6 | 4 × double pulse + electrofrog relay |
| 7 | 8 × double steady |

## 3. HELLO — logging in and configuration session

```text
host   -> 90
device -> 95 <protocol> <max steps per route> <step pool lo> <step pool hi> <flags>
protocol: protocol number, currently 2
flags  : bit0 = D0/D1 usable as I/O outside a config session
         bit1 = SERVO_LIVE supported
         bit2 = (no longer sent; was: recorder in external EEPROM)
         bit3 = BOARD settings (C0 / CF) supported, see section 9
         bit4 = analog inputs (GET_ANALOG D1 / D3) and the sound effect output (type 14)
         bit5 = Bluetooth: BT_SETUP (E0 / E3) and the announce after a short press on the
                config button (GUI mode, section 3.1)
step pool: number of route steps the board has room for with the current board settings
           (timing on/off), all routes together
```

The GUI sends `HELLO` right after the boot message (version). That is about 1.5 s after the reset that opening
the port causes through DTR. For boards without auto-reset it sends it after `REQ_VERSION`.

**What the decoder does (proposal, so that D0/D1 become usable):**
1. After power-up or reset it waits **5 s** with Serial active and does **nothing** yet on D0/D1.
2. If a `HELLO` arrives within those 5 s, there is a **configuration session**: D0/D1 stay Serial until the
   next reset. Outputs and inputs on D0/D1 do nothing in that session.
3. If no `HELLO` arrives and the configuration uses D0/D1, it does `Serial.end()` and sets D0/D1 as normal I/O
   (`pinMode` according to the configuration). Otherwise Serial stays open (§3.1).
4. If you want to configure again later, you reset the board. The GUI does this itself when connecting (DTR).
   Without a reset (over Bluetooth, or a USB chip without DTR): press the config button briefly (§3.1).

**Is using D0/D1 sensible at all?** It can be done, with three caveats:
- On an Uno/Nano, D0/D1 are connected to the USB-serial chip through 1 kΩ. An output simply works. An input with
  a button to GND works too, because the button wins over the 1 kΩ.
- A heavy load or a low-impedance circuit on **D0 (RX)** can disturb **uploading firmware**.
  Preferably use them for LEDs, or for relays through a transistor.
- They do not work during a configuration session. A "Test state" on an output on D0/D1 then does nothing.
- In the **simulator** (the firmware built for the PC, `IOCORE_HOST`, `serialPinsFree()` in `config.h`) D0/D1
  are always normal pins: the GUI does not talk to it over D0/D1.

The GUI lets you choose D0/D1 if the decoder reports bit0, and offline. It then shows a note (not an error).
Without bit0 they are reserved and red.

### 3.1 Serial window, GUI mode and the announce (HELLO bit5, firmware 2026-11-18)

The 5 s at start-up are the **serial window**: it runs from `serialWindowStart` (0 at start-up). Without
`HELLO` in the window the firmware does `Serial.end()` and D0/D1 become I/O (`serialToIO()`; a sound effect output
gets TX at 9600 baud for the DFPlayer) — **only when the configuration uses D0 or D1** (firmware 2026-11-18): an
input pin, an output or frog relay pin, or a sound effect output (TX). Otherwise Serial stays open at 115200
for good, so a GUI can connect at any time (USB without a reset, Bluetooth) without a press on the config button.
`serialCheck()` decides this every 50 ms after the window, outside a session: a configuration that starts using
D0/D1 later (e.g. in the simulator) closes the port then. A session keeps the port open until the next reset,
as before (the board does not notice the GUI going away); the next start-up decides again.

**GUI mode.** A **short press** on the config button (A7, Nano) **outside config mode**:
- serial port closed (`serialIsIO`): `Serial.begin(115200)` again, D0/D1 are Serial (what is on them pauses, as in a
  session), and a new 5 s window starts (`serialWindowStart = millis()`);
- serial port open, no session yet (start-up window): the window starts again;
- always: the board sends the **announce**, the `HELLO` reply (`95 ...`) unasked. It does **not** start a session.

The GUI then sends `REQ_VERSION` and `HELLO` as usual; with `HELLO` the session starts and the port stays open.
Without `HELLO` within 5 s, D0/D1 become I/O again (when the configuration uses them), exactly as after start-up.

**The GUI.** When it gets no answer to `REQ_VERSION` or `HELLO` while connecting, it does not give up: it shows
*"Waiting for the board — press its config button briefly (Nano)…"* and listens for up to 30 s
(`DeviceLink::announceWaitMs`) for the announce. Then it continues with `REQ_VERSION` and `HELLO`. After 30 s it
shows the "No IOcore board answered" message. Disconnect cancels the wait. This is what you need over Bluetooth:
opening a Bluetooth COM port does not reset the board, so after 5 s its serial port is closed (when the
configuration uses D0/D1).

## 4. Routes

### Step = 5 bytes (firmware 2026-11-20, EEPROM layout 3)

```text
byte 0 : bits 0-3 output id (0-15) | bit 4 raw value | bit 5 STEP_VALUES (custom value) | bits 6-7 reserved (0)
byte 1 : value   raw = 0: state 0 = curved, 1 = straight
                 raw = 1: aspect 0-255 (signal), angle 10-170 (servo), PWM 0-255, track 0-255 (sound, 0 = stop)
byte 2 : wait time bits 0-7
byte 3 : bits 0-5 = wait time bits 8-13 | bits 6-7 reserved (0)
byte 4 : extra   board link output: the target address 1-255 (0 = the output's own address); otherwise 0
wait   : 14 bits of 0.1 s = 0 .. 1638.3 s (~27 min), time after this step before the next one
```

Example: output 13, aspect 200, wait time 1638.3 s, extra 42 → `1D C8 FF 3F 2A`.

A **board link** step (§12): raw = 0 sends a basic frame (curved / straight) to the address in byte 4,
raw = 1 an extended frame with that value.

- **The GUI chooses the value kind per output type.** Turnouts, lights and relays get curved/straight.
  A 4-aspect signal gets an aspect number. A servo gets curved/straight **or** an angle, and may
  occur several times in one route (intermediate positions). PWM gets a value.
- **New steps** get a 0.3 s wait time.
- **Timing off** (board setting, §9): on the wire a step stays 5 bytes, but the decoder stores
  only byte 0, 1 and 4 (3 B per step) and after each step waits the fixed `delay` from the board settings.
  When read back, byte 2 and 3 are 0. The GUI hides the wait time column. Timing on: 5 B per step.
- **The decoder executes a step like this:**

```cpp
// raw == 0 -> like a DCC command:   callbackInput( id, value, 0 )
// raw == 1 -> signal / PWM:         output[id].setVal( value )
//             servo:                output[id].setPosition( value )   // new, moves to this angle
// then wait 'delay' x 100 ms before the next step
```

### Own values of a toggle or absolute input (firmware 2026-11-14)

A direct input (toggle or absolute) may have exactly **2 steps** in **its own route slot** (same index, `GET/SET_ROUTE`),
both with **bit 5 = `STEP_VALUES` (0x20)** in byte 0. Step 0 applies when the state of the input becomes 0 (off / curved),
step 1 when it becomes 1 (on / straight). Each step works like a route step: `raw = 0` sets curved / straight,
`raw = 1` sends an angle, aspect, PWM value or track. The output id of the step is used (the GUI puts the `outputID`
of the input there); the wait time does not count.

- This applies to the button, the analog condition and normal DCC accessory commands on the address of the input.
  DCC-extended still passes its value straight through.
- Without those 2 marked steps (another number, or steps without 0x20),
  the input works as always: the state goes to the output.
- For a direct input without own values the GUI sends an **empty route**, so that the slot is empty. The 2
  steps count towards the route pool. In the board file they are stored as `"values": [ {value, raw}, {value, raw} ]`.

### Transfer in pieces of at most 5 steps

```text
GET_ROUTE  82 <route> <first>
SET_ROUTE  8F <len = 4 + 5n> <route> <total> <first> <step ...>     (also the reply to GET_ROUTE)
```

- A route of 16 steps goes in three messages: 7 + 7 + 2. An empty route is one message with
  `total = 0`.
- With `first = 0` the decoder starts the route again. It saves the route as soon as `total` steps
  have arrived.
- Per input the GUI writes: `SET_INPUT_DATA`, then `SET_ROUTE`(s), and then reads back the input and route.

## 5. SERVO_LIVE

`A2 <output> <angle>` moves the servo to `angle` immediately. **Nothing** is saved and the
state machine does not change.
- The GUI sends this while you drag a servo slider, if the *Live* checkbox is on. A fast
  series of slider values is merged into only the last one.
- **Proposal for the decoder:** attach the servo, write the angle, and detach after about 1 s without a new
  `SERVO_LIVE`.

## 6. EEPROM budget

See **`EEPROM.md`**: the layout, what fits in 1024 B, and what can still be compressed.
In short: an input is 4 B, the routes have a pool of 762 B to themselves, and with
timing off 16 × 16 steps fit internally (512 B).

## 7. DCCnext (ArCoMoRa)

| Port | 1–10 | 11–16 | n/a |
|---|---|---|---|
| Arduino pin | D3–D12 | A0–A5 | D13, D0/D1 (USB), D2 (DCC) |

- PWM works on port 1, 3, 4 and 9 (D3, D5, D6, D11).
- D9/D10 give no PWM as long as the Servo library is active (timer 1). That also applies to IOcore on a
  normal Nano.
- Source: the [DCCNext schematic](https://www.arcomora.com/wp-content/uploads/2024/09/ArsigdecRocRailUK.pdf)
  and the [Mardec 7 manual](https://www.arcomora.com/wp-content/uploads/2021/08/Mardec7UK.pdf).
  ArCoMoRa calls the schematic "not complete", so check it on your own board.
- In the GUI you tick the *DCCnext board* checkbox. The pin choices then show "D3 · port 1", and D13 turns red.

## 8. Decoder firmware

The details are in `EEPROM.md` and in the commits of `software/IOcore`. In short: HELLO with
boot window, board settings (§9), route pool with dynamic allocation, per-step timing,
interlocking, SERVO_LIVE and `setPosition()`, A6/A7 as analog input, and protecting pin 2.

**Pin 2 (DCC input), D13 (config LED) and A7 (config button)** must never be used as input or output. A `SET_INPUT_DATA` or `SET_OUTPUT_DATA` with
pin 2 gets `NACK 5` (invalid pin) and is not saved. The same applies to D0/D1 as long as the
decoder does not report bit0 in HELLO, and to A6/A7 as output or on a board other than the Nano.

## 9. Board settings and name

```text
host   -> C0                                        (GET_BOARD, no payload)
device -> CF <len> <type> <flags> <delay> <name ...> (also SET_BOARD from host -> ACK)
len    : 4 + name length (name 0..16 ASCII bytes, no terminator)
type   : 0 = Arduino Uno (no longer offered: the GUI reads it as a Nano), 1 = Arduino Nano, 2 = DCCnext,
         3 = Jack of all trades
flags  : bit0   = D0/D1 as inputs / outputs without a PC; the board link is off (firmware 2026-11-23)
         bit1   = route step timing (per-step wait times stored)
         bit2-3 = route interlocking: 0 = off (all routes may run in parallel)
                                      1 = on  (one route at a time)
                                      2 = auto (routes that move a turnout run one at a time,
                                                other routes run in parallel)  <- default
         bit4-7 = block signal blink rate x 0.25 Hz (1-15 = 0.25-3.75 Hz), 0 = 1 Hz (default)
delay  : fixed wait after each route step when timing is off, x 0.1 s (default 3 = 0.3 s)
```

- **Name.** The name identifies the board. The GUI keeps a file `<name>.json` per board (names
  of inputs and outputs plus all settings). When connecting it reads `HELLO`, then `GET_BOARD`, loads
  `<name>.json` if it exists, and then reads **everything** from the board. What the board says wins; the
  names come from the file. A new board has an empty name; the GUI then asks for one.
- **Type.** Determines which pins exist. On the Nano, A6/A7 (pin index 20/21) are analog-only
  inputs without pull-up: the firmware reads them with `analogRead() < 512` as "pressed", and the button
  needs an external 10 kΩ pull-up. The type is set by the GUI; the firmware only uses it
  to check pins. Type 0 (Uno) is no longer offered: the GUI shows it as a Nano and the firmware applies the
  Nano's pin rules to it.
- **Recorder.** Removed on 2026-11-16 (it needed an external I2C EEPROM on A4/A5, ±4.6 KB flash and ±250 B RAM).
  A4/A5 are always normal pins.
- **Switching timing on/off.** The decoder converts the stored routes (3 ↔ 5 B per step). If it does not fit
  (timing on with too many steps), the answer is `NACK 4` and the old setting stays.
- **Interlocking.** A route that is refused by interlocking is ignored (not put in a
  queue): this protects against a wrong button press. "Moving a turnout" = the route
  contains an output of type servo, single pulse or double pulse.
- **NACK reasons:** 1 checksum, 2 too long, 3 timeout, 4 full (pool or route), 5 invalid pin.

### 9.1 How the firmware does it (2026-11-12, deviations and choices)

- **Default values** new board: Nano, timing **off**, interlocking auto, delay 0.3 s.
  A board without the magic `'G'`, `'2'` (`EEPROM.md`) gets these values.
- **SET_ROUTE writes immediately.** `first = 0` makes the route its new length at once (the routes behind it
  shift up, `NACK 4` if it does not fit); every following piece is written directly. An interrupted
  transfer therefore leaves a half-written route behind. The GUI always reads the route back afterwards.
- **Switching timing on** gives every existing step the board delay as wait time. The first step starts immediately. Wait times run down on the 50 ms tick of the inputs.
- **Interlocking:** a route counts as running until the wait time after the last step has passed.
- **Still open:** `setPosition()`/`SERVO_LIVE` do not switch the unifrog relay; `SET_OUTPUT_DATA` does not set
  `pinMode` for newly configured pins; an output on D0/D1 is not stopped during the
  serial window.

## 10. Config button (D2, before: A7), config LED (D13) and teaching servos (firmware, 2026-11-13)

**Since 2026-11-20 the button is on D2**, the DCC input: a button to GND holds D2 LOW, DCC changes the level every 58-116 µs. The firmware counts a press when D2 is LOW for 1 ms without one HIGH sample (sampled every 50 ms). A7 is a normal analog input again (Nano). Where this section says A7, read D2.

- **A7** = config button: push button to GND, with an **external** 10 kΩ pull-up to 5 V (A7 has no internal
  pull-up). The firmware reads it with `analogRead()` with hysteresis: pressed below 200, released above 800.
- **D13** = LED: on = normal, flashing = config mode.
- An output or input that uses D13 or A7 is set to *not in use* at start-up; `SET_*_DATA` with
  D13/A7 gets `NACK 5`.

| Action | Result |
|---|---|
| A7 pressed for 2 s | config mode on / off |
| A7 short press (in config mode) | select next servo; it toggles, so you can see which one it is |
| A7 short press (not in config mode) | GUI mode: serial port open for 5 s, the board announces itself (§3.1) |
| Loc **9999**, speed knob + direction | selected servo moves (1–10° per 100 ms) |
| Loc 9999 **F1** | save current angle as *curved* (`servoMin`) |
| Loc 9999 **F2** | save current angle as *straight* (`servoMax`) |
| Loc 9999 **F3** | toggle the selected servo |
| Loc 9999 **F4** | every press: invert the frog relays of the selected servo (`invertFrog`) on / off |
| Loc 9999 **F5** | next servo |

Same operation as the OS-Servo-Decoder. The angles are in the EEPROM immediately; the GUI reads them back when
connecting.

**Servos at start-up:** one by one, the next one every 500 ms. Each servo gets its stored position (curved or straight)
for ~500 ms and is then detached. An own angle (route step, live) is not remembered.

## 11. Bluetooth (HC-05, HELLO bit5, firmware 2026-11-18)

An HC-05 on D0/D1 replaces the USB cable. The GUI's *Board > Set up Bluetooth (HC-05)* sets it up through the board.

**Wiring** (HC-05 → Nano): VCC → 5 V, GND → GND, RXD → D1 through a voltage divider (1 kΩ from D1 to RXD, 2 kΩ from
RXD to GND: the HC-05 takes 3.3 V), TXD → D0 through a **Schottky diode** (BAT85 or 1N5819) with the **cathode at the
HC-05 TXD**. The diode can only pull D0 low, so the USB-serial chip still drives D0 and USB keeps working while the
HC-05 is attached.

**BT_SETUP (`E0`).** The HC-05 must be in **AT mode**: hold its button while it gets power (LED blinks slowly,
about every 2 s); AT mode is always 38400 baud. The handler only sets `btSetupPending`; `loop()` then runs
`btSetup()`, which blocks for up to ~2 s:

1. `Serial.flush()`; for each baud rate 38400, 9600, 57600, 115200, 19200: `Serial.begin(baud)`, empty the input, send
   `AT\r\n`, wait up to 300 ms for `OK`.
2. Found: `AT+UART=115200,0,0\r\n` (wait for `OK`), then `AT+NAME=IOcore\r\n`, or `AT+NAME=IOcore-<name>\r\n`
   with the board name (only letters, digits, `-` and `_`, max. 20 characters in total). The PIN is not changed.
3. `Serial.begin(115200)`, empty the input, send `BT_RESULT` (`E3 status baud÷2400 name_ok`).

Meanwhile the PC gets the AT commands at the wrong baud rate (noise) and maybe `NACK`s caused by the HC-05's
answers. The GUI sends nothing else during the set-up (one task, 8 s timeout) and ignores every byte until it finds
a valid `BT_RESULT` frame in the byte stream. The session goes on at 115200 afterwards.

**Afterwards:** unplug the board and plug it in again (without holding the HC-05 button); the HC-05 now runs at
115200 baud. Pair *IOcore…* in Windows Bluetooth settings (PIN 1234, or 0000 on some modules); Windows creates a
COM port for it. Connecting over that port does not reset the board: press the config button briefly (§3.1).

**Simulator.** `iocore_fw_host` can attach a fake HC-05 (frame `'H' <baud÷2400>`) that answers `OK` to every AT
command at that baud rate and reports what it received (`'h'`); bytes the firmware sends at a baud rate other than
115200 (and 9600, the DFPlayer) reach the GUI as noise (`F0` per byte), never as valid frames.

## 12. Board link (board to board, firmware 2026-11-20)

D0/D1 are never an input or output pin any more: they are the serial line. For 5 s after start-up (or after
a short press on the config button) the GUI can connect at 115200 baud. Without a GUI the UART then switches
to **9600 baud: the board link**. A DFPlayer on TX hears the same line and ignores the link frames.

**Frame** (4 bytes, back to back; after a gap of more than 3 ms a receiver starts over):

```
<start> <address> <value> <check>
start  = 0xA0 | extended << 4 | hops      (101x hhhh; hops = how many boards may still pass it on, 0-15)
check  = start ^ address ^ value          (XOR)
basic    : value = position, 1 = straight, 0 = curved   -> runs as a DCC accessory command for <address>
extended : value 0-255 (angle, aspect, PWM, track)    -> runs as a DCC extended command for <address>
```

Address 0 is not used. A board that receives a valid frame on RX (D0) **first passes it on** on TX (D1) with
`hops - 1` (not when hops is 0), then runs it like the same DCC command from the track: every input with that
DCC address reacts as usual. A board in a chain therefore needs no setting; a ring of boards stops after 15 hops.

**Board link output** (type 7, pin A = D1): `data[0]` = its own address (0 = none). A route step or custom
value has the target address in its extra byte (§4, byte 4; 0 = the own address): curved / straight sends a
basic frame, a raw value an extended frame. Switched on / off directly (toggle, absolute, DCC) it sends a basic
frame to its own address, if it has one. One link output can so reach every address, basic and extended.

Each frame sent is also reported as `OUTPUT_EVENT` state 2 with the value. With the GUI connected the UART is
the GUI's: nothing is sent or received on the link.
