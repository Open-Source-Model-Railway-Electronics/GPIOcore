# GPIOcore Configurator (Qt)

A desktop program to configure a GPIOcore board (Arduino Nano / Uno, or an ArCoMoRa DCCnext) for a model
railway: outputs, inputs, routes, board settings, with a canvas to link inputs to outputs, a pin map with
conflict checking, a simulator that runs the real firmware, a live test on the real board and a phone remote
for servos.

## Opening and building

**Qt Creator**: *File → Open File or Project* → `CMakeLists.txt` → kit *Desktop Qt 6.10.1 MinGW 64-bit* → Run.

**Command line** (Git Bash):

```bash
export PATH="/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:/c/Qt/6.10.1/mingw_64/bin:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.10.1/mingw_64 && cmake --build build && (cd build && ctest)
```

Required Qt modules: Widgets, SerialPort, Network, Test (LinguistTools for the translations). The build also
makes `gpiocore_fw_host`: the firmware in `../GPIOcore` compiled for the PC (with a small Arduino shim in
`fwhost/`). The simulator runs it, so *Simulate* and the tests use the real firmware.

`build.py` makes a release for end users in `<repo>/release/` (the root `build.py` also builds the firmware).

## The window

The program opens straight into the workspace with two tabs, **Configuration** and **Pin map**.

- **Configuration**: the input settings on the left, the canvas in the middle (inputs | wires | outputs), the
  output settings on the right; drag the splitter handles to resize. **+ Add input / + Add output** take the
  next free one; **Remove** in a panel sets it to *not in use*. Unused blocks are hidden unless
  *View → Show unused inputs and outputs* is on.
- **Drag an output onto an input** to link them. *Direct control* (toggle or absolute) drives one output; a
  *Route* gets a step per dragged output (up to 16 per route, within the board's step pool), each with a value
  (curved / straight, aspect, servo angle, PWM value, sound track) and, when the board setting is on, its own
  wait time. Every input has its own wire colour; an output takes the colour of the input that drives it.
- **Pin map**: one card per pin with what uses it; red = conflict. Click a card to edit its user.
- Every **(i)** button explains the thing next to it; *Help → Getting started* (F1) is a guide for beginners.

### Three modes (toolbar)

- **● Configure** (red frame when connected): change things, offline or on a connected board. Every change
  goes to the board by itself after 0.35 s and is read back. *Read board* loads the board, *Write all* puts
  the screen on it; connecting changes nothing on screen.
- **● Simulate** (yellow frame): your configuration on a simulated board (the real firmware). Press the
  buttons on the inputs and watch the outputs. An input with a digital pin is pressed on that pin (a push
  button goes low and high again, a switch holds ◀ low / ▶ high), so detectors that read the pin (block
  signals, semaphore arms) react too; inputs without a pin imitate a DCC command. Sensor inputs get an
  occupied / free button, analog inputs a voltage.
- **● Live test** (green frame): the same buttons on the real board. Connects by itself.

### User levels (View menu)

*Beginner* (servos, steady / pulse outputs, buttons, DCC addresses, simple routes), *Advanced* (+ light effects,
4-aspect signal, PWM, sound, frog relays, analog inputs, custom values, route start choice, D0 / D1, wait time
per step) and *Expert* (+ block signals, sensor inputs, semaphore arms). Features above the level are not shown.
Opening a file or an example, importing or reading a board that needs a higher level raises the level (with a
message); the level is never lowered by itself and never changes the configuration.

### Board types

*Arduino Nano* (A6 as analog input), *Arduino Uno* (no A6 / A7), *DCCnext* and *Jack of all trades*. On a DCCnext
the program uses the board's own port names **P1–P16** everywhere (pin lists show "P1 (D3)", the pin map shows P1
large with D3 small) and only offers those 16 ports. D2 (DCC) and D13 (config LED) are never offered; the config
button (A7) only exists on the Nano. A file with a pin the board type does not have keeps it, with a warning.

## Files (File menu)

- **Board files**: every board has a name (stored on the board); its file is
  `Documents/GPIOcore/boards/<name>.json` (readable JSON, explained in its `about` field). *Save* always asks
  for the file name; the previous version goes to `boards/backup/`. *Read board* takes the names of inputs and
  outputs from the board's file, the board wins for the settings.
- **New board (offline)…**: an empty board or a preset. **New board wizard…**: five short pages (board type;
  how many servo / coil / stall motor turnouts, lights and uncouplers; DCC addresses and buttons; pins in order or
  empty; summary) build a Beginner configuration and a **Next steps** list (a tick list above the canvas, kept in
  the board file as `nextSteps`; *File → Show next steps* reopens it).
- **Open example…**: the files in `examples/` (next to the exe, or in the source tree for a development build)
  with a short description. An example opens without a file name, so *Save* asks for a new one.
- **Export…**: the in-use outputs, inputs and / or board settings in a board file of their own (other slots
  *not in use*), e.g. to make your own example. **Import…**: adds the in-use items of the ticked parts of a board
  file, either at the **same numbers** (the items replaced are listed first) or **into free slots** (links between
  the imported items — input → output, route steps, custom values, block signal and semaphore arm detectors — are
  renumbered). A pin already in use goes to the imported item; your own item keeps its function, loses the pin,
  and is listed afterwards. The import is one edit: written live when connected, the file becomes unsaved.

## Board menu and more

*Connect / Disconnect*, *Read board*, *Write changes*, *Revert*, *Board settings…* (name, board type, wait time
per step, route interlocking), *Apply a preset…*, *Flash firmware…* (avrdude in `tools/avrdude`, default
`release/GPIOcore.hex`) and *Phone remote…* (a web page with a QR code to adjust servos from a phone on the same
Wi-Fi). *View → Theme* (automatic, light, dark) and *View → Language* (see `i18n/README.md`).

Start options (handy for tests and documentation):

```text
GPIOcoreGUI --demo                      start in Simulate
GPIOcoreGUI --port COM5                 connect at start
GPIOcoreGUI --open examples/routes-demo.json --demo
GPIOcoreGUI --demo --preset 2 --screenshot out.png --tab outputs --select 1   (tab: outputs, inputs, pins)
GPIOcoreGUI --size 1680x900 --theme dark --lang nl
```

**Inputs and pins:** the pins are `INPUT_PULLUP`, a button or contact goes to GND (pressed = low).
*Toggle* switches when pressed, *Absolute* follows the level, a *Route* starts when pressed or on a DCC
command (any, or straight / curved only).

## Structure

| File                         | Role                                                                                 |
|------------------------------|--------------------------------------------------------------------------------------|
| `src/protocol.*`             | framing (opcode / length nibble, checksum, ACK / NACK)                               |
| `src/gpioconfig.*`           | output / input / board model, bitfield widths, JSON, pins and pin names per board    |
| `src/connections.*`          | linking logic input → output(s): direct / route, steps                               |
| `src/boardtools.*`           | partial export / import (renumbering) and the configuration the wizard builds        |
| `src/devicelink.*`           | request / response queue, timeouts, retries, live writing                            |
| `src/simulateddevice.*`      | the simulator: starts `gpiocore_fw_host` (the real firmware) and talks to it          |
| `fwhost/`                    | Arduino shim and host program for the firmware on the PC                             |
| `src/mainwindow.*`           | main window, modes, files, dirty tracking, warnings, next steps                      |
| `src/connectionview.*`       | the canvas: blocks, wires, dragging, live buttons                                    |
| `src/outputeditor.*`, `src/inputeditor.*`, `src/pinmapview.*` | editors and pin map                             |
| `src/boardwizard.*`          | the new board wizard                                                                 |
| `src/uihelpers.*`            | style, theme, user levels, pin lists, aspect picker                                  |
| `src/infotexts.*`, `i18n/info/` | help texts behind the (i) buttons and the Help menu                               |
| `src/flasher.*`, `src/webremote.*`, `src/qrcode.*` | firmware flashing, phone remote                            |
| `tests/`                     | unit tests (protocol, model, link ↔ simulator) and GUI tests                          |
