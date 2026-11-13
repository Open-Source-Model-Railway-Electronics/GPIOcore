# GPIOcore Configurator (Qt)

Een desktop-GUI om een GPIOcore-board (Arduino Uno/Nano) via de seriële poort te configureren: outputs,
inputs, routes en presets. Daarnaast is er een pinmap met conflictcontrole en een serial log.

## Openen en bouwen

**Qt Creator**: *File → Open File or Project* → `CMakeLists.txt` → kit *Desktop Qt 6.10.1 MinGW 64-bit*
→ Run.

**Command line** (Git Bash):

```bash
export PATH="/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:/c/Qt/6.10.1/mingw_64/bin:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.10.1/mingw_64 && cmake --build build && (cd build && ctest)
```

Benodigde Qt-modules: Widgets, SerialPort, Test. De code is ook voor Qt 5.12+ geschreven, maar daarmee
nog niet getest.

## Gebruik

1. Kies de poort en klik **Connect**. De GUI wacht op het boot-bericht en meldt zich dan aan met
   `HELLO` (zie `../../PROTOCOL.md`). Een v2-decoder antwoordt met wat hij kan; de huidige firmware
   antwoordt niet, en dan werkt de GUI in v1-modus. De statusbalk toont welke van de twee het is.
   Daarna leest de GUI alle inputs, outputs en routes in.
2. Tab **Configuration**: in het midden de inputs (links) en outputs (rechts) met de verbindingen.
   - **Klik op een input**: de input-instellingen schuiven **links** in.
   - **Klik op een output**: de output-instellingen schuiven **rechts** in.
   - Er is steeds één paneel open; **Esc** of ✕ sluit het.
   - **Sleep een output op een input** om ze te koppelen.
   - Elke gekoppelde input heeft een eigen kleur. Een output neemt de kleur over van de input die hem
     bedient; directe inputs gaan daarbij voor routes. Extra stipjes tonen de andere inputs.
   - **Directe controle**: één output per input, *Toggle* of *Absoluut*. Slepen vervangt de output.
   - **Route**: elke gesleepte output wordt de volgende stap, tot maximaal 16 per route en binnen de
     pool van het board. Dezelfde output mag vaker voorkomen, bijvoorbeeld een servo met tussenstanden.
     Een route start bij het **indrukken** van de knop, of bij **elk** DCC-commando voor het adres
     (straight én curved). Daar valt niets te kiezen. Per stap stel je in het paneel in:
     - de waarde: curved/straight, een aspect (sein), een hoek van 10°–170° (servo) of een PWM-waarde;
     - de **wachttijd** in stappen van 0,1 s (standaard 0,3 s, maximaal 1638,3 s).

     Op de draad staat een label, bijvoorbeeld "1 S", "3 A2" of "5 45°".
   - Verwijderen: klik op een draad en druk op **Delete**, of gebruik het rechtsklikmenu.
   - Er kan maar **één recorder** zijn.
   - Er staat altijd minstens één stap-regel open. Bij een lege route kies je daar meteen een output.
     Verwijder een stap met de **prullenbak** achter de regel.
   - De **(i)-knopjes** leggen alles uitgebreid uit, voor beginners: recorder, feedback,
     electrofrog/unifrog, servo, pinnen, …. *Help → Getting started* (F1) is een stappenplan voor nieuwe
     gebruikers.
   - *View → Language* kiest de taal. Hoe je een vertaling toevoegt, staat in `i18n/README.md`.
3. Bij een output:
   - **Frog**: kies *geen*, *electrofrog* (2 relais) **of** *unifrog* (1 relais). Ze sluiten elkaar uit.
   - **Servo**: zet *Live* aan, dan beweegt de servo mee terwijl je de slider versleept. Dat vraagt een
     v2-decoder.
   - **4-lampsein**: het aspect komt van DCC of uit een route. Het board onthoudt het laatste aspect in
     EEPROM, dus er is geen start-aspect.
4. **Live opslaan.** Elke wijziging gaat na 0,35 s vanzelf naar het board en wordt teruggelezen.
   *Write changes* bestaat alleen nog in het menu *Board*.
5. **Board** in de toolbar: Arduino Uno, Arduino Nano (A6/A7 als analoge input), DCCnext (poortnummers,
   geen D13) of Jack of all trades. *Board settings…*: naam, recorder aan/uit, wachttijd per stap aan/uit
   (uit = vaste wachttijd, halve geheugenruimte), route-interlocking (uit / aan / automatisch).
6. **Board-bestanden.** Elk board heeft een naam (op het board zelf). De GUI houdt per board een bestand
   `Documenten/GPIOcore/boards/<naam>.json` bij met alle instellingen plus de namen van in- en outputs.
   Bij verbinden: naam lezen → bestand laden (namen) → alles van het board lezen (het board wint).
   Offline: *File → New board* of *Open*; dat bestand is leesbaar JSON met uitleg in het veld `about`, zodat
   je het ook met de hand of door een AI kunt laten invullen.
7. **Eenvoudig / expert.** Standaard eenvoudig: geen bytes, geen serial log, geen protocol-details, geen
   presets in de toolbar. *View → Expert mode* zet ze terug. *View → Show unused inputs and outputs*
   toont ook de ongebruikte blokken; anders voeg je ze toe met **+ Add input / + Add output** en haal je ze
   weg met **Remove** in het paneel.
8. **Firmware flashen:** *Board → Flash firmware…* (avrdude zit in `tools/avrdude`). Standaardbestand:
   `release/GPIOcore.hex`.
   `examples/routes-demo.json` is een voorbeeld met routes, een sein en servo's.

Met **Demo** verbind je met een ingebouwde simulatie van een **v2-decoder** (16 stappen, pool 256,
Live-servo). Zo kun je de GUI zonder hardware proberen.

Startopties (handig voor tests en documentatie):

```text
GPIOcoreGUI --demo                      start connected to the simulator (protocol v2)
GPIOcoreGUI --port COM5                 connect at start
GPIOcoreGUI --open examples/routes-demo.json --demo
GPIOcoreGUI --demo --preset 2 --screenshot out.png --tab outputs --select 1   (tab: outputs, inputs, pins)
```

**Hardware-afspraak voor inputs:** de pinnen staan op `INPUT_PULLUP`, dus een knop of contact gaat naar
GND. Indrukken geeft een dalende flank, loslaten een stijgende.
- *Toggle* schakelt bij het indrukken (dalende flank).
- *Absolute* volgt het niveau: ingedrukt = 0.
- *Route* start bij het indrukken, of bij elk DCC-commando (richting 0 en 1).

## Structuur

| Bestand                    | Rol                                                                                       |
|----------------------------|-------------------------------------------------------------------------------------------|
| `src/protocol.*`           | framing (opcode/lengte-nibble, checksum, ACK/NACK zonder checksum)                        |
| `src/gpioconfig.*`         | output/input-model, union-velden (little endian), bitfield-truncatie, JSON, pinnen        |
| `src/connections.*`        | koppel-logica input → output(s): direct/route, stappen, kleurbron per output              |
| `src/connectionview.*`     | het canvas: blokken, draden, slepen, Delete en het rechtsklikmenu                         |
| `src/infotexts.*`          | teksten van de (i)-knopjes en de popup                                                    |
| `src/devicelink.*`         | request/response-queue, timeouts, retries, drain van extra ACK's, legacy 0x2D-detectie    |
| `src/simulateddevice.*`    | firmware-simulator (spiegel van `Transceiver.cpp` + `messageParser.cpp`)                  |
| `src/mainwindow.*`         | hoofdvenster, dirty-tracking, waarschuwingen                                              |
| `src/outputeditor.*`, `src/inputeditor.*`, `src/pinmapview.*` | editors en pinmap                                      |
| `tests/`                   | unit tests (protocol, model, link ↔ simulator) en GUI-tests                               |

Zie `../../REVIEW_GPIOcore.md` voor de firmware-bevindingen. De firmware gebruikt sinds de fixes
(2026-11-09) `OPCODE_SET_OUTPUT_DATA` = 0x2E (14 bytes). Oude firmware (0x2D, 13 bytes, `data[3]` gaat
verloren) wordt automatisch herkend; de GUI toont dan een legacy-melding. Demo bootst de gefixte firmware
na; `SimulatedDevice::Options::oldFirmware()` bootst de oude na (gebruikt in de tests).

## Bijgewerkt 2026-11-13

- **Modusknoppen** bovenin: ● Configure (rood kader, echt board, live opslaan), ● Simulate (geel, je eigen
  configuratie op een gesimuleerd board, met input-knoppen), ● Live test (groen, input-knoppen op een echt board).
- **Panelen** input links en output rechts staan altijd in beeld (splitter, breedte wordt onthouden).
- **Pinnen**: bezette pinnen staan rood in de lijst met wie ze gebruikt. Kies je er een, dan wordt de vorige
  gebruiker *not in use*. D2, D13 (config-LED) en A7 (configknop) zijn nooit beschikbaar.
- **Presets** rekent de GUI zelf uit (*Board → Apply a preset*, Expert mode); ze werken ook offline.
- **Routes**: *Starts on DCC* = any command / straight only / curved only.
- **Thema**: *View → Theme* (automatisch, licht, donker). Lampkleuren van een sein in het board-bestand.
- **Help**: *Help → Getting started*, *Configure, simulate and live test*, *Board settings*, *Config button*.
