# GPIOcore — EEPROM usage

Date: 2026-11-12 · ATmega328P (Uno, Nano, DCCnext): **1024 B internal EEPROM**.
There is no external EEPROM (the recorder that used one was removed on 2026-11-16).

## 1. Internal EEPROM map (layout 1)

Source: `software/GPIOcore/routes.h` (`EE_*` constants).

| Address (dec) | Address (hex) | Bytes | Contents |
|---|---|---|---|
| 0 – 2 | 0x000 – 0x002 | 3 | Magic `'G'` `'2'`, layout version `1`. If this is missing, the board gets default values (delay 0.3 s, Nano, timing off, interlocking auto) |
| 3 | 0x003 | 1 | Board type: 0 Uno, 1 Nano, 2 DCCnext, 3 Jack of all trades |
| 4 | 0x004 | 1 | Board flags: bit0 reserved (was recorder), bit1 timing per step, bit2-3 interlocking (0 off, 1 on, 2 auto), bit4-7 block signal blink rate x 0.25 Hz (0 = 1 Hz) |
| 5 | 0x005 | 1 | Fixed wait time after a route step when timing is off, × 0.1 s |
| 6 – 21 | 0x006 – 0x015 | 16 | Board name, ASCII, padded with 0 |
| 22 – 181 | 0x016 – 0x0B5 | 160 | **16 outputs × 10 B** — output *n* at `22 + 10·n` |
| 182 – 245 | 0x0B6 – 0x0F5 | 64 | **16 inputs × 4 B** — input *n* at `182 + 4·n` |
| 246 – 261 | 0x0F6 – 0x105 | 16 | Number of steps per route (0–16) — route *n* at `246 + n` |
| 262 – 1023 | 0x106 – 0x3FF | 762 | **Route pool**: all route steps one after the other |

### Output record (10 B)

| Byte | Contents |
|---|---|
| 0 – 5 | Bit stream starting at bit 0 of byte 0: `pinA` bits 0–5, `pinB` 6–11, `pinC` 12–17, `pinD` 18–23, `relayPinA` 24–29, `relayPinB` 30–35, `relayPinC` 36–41, `type` 42–45 (0–14; 14 = sound effect, `pinA` = 1), `state` 46, `invertFrog` 47 (frog relays driven inverted) |
| 6 – 9 | `data[4]`, the meaning depends on the type (see `PROTOCOL.md` §2.1) |

Pin numbers: 0–13 = D0–D13, 14–19 = A0–A5, 20–21 = A6–A7, 63 (0x3F) = not used.

### Input record (4 B)

Bit stream starting at bit 0 of byte 0: `address` bits 0–11 (0xFFF = none), `pin` 12–17, `type` 18–20,
`outputID` 21–24, `level` 25–31. `pin` is a pin number or an analog comparison
`0x20 | mode << 3 | channel` (`PROTOCOL.md` §2.2); `level` = the analog threshold / 8 (0–127 → 0–1016), only
used for an analog input. A `static_assert` keeps the record at 4 B.

### Route pool

- Route *r* starts at `262 + (sum of the steps of route 0 … r-1) × step size`. So there are no fixed
  places per route; a route costs exactly as many steps as it has.
- **Step size** follows the board flag *timing*:
  - **off** (default): **2 B** — `id | raw`, `value`;
  - **on**: **4 B** — `id | raw`, `value`, wait time low, wait time high (14 bit × 0.1 s).
- If a route gets longer or shorter, the routes behind it shift. Switching timing on/off converts the whole pool.
- The slot of a **toggle or absolute input** is normally empty. With *own values* it contains 2 steps with
  bit 5 (`STEP_VALUES`, 0x20) in byte 0: the value for state 0 and for state 1 (`PROTOCOL.md` §4). Byte 0 is
  stored unchanged, so the marker also stays when timing is off (2 B per step).

| Timing | Step | Fits in 762 B | 16 routes × 16 steps (256) |
|---|---|---|---|
| off | 2 B | **381 steps** | 512 B ✓ (250 B left) |
| on | 4 B | 190 steps | 1024 B ✗ — max. ±11 steps per route if all 16 routes are full |

**Total in use without routes:** 262 B fixed + 0 B routes. Everything above address 261 is for routes.

## 2. What else can be compressed

| Idea | Gain | Cost / risk | Advice |
|---|---|---|---|
| Timing as a board setting | 2 B per step | less flexible | ✓ done |
| Recorder removed (it was in an I2C EEPROM) | 580 B internal | – | ✓ done (2026-11-16): −4.6 KB flash, +254 B free RAM |
| **Variable step**: 2 B, plus 2 B only if the wait time differs (flag bit in byte 0) | almost always 2 B per step, also with timing on | read the route sequentially (the decoder does that anyway) | good idea for later; then the timing setting can go |
| Wait time 14 → 8 bit with scale | 1 B per step | max ±2 min | only if needed |
| Board name 16 → 12 characters | 4 B | – | not needed |
| `malloc` / heap | – | fragmentation on 2 KB RAM | do not; the pool is already dynamic |

**RAM** (Nano, 2026-11-16): 1412 B of 2048 B in use, 636 B free. Flash: 24234 B of 30720 B (78 %).
