# Adding translations

The GUI is written in English. A translation consists of two parts, which you can make independently of each other.

| What | Where | How to translate |
|---|---|---|
| **Short UI texts** (buttons, labels, menus, messages) | `i18n/iocore_<language>.ts` | Qt Linguist (`C:\Qt\6.10.1\mingw_64\bin\linguist.exe`) |
| **Explanation texts** behind the (i) buttons and *Help → Getting started* | `i18n/info/<language>/<topic>.html` | A plain text editor (HTML) |

`<language>` is the ISO code of the language: `nl`, `de`, `fr`, `en`, and so on.

## Adding a new language, for example German (`de`)

### 1. Explanation texts

1. Copy the folder `i18n/info/en` to `i18n/info/de`.
2. Translate the `.html` files. Leave the file names unchanged.

A file that is (still) missing automatically falls back to English. So you can translate topic by topic.

**Testing without rebuilding:** also put the folder next to the program (`<folder of IOcoreGUI.exe>\i18n\info\de\`).
Files there take priority over the built-in ones. Restart the program and click an (i).

### 2. UI texts

1. Add the language in `CMakeLists.txt`:
   ```cmake
   set(IOCORE_LANGUAGES nl de CACHE STRING "...")
   ```
   You can also pass `-DIOCORE_LANGUAGES="nl;de"` when configuring.
2. Create or refresh the `.ts` file. In Qt Creator: *Build → Run CMake*, and then build the target
   **update_translations**. On the command line:
   ```bash
   cmake --build <build-folder> --target update_translations
   ```
   After that all texts are in `i18n/iocore_de.ts`.
3. Open that file in **Qt Linguist**. Translate text by text and mark it as "done" (check mark), and save.
4. Build the program again. The `.qm` file is created and built in automatically.

### 3. Choosing the language

In the program choose *View → Language → Deutsch (de)* and restart the program. With
*Automatic* it follows the language of Windows.

## Tips for translators

- **`%1`, `%2`** are places where the program fills in a number or name. Leave them in place; the order may
  change.
- **`&File`**: the `&` makes the next letter the shortcut key (Alt+F). Put it on a convenient letter in your language.
- **Texts with the context `Gpio`** are type names ("Servo", "Double pulse") and pin descriptions. Keep them short,
  because they are also shown on the blocks in the canvas.
- **Model railway terms:** use the words that are common on forums in your language. A "frog" is in
  Dutch the **puntstuk**, not "kikker".
- **New texts after an update:** run `update_translations` again. Existing translations stay;
  new texts appear in Linguist as "unfinished".

## Status (2026-11-23)

All 12 languages (`nl de fr es it pl cs da nb sv hu pt`) have every UI text translated (625 texts) and every
explanation text (27 topics). Terms have **not** yet been checked by native speakers on forums: signal aspects,
frog, block and "board link" are the first candidates.

## For programmers

- Texts in the context `Gpio` that are translated with `trGpio(...)` must be written as
  `trGpio(QT_TRANSLATE_NOOP("Gpio", "..."))`, otherwise lupdate does not find them.
- A help topic is one file per language; a new topic needs a CMake configure run (the files are globbed).
- Check a translated topic with a script: its HTML tag sequence must equal the English one.
