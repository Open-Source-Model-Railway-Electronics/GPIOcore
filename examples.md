# GPIOcore – what you can build with it

Ready-made ideas, from simple to clever. Each example says **what you need**, **how to wire it** and
**what to set in the GPIOcore program**. Words like *input*, *output* and *route* are explained in the
[manual](README.md).

Quick reminder:

- An **output** is something the decoder switches: a servo, a lamp, a turnout coil, a sound.
- An **input** is something that tells the decoder to act: a push button, a switch, a sensor, or a DCC
  address from your command station.
- A **route** is a list of steps the decoder does one after another, with a wait time after each step.
  Think of it as a little script.

> **No DCC? No problem.** Every example works with only buttons and sensors. DCC is optional.

---

## Contents

1. [Plain servo decoder](#1-plain-servo-decoder)
2. [Servo decoder with frog polarisation](#2-servo-decoder-with-frog-polarisation)
3. [Coil decoder (solenoid turnouts)](#3-coil-decoder-solenoid-turnouts)
4. [Coil decoder with frog polarisation](#4-coil-decoder-with-frog-polarisation)
5. [Stall motors (MTB MP1 and similar)](#5-stall-motors-mtb-mp1-and-similar)
6. [Set a whole route of turnouts with one button](#6-set-a-whole-route-of-turnouts-with-one-button)
7. [Many buttons on one pin (resistor ladder)](#7-many-buttons-on-one-pin-resistor-ladder)
8. [Water crane: a train arrives, the crane swings out and back](#8-water-crane-a-train-arrives-the-crane-swings-out-and-back)
9. [Engine shed doors that open and close by themselves](#9-engine-shed-doors-that-open-and-close-by-themselves)
10. [A crane with several servos](#10-a-crane-with-several-servos)
11. [Street and house lights that come on when it gets dark](#11-street-and-house-lights-that-come-on-when-it-gets-dark)
12. [Level crossing with flashing lights, barriers and a bell](#12-level-crossing-with-flashing-lights-barriers-and-a-bell)
13. [Playing sounds](#13-playing-sounds)
14. [Light effects](#14-light-effects)
15. [A signal](#15-a-signal)

---

## 1. Plain servo decoder

**What it does:** up to 16 servo turnouts, each on its own DCC address and/or push button.

**You need:** servos (SG90 or MG90), a separate 5 V supply for the servos (the Nano cannot power more
than one or two), wires.

**Wiring:** servo signal wire to an output pin (D3–D12, A0–A5), servo + to the 5 V servo supply,
servo − to GND. Connect the GND of the servo supply to the GND of the Nano.

**In the program:**

1. *File → New board*, choose the preset **"16 servos"**.
2. Click an output, set **Position 0 (curved)** and **Position 1 (straight)**. Tick **Live** to see the servo
   move while you slide.
3. Set the **Speed** (higher = slower, smoother).
4. Each input is already linked to the output with the same number, on DCC address 1, 2, 3 …, as
   **Absolute**: "straight" on your throttle sets the turnout straight, "curved" sets it curved. Change the
   addresses if you like. Add a push button to an input's pin and choose behaviour **Toggle**: every press
   throws the turnout over.

> Tip: set the end positions at the layout with your phone (*Board → Phone remote…*) or with the config
> button and your throttle. See the manual.

---

## 2. Servo decoder with frog polarisation

**What it does:** like example 1, but the decoder also switches the power on the frog of the turnout, so
locos don't stall on it.

**You need:** a relay module per turnout (a 5 V relay board with a transistor input works).

- **Unifrog / one relay:** one relay switches the frog between the two rails.
- **Electrofrog / two relays:** the decoder turns *both* relays off while the servo moves and switches the
  right one on halfway. No short circuit while the points move.

**In the program:** preset **"8 servos + unifrog relay"** or **"5 servos + electrofrog relays"**. Per
servo output, choose the **Frog** type and the relay pins.

---

## 3. Coil decoder (solenoid turnouts)

**What it does:** drives turnouts with two coils (e.g. Peco, Roco, Fleischmann): a short pulse on one coil
throws the turnout, a pulse on the other throws it back.

**You need:** a driver board, because coils need much more current than an Arduino pin gives. A
**ULN2803** (8 channels) or MOSFET board works. Plus a coil supply (often 16 V AC rectified, or 12–16 V DC).

**Wiring:** Arduino pins → driver inputs, driver outputs → one side of each coil, the common wire of the
turnout → + of the coil supply. Connect the GND of the coil supply to the Nano's GND.

**In the program:** preset **"8 double pulse"**. Per output type **Double pulse**, two pins (one per coil)
and the **pulse length** (start with 200 ms; too short = doesn't throw, too long = hot coils).

---

## 4. Coil decoder with frog polarisation

As example 3, plus a relay for the frog. Presets **"5 double pulse + unifrog relay"** or
**"4 double pulse + electrofrog relays"**.

---

## 5. Stall motors (MTB MP1 and similar)

**What it does:** stall motors (MTB MP1, Tortoise, Cobalt) don't want a pulse but a **constant voltage**
whose polarity decides the direction.

**You need:** a small motor driver per motor, e.g. an **L9110S** or **L293D** board (two inputs → motor
turns one way or the other). The MP1 has its own switch for the frog, so no relay is needed.

**Wiring:** two Arduino pins → the two inputs of the driver, driver outputs → the motor.

**In the program:** output type **Double steady**: pin A follows the state, pin B is always the opposite.
So the driver always gets "left" or "right" and the motor keeps pushing gently against its end stop, which is
how a stall motor is meant to work.

---

## 6. Set a whole route of turnouts with one button

**What it does:** you press one button on your control panel (or send one DCC address) and the decoder
throws several turnouts one after another, e.g. "track 3 into the station".

**In the program:**

1. Click an input, set **Function** to **Route**.
2. Add steps: *Out 1 straight*, *Out 2 curved*, *Out 5 curved* …
3. Set the wait time after each step (0.3 s is fine for servos; it keeps the power supply calm).

> Several routes may share turnouts. Under *Board settings* you can choose that only one route at a time
> may run (**interlocking**), so two routes don't fight over the same turnout.

---

## 7. Many buttons on one pin (resistor ladder)

**What it does:** saves pins. Five, eight or more push buttons share **one analog pin**. Each button
makes a different voltage; the decoder recognises which button was pressed.

**You need:** push buttons and resistors.

**Wiring (one common way):**

```text
5 V ──[ 10 kΩ ]──┬──────────── A0
                 │
                 ├─[button 1]─────────────────────── GND
                 ├─[button 2]──[ 1 kΩ ]───────────── GND
                 ├─[button 3]──[ 2.2 kΩ ]─────────── GND
                 ├─[button 4]──[ 4.7 kΩ ]─────────── GND
                 └─[button 5]──[ 10 kΩ ]──────────── GND
```

No button pressed: A0 reads about 1023. Each button pulls A0 down to its own value.

**In the program**, for each button one input:

1. **Read as:** *Analog – at a value (resistor ladder)*, **Analog pin:** A0 (all buttons use A0).
2. Press the real button and look at **Live reading**: e.g. "Now: 93". Click **Use this value**.
3. Choose what the button does: **Direct control** (Toggle or Absolute) of an output, or a **Route**.

The decoder accepts the value ±10, so leave at least about 40 between two buttons. Theory and practice
differ (resistor tolerances, cable length): **always use the live reading**, not a calculator.

> Only one button at a time: pressing two buttons together gives a third value that matches none (or the
> wrong one).

---

## 8. Water crane: a train arrives, the crane swings out and back

**What it does:** a loco stops at the water crane. After 5 seconds the crane arm swings over the tender,
"fills up" for 30 seconds and swings back.

**You need:** one servo for the arm, a sensor in the track: a reed switch with a magnet under the loco, an
IR reflection sensor, or a light sensor (LDR) between the sleepers.

**In the program:**

1. *Board settings…* → tick **Own wait time per route step** (otherwise every step waits the same fixed
   time).
2. Output: **Servo** for the crane arm. Straight = parked, curved = over the tender.
3. Input on the sensor pin:
   - a reed switch or IR sensor: a normal digital pin;
   - an LDR (the loco covers it): **Read as** *Analog – below a value* and use the live reading to find the
     value with and without a loco above it.
4. **Function: Route**, with these steps:

| Step | Output | Value | Wait after |
|---|---|---|---|
| 1 | Crane servo | straight (it is already there) | 5 s |
| 2 | Crane servo | curved (swing out) | 30 s |
| 3 | Crane servo | straight (swing back) | 0 s |

Step 1 changes nothing; it is only there to wait 5 s before the crane moves. Add a sound step (example 13)
for the water noise.

---

## 9. Engine shed doors that open and close by themselves

**What it does:** a loco drives up to the shed, both doors open; when the sensor is free again, the doors
close.

**You need:** two servos (one per door), one sensor in front of the doors (LDR or IR sensor on an analog
pin).

**In the program:** one sensor, two inputs that **share the same analog pin**:

1. Input 1: **Read as** *Analog – below a value* on A2, **Direct control – Absolute**, output = left door
   servo.
2. Input 2: exactly the same, output = right door servo.
3. Use **Custom values** to set the exact door angles for "open" and "closed".

Absolute means: the door follows the sensor. Sensor covered → door opens, sensor free → door closes.
Give the right door a slightly different threshold if you want the doors to move one after the other.

> With DCC instead of a sensor: two route inputs on the same DCC address. One with **Route starts on:
> straight only** (close both doors), one with **curved only** (open both doors).

---

## 10. A crane with several servos

**What it does:** a loading crane turns, lowers its hook, lifts, turns back – a little show.

**You need:** two or three servos (turn, boom up/down, hook).

**In the program:** one **Route** (from a button or a DCC address) with steps that set **angles**
directly (the value of a step can be an angle instead of straight/curved):

| Step | Output | Value | Wait after |
|---|---|---|---|
| 1 | Turn servo | 40° | 2 s |
| 2 | Hook servo | 150° (down) | 3 s |
| 3 | Hook servo | 60° (up) | 2 s |
| 4 | Turn servo | 140° | 2 s |
| 5 | Hook servo | 150° | 3 s |
| 6 | Hook servo | 60° | 0 s |

Every servo moves at its own **Speed**, so slow servos give a realistic, heavy movement.

---

## 11. Street and house lights that come on when it gets dark

**What it does:** when you dim the room light, the houses light up **one by one**; when it gets light,
they go off again one by one.

**You need:** one light sensor (LDR) and a resistor, LEDs with resistors (or a transistor per house for
more LEDs).

**Wiring:** `5 V ── LDR ──┬── A1`, and `A1 ── 10 kΩ ── GND`. Light = high value, dark = low value.

**In the program:** one input per house, all on **A1**:

| Input | Read as | Threshold | Function (Direct control) | Output |
|---|---|---|---|---|
| House 1 | Analog – above a value | 600 | Absolute | house 1 lights |
| House 2 | Analog – above a value | 520 | Absolute | house 2 lights |
| House 3 | Analog – above a value | 440 | Absolute | street lights |

While the room is light (above the threshold) the lights are off; below it they come on. Because the
thresholds differ, the houses switch one after another. The decoder waits 20 points before switching
back, so a light never flickers on the edge.

Check the real values with **Live reading** in your room, day and evening.

---

## 12. Level crossing with flashing lights, barriers and a bell

**What it does:** a train approaches: the red lights start flashing, the bell rings, a few seconds later
the barriers come down. After the train has passed, the barriers go up and the lights stop.

**You need:** two red LEDs (or the crossing's lamps), two servos for the barriers, a DFPlayer Mini with
a bell sound (see example 13), two track sensors (one before, one after the crossing).

**Outputs:**

- **Swing light** for the two red lamps (they flash alternately).
- **Servo** for each barrier.
- **Sound effect** for the bell.

**Inputs:**

Sensor before the crossing → **Route** "close":

| Step | Output | Value | Wait after |
|---|---|---|---|
| 1 | Swing light | on | 0 s |
| 2 | Sound | track 1 (bell) | 4 s |
| 3 | Barrier left | curved (down) | 0.5 s |
| 4 | Barrier right | curved (down) | 0 s |

Sensor after the crossing → **Route** "open":

| Step | Output | Value | Wait after |
|---|---|---|---|
| 1 | Barrier left | straight (up) | 0.5 s |
| 2 | Barrier right | straight (up) | 3 s |
| 3 | Swing light | off | 0 s |
| 4 | Sound | 0 (stop) | 0 s |

Set a slow servo **Speed** so the barriers move like the real thing.

---

## 13. Playing sounds

**What it does:** plays MP3 files: a bell, a station announcement, a steam whistle, water, birds.

**You need:** a **DFPlayer Mini** module, a micro-SD card, a small speaker (3 W, 4–8 Ω).

**Wiring:**

- DFPlayer VCC → 5 V, GND → GND.
- **DFPlayer RX ← Nano TX (D1)**, with a **1 kΩ resistor** in between.
- Speaker on SPK1 and SPK2.

**SD card:** name the files `0001.mp3`, `0002.mp3`, `0003.mp3` … (in this order: the number is the track).

**In the program:** one output of type **Sound effect** (always on D1). Then choose the track where you
start it:

- a **route step** with the track number (0 = stop);
- a **Direct control** input (Toggle or Absolute) with **Custom values**, e.g. *off = stop*, *on = track 3*
  (ticked by themselves when the input is linked to a sound output);
- a **DCC extended** command with the track number.

> The DFPlayer shares D1 with the USB connection. **Sound only plays when no PC is connected**: start the
> layout without the program (or wait 5 seconds after power-up before connecting). With the program
> connected, it shows "♪ 3" on the output instead.

---

## 14. Light effects

Every output can be a little light show, controlled like any other output (button, DCC or route):

| Type | Looks like | Pins |
|---|---|---|
| **Single flashing** | a blinking warning light | 1 |
| **Double flashing** | two lamps blinking in turn | 2 |
| **Swing light** | level crossing lights | 2 |
| **Fireplace** | flickering fire or a campfire (needs PWM pins: D3, D5, D6, D9, D10, D11) | 2 |
| **TL tube** | a fluorescent tube that flickers a few times before it stays on | 1 (PWM) |
| **Welder** | random blue welding flashes in a workshop | 1 |
| **PWM** | a lamp at a fixed brightness | 1 (PWM) |

> Fading (slowly dimming lights, e.g. for day and night) is **not** available yet.

---

## 15. A signal

**What it does:** a light signal with up to four lamps (e.g. red, green, yellow, white).

**In the program:** output type **4-aspect signal**, lamps on pins A–D. The command station sends the
aspect with **DCC extended**, or a route step sets it (click the aspect picker next to the value and tick
the lamps). Lamps can also flash; set the blink rate once per signal.
