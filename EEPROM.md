# GPIOcore — EEPROM-gebruik

Datum: 2026-11-12 · ATmega328P (Uno, Nano, DCCnext): **1024 B intern EEPROM**.
De recorder staat **niet** in het interne EEPROM maar in een **externe I2C-EEPROM** (24LC256, 32 KB,
adres 0x50, op A4 = SDA en A5 = SCL).

## 1. Interne EEPROM-map (firmware v2, layout 1)

Bron: `software/GPIOcore/routes.h` (`EE_*`-constanten).

| Adres (dec) | Adres (hex) | Bytes | Inhoud |
|---|---|---|---|
| 0 – 2 | 0x000 – 0x002 | 3 | Magic `'G'` `'2'`, layout-versie `1`. Ontbreekt dit, dan wordt een v1-board omgezet of krijgt een leeg board standaardwaarden |
| 3 | 0x003 | 1 | Board-type: 0 Uno, 1 Nano, 2 DCCnext, 3 Jack of all trades |
| 4 | 0x004 | 1 | Board-flags: bit0 recorder, bit1 timing per stap, bit2-3 interlocking (0 uit, 1 aan, 2 auto) |
| 5 | 0x005 | 1 | Vaste wachttijd na een routestap als timing uit staat, × 0,1 s |
| 6 – 21 | 0x006 – 0x015 | 16 | Board-naam, ASCII, opgevuld met 0 |
| 22 – 181 | 0x016 – 0x0B5 | 160 | **16 outputs × 10 B** — output *n* op `22 + 10·n` |
| 182 – 245 | 0x0B6 – 0x0F5 | 64 | **16 inputs × 4 B** — input *n* op `182 + 4·n` |
| 246 – 261 | 0x0F6 – 0x105 | 16 | Aantal stappen per route (0–16) — route *n* op `246 + n` |
| 262 – 1023 | 0x106 – 0x3FF | 762 | **Route-pool**: alle routestappen achter elkaar |

### Output-record (10 B)

| Byte | Inhoud |
|---|---|
| 0 – 5 | Bitstroom vanaf bit 0 van byte 0: `pinA` bits 0–5, `pinB` 6–11, `pinC` 12–17, `pinD` 18–23, `relayPinA` 24–29, `relayPinB` 30–35, `relayPinC` 36–41, `type` 42–45 (0–14; 14 = sound effect, `pinA` = 1), `state` 46, bit 47 ongebruikt |
| 6 – 9 | `data[4]`, de betekenis hangt af van het type (zie `PROTOCOL.md` §2.1) |

Pinnummers: 0–13 = D0–D13, 14–19 = A0–A5, 20–21 = A6–A7, 63 (0x3F) = niet gebruikt.

### Input-record (4 B)

Bitstroom vanaf bit 0 van byte 0: `address` bits 0–11 (0xFFF = geen), `pin` 12–17, `type` 18–20,
`outputID` 21–24, `level` 25–31. `pin` is een pinnummer of een analoge vergelijking
`0x20 | mode << 3 | kanaal` (`PROTOCOL.md` §2.2); `level` = de analoge drempel / 8 (0–127 → 0–1016), alleen
gebruikt bij een analoge input. Een `static_assert` houdt het record op 4 B.

### Route-pool

- Route *r* begint op `262 + (som van de stappen van route 0 … r-1) × stapgrootte`. Er zijn dus geen vaste
  plekken per route; een route kost precies zoveel stappen als hij heeft.
- **Stapgrootte** volgt de board-flag *timing*:
  - **uit** (standaard): **2 B** — `id | raw`, `value`;
  - **aan**: **4 B** — `id | raw`, `value`, wachttijd laag, wachttijd hoog (14 bit × 0,1 s).
- Wordt een route langer of korter, dan schuiven de routes erachter op. Timing aan/uit zet de hele pool om.
- Het slot van een **toggle- of absolute input** is normaal leeg. Met *eigen waarden* staan er 2 stappen in met
  bit 5 (`STEP_VALUES`, 0x20) in byte 0: de waarde voor stand 0 en voor stand 1 (`PROTOCOL.md` §4). Byte 0 wordt
  ongewijzigd bewaard, dus ook bij timing uit (2 B per stap) blijft de markering staan.

| Timing | Stap | Past in 762 B | 16 routes × 16 stappen (256) |
|---|---|---|---|
| uit | 2 B | **381 stappen** | 512 B ✓ (250 B over) |
| aan | 4 B | 190 stappen | 1024 B ✗ — max. ±11 stappen per route als alle 16 routes vol zijn |

**Totaal in gebruik zonder routes:** 262 B vast + 0 B routes. Alles boven adres 261 is voor routes.

## 2. Externe I2C-EEPROM (recorder)

| Adres | Inhoud |
|---|---|
| 0 – 32767 | Recorder-events van 8 B (`data1`, `data2` 16 bit, `data3`, `time2nextEvent` 32 bit): ±4096 events |

- De recorder werkt **alleen** als de board-flag *recorder* aan staat. Dan start de firmware I2C op
  A4/A5; staat de flag uit, dan zijn A4/A5 gewone pinnen.
- **Recorder aan ⇒ A4 en A5 zijn verboden** voor inputs en outputs. De firmware antwoordt `NACK 5` op een
  in- of output met A4/A5, en weigert de recorder aan te zetten (`NACK 5`) zolang A4 of A5 in gebruik is.
  De GUI toont A4/A5 dan als gereserveerd en waarschuwt bij het aanzetten.
- Een event van 8 B op een veelvoud van 8 valt nooit over een page-grens (64 B) van de 24LC256.
- Zonder EEPROM op de bus speelt de recorder niets af en neemt hij niets op; een time-out van 3 ms
  voorkomt dat de decoder blijft hangen.

## 3. Oud (firmware v1), ter vergelijking

| Adres | Inhoud | Bytes |
|---|---|---|
| 0–351 | 16 × (output 10 B + input 12 B, waarvan 8 B `linked`), om en om | 352 |
| 352–419 | ongebruikt | 68 |
| 420–999 | recorder, 8 B per event | 580 |
| 1000–1003 | `DEADBEEF` | 4 |

Een v1-board wordt bij de eerste start met v2 omgezet: outputs, inputs, en `linked[]` als 2-byte
routestappen. De recorder-opname gaat verloren.

## 4. Wat er verder te comprimeren valt

| Idee | Winst | Kosten / risico | Advies |
|---|---|---|---|
| `linked[8]` uit input | 128 B | geen | ✓ gedaan |
| Timing als board-instelling | 2 B per stap | minder flexibel | ✓ gedaan |
| Recorder naar I2C-EEPROM | 580 B intern | Wire-library: ±2,6 KB flash, ±220 B RAM | ✓ gedaan |
| **Variabele stap**: 2 B, plus 2 B alleen als de wachttijd afwijkt (vlag-bit in byte 0) | bijna altijd 2 B per stap, ook met timing aan | route sequentieel lezen (doet de decoder toch) | goed idee voor later; dan kan de timing-instelling weg |
| Wachttijd 14 → 8 bit met schaal | 1 B per stap | max ±2 min | alleen als nodig |
| Board-naam 16 → 12 tekens | 4 B | – | niet nodig |
| `malloc` / heap | – | fragmentatie op 2 KB RAM | niet doen; de pool is al dynamisch |

**RAM** (Nano, met I2C): 1589 B van 2048 B in gebruik, 459 B vrij. Flash: 26948 B van 30720 B (87 %).
