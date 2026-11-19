@echo off
setlocal
rem Burn the Arduino Nano bootloader (Optiboot) on an ATmega328P through its ICSP pins.
rem For boards you build yourself: afterwards the board takes firmware over its serial / USB port
rem like a Nano (GPIOcore: Board > Flash firmware..., "Arduino Uno / Nano (new bootloader)").
rem
rem   burn-bootloader.bat COM5          programmer: an Arduino running ArduinoISP, on COM5
rem   burn-bootloader.bat usbasp        programmer: a USBasp
rem   burn-bootloader.bat COM5 all      bootloader and the GPIOcore firmware in one go
rem
rem Fuses as the Arduino IDE sets them for a Nano (ATmega328P, 16 MHz crystal):
rem   lfuse 0xFF (external crystal), hfuse 0xDA (512-word boot section, start in the bootloader),
rem   efuse 0xFD (brown-out at 2.7 V); lock 0x0F afterwards (the bootloader cannot overwrite itself).
rem Everything on the chip is erased, the settings in EEPROM too.

set "HERE=%~dp0"
set "AVRDUDE=%HERE%..\tools\avrdude\avrdude.exe"
set "CONF=%HERE%..\tools\avrdude\avrdude.conf"
set "HEX=%HERE%optiboot_atmega328.hex"
set "WHAT=the bootloader"
if /i "%~2"=="all" set "HEX=%HERE%GPIOcore_with_bootloader.hex"
if /i "%~2"=="all" set "WHAT=the bootloader and the GPIOcore firmware"

if "%~1"=="" goto usage
rem programmer; a new chip runs at 1 MHz until its fuses are set, so a USBasp talks slowly (-B 10) in step 1
set "PROG=-c stk500v1 -P %~1 -b 19200"
set "SLOW="
if /i "%~1"=="usbasp" set "PROG=-c usbasp"
if /i "%~1"=="usbasp" set "SLOW=-B 10"
if not exist "%AVRDUDE%" goto noavrdude
if not exist "%HEX%" goto nohex

echo.
echo Step 1/2: erase the chip and set the fuses
"%AVRDUDE%" "-C%CONF%" -p atmega328p %PROG% %SLOW% -e -U lock:w:0x3F:m -U efuse:w:0xFD:m -U hfuse:w:0xDA:m -U lfuse:w:0xFF:m
if errorlevel 1 goto failed

echo.
echo Step 2/2: write %WHAT% and lock the bootloader
"%AVRDUDE%" "-C%CONF%" -p atmega328p %PROG% -U "flash:w:%HEX%:i" -U lock:w:0x0F:m
if errorlevel 1 goto failed

echo.
echo Done. The board now has the Nano bootloader: update it over its serial / USB port.
exit /b 0

:failed
echo.
echo FAILED. Check the six ICSP wires (MISO, MOSI, SCK, RESET, 5 V, GND), that the board has power,
echo and the programmer: its COM port, or for ArduinoISP a 10 uF capacitor between RESET and GND
echo of the programmer Arduino.
exit /b 1

:noavrdude
echo avrdude not found next to this folder: tools\avrdude\avrdude.exe
exit /b 1

:nohex
echo Hex file not found: "%HEX%"
exit /b 1

:usage
echo Burns the Arduino Nano bootloader on an ATmega328P through its ICSP pins.
echo.
echo   %~nx0 COM5          programmer: an Arduino running ArduinoISP, on COM5
echo   %~nx0 usbasp        programmer: a USBasp
echo   %~nx0 COM5 all      bootloader and the GPIOcore firmware in one go
exit /b 1
