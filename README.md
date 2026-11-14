# GPIOcore

Arduino-based DCC accessory decoder: 16 inputs, 16 outputs, configured over USB with a Qt GUI.

| Folder | Contents |
|---|---|
| `hardware/` | PCB and schematics (empty for now) |
| `software/GPIOcore/` | Arduino firmware (`GPIOcore.ino`) |
| `software/gui/` | Qt configuration tool (`CMakeLists.txt`, open in Qt Creator) |
| `release/` | Built firmware (`GPIOcore.hex`) and GUI releases |

## Build and release

Run from anywhere (Python 3; also as VS Code tasks in `software/GPIOcore`):

| Command | What it does |
|---|---|
| `python software/GPIOcore/src/build.py` | Compiles the firmware (arduino-cli) straight to `release/GPIOcore.hex`; no `build/` folder is left |
| `python software/GPIOcore/src/upload.py [COM9] [--build]` | Puts `release/GPIOcore.hex` on a Nano with `software/gui/tools/avrdude` (port: the only USB serial port, or a choice) |
| `python software/gui/release.py [--gui]` | Firmware + GUI (Release build, Qt from `C:/Qt`) with Qt files, avrdude and examples into `release/` |

Only `release/GPIOcore.hex` is tracked in git; the GUI files in `release/` (about 45 MB) are not:
zip the folder for a GitHub release.

Documentation: `PROTOCOL.md` (serial protocol), `EEPROM.md` (memory use), `REVIEW_GPIOcore.md` (review and change log).
