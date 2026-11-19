# Bootloader for boards you build yourself

A bare ATmega328P (or a board without a bootloader) cannot take firmware over its serial / USB port.
Burn the **Arduino Nano bootloader** on it once through the ICSP pins; after that it behaves like a Nano:
update the firmware with the GPIOcore program (*Board > Flash firmware…*, "Arduino Uno / Nano (new bootloader)").

## What you need

- A programmer on the ICSP header of your board: an Arduino with the **ArduinoISP** sketch, or a **USBasp**.
- A 16 MHz crystal on your board (the fuses select it).

## Burning

In this folder (in the installed program: `C:\Program Files\GPIOcore\bootloader`):

```
burn-bootloader.bat COM5          ArduinoISP on COM5
burn-bootloader.bat usbasp        USBasp
burn-bootloader.bat COM5 all      bootloader and the GPIOcore firmware in one go
```

The chip is erased first (the settings in EEPROM too). The fuses are the Arduino IDE's for a Nano:
lfuse `0xFF`, hfuse `0xDA`, efuse `0xFD`, lock `0x0F` afterwards.

## Files

| File | What |
|---|---|
| `optiboot_atmega328.hex` | the bootloader of the Arduino Nano (ATmega328, "new bootloader", 115200 baud) |
| `GPIOcore_with_bootloader.hex` | the GPIOcore firmware with that bootloader (made by `src/build.py`) |
| `burn-bootloader.bat` | burns one of them with the avrdude in `..\tools\avrdude` |

## Licence of the bootloader

`optiboot_atmega328.hex` is **Optiboot** by Peter Knight, Bill Westfield and others, as shipped in the Arduino AVR
core 1.8.6 (`bootloaders/optiboot`). It is free software under the **GNU GPL version 2**; the source is at
<https://github.com/arduino/ArduinoCore-avr/tree/master/bootloaders/optiboot> and <https://github.com/Optiboot/optiboot>.
