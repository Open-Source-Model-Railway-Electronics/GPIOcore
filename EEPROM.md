# GPIOcore — EEPROM-gebruik

Datum: 2026-11-12 · Geldt voor ATmega328P (Uno, Nano, DCCnext): **1024 B intern EEPROM**.

## 1. Nu (firmware v1)

| Adres | Inhoud | Bytes |
|---|---|---|
| 0–351 | 16 × (output 10 B + input 12 B), om en om | 352 |
| 352–419 | ongebruikt | 68 |
| 420–999 | recorder, 8 B per event (72 events) | 580 |
| 1000–1003 | `DEADBEEF` (eerste start) | 4 |
| 1004–1023 | ongebruikt | 20 |

Per input gaat 8 B naar `linked[8]` (route van max 8 outputs, alleen stand). Dat is 128 B, en het kan
maar 8 stappen zonder tijd of waarde.

## 2. Nieuw (firmware v2)

| Adres | Inhoud | Bytes |
|---|---|---|
| 0–2 | magic `G` `2` + layout-versie | 3 |
| 3–21 | board: type 1, flags 1, delay 1, naam 16 | 19 |
| 22–181 | 16 outputs × 10 B (ongewijzigd) | 160 |
| 182–245 | 16 inputs × **4 B** (adres 12 bit, pin 6, type 3, outputID 4; 7 bits vrij) | 64 |
| 246–261 | aantal stappen per route (16 × 1 B) | 16 |
| 262–1023 | **gedeelde pool**: routes groeien van onder, recorder-events van boven | 762 |

**Dynamische toewijzing.** Routes staan achter elkaar in de pool, zonder vaste 16 plekken van 16 stappen.
Het begin van route *r* is de som van de lengtes van route 0 … r-1. Wordt een route langer of korter,
dan schuiven de routes erachter op. Dat kost alleen EEPROM-schrijfacties bij het configureren, nooit
tijdens het rijden. Een route van 3 stappen kost dus 3 stappen, niet 16.

### Wat past er in de pool (762 B)

| Timing | Stap | Max. stappen, recorder uit | 16 × 16 stappen (256) | Over voor recorder |
|---|---|---|---|---|
| **uit** (vaste wachttijd) | 2 B | **381** | 512 B ✓ | 250 B = 31 events |
| aan (per stap) | 4 B | 190 | 1024 B ✗ | – |
| aan, realistisch: 8 routes × 8 stappen | 4 B | – | 256 B ✓ | 506 B = 63 events |

Met de recorder aan houdt de decoder minimaal 20 events (160 B) vrij; de rest mag de recorder gebruiken
zolang routes het niet nodig hebben. HELLO meldt altijd hoeveel stappen er nog bij kunnen.

**Conclusie.** Met timing uit past alles wat de DCCnext kan (16 inputs × 16 stappen) intern. Externe
EEPROM is alleen nodig voor veel getimede routes plus een grote recorder.

## 3. Wat er verder te comprimeren valt

| Idee | Winst | Kosten / risico | Advies |
|---|---|---|---|
| `linked[8]` uit input (gedaan in v2) | 128 B | geen | ✓ gedaan |
| Timing per route als instelling (gedaan, als board-instelling) | 2 B per stap | minder flexibel | ✓ gedaan |
| **Variabele stap**: 2 B, plus 2 B alleen als de wachttijd ≠ standaard (vlag-bit in byte 0) | bijna alle 2 B per stap, ook met timing aan | route moet sequentieel gelezen worden (doet de decoder toch) | **goed idee voor later**; maakt de timing-instelling overbodig |
| Wachttijd 14 → 8 bit met schaal (0–25,5 s in 0,1 s, of ×1 s boven 12,7 s) | 1 B per stap | max ±2 min in plaats van 27 min | alleen als nodig |
| Output: 7 pinnen × 6 bit in één `uint64` (nu al bitfields) | 0 | – | zit al op 10 B |
| Output: `relayA–C` alleen bij types die relais gebruiken | 2–3 B per output | twee record-formaten | niet waard |
| Recorder-event 8 B → 4 B (tijd 16 bit in 0,1 s, id 4, waarde 1, type 3) | 4 B per event = 2× zo veel events | kortere max. tijd per event (109 min) | **doen** bij volgende recorder-werk |
| Board-naam 16 → 12 tekens | 4 B | – | niet nodig |
| `malloc` / heap | – | heap-fragmentatie op 2 KB RAM; EEPROM is geen heap | **niet doen**; de pool hierboven is al dynamisch |

## 4. Firmware-taken v2 (volgorde van de commits)

1. Protocol: `HELLO`, `GET_BOARD`/`SET_BOARD`, `NACK 5` voor pin 2.
2. EEPROM-layout v2 + migratie van v1 (outputs, inputs, `linked` → route-stappen).
3. Route-pool met dynamische toewijzing, `GET_ROUTE`/`SET_ROUTE`.
4. Route-uitvoering: per-stap of vaste wachttijd, `raw`-waarden, meerdere routes tegelijk, interlocking.
5. `SERVO_LIVE` en `Output::setPosition()`.
6. Recorder aan/uit, A6/A7 als analoge input.
7. 5 s-bootvenster en D0/D1 als I/O.
