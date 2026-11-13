# Review GPIOcore: firmware, Python GUI en nieuwe Qt GUI

Datum: 2026-11-04 · Scope: de hele map `GPIOcore/` (firmware, `GUI_Python/`, `GUI/untitled/`)

## Samenvatting

- **Firmware**: de basis is goed (state machines, EEPROM-model, compact protocol), maar er zitten een paar
  bugs in die op echte hardware direct pijn doen:
  - pulse-uitgangen schakelen nooit uit (spoel blijft bekrachtigd, **F9**);
  - de defaults en presets zetten uitgangen op D0/D1 (Serial) en D2 (DCC-ingang, **F7**);
  - elk DCC-commando zet `servoMin` op 0 (**F8**);
  - de output-opcode kondigt 13 bytes aan, maar de payload is 14 bytes (**F1**).
- **Python GUI**: werkt niet met de huidige firmware. Hij crasht bij het eerste bericht, ontvangt niets
  (de receive-loop wordt maar één keer aangeroepen) en gebruikt een oude payload-layout. Zie **P1–P9**.
- **Qt GUI**: nieuw project `software/gui/`. Het werkt met de huidige firmware (inclusief de
  protocol-eigenaardigheden hieronder) en ook met een gefixte firmware (0x2E). Er zijn 19 automatische
  tests, die draaien tegen een firmware-simulator. Nog **niet getest op echte hardware**.

Ernst: **H** = kapot/gevaarlijk op hardware, **M** = fout gedrag of datacorruptie, **L** = netheid/robustheid.

## Status 2026-11-09: fixes toegepast

De firmware compileert voor `arduino:avr:nano` en `arduino:avr:uno` (20066 bytes flash, 1438 bytes RAM).
Vóór de fixes was dat 19554 / 1430. Er is nog **niet op hardware getest**. Een backup van de oude
bronbestanden staat buiten het project.

| ID      | Status | Wat er gedaan is |
|---------|--------|------------------|
| F1      | ✅ | `OPCODE_SET_OUTPUT_DATA` = 0x2E (14 bytes) |
| F2      | ✅ | `sendAck()` uit beide SET-handlers gehaald |
| F3      | ✅ | `if( index >= 16 ) return ;` in alle 4 data-handlers |
| F4      | ✅ | preset 4: `for( i=0; ...)` en `setupInputs(8)` toegevoegd |
| F5      | ✅ (anders dan voorgesteld) | broadcasts **niet** weggehaald, maar gelijkgetrokken met de GET-layout: pinC/pinD (= NA) erbij, switchTime little-endian. Presets resetten nu ook pinC/pinD. |
| F6      | ✅ | `sendMyVersion()` stuurt `version` uit `src/version.h` ("v0.0.0") |
| F7      | ✅ | defaults en presets beginnen bij D3 (16 outputs = D3..A4) |
| F8      | ✅ | `setVal()` alleen voor PWM en 4-lampsein |
| F9      | ✅ | `if( timer.update( 1 ) )` in de pulse-tak |
| F10     | ✅ | flank-detectie via `oldPos` (zie toelichting hieronder) |
| F11     | ✅ | single steady volgt de state; `exitState` (pinnen laag) alleen bij pulse-types |
| F12     | ✅ | `!= NA`-guards op relayPinC (pulse) en pinB (exit, flashing) |
| F13     | ✅ | `pinMode` voor relayPinC |
| F14     | ✅ | `return 0 ;` in `setVal()`/`setState()` |
| F15     | ✅ deels | deling door 0 bij servoSpeed 0 afgevangen; startpositie 0° en `servoPattern` ongewijzigd |
| F16     | ✅ | route-default 0xFF; `triggerOutputs()` accepteert alleen `outID < 16` |
| F17     | ✅ deels (11-10) | presets zetten ongebruikte inputs op `NOT_IN_USE`, in plaats van latching met output 15 |
| F18     | — | ongewijzigd (DCC-lockout blijft TODO) |
| F19–F21 | ✅ | include-hoofdletters, `uin8t`-typfout, `#pragma once` |
| F22     | — | `SET/GET_*_STATE` nog leeg |
| **F23** | ✅ nieuw | zie hieronder |

**F23 (H, nieuw gevonden tijdens het fixen): `TIMER_ON` werkt maar één keer.** `Timer::update()`
(`src/ST.cpp`) start pas opnieuw als `IN` een keer 0 is geweest. `turnOnRelays`, `turnOffRelays` en de
pulse-tak roepen alleen `update( 1 )` aan, dus vanaf de tweede keer komt `Q` nooit meer en blijft de
state machine hangen. Fix: `timer.update( 0 ) ;` in de entry van die drie states.

**Gedrag na F10, goed om te weten:**
- Een output (servo, steady, pulse, relais) doet alleen iets bij een **statewissel**.
- Schrijven via `SET_OUTPUT_DATA` (GUI "Write" of "Test state") voert de state altijd één keer uit,
  ook als hij niet veranderd is.
- Na het opstarten gaat elke output één keer naar de opgeslagen state. **Pulse-spoelen niet**, zodat
  er bij power-up geen 16 spoelen tegelijk vuren.

**Bestaande boards:** de nieuwe pin-defaults (F7) gelden alleen na een factory reset (DEADBEEF ontbreekt
in EEPROM) of na een preset. Een board dat al geconfigureerd is, houdt zijn oude pinnen. Kijk na het
flashen in de GUI-pinmap of er nog iets op D0/D1/D2 staat.

**Qt GUI:** Demo bootst nu de gefixte firmware na. Oude firmware wordt nog steeds herkend
(legacy-melding, plus een pulse-waarschuwing alleen bij oude firmware). Er zijn 19 tests (17 core +
2 GUI), allemaal groen. Daaronder vallen nieuwe tests voor de gefixte firmware (pinnen vanaf D3,
route-default, preset-broadcast-layout, preset 4).

## Update 2026-11-11 (2): route-activatie vereenvoudigd, help en vertalingen

- **Route-activatie heeft geen keuze meer.** Een knop start de route bij het indrukken (falling edge), een
  DCC-adres bij elk commando (richting 0 en 1). `ROUTE_FALLING` (6) is uit `config.h` en `input.cpp`
  gehaald. De firmware compileert (20162 bytes).
- **GUI-stappenlijst:**
  - er staat altijd minstens één regel open; bij een lege route kies je daar de eerste output;
  - verwijderen gaat met een prullenbak-knop per regel;
  - bugfix: rij-knoppen werden verwijderd terwijl hun eigen klik nog liep (`delete` → `deleteLater`).
- **Help:** 17 uitgebreide uitlegteksten voor beginners in `software/gui/i18n/info/en/*.html`, plus
  *Help → Getting started*.
- **Vertalingen:**
  - schermteksten via Qt Linguist (`i18n/gpiocore_<taal>.ts`; `gpiocore_nl.ts` aangemaakt, 269 teksten,
    nog onvertaald);
  - uitlegteksten als HTML per taal, met terugval op Engels;
  - een *View → Language*-menu.

  Uitleg voor vertalers: `software/gui/i18n/README.md`.
- Er zijn 39 tests (31 core + 8 GUI), allemaal groen.

## Update 2026-11-11: protocol v2 (GUI klaar, decoder nog te doen)

Zie **[PROTOCOL.md](PROTOCOL.md)** voor:
- **`HELLO`**: aanmelden met capabilities, plus een 5 s-bootvenster waarna D0/D1 vrij komen;
- **routes**: 16 stappen van 4 bytes (output, waarde, raw-vlag, 14-bit wachttijd in 0,1 s);
- **`SERVO_LIVE`**;
- het **EEPROM-budget**: 16 × 16 stappen past niet in de interne 1 KB, dus de decoder meldt zijn
  grenzen in `HELLO`;
- de DCCnext-pinnen.

De GUI en de simulator implementeren v2. Met de huidige firmware valt de GUI terug op v1: routes van
8 stappen, alleen de stand, 0,5 s per stap. De firmware zelf is nog **niet** aangepast; dat staat als
takenlijst in §9 van PROTOCOL.md.

GUI-wijzigingen:
- input-instellingen schuiven links in, output-instellingen rechts;
- (i)-uitleg overal;
- frog-interlocking: electrofrog **of** unifrog;
- het servo-"pattern" (ongebruikt) en het start-aspect van het sein zijn weg;
- Live-servo;
- maximaal één recorder;
- DCCnext-profiel.

Er zijn 38 tests (31 core + 7 GUI), allemaal groen.

## Update 2026-11-10: input-gedrag en nieuwe configuratieweergave

**F24 (H, input-gedrag), toegepast in `input.cpp` en `config.h`:**
- **Pull-up.** Input-pinnen kregen nergens een `pinMode` en zweefden dus. Ze staan nu op `INPUT_PULLUP`,
  in `loadEEPROM()` en `set()`. Een knop of contact gaat naar GND; indrukken = falling edge.
- **Toggle.** `MOMENTARY` reageerde op `toggled()`, dus op **beide** flanken: per druk twee keer
  wisselen, wat netto hetzelfde is als absoluut. Toggle wisselt nu alleen op de falling edge
  (= indrukken).
- **Toggle via DCC.** `setDccAddress()` deed eerst `state = direction` en daarna `^= 1`. Dat is geen
  toggle maar "omgekeerde richting". De regel `state = direction` is weg; `LATCHING` zet zelf de
  richting.
- **Wisselstraat-flank.** Een route startte op elke flank. Nu kies je per input:
  - `ROUTE` (2) = rising edge, of DCC-richting 1;
  - nieuw `ROUTE_FALLING` (6) = falling edge, of DCC-richting 0.

  Het 3-bit typeveld heeft daar ruimte voor.
- **Callback alleen voor directe inputs.** `callbackInput()` wordt alleen nog aangeroepen voor
  `MOMENTARY`/`LATCHING`, ook bij DCC. Voorheen kon een recorder- of feedback-input met een
  overeenkomend DCC-adres per ongeluk een output schakelen.
- De firmware compileert (20200 bytes flash). Op hardware is het nog niet getest.

**Let op bij *absoluut*:** het niveau wordt rauw gelezen, dus ingedrukt/gesloten contact = state 0.
Moet dat 1 worden, dan is `trigger.update( !digitalRead( eedata.pin ) )` de minimale wijziging. Daarmee
draaien de flanken van toggle en route wel mee om.

**Qt GUI:** de tabs *Outputs* en *Inputs* zijn vervangen door één **Configuration**-weergave. Links
staan de inputs, rechts de outputs, ertussen de draden, en de instellingen staan in een zijbalk rechts.
- Koppelen doe je door een output op een input te slepen.
- Elke gekoppelde input heeft een unieke kleur, en een output neemt die kleur over.
- Directe controle (toggle/absoluut) en route (rising/falling, maximaal 8 stappen met state) sluiten
  elkaar uit.

Er zijn 25 tests (21 core + 4 GUI, waaronder echte sleep-tests), allemaal groen.

---

## 1. Firmware: protocol (`messageParser.*`, `src/Transceiver.*`)

### F1 (H): `OPCODE_SET_OUTPUT_DATA` kondigt 13 bytes aan, de payload is er 14

`messageParser.h:10`. De lage nibble van de opcode is de lengte. De payload is
`index + pinA..pinD + relayA..C + type + state + data[4]` = **14** bytes, maar 0x2D zegt 13.

Gevolgen:
- bij GET komt `data[3]` niet mee (servoPattern, hoge byte van pauseTime/switchTime);
- bij SET leest de firmware `payload[13]` toch, en die byte komt dan uit de buffer van het **vorige**
  bericht. Een swing-light kan daardoor een pauze van tientallen seconden krijgen.

```cpp
// was
const int OPCODE_SET_OUTPUT_DATA   = 0x2D ;
// wordt
const int OPCODE_SET_OUTPUT_DATA   = 0x2E ;   // 14 payload bytes
```

De Qt GUI herkent automatisch welke variant de firmware gebruikt: het antwoord op `GET_OUTPUT_DATA` is
0x2D of 0x2E. In legacy-modus toont de GUI een waarschuwing.

### F2 (M): dubbele ACK na elke SET

`messageParser.cpp:102` en `:153` sturen `transceiver.sendAck()`. Daarna stuurt `Transceiver::receive()`
(`src/Transceiver.cpp:116`) nóg een ACK. Een host die "één ACK per request" verwacht, raakt daardoor uit
sync. De Qt GUI vangt dit op met een korte drain-periode na elke transactie.

```cpp
// was (both SET handlers)
    transceiver.sendAck() ;
// wordt
    // ACK is sent by Transceiver::receive()
```

### F3 (H): geen bounds-check op `index`

`messageParser.cpp:82, 107, 131, 160`: `input[index]` en `output[index]` worden gebruikt zonder te checken
of `index < 16`. Eén verkeerd byte op de seriële lijn levert dan een schrijfactie buiten de array op
(RAM-corruptie).

```cpp
// add after each 'uint8_t index = msg->payload[0] ;'
    if( index >= 16 ) return ;
```

### F4 (H): preset 4 gebruikt een niet-geïnitialiseerde `i`

`messageParser.cpp:325`: `for( int i=0; ...)` declareert een nieuwe `i` die de buitenste `uint8_t i`
verbergt. Daardoor krijgt `clearUnusedOutputs( i )` (`:436`) een willekeurige waarde mee. Ook ontbreekt
`setupInputs(8)` in dit geval.

```cpp
// was
        for( int i=0; i<8; i++ )
// wordt
        for( i=0; i<8; i++ )
```

En na de loop, vóór `break ;`: `setupInputs(8) ;`

### F5 (M): preset-broadcasts gebruiken de oude layout, en pinC/pinD worden niet gereset

De presets (`messageParser.cpp:218–433`) en `clearUnusedOutputs()` sturen 12 bytes in de **oude** volgorde:
`pinA pinB relayA relayB relayC type ...`, dus zonder pinC/pinD. Dat past niet op de layout die
`GET_OUTPUT_DATA` gebruikt. Daarnaast laten ze `pinC`/`pinD` staan op wat er eerder stond.

Voorstel: in de presets ook `pinC`/`pinD` op `0xFF` zetten, en de broadcasts weghalen. Een host leest na
de ACK alles opnieuw in; de Qt GUI doet dat al en negeert de broadcasts.

### F6 (L): versie is een placeholder

`messageParser.cpp:448`: er wordt `"12345678"` gestuurd, terwijl `src/version.h` `"v0.0.0"` bevat (en
ongebruikt is). Voorstel: `version` versturen.

### Protocol-notities voor host-implementaties

Deze punten zijn geen bugs, maar wel valkuilen voor elke host-implementatie. De Python GUI liep er
allemaal in.

- `ACK` (0x70) is 1 byte en `NACK` (0x71 + reden) is 2 bytes, allebei **zonder** checksum.
- Een host→device-bericht met lengte 0 (bijv. `REQ_VERSION` 0x50) wordt meteen uitgevoerd. De host mag
  er **geen** checksum achter sturen, anders telt dat byte als een nieuw commando.
- `REQ_VERSION` krijgt geen ACK, alleen het versiebericht (0x5n).
- Elke GET krijgt eerst het data-antwoord en daarna nog een ACK.

---

## 2. Firmware: outputs (`output.*`, `GPIOcore.ino`)

### F7 (H): defaults en presets beginnen bij pin 0 (D0/D1 = Serial, D2 = DCC-ingang)

- `output.cpp:49` zet `eedata.pinA = index ++` → output 0..15 = D0..A1.
- `messageParser.cpp:213` zet `uint8_t pinIndex = 0;`.
- Na een factory reset doet `loadEEPROM()` daardoor `pinMode( 2, OUTPUT )` op de **DCC-ingang**, en dan
  komt er geen DCC meer binnen. Output 0/1 doen niets (UART heeft voorrang).

D3..A4 zijn precies 16 vrije pinnen (preset 2 en 6 gebruiken er 15/16):

```cpp
// output.cpp:49  was
    eedata.pinA = index ++  ;
// wordt
    eedata.pinA = 3 + index ++  ;   // skip D0/D1 (Serial) and D2 (DCC)

// messageParser.cpp:213  was
    uint8_t pinIndex = 0;
// wordt
    uint8_t pinIndex = 3;
```

De Qt GUI markeert D0/D1/D2 als gereserveerd, kleurt conflicten rood (tabel + pinmap) en geeft een
waarschuwing in de editor.

### F8 (H): elk DCC/knop-commando overschrijft `data[0]`

`GPIOcore.ino:121`: `callbackInput()` roept altijd `output[index].setVal( val )` aan. Voor gewone
wisselcommando's is `val` 0. `pwmVal` deelt een union met `servoMin`, de lage byte van `switchTime` en
`swing.pulseTime`. Elke bediening zet die dus op 0 en schrijft dat naar EEPROM.

```cpp
// was
    output[index].setVal(   val   ) ;
// wordt
    if( output[index].eedata.type == PWM || output[index].eedata.type == fourToneSignal ) output[index].setVal( val ) ;
```

### F9 (H): pulse-uitgangen schakelen nooit uit

`output.cpp:217`: in `moveActuator` wordt bij `singlePulse`/`doublePulse` alleen `timer.Q` gelezen.
`timer.update()` wordt nooit aangeroepen, dus `Q` blijft 0. Het pulse-einde komt er niet, en een
wisselspoel blijft bekrachtigd (risico op een verbrande spoel).

```cpp
// was
            if( timer.Q ) // pulse expired, -> exit
// wordt
            if( timer.update( 1 ) ) // pulse expired, -> exit
```

### F10 (H): de output-state machine draait continu, ook zonder statewissel

`output.cpp:476–478`: `idle` gaat bij elke `update()` meteen door naar `turnOffRelays`/`moveActuator`.
Gevolgen:
- de frog-relais gaan elke cyclus 100 ms uit en weer aan (klapperen);
- pulse-uitgangen pulsen eindeloos opnieuw (na fix F9);
- servo's worden steeds opnieuw ge-attached.

Er is een flank-detectie nodig. Het bestaande, ongebruikte lid `oldPos` (`output.h:127`) kan daarvoor
dienen:

```cpp
// output.cpp:476  was
            else if( eedata.relayPinA != NA
                 &&  eedata.relayPinB != NA )       { sm.nextState( turnOffRelays      ) ; }
            else {                                    sm.nextState( moveActuator        ) ; } }
// wordt
            else if( eedata.state == oldPos )       { }                                 // nothing to do
            else if( eedata.relayPinA != NA
                 &&  eedata.relayPinB != NA )       { oldPos = eedata.state ; sm.nextState( turnOffRelays ) ; }
            else {                                    oldPos = eedata.state ; sm.nextState( moveActuator  ) ; } }
```

Let op: `oldPos` start op 0. Een output die in EEPROM state 1 heeft, beweegt dus bij het opstarten; een
output met state 0 niet. Kies bewust wat je bij boot wilt.

### F11 (M): single steady volgt de state niet

`output.cpp:193` schrijft altijd `HIGH`. Daarna zet de `exitState` (`:232–236`) pinA/pinB weer `LOW`,
dus een steady-uitgang flitst alleen kort.

```cpp
// output.cpp:193  was
            digitalWrite( eedata.pinA, HIGH ) ;
// wordt
            digitalWrite( eedata.pinA, eedata.state ) ;
```

En de `exitState` alleen gebruiken voor de pulse-types (`singlePulse`, `doublePulse`).

### F12 (M): `digitalWrite()` op niet-toegewezen pinnen (NA = 63)

`output.cpp:219` (relayPinC), `:235` (pinB bij single-types) en `:299/:304` (pinB bij singleFlashing)
schrijven zonder `!= NA`-check. Op AVR leest `digitalWrite(63)` buiten de PROGMEM-pintabellen en zet
het willekeurige poortbits. Guard ze zoals op `:333`:

```cpp
// was
        digitalWrite( eedata.pinB, LOW ) ;
// wordt
        if( eedata.pinB != NA ) digitalWrite( eedata.pinB, LOW ) ;
```

### F13 (M): unifrog-relais krijgt geen `pinMode`

`output.cpp:73`: `loadEEPROM()` zet relayPinA/B als OUTPUT, maar relayPinC niet. Daardoor schakelt
`digitalWrite` alleen de pull-up.

```cpp
// add after output.cpp:73
    if( eedata.relayPinC != NA ) pinMode( eedata.relayPinC, OUTPUT ) ;
```

### F14 (M): `setVal()`/`setState()` hebben geen return

`output.cpp:21` en `:38` zijn `uint8_t`-functies zonder `return`; dat is undefined behaviour. Minimale
fix: `return 0 ;`. Kanttekening: elke statewissel schrijft EEPROM (één byte per toggle). Met ~100k
cycli is dat voor drukbereden wissels iets om in de gaten te houden.

### F15 (L): servo-details

- `output.cpp:95`: `500 / timer.PT` deelt door 0 bij `servoSpeed = 0`. De GUI dwingt ≥ 1 af.
- `pos` start op 0, dus de eerste beweging na boot zwaait vanaf 0°.
- `servoPattern` wordt opgeslagen, maar nergens gebruikt.

---

## 3. Firmware: inputs (`input.*`)

### F16 (M): standaard route-stappen wijzen naar output 63

`input.cpp:43` vult `linkedOutput[]` met `NA` (0x3F), maar het einde-teken in `triggerOutputs()` is 0x7F
(`:139`). Wordt een input op ROUTE gezet zonder de stappen in te vullen, dan volgt
`callbackInput( 63, ...)`, oftewel `output[63]` buiten de array.

```cpp
// input.cpp:43  was
        eedata.linkedOutput[i] = NA ;
// wordt
        eedata.linkedOutput[i] = 0xFF ;   // 0x7F in bits 6-0 = end of route

// input.cpp:139  was
        if( outID != 0x7F )
// wordt
        if( outID < 16 )
```

De Qt GUI toont zulke stappen als "⚠ output 63 (invalid)" en waarschuwt.

### F17 (L): `outputID` is 4 bits

`input.h:11`: "geen output" (0xFF in `setupInputs`) wordt 15. Dat is onschuldig zolang adres en pin ook
leeg zijn, maar het is goed om te weten.

### F18 (L): bekende TODO: herhaalde DCC-pakketten

Een DCC-centrale herhaalt accessory-pakketten. Bij MOMENTARY toggelt de output daardoor meerdere keren
(staat al als TODO in `GPIOcore.ino:161`).

---

## 4. Firmware: overig

| ID  | Ernst | Plek                     | Bevinding                                                                                                        |
|-----|-------|--------------------------|------------------------------------------------------------------------------------------------------------------|
| F19 | M     | `servoManager.cpp:1`     | `#include "ServoManager.h"`, maar het bestand heet `servoManager.h`. Op Linux/macOS faalt de build (hoofdlettergevoelig). |
| F20 | L     | `src/macros.h:36`        | Typfout `uin8t` in `toggleState` (macro ongebruikt).                                                             |
| F21 | L     | `messageParser.h`        | Geen `#pragma once`.                                                                                             |
| F22 | L     | `messageParser.cpp:187`  | `SET/GET_*_STATE` zijn leeg. De GUI test outputs daarom via `SET_OUTPUT_DATA`.                                   |
| —   | OK    | EEPROM                   | Outputs (~12 B) + inputs (~13 B) × 16 ≈ 400 B < 420 (recorder-start); DEADBEEF op 1000. Past, maar krap.         |

---

## 5. Python GUI (`GUI_Python/`)

| ID | Plek                      | Bevinding                                                                                                                           |
|----|---------------------------|-------------------------------------------------------------------------------------------------------------------------------------|
| P1 | `main.py:39`              | `Transceiver.OPCODE_SET_INPUT_DATA` e.a. bestaan niet in `Transceiver.py` → `AttributeError` bij het eerste bericht.                |
| P2 | `Transceiver.py:60`       | `time.ticks_ms()` / `ticks_diff()` zijn MicroPython; in CPython geeft dit een `AttributeError`.                                     |
| P3 | `main.py:254, 268`        | `periodic_receive()` wordt één keer aangeroepen en nooit opnieuw ingepland → er wordt nooit iets ontvangen.                         |
| P4 | `main.py:120–129`, `parser.py:44–58` | Oude output-layout (geen pinC/pinD). Typelijst wijkt af (Servo = 6, firmware = 11); type wordt altijd als 0 verstuurd, data altijd 0. |
| P5 | `parser.py:112`           | `switchTime` wordt big-endian gelezen; de firmware (AVR) is little-endian.                                                          |
| P6 | `parser.py:3–6`, `main.py:104` | Maar 3 van de 6 input-types.                                                                                                   |
| P7 | `main.py:55–65`, `Transceiver.py:112` | NACK-tak: `(str)`-concatenatie geeft een `TypeError`, `err_msg` kan ongedefinieerd zijn. Host stuurt bovendien zelf een ACK op elk ontvangen frame. |
| P8 | `status_icons.py:112`, `main.py:264` | `update_icons()` plant zichzelf bij elke aanroep opnieuw in en wordt ook elke 500 ms aangeroepen → het aantal timers groeit onbeperkt, plus een flood aan debug-prints. |
| P9 | `parser.py:85–116`        | Dynamische servo/PWM-velden worden opnieuw aangemaakt maar nergens bewaard → deze waarden kunnen nooit verstuurd worden.            |

Conclusie: de Python GUI is niet bruikbaar met de huidige firmware. De Qt GUI vervangt hem functioneel.

---

## 6. Nieuwe Qt GUI (`software/gui/`)

**Functies**
- Seriële verbinding (115200 8N1). Herkent Arduino/CH340/FTDI/CP210x-poorten automatisch en wacht op het
  boot-bericht, met `REQ_VERSION` als fallback.
- **Outputs**: alle 14 types. De editor past zich aan per type: de juiste pinnen, frog-relais,
  parameters met eenheden (bijv. "× 10 ms = 500 ms"), servo-sliders, een 4-lampsein per LED en een
  PWM-slider. Knoppen "Test state 0/1".
- **Inputs**: alle 6 types, DCC-adres en/of pin, doel-output en een route-editor (8 stappen + state).
- **Pin map**: D0..A5 met wie welke pin gebruikt. Conflicten en gereserveerde pinnen zijn rood, PWM-pinnen
  zijn gemarkeerd. Klik op een kaart om die output/input te openen.
- **Wijzigingen bijhouden**: gewijzigde rijen zijn vet met een "●". "Write N change(s)" schrijft alleen
  wat gewijzigd is en leest het daarna terug ter verificatie. Revert, en Open/Save als JSON (offline
  configureren).
- **Presets** 0–7 met bevestiging; daarna wordt het hele board netjes teruggelezen.
- **Serial log** met TX/RX-hex en ruwe payload per editor (handig bij firmware-debugging).
- **Demo-modus**: een ingebouwde firmware-simulator die het gedrag van de huidige firmware nabootst
  (inclusief F1, F2, F5 en de bitfield-truncatie).

**Bouwen**: open `software/gui/CMakeLists.txt` in Qt Creator (kit Qt 6.10.1 MinGW). Getest met
Qt 6.10.1 / MinGW 13.1. De code gebruikt geen Qt6-only API's en `find_package` valt terug op Qt5, maar
een Qt5-build heb ik **niet** kunnen testen (alleen Qt 6 staat op deze machine).

**Tests** (`ctest`): 17 unit tests (framing, model, link ↔ simulator: legacy/fixed, dubbele ACK, NACK-retry,
preset) en 2 GUI-tests (bewerken → schrijven → teruglezen → schoon; preset + conflict-markering). Alles
groen.

**Niet getest**: echte hardware. Eerste test-advies: Demo proberen, dan verbinden met een board, "Read
board" en de serial log bekijken. Gebruik de pulse-types (F9) en relais (F10) pas na de firmware-fixes.

`GUI/untitled/` is de lege Qt Creator-template; die heb ik laten staan en kan weg.

## Update 2026-11-12 / 13: firmware v2 op het board getest

Op een echte Nano ("Test decoder") getest met een script over USB en met de GUI. Gevonden en opgelost:

| Probleem | Ernst | Oplossing |
|---|---|---|
| **Outputs kwamen nooit uit `idle`**: `idleF()` gaf 0 terug en de `State()`-macro voert de state alleen uit bij ≠ 0. Er schakelde nooit een pin (al zo in de originele firmware) | H | `idleF()` geeft 1 |
| Sein: knipperen gebruikte `timer.Q` (één call hoog per 500 ms), lamp brandde µs | M | eigen fase-bit |
| Spoelwissels: puntstukrelais niet gezet na opstarten (electrofrog dood, unifrog mogelijk verkeerde polariteit) | H | relais bij `loadEEPROM()` |
| F15 servo startte op 0° | M | start op opgeslagen eindstand; servo's één voor één (500 ms) |
| F18 herhaalde DCC-pakketten flippen een toggle | M | 500 ms blokkeertijd per input (toggle en route) |
| Nieuwe output-pin pas na reset een output | M | `pinMode` in `Output::set()`, oude pinnen losgelaten |
| GUI stuurde een route eindeloos opnieuw (timing uit → wachttijd 0 teruggelezen) | M | vergelijk zonder wachttijd + nooit twee keer hetzelfde |
| GUI time-out bij route-schrijven (EEPROM-schuiven duurt seconden) | L | 4 s time-out |

Nieuw: OUTPUT_EVENT (0x45) met servo-graden en puntstukrelais, SET_INPUT_STATE (0x32), DCC_EXT (0x33),
routes op *straight only* / *curved only*, configknop A7 + LED D13 + inleren via loc 9999, presets naar de GUI,
board stil zonder GUI. Flash 89 %, RAM 80 %.
