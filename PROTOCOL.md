# GPIOcore seriële protocol — v1 en v2

Datum: 2026-11-11 (bijgewerkt) · Status: **v1** is de huidige firmware. **v2** is de specificatie voor de nieuwe decoder;
de Qt GUI en de simulator (`software/gui/src/simulateddevice.cpp`) implementeren v2 al.

De GUI werkt met beide versies. Na het openen van de poort stuurt hij `HELLO`:
- Een v2-decoder antwoordt met zijn mogelijkheden.
- De huidige firmware antwoordt niet; de GUI schakelt dan terug naar v1.

## 1. Framing (v1 en v2)

```text
<command> <payload ...> <checksum>
command  : high nibble = message group, low nibble = payload length (0..14)
           low nibble 0xF = extended length: payload[0] = length incl. itself (max 32)
checksum : XOR of command and all payload bytes
```

| Uitzondering | Bytes | Opmerking |
|---|---|---|
| `ACK` | `70` | 1 byte, geen checksum |
| `NACK` | `71 <reden>` | 2 bytes, geen checksum (1 = checksum, 2 = lengte, 3 = timeout) |
| Host → device met lengte 0 | `<cmd>` | Wordt direct uitgevoerd en krijgt **geen** checksum en geen ACK (`REQ_VERSION`, `HELLO`) |

Het protocol is binair met volle 8-bit bytes. Er is dus geen 7-bit beperking zoals bij LocoNet. De
"bit 7 = stand, bits 0–6 = id"-indeling hoort alleen bij de oude `linkedOutput[]`-bytes (v1).

Elk compleet frame met een geldige checksum krijgt na verwerking één `ACK` van `Transceiver::receive()`.
Een GET krijgt eerst het data-antwoord en daarna die `ACK`.

**Timing en grenzen** (`src/Transceiver.cpp`):
- **Byte-timeout.** Er mag hooguit **5 ms** tussen twee bytes van één frame zitten. Daarna antwoordt de
  firmware met `NACK 3` en begint hij opnieuw. Stuur een frame dus in één keer.
- **Maximale lengte.** Een payload is hooguit **32 bytes**; langer geeft `NACK 2`.
- **Opstarten.** Na een reset stuurt de firmware zelf het versiebericht. Openen van de poort reset een
  Uno of Nano via DTR. De bootloader en `setup()` samen duren ongeveer 1–2 s.
- **Index ≥ 16.** Een `index` van 16 of hoger wordt genegeerd, maar het frame krijgt wel een `ACK`. Een GET
  geeft dan geen data-antwoord.
- **Opslaan.** `SET_INPUT_DATA` en `SET_OUTPUT_DATA` gaan direct naar EEPROM. Een `SET_OUTPUT_DATA` voert de
  state één keer uit, ook als die niet veranderd is (zo werkt "Test state").

## 2. Opcodes

| Opcode | Richting | Payload | Versie |
|---|---|---|---|
| `1E` SET_INPUT_DATA | ↔ | `index addrH addrL pin type outputID linked[8]` (14 B), zie §2.2 | v1, v2* |
| `11` GET_INPUT_DATA | → | `index` → antwoord `1E` | v1, v2 |
| `2E` SET_OUTPUT_DATA | ↔ | `index pinA pinB pinC pinD relayA relayB relayC type state data[4]` (14 B), zie §2.1 | v1, v2 |
| `21` GET_OUTPUT_DATA | → | `index` → antwoord `2E` | v1, v2 |
| `50` REQ_VERSION | → | – → antwoord `5n` + versietekst (`src/version.h`, nu `v0.0.0`) | v1, v2 |
| `61` SET_PRESET | → | `preset` (0–7), zie §2.3 | v1, v2 |
| `32` SET_INPUT_STATE | → | `index state` — **gereserveerd**: de firmware antwoordt alleen met `ACK` | – |
| `31` GET_INPUT_STATE | → | `index` — **gereserveerd**, alleen `ACK` | – |
| `42` SET_OUTPUT_STATE | → | `index state` — **gereserveerd**, alleen `ACK` | – |
| `41` GET_OUTPUT_STATE | → | `index` — **gereserveerd**, alleen `ACK` | – |
| `B1` CLEAR_OUTPUT | – | staat in `messageParser.h`, maar wordt niet gebruikt | – |
| `2D` (oud) | ↔ | `SET_OUTPUT_DATA` van firmware vóór 2026-11-09: 13 bytes, `data[3]` gaat verloren. De GUI herkent dit aan het antwoord op `GET_OUTPUT_DATA` en schakelt mee. | oud |
| **`90` HELLO** | → | – → antwoord `95` | **v2** |
| **`82` GET_ROUTE** | → | `route first` → antwoord `8F` | **v2** |
| **`8F` SET_ROUTE** | ↔ | `len route total first step[n]` (n ≤ 7) | **v2** |
| **`A2` SERVO_LIVE** | → | `output angle` | **v2** |
| **`C0` GET_BOARD** | → | – → antwoord `CF` | **v2** |
| **`CF` SET_BOARD** | ↔ | `len type flags delay name[0..16]`, zie §10 | **v2** |

\* In v2 stuurt de GUI de 8 `linked`-bytes nog mee, afgeleid van de route: eerste 8 stappen, alleen de stand.
Een v2-decoder mag ze negeren; de echte route komt via `SET_ROUTE`.

### 2.1 Output-velden (`Settings` in `output.h`, 10 bytes in EEPROM)

| Veld | Bits | Waarden |
|---|---|---|
| `pinA`–`pinD`, `relayA`–`relayC` | 6 | 0–13 = D0–D13, 14–19 = A0–A5, **0x3F = niet gebruikt** (0xFF wordt 0x3F) |
| `type` | 4 | zie de tabel hieronder |
| `state` | 1 | 0 = curved / uit, 1 = straight / aan |
| `data[4]` | 32 | union; de betekenis hangt af van het type |

Frog-relais: `relayA` + `relayB` = **electrofrog** (A bij straight, B bij curved, eerst allebei uit);
`relayC` = **unifrog**. De GUI staat maar één van de twee toe.

| `type` | Naam | Pinnen | `data[4]` |
|---|---|---|---|
| 0 | single steady | A | – |
| 1 | single pulse | A (+ unifrog C) | `uint32` LE schakeltijd × 100 ms |
| 2 | single flashing | A | `uint32` LE halve periode × 10 ms |
| 3 | double steady | A, B | – |
| 4 | double pulse | A, B | `uint32` LE pulstijd × 10 ms |
| 5 | double flashing | A, B | `uint32` LE halve periode × 10 ms |
| 6 | swing light | A, B | `data[0..1]` aan-tijd, `data[2..3]` pauze; `uint16` LE × 10 ms |
| 7 | fireplace | A, B (PWM) | – |
| 8 | TL tube | A (PWM) | – |
| 9 | welder | A | – |
| 10 | 4-aspect signal | A–D | `data[0]` = aspect: bits 0–3 aan, bits 4–7 knipperen (gezet door DCC-ext of een route; de GUI laat hem staan) |
| 11 | servo | A (+ relais) | `data[0]` hoek bij state 0, `data[1]` hoek bij state 1, `data[2]` snelheid in ms/° (≥ 1), `data[3]` ongebruikt |
| 12 | PWM | A (PWM) | `data[0]` = 0–255 |
| 13 | not in use | – | – |

PWM-pinnen zijn D3, D5, D6, D9, D10 en D11. D9/D10 doen geen PWM zolang er een servo-output bestaat
(Servo-library, timer 1).

### 2.2 Input-velden (`InputSettings` in `input.h`, 12 bytes in EEPROM)

| Veld | Bits | Waarden |
|---|---|---|
| `address` | 12 | DCC-accessory-adres 1–4094, **0xFFF = geen** |
| `pin` | 6 | zoals bij outputs; de firmware zet `INPUT_PULLUP` (knop naar GND) |
| `type` | 3 | zie hieronder |
| `outputID` | 4 | 0–15: de output (direct), of het sensornummer (feedback) |
| `linked[8]` | 8 × 8 | v1-route: bit 7 = stand, bits 0–6 = output-id; 0x7F/0xFF = einde |

Input-types:
- 0 toggle — wisselt bij het indrukken (falling edge) of bij elk DCC-commando;
- 1 absoluut — volgt het niveau (gesloten = 0) of de DCC-richting;
- 2 route — start bij het indrukken van de knop (falling edge) of bij **elk** DCC-commando voor het adres,
  richting 0 en 1;
- 3 niet in gebruik;
- 4 recorder — lang drukken (1,5 s) = opnemen aan/uit, kort drukken = afspelen aan/uit;
- 5 feedback — sensor waar een opname tijdens het afspelen op wacht.

Waarde 6 werd op 2026-11-10 kort gebruikt voor "route op de falling edge" en is vervallen. De GUI leest
een 6 als route en schrijft er 2 voor terug.

### 2.3 Presets

`61 <preset>` vult de outputs (en meestal de inputs) met een standaardopstelling. Pinnen beginnen bij D3.
Het verloop:
1. Per output stuurt de firmware een broadcast `2E` (zelfde indeling als het GET-antwoord), met 50–100 ms
   ertussen.
2. Daarna komt per input een `1E`, behalve bij preset 4, dat de inputs ongemoeid laat.
3. Als laatste komt de `ACK`.

Dit kan ruim 3 s duren. De GUI wacht op de `ACK` (timeout 10 s), negeert de broadcasts en leest daarna
alles opnieuw in. Ongebruikte inputs worden "niet in gebruik".

| Preset | Opstelling |
|---|---|
| 0 | 16 servo's |
| 1 | 8 servo's + unifrog-relais |
| 2 | 5 servo's + electrofrog-relais |
| 3 | 16 × single steady |
| 4 | 8 × double pulse |
| 5 | 5 × double pulse + unifrog-relais |
| 6 | 4 × double pulse + electrofrog-relais |
| 7 | 8 × double steady |

## 3. HELLO — aanmelden en configuratiesessie (v2)

```text
host   -> 90
device -> 95 <protocol> <max steps per route> <step pool lo> <step pool hi> <flags>
flags  : bit0 = D0/D1 usable as I/O outside a config session
         bit1 = SERVO_LIVE supported
         bit2 = routes / recorder in external EEPROM
         bit3 = BOARD settings (C0 / CF) supported, see section 10
step pool: number of route steps that still fit with the current board settings
           (timing on/off, recorder on/off), all routes together
```

De GUI stuurt `HELLO` direct na het boot-bericht (versie). Dat is ongeveer 1,5 s na de reset die het
openen van de poort via DTR veroorzaakt. Bij boards zonder auto-reset stuurt hij het na `REQ_VERSION`.

**Wat de decoder doet (voorstel, zodat D0/D1 bruikbaar worden):**
1. Na power-up of reset wacht hij **5 s** met Serial actief en voert hij nog **niets** uit op D0/D1.
2. Komt er binnen die 5 s een `HELLO`, dan is er een **configuratiesessie**: D0/D1 blijven Serial tot de
   volgende reset. Outputs en inputs op D0/D1 doen in die sessie niets.
3. Komt er geen `HELLO`, dan doet hij `Serial.end()` en zet hij D0/D1 als gewone I/O
   (`pinMode` volgens de configuratie).
4. Wil je later weer configureren, dan reset je het board. De GUI doet dat zelf bij het verbinden (DTR).

**Is D0/D1 gebruiken überhaupt verstandig?** Het kan, met drie kanttekeningen:
- Op een Uno/Nano zitten D0/D1 via 1 kΩ aan de USB-serial chip. Een output werkt gewoon. Een input met
  een knop naar GND ook, want de knop wint van de 1 kΩ.
- Een zware last of een laagohmig circuit op **D0 (RX)** kan het **uploaden van firmware** verstoren.
  Gebruik ze bij voorkeur voor LEDs, of voor relais via een transistor.
- Tijdens een configuratiesessie werken ze niet. Een "Test state" op een output aan D0/D1 doet dan niets.

De GUI laat D0/D1 kiezen als de decoder bit0 meldt, en offline. Hij geeft er dan een waarschuwing bij.
Bij v1-firmware zijn ze gereserveerd en rood.

## 4. Routes (v2)

### Stap = 4 bytes

```text
byte 0 : bits 0-3 output id (0-15) | bit 4 raw value | bits 5-7 reserved (0)
byte 1 : value   raw = 0: state 0 = curved, 1 = straight
                 raw = 1: aspect 0-255 (signal), angle 10-170 (servo), PWM 0-255
byte 2 : wait time bits 0-7
byte 3 : bits 0-5 = wait time bits 8-13 | bits 6-7 reserved (0)
wait   : 14 bits of 0.1 s = 0 .. 1638.3 s (~27 min), time after this step before the next one
```

Voorbeeld: output 13, aspect 200, wachttijd 1638,3 s → `1D C8 FF 3F`.

- **De GUI kiest de waarde-soort per output-type.** Wissels, licht en relais krijgen curved/straight.
  Een 4-lampsein krijgt een aspect-nummer. Een servo krijgt curved/straight **of** een hoek, en mag
  meerdere keren in één route voorkomen (tussenstanden). PWM krijgt een waarde.
- **Nieuwe stappen** krijgen 0,3 s wachttijd.
- **Timing uit** (board-instelling, §10): op de draad blijft een stap 4 bytes, maar de decoder bewaart
  alleen byte 0 en 1 (2 B per stap) en wacht na elke stap de vaste `delay` uit de board-instellingen.
  Bij teruglezen zijn byte 2 en 3 dan 0. De GUI verbergt de wachttijd-kolom.
- **De decoder voert een stap zo uit:**

```cpp
// raw == 0 -> like a DCC command:   callbackInput( id, value, 0 )
// raw == 1 -> signal / PWM:         output[id].setVal( value )
//             servo:                output[id].setPosition( value )   // new, moves to this angle
// then wait 'delay' x 100 ms before the next step
```

### Overdracht in stukken van maximaal 7 stappen

```text
GET_ROUTE  82 <route> <first>
SET_ROUTE  8F <len = 4 + 4n> <route> <total> <first> <step ...>     (also the reply to GET_ROUTE)
```

- Een route van 16 stappen gaat in drie berichten: 7 + 7 + 2. Een lege route is één bericht met
  `total = 0`.
- Bij `first = 0` begint de decoder de route opnieuw. Hij slaat de route op zodra `total` stappen
  binnen zijn.
- De GUI schrijft per input: `SET_INPUT_DATA`, dan `SET_ROUTE`(s), en leest daarna input en route terug.

## 5. SERVO_LIVE (v2)

`A2 <output> <angle>` beweegt de servo meteen naar `angle`. Er wordt **niets** opgeslagen en de
state machine verandert niet.
- De GUI stuurt dit terwijl je een servo-slider versleept, als het vinkje *Live* aan staat. Een snelle
  reeks slider-waarden wordt samengevoegd tot alleen de laatste.
- **Voorstel voor de decoder:** attach de servo, schrijf de hoek, en detach na ongeveer 1 s zonder nieuw
  `SERVO_LIVE`.

## 6. EEPROM-budget

Zie **`EEPROM.md`**: de indeling van v2, wat er in 1024 B past, en wat er nog te comprimeren valt.
Kort: zonder `linked[8]` is een input 4 B, routes en recorder delen één pool van 762 B, en met timing
uit past 16 × 16 stappen intern (512 B) met nog ±31 recorder-events over.

## 7. Wat de GUI doet bij v1-firmware (huidige stand)

- **Geen `HELLO`-antwoord** → protocol v1. Dat telt niet als fout.
- **Routes:** maximaal 8 stappen, alleen curved/straight, vaste 0,5 s per stap. Ze worden opgeslagen in de
  `linked`-bytes van `SET_INPUT_DATA`. `GET/SET_ROUTE` worden niet verstuurd.
- **Geen Live-servo**, en D0/D1 zijn gereserveerd.
- **Statusbalk:** "protocol v1 · routes: 8 steps, no times".

## 8. DCCnext (ArCoMoRa)

| Poort | 1–10 | 11–16 | n.v.t. |
|---|---|---|---|
| Arduino-pin | D3–D12 | A0–A5 | D13, D0/D1 (USB), D2 (DCC) |

- PWM werkt op poort 1, 3, 4 en 9 (D3, D5, D6, D11).
- D9/D10 geven geen PWM zolang de Servo-library actief is (timer 1). Dat geldt ook voor GPIOcore op een
  gewone Nano.
- Bron: het [DCCNext-schema](https://www.arcomora.com/wp-content/uploads/2024/09/ArsigdecRocRailUK.pdf)
  en de [Mardec 7-handleiding](https://www.arcomora.com/wp-content/uploads/2021/08/Mardec7UK.pdf).
  ArCoMoRa noemt het schema "niet compleet", dus controleer het op je eigen board.
- In de GUI zet je het vinkje *DCCnext board*. De pin-keuzes tonen dan "D3 · port 1", en D13 wordt rood.

## 9. Decoder-firmware v2

De taken staan in `EEPROM.md` §4 en in de commits van `software/GPIOcore`. Samengevat: HELLO met
bootvenster, board-instellingen (§10), route-pool met dynamische toewijzing, per-stap timing,
interlocking, SERVO_LIVE en `setPosition()`, A6/A7 als analoge input, en pin 2 beschermen.

**Pin 2 (DCC-ingang)** mag nooit als input of output. Een `SET_INPUT_DATA` of `SET_OUTPUT_DATA` met
pin 2 krijgt `NACK 5` (ongeldige pin) en wordt niet opgeslagen. Hetzelfde geldt voor D0/D1 zolang de
decoder geen bit0 in HELLO meldt, en voor A6/A7 als output of op een ander board dan de Nano.

## 10. Board-instellingen en naam (v2)

```text
host   -> C0                                        (GET_BOARD, no payload)
device -> CF <len> <type> <flags> <delay> <name ...> (also SET_BOARD from host -> ACK)
len    : 4 + name length (name 0..16 ASCII bytes, no terminator)
type   : 0 = Arduino Uno, 1 = Arduino Nano, 2 = DCCnext, 3 = Jack of all trades
flags  : bit0   = recorder enabled
         bit1   = route step timing (per-step wait times stored)
         bit2-3 = route interlocking: 0 = off (all routes may run in parallel)
                                      1 = on  (one route at a time)
                                      2 = auto (routes that move a turnout run one at a time,
                                                other routes run in parallel)  <- default
delay  : fixed wait after each route step when timing is off, x 0.1 s (default 3 = 0.3 s)
```

- **Naam.** De naam identificeert het board. De GUI bewaart per board een bestand `<naam>.json` (namen
  van in- en outputs plus alle instellingen). Bij verbinden leest hij `HELLO`, dan `GET_BOARD`, laadt hij
  `<naam>.json` als dat bestaat, en leest daarna **alles** van het board. Wat het board zegt wint; de
  namen komen uit het bestand. Een nieuw board heeft een lege naam; de GUI vraagt er dan om.
- **Type.** Bepaalt welke pinnen bestaan. Op de Nano zijn A6/A7 (pin-index 20/21) alleen analoge
  inputs zonder pull-up: de firmware leest ze met `analogRead() < 512` als "ingedrukt", en de knop heeft
  een externe pull-up van 10 kΩ nodig. Het type wordt door de GUI gezet; de firmware gebruikt het alleen
  om pinnen te controleren.
- **Recorder uit.** Een input van het type recorder doet niets, en de recorder-ruimte gaat naar de
  route-pool. HELLO meldt daarna de grotere pool.
- **Timing aan/uit omzetten.** De decoder zet de opgeslagen routes om (2 ↔ 4 B per stap). Past het niet
  (timing aan met te veel stappen), dan `NACK 4` en blijft de oude instelling staan.
- **Interlocking.** Een route die door interlocking geweigerd wordt, wordt genegeerd (niet in een
  wachtrij gezet): dat beschermt tegen een verkeerde druk op de knop. "Een wissel bewegen" = de route
  bevat een output van het type servo, single pulse of double pulse.
- **NACK-redenen** in v2: 1 checksum, 2 te lang, 3 timeout, 4 vol (pool of route), 5 ongeldige pin.

### 10.1 Zo doet de firmware het (2026-11-12, afwijkingen en keuzes)

- **Standaardwaarden** nieuw board: Nano, recorder aan, timing **uit**, interlocking auto, delay 0,3 s.
  Een gemigreerd v1-board krijgt delay 0,5 s, zodat routes even snel blijven. De oude opname wordt gewist.
- **SET_ROUTE schrijft meteen.** `first = 0` maakt de route meteen de nieuwe lengte (de routes erachter
  schuiven op, `NACK 4` als het niet past); elk volgend stuk wordt direct geschreven. Een afgebroken
  overdracht laat dus een half geschreven route achter. De GUI leest de route daarna altijd terug.
- **`NACK 4` bij de recorder:** recorder aanzetten terwijl er minder dan 160 B vrij is, wordt ook geweigerd.
- **Recorder:** gebruikt hele events van 8 B van net boven de routes tot adres 1023. Verschuift dat begin
  (route langer/korter, timing of recorder omgezet), dan wordt de opname gewist.
- **Timing aanzetten** geeft elke bestaande stap de board-delay als wachttijd. De eerste stap start meteen
  (v1 wachtte eerst 0,5 s). Wachttijden lopen af op de 50 ms-tik van de inputs.
- **Interlocking:** een route telt als lopend tot de wachttijd na de laatste stap voorbij is.
- **Nog open:** `setPosition()`/`SERVO_LIVE` zetten het unifrog-relais niet om; `SET_OUTPUT_DATA` zet geen
  `pinMode` voor nieuw ingestelde pinnen (was in v1 ook zo); een output op D0/D1 wordt tijdens het
  bootvenster niet tegengehouden.
