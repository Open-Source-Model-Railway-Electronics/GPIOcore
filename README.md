# GPIOcore

**Turn a cheap Arduino Nano into a decoder for your model railway.**

GPIOcore drives up to **16 things** on your layout (servos, turnouts, lights, signals, sounds) and listens to
up to **16 things** (push buttons, switches, track sensors, your DCC command station). You set it all up with
a program on your Windows PC. You don't have to write any code.

Part of [Open-Source-Model-Railway-Electronics](https://github.com/Open-Source-Model-Railway-Electronics).

![The GPIOcore program: what listens on the left, what is switched on the right, lines show what drives what](docs/images/servo-output.png)

**➜ Ideas and step-by-step recipes: [examples.md](examples.md)** (servo decoder, coil decoder, level crossing
with a bell, lights that come on at dusk, a water crane, and more).

---

## Contents

1. [What can it do?](#1-what-can-it-do)
2. [What you need](#2-what-you-need)
3. [Getting started](#3-getting-started)
4. [The screen](#4-the-screen)
5. [Outputs: what the decoder switches](#5-outputs-what-the-decoder-switches)
6. [Inputs: what tells the decoder to act](#6-inputs-what-tells-the-decoder-to-act)
7. [Routes: a list of steps](#7-routes-a-list-of-steps)
8. [Custom values](#8-custom-values)
9. [Analog inputs: sensors and many buttons on one pin](#9-analog-inputs-sensors-and-many-buttons-on-one-pin)
10. [Sound (DFPlayer Mini)](#10-sound-dfplayer-mini)
10a. [Automatic block signals](#10a-automatic-block-signals)
11. [Setting servos at the layout](#11-setting-servos-at-the-layout)
12. [Saving your work](#12-saving-your-work)
13. [When something doesn't work](#13-when-something-doesnt-work)
14. [For developers](#14-for-developers)

---

## 1. What can it do?

- **Turnouts:** servos (with adjustable speed), coil turnouts (two coils, short pulse), stall motors.
  It can also switch the **frog** power with a relay, so locos never stall on the turnout.
- **Lights:** on/off, blinking, level crossing lights, a flickering fireplace, a fluorescent tube that
  starts up, welding flashes, a lamp at a set brightness that can fade slowly (day and night).
- **Signals:** up to four lamps per signal.
- **Sound:** plays MP3 files (bell, announcement, whistle) with a DFPlayer Mini module.
- **Routes:** one button press or one DCC command does several things one after another, with wait times
  in between. For a station entry, a crane, shed doors, a level crossing.
- **Sensors:** a light sensor, a reed switch, an IR sensor, or **many buttons on one pin** (to save pins).
- **With or without DCC:** works with a DCC command station, but just as well with only buttons and sensors.

---

## 2. What you need

| Part | Why |
|---|---|
| **Arduino Nano** (the cheap clones with a "CH340" USB chip are fine) or an Arduino Uno | the decoder itself |
| A USB cable and a **Windows PC** | to set it up (not needed while the layout runs) |
| A **5 V power supply** for servos | one Nano can't power more than one or two servos |
| Optional: a DCC input circuit (optocoupler) on pin **D2** | only if you use a DCC command station |
| Optional: a push button on pin **A7** (with a 10 kΩ resistor to 5 V) | to set servos at the layout with your throttle |
| Optional: relay modules, a coil driver (ULN2803), a DFPlayer Mini … | depending on what you build, see [examples.md](examples.md) |

**Pins with a fixed job:** D2 = DCC, D13 = the little LED on the Nano (shows the config mode), A7 = config
button. All other pins are yours.

**Good to know about pins:**

- D0 and D1 are also the USB connection. They only work as normal pins when no PC is connected.
- Some effects need a **PWM pin** (marked "PWM" in the program): D3, D5, D6, D9, D10, D11.
- A6 can only be used as an input, and only on a Nano.

---

## 3. Getting started

1. **Download** the newest release (the `GPIOcore-…zip` file) and unpack it anywhere, e.g. on your desktop.
   Nothing needs to be installed.
2. **Start** `GPIOcoreGUI.exe`.
3. **Put the GPIOcore software on the Nano** (once, and again after an update): plug in the Nano, choose
   *Board → Flash firmware…*, pick the port and click **Flash**.
4. **Connect:** choose the port at the top (e.g. "COM5 — USB Serial") and click **Connect**. The frame
   around the drawing turns red: you are connected.
5. **Start configuring:**
   - click **Read board** to see what is on the board now, or
   - pick a **Preset** (a ready-made setup, e.g. "16 servos") and click **Apply**, or
   - open an example file (*File → Open*, folder `examples`).

> **No board yet?** You can do everything without one: *File → New board (offline)…*. Try it in the
> **Simulate** mode and write it to the board later with **Write all**.

---

## 4. The screen

- **Left column:** the inputs (what listens). **Right column:** the outputs (what is switched).
- **Lines** show which input drives which output. Each input has its own colour.
- **Click a block** to change it. Its settings open in the panel on the left (inputs) or right (outputs).
- **+ Add input / + Add output** adds a block. Hover over a block and click the trash can to remove it.
- Every setting has an **i** button that explains it.

### Three modes

At the top you always choose one of three modes. The colour of the frame tells you which one you're in:

| Mode | Frame | What it is for |
|---|---|---|
| **Configure** | red when connected | Change things. When connected, every change is saved on the board straight away. |
| **Simulate** | yellow | Try your setup without a board: click the buttons on the inputs and watch the outputs. You can't change anything here. |
| **Live test** | green | Click the buttons on the inputs and watch the real layout react. You can't change anything here. |

The **Pin map** tab shows every pin of the Nano and what uses it. Red = two things on one pin.

---

## 5. Outputs: what the decoder switches

Click an output and choose its **Type**:

| Type | What it does | Pins |
|---|---|---|
| **Servo** | turns a servo between two positions you choose, at the speed you choose | 1 |
| **Double pulse** | coil turnout: a short pulse on coil A or coil B | 2 |
| **Single pulse** | a short pulse on one pin (e.g. an uncoupler) | 1 |
| **Single steady** | one pin on or off (a lamp, a relay) | 1 |
| **Double steady** | two pins, always opposite (stall motor via a motor driver, two-colour LED) | 2 |
| **Single flashing / Double flashing** | blinking light(s) | 1 / 2 |
| **Swing light** | level crossing lights, two lamps blinking in turn | 2 |
| **Fireplace / TL tube / Welder** | light effects | 1–2 |
| **PWM** | a lamp at a set brightness; with a **Fade time** it changes slowly (day / night) | 1 |
| **4-aspect signal** | a light signal with up to four lamps | 4 |
| **Sound effect** | plays MP3 tracks on a DFPlayer Mini (see chapter 10) | D1 |
| **Block signal** | a light signal that sets itself from track occupancy (see chapter 10a) | 1–4 |

Every output has two states: **curved / off** and **straight / on**. **Test: curved / off** and
**Test: straight / on** let you try it.

**Choosing pins:** pins that are already used are shown in **red** in the list. If you choose one anyway,
it moves to this output and the other block becomes "not in use".

**Servo settings:**

- **Position 0 (curved)** and **Position 1 (straight)**: the two end positions (10–170°).
- **Live**: when ticked, the servo moves along while you slide. Handy to find the right position.
- **Speed**: higher = slower and smoother.
- **Frog**: switch the frog power with one relay (unifrog) or two relays (electrofrog).
  With two relays, both are off while the points move, so there is never a short circuit.

---

## 6. Inputs: what tells the decoder to act

Click an input. Under **Triggered by** you choose what starts it:

- **DCC accessory address:** a turnout number on your command station or throttle.
- **Input pin:** a button, switch or sensor between that pin and GND. (The decoder switches on the pin's
  internal resistor, so you don't need one.)
- Both at once is fine.

The **Function** decides what happens:

| Function | What it does |
|---|---|
| **Direct control**, behaviour **Toggle** | every press switches the output over (straight ↔ curved, on ↔ off) |
| **Direct control**, behaviour **Absolute** | the output follows: button pressed = curved / off, released = straight / on. With DCC: "straight" = straight, "curved" = curved |
| **Route** | does a list of steps, see chapter 7 |
| **Sensor (occupancy)** | a track occupancy detector or other sensor; it switches nothing itself, block signals and semaphore arms read it |

If your command station sends the same command a few times in a row, the decoder only reacts to the
first one, so a toggle doesn't switch back and forth.

---

## 7. Routes: a list of steps

![A route with six steps](docs/images/route-input.png)

A route is a little script: **up to 16 steps**, done one after another. Each step says:

- which **output**;
- what **value**: straight / curved, or a number (a servo angle, a signal picture, a brightness, a sound
  track);
- how long to **wait** before the next step.

**Route starts on** decides which DCC command starts it: any command, only *straight* or only *curved*.
That way one DCC address can start two different routes (e.g. barriers down with "curved", up with
"straight"). A button always starts the route when you press it.

Tips:

- You can also make a route by **dragging**: drag from an output to the route's input to add a step.
- For different wait times per step, tick **Own wait time per route step** under *Board settings…*.
  Otherwise every step waits the same time.
- Under *Board settings…* you can also choose that only **one route at a time** may run.

---

## 8. Custom values

Normally a **Direct control** input (Toggle or Absolute) only says "straight" or "curved" to its output. With **Custom values**
you decide exactly what it sends for each of the two states, just like a route step. Tick **Custom values**
under *Controls* and fill in the two rows:

- **Off / curved:** e.g. servo to 45°, signal red, sound off.
- **On / straight:** e.g. servo to 135°, signal green, sound track 7.

Handy for a servo that should go to other angles than its normal end positions, for a signal, and for
sound (chapter 10).

---

## 9. Analog inputs: sensors and many buttons on one pin

The pins A0–A6 can measure a **voltage** (0–1023) instead of only "pressed / not pressed". Set
**Read as** to one of:

| Read as | Counts as "pressed" when … | Good for |
|---|---|---|
| **Analog – below a value** | the value is below your number | a light sensor that gets covered by a train |
| **Analog – above a value** | the value is above your number | a light sensor in daylight, a current sensor |
| **Analog – at a value** | the value is within ±10 of your number | **many buttons on one pin** (resistor ladder) |

After "pressed" the decoder does exactly what it would do for a normal button: toggle, absolute, start a
route …

- **Live reading** shows what the pin measures right now. Press your button or cover your sensor, read the
  number, and click **Use this value**. Always do this: real resistors and sensors never match the
  calculation exactly.
- To stop it from switching on and off on the edge, "below" and "above" switch back only 20 points later.
- **Several inputs may use the same analog pin.** That is how a resistor ladder works: each button has
  its own value. See [examples.md](examples.md#7-many-buttons-on-one-pin-resistor-ladder).
- The number is rounded to steps of 8.
- In **Simulate** you can type a value for an analog pin to test your setup without hardware.

---

## 10. Sound (DFPlayer Mini)

**Wiring:** DFPlayer RX ← **Nano TX (D1)** through a 1 kΩ resistor, VCC → 5 V, GND → GND, a small speaker
on SPK1/SPK2.

**SD card:** name the files `0001.mp3`, `0002.mp3`, … The number is the track number.

**In the program:** add one output of type **Sound effect**. It has no settings: the thing that starts the
sound chooses the track:

- a **route step** with the track number;
- a **Direct control** input (Toggle or Absolute) with **Custom values** (e.g. off = stop, on = 3). These are
  ticked by themselves when you link an input to a sound output;
- a **DCC extended** command with the track number.

Track **0 = stop**. Without custom values: on = track 1, off = stop.

> **Important:** D1 is also the USB connection. **Sound only plays when no PC is connected.** With the
> program connected, the output shows "♪ 3" instead, so you can still check that the right track would play.

---

## 10a. Automatic block signals

A **Block signal** output sets its own aspect from track occupancy, without DCC or a route. You choose:

- **Own block:** the detector input of the block behind the signal;
- **Next block:** the detector input of the block after that, or *none*;
- three **aspects** (any combination of lamps, also flashing at 1 Hz): *own block occupied*,
  *next block occupied* and *both free*.

The decoder looks at the detectors every 50 ms, so the signals are right again straight after power-up.
Inputs, routes and DCC cannot set a block signal.

| Signal | Own block | Next block | Occupied | Next occupied | Free |
|---|---|---|---|---|---|
| Dutch 3-aspect, linked blocks | its block | the next block | red | yellow | green |
| Main signal, blocks not linked | its block | none | red | – | green |
| Distant signal (own output) | the block of the main signal ahead | none | yellow | – | green |

One detector may drive several signals (a main signal and its distant signal).

**Detectors:** use inputs with function **Sensor (occupancy)**: occupied = the pin is pulled to GND (or the
analog condition is true). Any input in use counts as a detector, so an **Absolute** input can be a detector
*and* switch an output, e.g. a panel LED or the line to the next board.

**Semaphore signals:** a servo can follow a detector (servo settings, *Follows a detector*): occupied =
curved (stop), free = straight (clear), or swapped.

**Linking boards:** the last signal of a board needs the first block of the next board. Use D0 (RX) with an
optocoupler as a Sensor input for "next board's first block", and D1 (TX) as a *Single steady* output driven
by an Absolute input on your own first detector. D0/D1 only work when no PC is connected (5 s after
power-up without the program); in the simulator they always work.

---

## 11. Setting servos at the layout

### With your phone

*Board → Phone remote…* shows a QR code. Scan it with a phone on the **same Wi-Fi** as the PC. You get a
page with all servos: slide **curved** or **straight** and the servo moves along. The position is saved at
once, and the slider in the program moves along too.

### With the config button and your throttle (no PC needed)

1. **Hold the button on A7 for 2 seconds.** The LED on the Nano blinks: config mode.
2. **Short press:** the next servo is selected and moves, so you see which one it is.
3. On your throttle, select **loco address 9999**:
   - the **speed knob** moves the servo;
   - **F1** saves the curved position, **F2** the straight position;
   - **F3** moves the servo over, **F4** inverts its frog relays (for relay modules that switch on LOW),
     **F5** selects the next servo.
4. **Hold the button 2 seconds** again to stop.

---

## 12. Saving your work

- The decoder remembers everything itself, also after switching off.
- **Save** (Ctrl+S) stores your setup in a file on your PC, including the **names** you gave the inputs and
  outputs (the names are not stored on the board). **Open** is Ctrl+O.
- When you close the program with unsaved changes, it asks what to do.
- **Write all** puts everything that is on screen on the board, e.g. after working offline.

---

## 13. When something doesn't work

| Problem | Try this |
|---|---|
| The port is not in the list | Use a data USB cable (some cables only charge). Click the refresh button. A CH340 clone may need the CH340 driver. |
| "Port busy" / can't connect | Close other programs that use the port (Arduino IDE, serial monitor). |
| Servos twitch or the Nano restarts | Give the servos their own 5 V supply and connect its GND to the Nano's GND. |
| A coil turnout doesn't throw | Make the pulse longer (start at 200 ms); check the coil supply and the driver. |
| No sound | Sound only plays without the PC connected. Check the file names (`0001.mp3`) and the 1 kΩ resistor. |
| A button on a resistor ladder triggers the wrong input | Use **Live reading** and **Use this value** for every button; keep at least ~40 between the values. |
| The program warns that the firmware is too old | *Board → Flash firmware…* puts the newest version on the board. |

---

## 14. For developers

| Folder | Contents |
|---|---|
| `software/GPIOcore/` | Arduino firmware (`GPIOcore.ino`) |
| `software/gui/` | Qt 6 configuration program (`CMakeLists.txt`, open in Qt Creator) |
| `release/` | build output for end users (not in git) |
| `docs/` | screenshots, presentation |

From the repository root:

| Command | What it does |
|---|---|
| `python build.py` | empties `release/` and builds everything into it: firmware (`GPIOcore.hex`) and the program with everything it needs |
| `python release.py` | asks the version and release notes, puts the version in the firmware and the program, builds, then commits, tags, pushes and publishes a GitHub release with `release/` as a zip (`--dry-run` to try, `--no-build` to publish `release/` as it is) |
| `python software/GPIOcore/src/build.py` | firmware only, straight to `release/GPIOcore.hex` |
| `python software/GPIOcore/src/upload.py [COM9] [--build]` | puts `release/GPIOcore.hex` on a Nano |
| `python software/gui/build.py` | the program only, into `release/` |

Screenshots: `GPIOcoreGUI.exe --lang en --size 1600x900 --open examples/routes-demo.json --demo --tab outputs --select 0 --screenshot out.png`
(`--tab inputs|outputs|pins`, `--theme light|dark`).

Technical documents: [PROTOCOL.md](PROTOCOL.md) (serial protocol), [EEPROM.md](EEPROM.md) (memory map),
[REVIEW_GPIOcore.md](REVIEW_GPIOcore.md) (review and change log).

## License

CC0 1.0: you may copy, change and use it, also commercially, without asking.
