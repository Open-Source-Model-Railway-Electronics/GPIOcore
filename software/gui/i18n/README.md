# Vertalingen toevoegen

De GUI is in het Engels geschreven. Een vertaling bestaat uit twee delen, die je los van elkaar kunt maken.

| Wat | Waar | Hoe vertalen |
|---|---|---|
| **Korte schermteksten** (knoppen, labels, menu's, meldingen) | `i18n/gpiocore_<taal>.ts` | Qt Linguist (`C:\Qt\6.10.1\mingw_64\bin\linguist.exe`) |
| **Uitlegteksten** achter de (i)-knopjes en *Help → Getting started* | `i18n/info/<taal>/<onderwerp>.html` | Een gewone teksteditor (HTML) |

`<taal>` is de ISO-code van de taal: `nl`, `de`, `fr`, `en`, enzovoort.

## Een nieuwe taal toevoegen, bijvoorbeeld Duits (`de`)

### 1. Uitlegteksten

1. Kopieer de map `i18n/info/en` naar `i18n/info/de`.
2. Vertaal de `.html`-bestanden. Laat de bestandsnamen ongewijzigd.

Een bestand dat (nog) ontbreekt, valt automatisch terug op het Engels. Je kunt dus per onderwerp vertalen.

**Testen zonder opnieuw te bouwen:** zet de map ook naast het programma (`<map van GPIOcoreGUI.exe>\i18n\info\de\`).
Bestanden daar gaan voor op de ingebouwde. Start het programma opnieuw en klik op een (i).

### 2. Schermteksten

1. Voeg de taal toe in `CMakeLists.txt`:
   ```cmake
   set(GPIOCORE_LANGUAGES nl de CACHE STRING "...")
   ```
   Je kunt ook bij het configureren `-DGPIOCORE_LANGUAGES="nl;de"` meegeven.
2. Maak of ververs het `.ts`-bestand. In Qt Creator: *Build → Run CMake*, en daarna het target
   **update_translations** bouwen. Op de command line:
   ```bash
   cmake --build <build-map> --target update_translations
   ```
   Daarna staan alle teksten in `i18n/gpiocore_de.ts`.
3. Open dat bestand in **Qt Linguist**. Vertaal per tekst en zet hem op "klaar" (vinkje), en sla op.
4. Bouw het programma opnieuw. Het `.qm`-bestand wordt automatisch gemaakt en ingebouwd.

### 3. Taal kiezen

Kies in het programma *View → Language → Deutsch (de)* en start het programma opnieuw. Bij
*Automatic* volgt het de taal van Windows.

## Tips voor vertalers

- **`%1`, `%2`** zijn plekken waar het programma een getal of naam invult. Laat ze staan; de volgorde mag
  veranderen.
- **`&File`**: de `&` maakt de volgende letter de sneltoets (Alt+F). Zet hem op een handige letter in jouw taal.
- **Teksten met de context `Gpio`** zijn type-namen ("Servo", "Double pulse") en pinomschrijvingen. Houd ze kort,
  want ze staan ook op de blokken in het canvas.
- **Modelspoortermen:** gebruik de woorden die op forums in jouw taal gebruikelijk zijn. Een "frog" is in het
  Nederlands het **puntstuk**, niet "kikker".
- **Nieuwe teksten na een update:** bouw opnieuw `update_translations`. Bestaande vertalingen blijven staan;
  nieuwe teksten verschijnen in Linguist als "onvertaald".

## Status (2026-11-13)

| Taal | Schermteksten | Uitlegteksten |
|---|---|---|
| English (`en`) | bron (441 teksten) | volledig (21) |
| Nederlands (`nl`), Deutsch (`de`), Français (`fr`), Español (`es`), Italiano (`it`) | 441 van 441 vertaald | volledig (21) |
| `pl` `cs` `da` `nb` `sv` `hu` `pt` | 441 van 441 vertaald | nog niet (valt terug op Engels) |

Termen zijn nog **niet** op forums gecontroleerd. Afwijkingen van de bestaande handleidingen: NL puntstuk (niet
"hartpunt"), SV hjärtstycke, HU szív, NB sporveksel, en ES/IT/PT/NB een eigen woord voor frog (corazón, cuore, coração,
hjertestykke). In het Nederlands heet de print in de GUI "board" (het product heet "GPIOcore board").
