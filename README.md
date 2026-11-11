# GPIOcore

Arduino-based DCC accessory decoder: 16 inputs, 16 outputs, configured over USB with a Qt GUI.

| Folder | Contents |
|---|---|
| `hardware/` | PCB and schematics (empty for now) |
| `software/GPIOcore/` | Arduino firmware (`GPIOcore.ino`) |
| `software/gui/` | Qt configuration tool (`CMakeLists.txt`, open in Qt Creator) |
| `release/` | Built firmware (`GPIOcore.hex`) and GUI releases |

Documentation: `PROTOCOL.md` (serial protocol), `EEPROM.md` (memory use), `REVIEW_GPIOcore.md` (review and change log).
