# GPIOcore Configurator (Qt)

A desktop GUI to configure a GPIOcore board (Arduino Uno/Nano) over the serial port: outputs,
inputs, routes and presets. It also has a pin map with conflict checking and a serial log.

## Opening and building

**Qt Creator**: *File → Open File or Project* → `CMakeLists.txt` → kit *Desktop Qt 6.10.1 MinGW 64-bit*
→ Run.

**Command line** (Git Bash):

```bash
export PATH="/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:/c/Qt/6.10.1/mingw_64/bin:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.10.1/mingw_64 && cmake --build build && (cd build && ctest)
```

Required Qt modules: Widgets, SerialPort, Test. The code is also written for Qt 5.12+, but has not
been tested with it yet.

## Usage

1. Choose the port and click **Connect**. The GUI waits for the boot message and then logs in with
   `HELLO` (see `../../PROTOCOL.md`). A v2 decoder answers with what it can do; the current firmware
   does not answer, and then the GUI works in v1 mode. The status bar shows which of the two it is.
   After that the GUI reads all inputs, outputs and routes.
2. Tab **Configuration**: in the middle are the inputs (left) and outputs (right) with the connections.
   - **Click an input**: the input settings slide in from the **left**.
   - **Click an output**: the output settings slide in from the **right**.
   - One panel is open at a time; **Esc** or ✕ closes it.
   - **Drag an output onto an input** to link them.
   - Every linked input has its own colour. An output takes over the colour of the input that
     controls it; direct inputs take priority over routes. Extra dots show the other inputs.
   - **Direct control**: one output per input, *Toggle* or *Absolute*. Dragging replaces the output.
   - **Route**: every dragged output becomes the next step, up to 16 per route and within the
     pool of the board. The same output may occur more than once, for example a servo with intermediate positions.
     A route starts when the button is **pressed**, or on **every** DCC command for the address
     (straight and curved). There is nothing to choose there. Per step you set in the panel:
     - the value: curved/straight, an aspect (signal), an angle of 10°–170° (servo) or a PWM value;
     - the **wait time** in steps of 0.1 s (default 0.3 s, maximum 1638.3 s).

     There is a label on the wire, for example "1 S", "3 A2" or "5 45°".
   - Deleting: click a wire and press **Delete**, or use the right-click menu.
   - There can be only **one recorder**.
   - At least one step row is always open. For an empty route you choose an output there straight away.
     Delete a step with the **trash can** behind the row.
   - The **(i) buttons** explain everything in detail, for beginners: recorder, feedback,
     electrofrog/unifrog, servo, pins, …. *Help → Getting started* (F1) is a step-by-step guide for new
     users.
   - *View → Language* chooses the language. How to add a translation is described in `i18n/README.md`.
3. For an output:
   - **Frog**: choose *none*, *electrofrog* (2 relays) **or** *unifrog* (1 relay). They exclude each other.
   - **Servo**: switch *Live* on, and the servo moves along while you drag the slider. That needs a
     v2 decoder.
   - **4-aspect signal**: the aspect comes from DCC or from a route. The board remembers the last aspect in
     EEPROM, so there is no start aspect.
4. **Live saving.** Every change goes to the board by itself after 0.35 s and is read back.
   *Write changes* now only exists in the *Board* menu.
5. **Board** in the toolbar: Arduino Uno, Arduino Nano (A6/A7 as analog input), DCCnext (port numbers,
   no D13) or Jack of all trades. *Board settings…*: name, recorder on/off, wait time per step on/off
   (off = fixed wait time, half the memory space), route interlocking (off / on / automatic).
6. **Board files.** Every board has a name (on the board itself). The GUI keeps a file per board
   `Documents/GPIOcore/boards/<name>.json` with all settings plus the names of inputs and outputs.
   When connecting: read the name → load the file (names) → read everything from the board (the board wins).
   Offline: *File → New board* or *Open*; that file is readable JSON with an explanation in the field `about`, so that
   you can also fill it in by hand or let an AI do it.
7. **Simple / expert.** Simple by default: no bytes, no serial log, no protocol details.
   *View → Expert mode* brings them back. The presets are always on the second toolbar row. *View → Show unused inputs and outputs*
   also shows the unused blocks; otherwise you add them with **+ Add input / + Add output** and remove them
   with **Remove** in the panel.
8. **Flashing firmware:** *Board → Flash firmware…* (avrdude is in `tools/avrdude`). Default file:
   `release/GPIOcore.hex`.
   `examples/routes-demo.json` is an example with routes, a signal and servos.

With **Demo** you connect to a built-in simulation of a **v2 decoder** (16 steps, pool 256,
live servo). This way you can try the GUI without hardware.

Start options (handy for tests and documentation):

```text
GPIOcoreGUI --demo                      start connected to the simulator (protocol v2)
GPIOcoreGUI --port COM5                 connect at start
GPIOcoreGUI --open examples/routes-demo.json --demo
GPIOcoreGUI --demo --preset 2 --screenshot out.png --tab outputs --select 1   (tab: outputs, inputs, pins)
```

**Hardware convention for inputs:** the pins are set to `INPUT_PULLUP`, so a button or contact goes to
GND. Pressing gives a falling edge, releasing a rising edge.
- *Toggle* switches when pressed (falling edge).
- *Absolute* follows the level: pressed = 0.
- *Route* starts when pressed, or on every DCC command (direction 0 and 1).

## Structure

| File                       | Role                                                                                      |
|----------------------------|-------------------------------------------------------------------------------------------|
| `src/protocol.*`           | framing (opcode/length nibble, checksum, ACK/NACK without checksum)                       |
| `src/gpioconfig.*`         | output/input model, union fields (little endian), bitfield truncation, JSON, pins         |
| `src/connections.*`        | linking logic input → output(s): direct/route, steps, colour source per output            |
| `src/connectionview.*`     | the canvas: blocks, wires, dragging, Delete and the right-click menu                      |
| `src/infotexts.*`          | texts of the (i) buttons and the popup                                                    |
| `src/devicelink.*`         | request/response queue, timeouts, retries, draining of extra ACKs, legacy 0x2D detection  |
| `src/simulateddevice.*`    | firmware simulator (mirror of `Transceiver.cpp` + `messageParser.cpp`)                    |
| `src/mainwindow.*`         | main window, dirty tracking, warnings                                                     |
| `src/outputeditor.*`, `src/inputeditor.*`, `src/pinmapview.*` | editors and pin map                                    |
| `tests/`                   | unit tests (protocol, model, link ↔ simulator) and GUI tests                              |

See `../../REVIEW_GPIOcore.md` for the firmware findings. Since the fixes (2026-11-09) the firmware uses
`OPCODE_SET_OUTPUT_DATA` = 0x2E (14 bytes). Old firmware (0x2D, 13 bytes, `data[3]` is lost)
is recognised automatically; the GUI then shows a legacy message. Demo imitates the fixed firmware;
`SimulatedDevice::Options::oldFirmware()` imitates the old one (used in the tests).

## Updated 2026-11-13

- **Mode buttons** at the top: ● Configure (red frame, real board, live saving), ● Simulate (yellow, your own
  configuration on a simulated board, with input buttons), ● Live test (green, input buttons on a real board).
- **Panels** input on the left and output on the right are always in view (splitter, width is remembered).
- **Pins**: occupied pins are red in the list with who uses them. If you choose one, the previous
  user becomes *not in use*. D2, D13 (config LED) and A7 (config button) are never available.
- **Presets** are calculated by the GUI itself (*Board → Apply a preset*, Expert mode); they also work offline.
- **Routes**: *Starts on DCC* = any command / straight only / curved only.
- **Theme**: *View → Theme* (automatic, light, dark). Lamp colours of a signal in the board file.
- **Help**: *Help → Getting started*, *Configure, simulate and live test*, *Board settings*, *Config button*.
