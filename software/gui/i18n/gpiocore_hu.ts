<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="hu_HU">
<context>
    <name>ConnectionView</name>
    <message>
        <location filename="../src/connectionview.cpp" line="209"/>
        <location filename="../src/connectionview.cpp" line="214"/>
        <source>value ▾</source>
        <translation>érték ▾</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="214"/>
        <source>aspect ▾</source>
        <translation>jelzési kép ▾</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="225"/>
        <source>▶ start</source>
        <translation>▶ indít</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="225"/>
        <source>▶ press</source>
        <translation>▶ nyomás</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="599"/>
        <source>In %1</source>
        <translation>Be %1</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="624"/>
        <source>now %1</source>
        <translation>most %1</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="659"/>
        <source>Out %1</source>
        <translation>Ki %1</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="722"/>
        <source>Inputs</source>
        <translation>Bemenetek</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="723"/>
        <source>Outputs</source>
        <translation>Kimenetek</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="735"/>
        <source>● SIMULATION — click the input buttons</source>
        <translation>● SZIMULÁCIÓ — kattintson a bemenetek gombjaira</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="735"/>
        <source>SIMULATION</source>
        <translation>SZIMULÁCIÓ</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="740"/>
        <source>● LIVE TEST</source>
        <translation>● ÉLŐ TESZT</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="745"/>
        <source>CONFIGURING THE BOARD</source>
        <translation>A PANEL KONFIGURÁLÁSA</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="766"/>
        <source>drag an output onto an input</source>
        <translation>húzzon egy kimenetet egy bemenetre</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="980"/>
        <source>Remove connection In %1 → Out %2</source>
        <translation>Kapcsolat törlése: Be %1 → Ki %2</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="985"/>
        <source>Set curved (0)</source>
        <translation>Kitérő irányba (0)</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="985"/>
        <source>Set straight (1)</source>
        <translation>Egyenes irányba (1)</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="988"/>
        <source>Edit value in the input panel</source>
        <translation>Érték szerkesztése a bemeneti panelen</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="990"/>
        <source>Move earlier in the route</source>
        <translation>Korábbra a vágányútban</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="993"/>
        <source>Move later in the route</source>
        <translation>Későbbre a vágányútban</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="1000"/>
        <source>Remove all connections of In %1</source>
        <translation>Be %1 összes kapcsolatának törlése</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="1023"/>
        <source>In %1 → Out %2 (%3)</source>
        <translation>Be %1 → Ki %2 (%3)</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="1025"/>
        <source>
step %1: %2, then wait %3</source>
        <translation>
%1. lépés: %2, majd várakozás %3</translation>
    </message>
</context>
<context>
    <name>DeviceLink</name>
    <message>
        <location filename="../src/devicelink.cpp" line="56"/>
        <source>Serial port lost: %1</source>
        <translation>A soros port elveszett: %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="94"/>
        <source>Opened %1</source>
        <translation>Megnyitva: %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="100"/>
        <source>Waiting for boot message…</source>
        <translation>Várakozás az indítási üzenetre…</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="129"/>
        <source>Closed %1</source>
        <translation>Bezárva: %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="268"/>
        <source>Firmware uses legacy SET_OUTPUT_DATA (0x2D, 13 bytes): the 4th data byte (servo pattern / high byte of times) is not transmitted.</source>
        <translation>A firmware a régi SET_OUTPUT_DATA parancsot használja (0x2D, 13 bájt): a 4. adatbájt (szervóminta / az idők felső bájtja) nem kerül átvitelre.</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="331"/>
        <source>This firmware speaks protocol v1: routes are stored in the 8 linked bytes of SET_INPUT_DATA (state only, fixed 0.5 s per step).</source>
        <translation>Ez a firmware a v1 protokollt beszéli: a vágányutak a SET_INPUT_DATA 8 összekapcsolt bájtjában vannak (csak állapot, lépésenként fix 0,5 s).</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="383"/>
        <source>No answer to %1, retrying</source>
        <translation>Nincs válasz erre: %1, újrapróbálás</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="389"/>
        <source>No HELLO reply: firmware speaks protocol v1</source>
        <translation>Nincs HELLO-válasz: a firmware a v1 protokollt beszéli</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="401"/>
        <source>No answer to %1</source>
        <translation>Nincs válasz erre: %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="416"/>
        <source>No boot message, requesting version</source>
        <translation>Nincs indítási üzenet, verzió lekérése</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="431"/>
        <source>Incomplete frame discarded</source>
        <translation>Hiányos keret eldobva</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="472"/>
        <source>NACK (%1) on %2</source>
        <translation>NACK (%1) erre: %2</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="481"/>
        <source>Device rejected %1: %2</source>
        <translation>Az eszköz elutasította: %1: %2</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="550"/>
        <source>Protocol v%1: %2 steps per route, %3 steps in total%4</source>
        <translation>v%1 protokoll: vágányutanként %2 lépés, összesen %3 lépés%4</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="552"/>
        <source>, external EEPROM</source>
        <translation>, külső EEPROM</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="627"/>
        <source>Preset broadcast ignored (old payload layout)</source>
        <translation>Preset-üzenetszórás figyelmen kívül hagyva (régi formátum)</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="652"/>
        <source>Unsolicited %1 ignored</source>
        <translation>Kéretlen %1 figyelmen kívül hagyva</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="659"/>
        <source>GPIOcore version &apos;%1&apos;</source>
        <translation>GPIOcore verzió: &apos;%1&apos;</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="670"/>
        <source>version request</source>
        <translation>verzió lekérése</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="671"/>
        <source>read input %1</source>
        <translation>%1. bemenet olvasása</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="672"/>
        <source>read output %1</source>
        <translation>%1. kimenet olvasása</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="673"/>
        <source>write input %1</source>
        <translation>%1. bemenet írása</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="674"/>
        <source>write output %1</source>
        <translation>%1. kimenet írása</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="675"/>
        <source>preset %1</source>
        <translation>preset %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="676"/>
        <source>hello</source>
        <translation>hello</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="677"/>
        <source>read route %1</source>
        <translation>%1. vágányút olvasása</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="678"/>
        <source>write route %1</source>
        <translation>%1. vágányút írása</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="679"/>
        <source>live servo %1</source>
        <translation>élő szervó %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="680"/>
        <source>read board settings</source>
        <translation>panelbeállítások olvasása</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="681"/>
        <source>write board settings</source>
        <translation>panelbeállítások írása</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="682"/>
        <source>trigger input %1</source>
        <translation>%1. bemenet indítása</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="683"/>
        <source>read state of output %1</source>
        <translation>%1. kimenet állapotának olvasása</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="684"/>
        <source>DCC extended to address %1</source>
        <translation>DCC extended a(z) %1 címre</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="685"/>
        <source>read analog pin A%1</source>
        <translation>A%1 analóg láb olvasása</translation>
    </message>
</context>
<context>
    <name>FlashDialog</name>
    <message>
        <location filename="../src/flasher.cpp" line="387"/>
        <source>Update firmware</source>
        <translation>Firmware frissítése</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="394"/>
        <source>Puts the GPIOcore program (firmware) on your Arduino. Plug the board in with USB, check the settings below and click &lt;b&gt;Flash&lt;/b&gt;. Your settings on the board are normally kept.</source>
        <translation>Felteszi a GPIOcore programot (firmware) az Arduinóra. Csatlakoztassa a panelt USB-n, ellenőrizze az alábbi beállításokat, és kattintson a &lt;b&gt;Feltöltés&lt;/b&gt; gombra. A panelen tárolt beállítások általában megmaradnak.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="407"/>
        <source>Refresh</source>
        <translation>Frissítés</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="408"/>
        <source>Look again for connected boards</source>
        <translation>Csatlakoztatott panelek keresése újra</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="412"/>
        <source>Port:</source>
        <translation>Port:</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="416"/>
        <source>Browse…</source>
        <translation>Tallózás…</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="420"/>
        <source>Firmware file:</source>
        <translation>Firmware-fájl:</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="424"/>
        <source>Arduino Uno / Nano (new bootloader)</source>
        <translation>Arduino Uno / Nano (új bootloader)</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="425"/>
        <source>Arduino Nano (old bootloader)</source>
        <translation>Arduino Nano (régi bootloader)</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="426"/>
        <source>Via ArduinoISP programmer (advanced)</source>
        <translation>ArduinoISP programozóval (haladó)</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="432"/>
        <source>Board type:</source>
        <translation>Paneltípus:</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="435"/>
        <source>&lt;b&gt;Advanced:&lt;/b&gt; choose the port of the &lt;i&gt;programmer&lt;/i&gt; Arduino (running ArduinoISP), wired to the ICSP pins of your board. This also sets the fuses and &lt;b&gt;removes the USB bootloader&lt;/b&gt;: afterwards the board can only be updated with the programmer again. The settings stored on the board are erased.</source>
        <translation>&lt;b&gt;Haladó:&lt;/b&gt; válassza ki a &lt;i&gt;programozó&lt;/i&gt; Arduino portját (ArduinoISP-vel), amely a panel ICSP lábaira van kötve. Ez a fuse-okat is beállítja, és &lt;b&gt;eltávolítja az USB bootloadert&lt;/b&gt;: utána a panel csak programozóval frissíthető. A panelen tárolt beállítások törlődnek.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="444"/>
        <source>Flash</source>
        <translation>Feltöltés</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="483"/>
        <source>Close</source>
        <translation>Bezárás</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="508"/>
        <source>Trying again with the old Nano bootloader…</source>
        <translation>Újrapróbálás a régi Nano bootloaderrel…</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="509"/>
        <location filename="../src/flasher.cpp" line="573"/>
        <source>Uploading… do not unplug the board.</source>
        <translation>Feltöltés… ne húzza ki a panelt.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="529"/>
        <source>(no board found – plug it in and click Refresh)</source>
        <translation>(nem található panel – csatlakoztassa, és kattintson a Frissítés gombra)</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="544"/>
        <source>Choose firmware file</source>
        <translation>Firmware-fájl kiválasztása</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="545"/>
        <source>Firmware (*.hex);;All files (*)</source>
        <translation>Firmware (*.hex);;Minden fájl (*)</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="560"/>
        <source>The firmware file was not found.</source>
        <translation>A firmware-fájl nem található.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="565"/>
        <source>No board found.</source>
        <translation>Nem található panel.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="599"/>
        <source>Done! The new firmware is on the board.</source>
        <translation>Kész! Az új firmware a panelen van.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="602"/>
        <source>Your board has the old Nano bootloader. Next time choose &quot;Arduino Nano (old bootloader)&quot;.</source>
        <translation>A panelen a régi Nano bootloader van. Legközelebb válassza ezt: &quot;Arduino Nano (régi bootloader)&quot;.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="610"/>
        <source>Uploading did not work.</source>
        <translation>A feltöltés nem sikerült.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="624"/>
        <source>The folder &lt;i&gt;tools/avrdude&lt;/i&gt; next to GPIOcoreGUI is missing. Install the program again.</source>
        <translation>Hiányzik a &lt;i&gt;tools/avrdude&lt;/i&gt; mappa a GPIOcoreGUI mellől. Telepítse újra a programot.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="626"/>
        <source>Click &lt;b&gt;Browse…&lt;/b&gt; and choose the GPIOcore &lt;i&gt;.hex&lt;/i&gt; file.</source>
        <translation>Kattintson a &lt;b&gt;Tallózás…&lt;/b&gt; gombra, és válassza ki a GPIOcore &lt;i&gt;.hex&lt;/i&gt; fájlt.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="628"/>
        <source>Close other programs that use the board (Arduino IDE serial monitor, other configuration tools) and try again.</source>
        <translation>Zárja be a panelt használó többi programot (az Arduino IDE soros monitora, más konfigurálóeszközök), és próbálja újra.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="631"/>
        <source>Check the USB cable, click &lt;b&gt;Refresh&lt;/b&gt; and choose the right port.</source>
        <translation>Ellenőrizze az USB-kábelt, kattintson a &lt;b&gt;Frissítés&lt;/b&gt; gombra, és válassza ki a helyes portot.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="634"/>
        <source>Check that the programmer Arduino runs ArduinoISP, that the right port is chosen and that the wires to the board are correct. A 10 µF capacitor between RESET and GND of the programmer often helps.</source>
        <translation>Ellenőrizze, hogy a programozó Arduinón az ArduinoISP fut-e, a helyes port van-e kiválasztva, és jók-e a vezetékek a panel felé. Gyakran segít egy 10 µF-os kondenzátor a programozó RESET és GND lába között.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="637"/>
        <source>Choose another &lt;b&gt;board type&lt;/b&gt; (for example &quot;Arduino Nano (old bootloader)&quot;) and try again. Also check that the right port is chosen and close other programs that use the board.</source>
        <translation>Válasszon másik &lt;b&gt;paneltípust&lt;/b&gt; (például &quot;Arduino Nano (régi bootloader)&quot;), és próbálja újra. Ellenőrizze azt is, hogy a helyes port van-e kiválasztva, és zárja be a panelt használó többi programot.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="640"/>
        <source>Check the six wires between the programmer and the board, and that the board has power.</source>
        <translation>Ellenőrizze a hat vezetéket a programozó és a panel között, és hogy a panel kap-e tápot.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="641"/>
        <source>This board does not have an ATmega328P chip. GPIOcore needs an Arduino Uno or Nano.</source>
        <translation>Ezen a panelen nincs ATmega328P chip. A GPIOcore-hoz Arduino Uno vagy Nano kell.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="643"/>
        <source>Try again. If it keeps failing, try another USB cable or USB port.</source>
        <translation>Próbálja újra. Ha továbbra sem sikerül, próbáljon másik USB-kábelt vagy másik USB-portot.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="647"/>
        <source>Click &lt;b&gt;Show details&lt;/b&gt; to see what happened.</source>
        <translation>Kattintson a &lt;b&gt;Részletek&lt;/b&gt; gombra, hogy lássa, mi történt.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="673"/>
        <source>Hide details</source>
        <translation>Részletek elrejtése</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="673"/>
        <source>Show details</source>
        <translation>Részletek</translation>
    </message>
</context>
<context>
    <name>Flasher</name>
    <message>
        <location filename="../src/flasher.cpp" line="167"/>
        <source>The upload program (avrdude) was not found next to GPIOcoreGUI (tools/avrdude).</source>
        <translation>A feltöltőprogram (avrdude) nem található a GPIOcoreGUI mellett (tools/avrdude).</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="170"/>
        <source>The firmware file was not found: %1</source>
        <translation>A firmware-fájl nem található: %1</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="173"/>
        <source>No port selected.</source>
        <translation>Nincs kiválasztott port.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="308"/>
        <source>Cancelled.</source>
        <translation>Megszakítva.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="312"/>
        <source>Firmware written and checked.</source>
        <translation>Firmware megírva és ellenőrizve.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="319"/>
        <source>No answer at 115200 baud. Trying again at 57600 baud (old Nano bootloader)...</source>
        <translation>Nincs válasz 115200 baudon. Újrapróbálás 57600 baudon (régi Nano bootloader)...</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="326"/>
        <source>The port could not be opened: another program is using it.</source>
        <translation>A port nem nyitható meg: egy másik program használja.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="327"/>
        <source>The port could not be opened: the board was not found.</source>
        <translation>A port nem nyitható meg: a panel nem található.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="328"/>
        <source>The firmware file could not be read.</source>
        <translation>A firmware-fájl nem olvasható.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="329"/>
        <source>The board did not answer.</source>
        <translation>A panel nem válaszolt.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="330"/>
        <source>The chip on the board was not recognised.</source>
        <translation>A panelen lévő chip nem ismerhető fel.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="331"/>
        <source>Writing failed: the check after writing found differences.</source>
        <translation>Az írás sikertelen: az írás utáni ellenőrzés eltéréseket talált.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="333"/>
        <source>Uploading failed (avrdude exit code %1).</source>
        <translation>A feltöltés sikertelen (avrdude kilépési kód: %1).</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="334"/>
        <source>The upload program stopped unexpectedly.</source>
        <translation>A feltöltőprogram váratlanul leállt.</translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="344"/>
        <source>The upload program (avrdude) could not be started: %1</source>
        <translation>A feltöltőprogram (avrdude) nem indítható el: %1</translation>
    </message>
</context>
<context>
    <name>Gpio</name>
    <message>
        <location filename="../src/gpioconfig.cpp" line="13"/>
        <source>Single steady</source>
        <translation>Egyszeres folyamatos</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="14"/>
        <source>One pin follows the state.</source>
        <translation>Egy láb követi az állapotot.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="15"/>
        <source>Single pulse</source>
        <translation>Egyszeres impulzus</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="16"/>
        <source>One pin is pulsed for the switch time (firmware: value x 100 ms).</source>
        <translation>Egy láb impulzust kap a kapcsolási ideig (firmware: érték x 100 ms).</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="17"/>
        <source>Single flashing</source>
        <translation>Egyszeres villogó</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="18"/>
        <source>Pin A blinks while the state is on (value x 10 ms per half period).</source>
        <translation>Az A láb villog, amíg az állapot be van kapcsolva (érték x 10 ms félperiódusonként).</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="19"/>
        <source>Double steady</source>
        <translation>Kettős folyamatos</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="20"/>
        <source>Pin A = state, pin B = inverted state.</source>
        <translation>A láb = állapot, B láb = fordított állapot.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="21"/>
        <source>Double pulse</source>
        <translation>Kettős impulzus</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="22"/>
        <source>Turnout coil pair: pin A or B is pulsed (value x 10 ms).</source>
        <translation>Kitérő tekercspárja: az A vagy B láb impulzust kap (érték x 10 ms).</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="23"/>
        <source>Double flashing</source>
        <translation>Kettős villogó</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="24"/>
        <source>Pins A and B blink alternately (value x 10 ms per half period).</source>
        <translation>Az A és B láb felváltva villog (érték x 10 ms félperiódusonként).</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="25"/>
        <source>Swing light</source>
        <translation>Váltakozó fény</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="26"/>
        <source>Level crossing style: A, B, pause (values x 10 ms).</source>
        <translation>Mint egy vasúti átjárónál: A, B, szünet (értékek x 10 ms).</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="27"/>
        <source>Fireplace</source>
        <translation>Kandalló</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="28"/>
        <source>Random flicker on PWM pins A and B.</source>
        <translation>Véletlenszerű vibrálás az A és B PWM-lábon.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="29"/>
        <source>TL tube</source>
        <translation>Fénycső</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="30"/>
        <source>Fluorescent tube start-up flicker on PWM pin A.</source>
        <translation>Fénycső indulási villódzása az A PWM-lábon.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="31"/>
        <source>Welder</source>
        <translation>Hegesztő</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="32"/>
        <source>Random welding flashes on pin A.</source>
        <translation>Véletlenszerű hegesztési villanások az A lábon.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="33"/>
        <source>4-aspect signal</source>
        <translation>4 jelzési képes jelző</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="34"/>
        <source>Four LEDs A-D. The aspect comes from DCC extended (signal) packets or a route; the board keeps the last aspect in EEPROM.</source>
        <translation>Négy LED A-D. A jelzési kép DCC extended (jelző) parancsokból vagy egy vágányútból jön; a panel az utolsó jelzési képet EEPROM-ban tárolja.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="35"/>
        <source>Servo</source>
        <translation>Szervó</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="36"/>
        <source>Servo on pin A, sweeps between min and max.</source>
        <translation>Szervó az A lábon, a min és max között mozog.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="37"/>
        <source>PWM</source>
        <translation>PWM</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="38"/>
        <source>Fixed PWM value on pin A.</source>
        <translation>Fix PWM-érték az A lábon.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="39"/>
        <location filename="../src/gpioconfig.cpp" line="59"/>
        <source>Not in use</source>
        <translation>Nincs használatban</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="40"/>
        <source>Output disabled.</source>
        <translation>Kimenet kikapcsolva.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="41"/>
        <source>Sound effect</source>
        <translation>Hangeffekt</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="42"/>
        <source>Plays an MP3 track on a DFPlayer Mini wired to TX (D1). It only plays when no PC is connected: with the GUI connected, D1 is the USB line and the board only reports the track.</source>
        <translation>MP3-számot játszik le a TX (D1) lábra kötött DFPlayer Mini modulon. Csak akkor szól, ha nincs PC csatlakoztatva: csatlakoztatott GUI mellett a D1 az USB-vonal, és a panel csak jelenti a számot.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="44"/>
        <source>Unknown type</source>
        <translation>Ismeretlen típus</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="45"/>
        <location filename="../src/gpioconfig.cpp" line="64"/>
        <source>Type value not known by this GUI.</source>
        <translation>A típusérték ismeretlen ennek a programnak.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="56"/>
        <source>Toggle</source>
        <translation>Váltás</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="56"/>
        <source>Each press (falling edge) or DCC command toggles the output.</source>
        <translation>Minden gombnyomás (lefutó él) vagy DCC-parancs átváltja a kimenetet.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="57"/>
        <source>Absolute</source>
        <translation>Abszolút</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="57"/>
        <source>The output follows the input level or the DCC direction.</source>
        <translation>A kimenet követi a bemenet szintjét vagy a DCC-irányt.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="58"/>
        <location filename="../src/gpioconfig.cpp" line="62"/>
        <location filename="../src/gpioconfig.cpp" line="63"/>
        <source>Route</source>
        <translation>Vágányút</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="58"/>
        <source>Sets up to 16 outputs one after another, each step with its own wait time. Starts when the button is pressed, or on any DCC command for its address (straight and curved).</source>
        <translation>Legfeljebb 16 kimenetet állít egymás után, minden lépés saját várakozási idővel. A gomb megnyomására vagy a címére érkező bármely DCC-parancsra indul (egyenes és kitérő irány).</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="59"/>
        <source>Input disabled.</source>
        <translation>Bemenet kikapcsolva.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="60"/>
        <source>Recorder</source>
        <translation>Rögzítő</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="60"/>
        <source>Long press = record on/off, short press = play on/off.</source>
        <translation>Hosszú nyomás = felvétel be/ki, rövid nyomás = lejátszás be/ki.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="61"/>
        <source>Feedback</source>
        <translation>Visszajelzés</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="61"/>
        <source>Sensor for the recorder; &apos;output&apos; is used as feedback id.</source>
        <translation>Érzékelő a rögzítőhöz; a &apos;kimenet&apos; visszajelzési azonosítóként szolgál.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="62"/>
        <source>Route that starts on a button press, or only on a DCC &apos;straight&apos; command for its address.</source>
        <translation>Vágányút, amely gombnyomásra indul, vagy csak a címére küldött &apos;egyenes&apos; DCC-paranccsal.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="63"/>
        <source>Route that starts on a button press, or only on a DCC &apos;curved&apos; command for its address.</source>
        <translation>Vágányút, amely gombnyomásra indul, vagy csak a címére küldött &apos;kitérő irány&apos; DCC-paranccsal.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="64"/>
        <source>Unknown</source>
        <translation>Ismeretlen</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1491"/>
        <source>Pin A</source>
        <translation>A láb</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1491"/>
        <source>Pin B</source>
        <translation>B láb</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1492"/>
        <source>Pin C</source>
        <translation>C láb</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1492"/>
        <source>Pin D</source>
        <translation>D láb</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1493"/>
        <source>Electrofrog A</source>
        <translation>Electrofrog A</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1493"/>
        <source>Electrofrog B</source>
        <translation>Electrofrog B</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1494"/>
        <source>Unifrog</source>
        <translation>Unifrog</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="444"/>
        <source>A</source>
        <translation>A</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="444"/>
        <source>B</source>
        <translation>B</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="444"/>
        <source>C</source>
        <translation>C</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="445"/>
        <source>D</source>
        <translation>D</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="445"/>
        <source>relay A</source>
        <translation>A relé</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="446"/>
        <source>relay B</source>
        <translation>B relé</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="446"/>
        <source>relay C</source>
        <translation>C relé</translation>
    </message>
</context>
<context>
    <name>Info</name>
    <message>
        <location filename="../src/infotexts.cpp" line="43"/>
        <source>No help text for &apos;%1&apos; yet.</source>
        <translation>Még nincs magyarázat ehhez: &apos;%1&apos;.</translation>
    </message>
    <message>
        <location filename="../src/infotexts.cpp" line="51"/>
        <source>What is this? Click for an explanation.</source>
        <translation>Mi ez? Kattintson a magyarázatért.</translation>
    </message>
</context>
<context>
    <name>InputEditor</name>
    <message>
        <location filename="../src/inputeditor.cpp" line="51"/>
        <source>Output %1</source>
        <translation>Kimenet %1</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="64"/>
        <location filename="../src/inputeditor.cpp" line="443"/>
        <source>Out %1 · %2</source>
        <translation>Ki %1 · %2</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="80"/>
        <source>Direct control</source>
        <translation>Közvetlen vezérlés</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="81"/>
        <source>Route</source>
        <translation>Vágányút</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="82"/>
        <source>Not in use</source>
        <translation>Nincs használatban</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="83"/>
        <location filename="../src/inputeditor.cpp" line="207"/>
        <source>Recorder</source>
        <translation>Rögzítő</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="84"/>
        <source>Feedback</source>
        <translation>Visszajelzés</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="105"/>
        <source>Behaviour</source>
        <translation>Működés</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="119"/>
        <source>Triggered by</source>
        <translation>Indítja</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="121"/>
        <source>DCC accessory address</source>
        <translation>DCC kiegészítő cím</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="131"/>
        <source>DCC</source>
        <translation>DCC</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="162"/>
        <source>Input pin</source>
        <translation>Bemeneti láb</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="193"/>
        <source>Controls</source>
        <translation>Vezérli</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="198"/>
        <location filename="../src/inputeditor.cpp" line="426"/>
        <location filename="../src/inputeditor.cpp" line="653"/>
        <source>Output</source>
        <translation>Kimenet</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="209"/>
        <source>Long press: record on / off.  Short press: play on / off.</source>
        <translation>Hosszú nyomás: felvétel be / ki.  Rövid nyomás: lejátszás be / ki.</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="217"/>
        <source>Route steps</source>
        <translation>A vágányút lépései</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="134"/>
        <source>any command (straight and curved)</source>
        <translation>bármely parancs (egyenes és kitérő irány)</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="87"/>
        <source>Function</source>
        <translation>Funkció</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="135"/>
        <source>straight only</source>
        <translation>csak egyenes</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="136"/>
        <source>curved only</source>
        <translation>csak kitérő irány</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="137"/>
        <source>With &apos;straight only&apos; and &apos;curved only&apos; two routes can share one DCC address, e.g. a level crossing: curved = close, straight = open. A button always starts the route.</source>
        <translation>A &apos;csak egyenes&apos; és &apos;csak kitérő irány&apos; beállítással két vágányút osztozhat egy DCC-címen, pl. egy vasúti átjáró: kitérő irány = zárás, egyenes = nyitás. Gombbal a vágányút mindig elindul.</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="143"/>
        <source>Route starts on</source>
        <translation>A vágányút indítója</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="147"/>
        <source>Digital — button to GND</source>
        <translation>Digitális — nyomógomb a GND-re</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="148"/>
        <source>Analog — below a value</source>
        <translation>Analóg — egy érték alatt</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="149"/>
        <source>Analog — above a value</source>
        <translation>Analóg — egy érték felett</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="150"/>
        <source>Analog — at a value (resistor ladder)</source>
        <translation>Analóg — egy értéknél (ellenálláslétra)</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="151"/>
        <source>Read as</source>
        <translation>Olvasás módja</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="167"/>
        <source>Analog pin</source>
        <translation>Analóg láb</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="173"/>
        <source>0 = 0 V, 1023 = 5 V. Stored in steps of 8.</source>
        <translation>0 = 0 V, 1023 = 5 V. 8-as lépésekben tárolva.</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="181"/>
        <source>Threshold</source>
        <translation>Küszöb</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="183"/>
        <source>Use this value</source>
        <translation>Ezt az értéket használja</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="184"/>
        <source>Take the reading as the threshold (rounded to a step of 8)</source>
        <translation>A mért érték legyen a küszöb (8-as lépésre kerekítve)</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="189"/>
        <source>Live reading</source>
        <translation>Élő mérés</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="225"/>
        <source>This board speaks protocol v1: max. 8 steps, curved / straight only, fixed 0.5 s per step.</source>
        <translation>Ez a panel a v1 protokollt beszéli: max. 8 lépés, csak kitérő / egyenes irány, lépésenként fix 0,5 s.</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="234"/>
        <source>+ Add step</source>
        <translation>+ Lépés hozzáadása</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="235"/>
        <source>Or drag an output onto this input in the canvas</source>
        <translation>Vagy húzzon egy kimenetet erre a bemenetre a vásznon</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="335"/>
        <source>Pressed below %1, released again from %2.</source>
        <translation>Lenyomva %1 alatt, újra elengedve %2-tól.</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="338"/>
        <source>Pressed above %1, released again at %2 or lower.</source>
        <translation>Lenyomva %1 felett, újra elengedve %2 értéknél vagy alatta.</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="341"/>
        <source>Pressed from %1 to %2.</source>
        <translation>Lenyomva %1 és %2 között.</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="346"/>
        <source>Now: —</source>
        <translation>Most: —</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="347"/>
        <source>Needs a board with analog input support (connected or simulated).</source>
        <translation>Analóg bemeneteket ismerő panel kell hozzá (csatlakoztatva vagy szimulálva).</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="351"/>
        <source>Now: %1</source>
        <translation>Most: %1</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="351"/>
        <source>Now: …</source>
        <translation>Most: …</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="383"/>
        <source>There already is a recorder input; only one is allowed</source>
        <translation>Már van rögzítő bemenet; csak egy engedélyezett</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="426"/>
        <source>#</source>
        <translation>#</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="426"/>
        <source>Value</source>
        <translation>Érték</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="426"/>
        <source>Wait after</source>
        <translation>Utána vár</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="440"/>
        <source>— choose an output —</source>
        <translation>— válasszon kimenetet —</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="444"/>
        <source>or drag an output onto this input</source>
        <translation>vagy húzzon egy kimenetet erre a bemenetre</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="480"/>
        <source> s</source>
        <translation> s</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="483"/>
        <source>Earlier</source>
        <translation>Korábbra</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="484"/>
        <source>Later</source>
        <translation>Későbbre</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="488"/>
        <source>Delete step %1</source>
        <translation>%1. lépés törlése</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="498"/>
        <source>Choose the aspect lamp by lamp</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="557"/>
        <source>Curved (0)</source>
        <translation>Kitérő irány (0)</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="558"/>
        <source>Straight (1)</source>
        <translation>Egyenes (1)</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="560"/>
        <source>Angle</source>
        <translation>Szög</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="578"/>
        <source>aspect </source>
        <translation>jelzési kép </translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="586"/>
        <source>PWM </source>
        <translation>PWM </translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="593"/>
        <source>track </source>
        <translation>szám </translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="595"/>
        <source>stop</source>
        <translation>leállítás</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="596"/>
        <source>MP3 track 1-255 (0001.mp3 …), 0 = stop</source>
        <translation>MP3-szám 1-255 (0001.mp3 …), 0 = leállítás</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="613"/>
        <source>%1 / %2 steps  ·  board total %3 / %4</source>
        <translation>%1 / %2 lépés  ·  panel összesen %3 / %4</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="624"/>
        <source>Input %1</source>
        <translation>Bemenet %1</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="633"/>
        <source>Toggle — every press switches the output over</source>
        <translation>Váltás — minden nyomás átváltja a kimenetet</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="634"/>
        <source>Absolute — the output follows the input</source>
        <translation>Abszolút — a kimenet követi a bemenetet</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="653"/>
        <source>Feedback id</source>
        <translation>Visszajelzési azonosító</translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <location filename="../src/mainwindow.cpp" line="88"/>
        <location filename="../src/mainwindow.cpp" line="1334"/>
        <location filename="../src/mainwindow.cpp" line="2082"/>
        <location filename="../src/mainwindow.cpp" line="2135"/>
        <location filename="../src/mainwindow.cpp" line="2255"/>
        <source>GPIOcore Configurator</source>
        <translation>GPIOcore Configurator</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="166"/>
        <source>Board read</source>
        <translation>Panel beolvasva</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="170"/>
        <source>Done with %1 error(s)</source>
        <translation>Kész, hibák száma: %1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="179"/>
        <source>Preset %1 applied, reading back</source>
        <translation>%1 preset alkalmazva, visszaolvasás</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="224"/>
        <source>Close (Esc)</source>
        <translation>Bezárás (Esc)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="228"/>
        <source>Remove</source>
        <translation>Eltávolítás</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="229"/>
        <source>Set this to &apos;not in use&apos; and take it off the canvas</source>
        <translation>Beállítás &apos;nincs használatban&apos; állapotra és levétel a munkaterületről</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="241"/>
        <source>Name, e.g. &quot;Turnout station left&quot; (kept on this computer)</source>
        <translation>Név, pl. &quot;Váltó állomás bal&quot; (ezen a számítógépen tárolva)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="252"/>
        <source>INPUT SETTINGS</source>
        <translation>BEMENET BEÁLLÍTÁSAI</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="253"/>
        <source>OUTPUT SETTINGS</source>
        <translation>KIMENET BEÁLLÍTÁSAI</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="306"/>
        <source>+ Add input</source>
        <translation>+ Bemenet hozzáadása</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="307"/>
        <source>+ Add output</source>
        <translation>+ Kimenet hozzáadása</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="308"/>
        <source>Take the next free input and open its settings</source>
        <translation>A következő szabad bemenet kiválasztása és beállításainak megnyitása</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="309"/>
        <source>Take the next free output and open its settings</source>
        <translation>A következő szabad kimenet kiválasztása és beállításainak megnyitása</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="341"/>
        <source>Configuration</source>
        <translation>Konfiguráció</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="348"/>
        <source>Pin map</source>
        <translation>Lábkiosztás</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="360"/>
        <source>Serial log</source>
        <translation>Soros napló</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="372"/>
        <source>Legacy output format 0x2D</source>
        <translation>Régi kimeneti formátum 0x2D</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="374"/>
        <source>The firmware sends/receives output data with 13 bytes; the 4th data byte (servo pattern, high bytes of times) is lost. See review: OPCODE_SET_OUTPUT_DATA.</source>
        <translation>A firmware 13 bájttal küldi/fogadja a kimeneti adatokat; a 4. adatbájt (szervóminta, az idők felső bájtjai) elveszik. Lásd a felülvizsgálatot: OPCODE_SET_OUTPUT_DATA.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="409"/>
        <source>Input %1 → output %2</source>
        <translation>Bemenet %1 → kimenet %2</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="412"/>
        <source>Input %1 already drives output %2</source>
        <translation>A(z) %1. bemenet már vezérli a(z) %2. kimenetet</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="415"/>
        <source>The route of input %1 already has %2 steps (maximum)</source>
        <translation>A(z) %1. bemenet vágányútja már %2 lépést tartalmaz (maximum)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="419"/>
        <source>The board has no room for more route steps (%1 in total)</source>
        <translation>A panelen nincs több hely vágányút-lépésnek (összesen %1)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="423"/>
        <source>Input %1 is a recorder / feedback input and cannot drive outputs</source>
        <translation>A(z) %1. bemenet rögzítő / visszajelző bemenet, nem vezérelhet kimeneteket</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="590"/>
        <source>protocol v%1 · %2 steps/route · %3 in total</source>
        <translation>v%1 protokoll · %2 lépés/vágányút · összesen %3</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="591"/>
        <source>protocol v1 · routes: 8 steps, no times</source>
        <translation>v1 protokoll · vágányutak: 8 lépés, idők nélkül</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="625"/>
        <source>Main</source>
        <translation>Fő</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="632"/>
        <source>Open</source>
        <translation>Megnyitás</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="633"/>
        <source>Save</source>
        <translation>Mentés</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="634"/>
        <source>Open a board file (Ctrl+O)</source>
        <translation>Panelfájl megnyitása (Ctrl+O)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="635"/>
        <location filename="../src/mainwindow.cpp" line="729"/>
        <source>Save to this board&apos;s file (Ctrl+S)</source>
        <translation>Mentés a panel saját fájljába (Ctrl+S)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="643"/>
        <source>  Port </source>
        <translation>  Port </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="646"/>
        <source>Refresh port list</source>
        <translation>Portlista frissítése</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="647"/>
        <location filename="../src/mainwindow.cpp" line="998"/>
        <location filename="../src/mainwindow.cpp" line="1013"/>
        <source>Connect</source>
        <translation>Csatlakozás</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="661"/>
        <source>Change the configuration: offline, or connected (red frame) so every change goes to the board at once.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="664"/>
        <source>Try your configuration without a board: press the buttons on the inputs and watch the outputs. Nothing can be changed here.</source>
        <translation>Próbálja ki a konfigurációt panel nélkül: nyomja meg a bemenetek gombjait, és figyelje a kimeneteket. Itt semmi sem módosítható.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="667"/>
        <source>Press the buttons on the inputs and watch what the real board does. Connects by itself. Blue = straight / on, red = curved / off; servos show their angle.</source>
        <translation>Nyomja meg a bemenetek gombjait, és nézze meg, mit csinál a valódi panel. Magától csatlakozik. Kék = egyenes / be, piros = kitérő irány / ki; a szervók a szögüket mutatják.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="687"/>
        <source>Read board</source>
        <translation>Panel olvasása</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="691"/>
        <location filename="../src/mainwindow.cpp" line="1381"/>
        <source>Write changes</source>
        <translation>Változások írása</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="693"/>
        <source>Revert</source>
        <translation>Változások elvetése</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="699"/>
        <source> Preset </source>
        <translation> Preset </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="701"/>
        <source>Apply</source>
        <translation>Alkalmaz</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="708"/>
        <source>Which board this is: decides which pins exist</source>
        <translation>Melyik panelről van szó: ez határozza meg, mely lábak léteznek</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="715"/>
        <source> Board </source>
        <translation> Panel </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="681"/>
        <source>Board</source>
        <translation>Panel</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="718"/>
        <source>Board settings…</source>
        <translation>Panelbeállítások…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="726"/>
        <source>&amp;Save</source>
        <translation>Men&amp;tés</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="730"/>
        <source>Save &amp;as…</source>
        <translation>Mentés má&amp;sként…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="750"/>
        <source>&amp;Phone remote…</source>
        <translation>&amp;Telefonos távvezérlés…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="986"/>
        <source>● Simulation</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="988"/>
        <source>● Connected: %1 (%2)</source>
        <translation>● Csatlakoztatva: %1 (%2)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="988"/>
        <source>board</source>
        <translation>panel</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="991"/>
        <source>◌ Connecting to %1…</source>
        <translation>◌ Csatlakozás: %1…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="993"/>
        <source>○ Offline</source>
        <translation>○ Offline</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1057"/>
        <source>Connected. Read board loads its configuration; Write all puts the one on screen on the board. Changes you make now go to the board.</source>
        <translation>Csatlakozva. A Panel olvasása betölti a panel konfigurációját; az Összes írása a képernyőn lévő konfigurációt a panelre írja. A most végzett módosítások a panelre kerülnek.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1117"/>
        <source>Read everything from the board?

What is on screen now is replaced by the board&apos;s configuration. Save it first (Ctrl+S) if you want to keep it.</source>
        <translation>Mindent beolvas a panelről?

A képernyőn lévő tartalmat a panel konfigurációja váltja fel. Ha meg szeretné tartani, előbb mentse el (Ctrl+S).</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1136"/>
        <source>the board</source>
        <translation>a panel</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1138"/>
        <source>Write the whole configuration on screen to %1?

Everything on the board is replaced.</source>
        <translation>Felírja a képernyőn lévő teljes konfigurációt ide: %1?

A panelen minden felülíródik.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1187"/>
        <source>The board keeps a different value for %1; not sending it again.</source>
        <translation>A panel más értéket tart meg ehhez: %1; nem küldjük el újra.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1246"/>
        <source>Preset applied</source>
        <translation>Preset alkalmazva</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1288"/>
        <source>Send this configuration to the connected board now?</source>
        <translation>Elküldi most ezt a konfigurációt a csatlakoztatott panelre?</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1310"/>
        <source>Loaded %1</source>
        <translation>Betöltve: %1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1536"/>
        <source>%1 (%2): conflict with the servo(s). The Servo library uses timer 1, so PWM on D9 and D10 stops working as soon as at least one servo output exists. Use D3, D5, D6 or D11 for PWM, or remove the servo.</source>
        <translation>%1 (%2): ütközés a szervó(k)kal. A Servo könyvtár az 1-es időzítőt használja, ezért a PWM a D9 és D10 lábon nem működik, amint legalább egy szervókimenet van. PWM-hez használja a D3, D5, D6 vagy D11 lábat, vagy távolítsa el a szervót.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1612"/>
        <source>The configuration has changes that are not saved in a file. Save them?</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1668"/>
        <source>The pin was in use: %1 is now not in use.</source>
        <translation>A láb foglalt volt: %1 most nincs használatban.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1706"/>
        <source>Phone: output %1 %2 = %3°</source>
        <translation>Telefon: %1. kimenet %2 = %3°</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1707"/>
        <source>straight</source>
        <translation>egyenes</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1707"/>
        <source>curved</source>
        <translation>kitérő</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1732"/>
        <source>Phone remote</source>
        <translation>Telefonos távvezérlés</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1734"/>
        <source>Adjust servos from your phone: it must be on the same Wi-Fi as this computer. Scan the code with the phone&apos;s camera, or type the address in its browser. Keep this program open; the code changes every time the remote is switched on.</source>
        <translation>Szervók beállítása telefonról: a telefonnak ugyanazon a Wi-Fi-hálózaton kell lennie, mint ennek a számítógépnek. Olvassa be a kódot a telefon kamerájával, vagy írja be a címet a böngészőjébe. Hagyja nyitva ezt a programot; a kód a távvezérlés minden bekapcsolásakor megváltozik.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1739"/>
        <source>Allow phones on my network</source>
        <translation>Telefonok engedélyezése a hálózatomon</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1753"/>
        <source>The first time, Windows may ask whether the program may use the network: allow it for private networks.</source>
        <translation>Az első alkalommal a Windows megkérdezheti, hogy a program használhatja-e a hálózatot: engedélyezze magánhálózatokhoz.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1767"/>
        <source>Off</source>
        <translation>Kikapcsolva</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1786"/>
        <source>Could not open a network port (8080-8089).</source>
        <translation>Nem sikerült hálózati portot nyitni (8080-8089).</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1813"/>
        <source>Live test: click ◀ / ▶ or ▶ start on an input. The board switches its pins and reports back; nothing needs to be connected.</source>
        <translation>Élő teszt: kattintson egy bemenetnél a ◀ / ▶ vagy a ▶ indít gombra. A panel kapcsolja a lábait és visszajelez; semmit sem kell csatlakoztatni.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1824"/>
        <source>Input %1 has no DCC address, so it cannot receive a DCC extended command.</source>
        <translation>A(z) %1. bemenetnek nincs DCC-címe, így nem fogadhat DCC extended parancsot.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1831"/>
        <source>DCC extended — address %1</source>
        <translation>DCC extended — cím: %1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1835"/>
        <source>Value sent: %1 (0x%2)</source>
        <translation>Elküldött érték: %1 (0x%2)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1856"/>
        <source>flashing</source>
        <translation>villog</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1856"/>
        <source>flashing, opposite phase</source>
        <translation>villog, ellenfázisban</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1861"/>
        <source>Lamp %1</source>
        <translation>Lámpa %1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1871"/>
        <source>PWM value</source>
        <translation>PWM-érték</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1875"/>
        <source>Sent as if your command station sent a DCC extended accessory command to address %1. Every change is sent straight away.</source>
        <translation>Úgy küldjük el, mintha a digitális központ DCC extended tartozékparancsot küldene a(z) %1 címre. Minden változás azonnal elküldésre kerül.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1880"/>
        <location filename="../src/mainwindow.cpp" line="1929"/>
        <source>Close</source>
        <translation>Bezárás</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2001"/>
        <source>&lt;h2&gt;GPIOcore Configurator&lt;/h2&gt;&lt;p&gt;Plug a GPIOcore board into a USB port, choose its port at the top and press &lt;b&gt;Connect&lt;/b&gt;. Everything you change is sent to the board straight away.&lt;/p&gt;</source>
        <translation>&lt;h2&gt;GPIOcore Configurator&lt;/h2&gt;&lt;p&gt;Dugjon egy GPIOcore panelt egy USB-portba, felül válassza ki a portját, és nyomja meg a &lt;b&gt;Csatlakozás&lt;/b&gt; gombot. Minden módosítás azonnal a panelre kerül.&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2017"/>
        <source>Connect to the board</source>
        <translation>Csatlakozás a panelhez</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2017"/>
        <source>Uses the port selected at the top.</source>
        <translation>A felül kiválasztott portot használja.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2020"/>
        <source>Open a board file…</source>
        <translation>Panelfájl megnyitása…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2020"/>
        <source>Look at or prepare the settings of a board without connecting it.</source>
        <translation>Egy panel beállításainak megtekintése vagy előkészítése csatlakoztatás nélkül.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2021"/>
        <source>New board (offline)…</source>
        <translation>Új panel (offline)…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2021"/>
        <source>Prepare a new board on this computer and send it later.</source>
        <translation>Új panel előkészítése ezen a számítógépen, és elküldése később.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2022"/>
        <source>Flash firmware…</source>
        <translation>Firmware feltöltése…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2022"/>
        <source>Put the GPIOcore firmware on an Arduino.</source>
        <translation>A GPIOcore firmware feltöltése egy Arduinóra.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2054"/>
        <source>The board is &quot;%1&quot;, the screen shows &quot;%2&quot;. The live test shows the screen&apos;s wiring, so read the board first?</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2060"/>
        <source>Connected to &quot;%1&quot;, the screen shows &quot;%2&quot;. Read board or Write all.</source>
        <translation>Csatlakozva ehhez: &quot;%1&quot;, a képernyőn ez látható: &quot;%2&quot;. Panel olvasása vagy Összes írása.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2077"/>
        <source>Names loaded from %1</source>
        <translation>Nevek betöltve innen: %1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1335"/>
        <location filename="../src/mainwindow.cpp" line="2082"/>
        <location filename="../src/mainwindow.cpp" line="2136"/>
        <location filename="../src/mainwindow.cpp" line="2255"/>
        <source>%1 — GPIOcore Configurator</source>
        <translation>%1 — GPIOcore Configurator</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2267"/>
        <source>max. 16 letters, e.g. Station-left</source>
        <translation>max. 16 karakter, pl. Állomás-bal</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2270"/>
        <source>Empty (1 input, 1 output)</source>
        <translation>Üres (1 bemenet, 1 kimenet)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2272"/>
        <source>Preset: %1</source>
        <translation>Preset: %1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2274"/>
        <source>Start with</source>
        <translation>Kiindulás</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2275"/>
        <source>When you later connect a board and give it this name, these settings belong to it. Use Write all to put them on the board.</source>
        <translation>Ha később csatlakoztat egy panelt, és ezt a nevet adja neki, ezek a beállítások hozzá tartoznak. Az Összes írása gombbal viheti át őket a panelre.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2356"/>
        <source>Output %1 removed; %2 drove it and is now not in use as well.</source>
        <translation>A(z) %1. kimenet eltávolítva; %2 vezérelte, és most az sincs használatban.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2044"/>
        <source>● %1 — %2</source>
        <translation>● %1 — %2</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2044"/>
        <source>board without a name</source>
        <translation>név nélküli panel</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2158"/>
        <source>Name this board</source>
        <translation>A panel elnevezése</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2159"/>
        <source>This board has no name yet. Give it a short, unique name (max. 16 letters, e.g. &quot;Station-left&quot;).
The name is stored on the board; your names for inputs and outputs are kept in a file with this name.</source>
        <translation>Ennek a panelnek még nincs neve. Adjon neki rövid, egyedi nevet (max. 16 karakter, pl. &quot;Allomas-bal&quot;).
A név a panelen tárolódik; a bemenetek és kimenetek Ön által adott nevei egy ilyen nevű fájlban maradnak meg.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="829"/>
        <location filename="../src/mainwindow.cpp" line="2174"/>
        <location filename="../src/mainwindow.cpp" line="2227"/>
        <source>Board settings</source>
        <translation>Panelbeállítások</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1800"/>
        <source>Live test needs a connected board with firmware v2.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2186"/>
        <source>Own wait time per route step</source>
        <translation>Saját várakozási idő minden vágányút-lépéshez</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2188"/>
        <source>Off: every step waits the same time and routes need half the memory</source>
        <translation>Ki: minden lépés ugyanannyit vár, és a vágányutak feleannyi memóriát használnak</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2193"/>
        <source> s</source>
        <translation> s</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2196"/>
        <source>Off — routes may run at the same time</source>
        <translation>Ki — a vágányutak egyszerre is futhatnak</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2197"/>
        <source>On — one route at a time</source>
        <translation>Be — egyszerre csak egy vágányút</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2198"/>
        <source>Automatic — turnout routes one at a time, lights in parallel</source>
        <translation>Automatikus — váltós vágányutak egymás után, világítás párhuzamosan</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2200"/>
        <location filename="../src/mainwindow.cpp" line="2273"/>
        <source>Name</source>
        <translation>Név</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2201"/>
        <source>Board type</source>
        <translation>Paneltípus</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2204"/>
        <source>Wait time per step</source>
        <translation>Várakozás lépésenként</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2205"/>
        <source>Route interlocking</source>
        <translation>Vágányutak kizárása</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2207"/>
        <source>This board&apos;s firmware cannot store these settings yet; they are only kept in the file.</source>
        <translation>Ennek a panelnek a firmware-je még nem tudja tárolni ezeket a beállításokat; csak a fájlban maradnak meg.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2228"/>
        <source>The recorder needs an I2C EEPROM on A4 (SDA) and A5 (SCL), but A4 or A5 is in use by an input or output. Move that first, then switch the recorder on.</source>
        <translation>A rögzítőhöz I2C EEPROM kell az A4 (SDA) és A5 (SCL) lábon, de az A4-et vagy az A5-öt egy bemenet vagy kimenet használja. Először helyezze át azt, majd kapcsolja be a rögzítőt.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2263"/>
        <source>New board</source>
        <translation>Új panel</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2312"/>
        <source>All 16 outputs are in use.</source>
        <translation>Mind a 16 kimenet foglalt.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2324"/>
        <source>All 16 inputs are in use.</source>
        <translation>Mind a 16 bemenet foglalt.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="721"/>
        <source>&amp;File</source>
        <translation>&amp;Fájl</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="127"/>
        <source>Output %1</source>
        <translation>Kimenet %1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="128"/>
        <source>frog relays A %1, B %2, unifrog %3</source>
        <translation>szívrelék: A %1, B %2, unifrog %3</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="129"/>
        <location filename="../src/mainwindow.cpp" line="130"/>
        <location filename="../src/mainwindow.cpp" line="1856"/>
        <source>on</source>
        <translation>be</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="129"/>
        <location filename="../src/mainwindow.cpp" line="130"/>
        <location filename="../src/mainwindow.cpp" line="1856"/>
        <source>off</source>
        <translation>ki</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="132"/>
        <source>track %1</source>
        <translation>%1. szám</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="132"/>
        <source>sound stopped</source>
        <translation>hang leállítva</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="133"/>
        <source>value %1</source>
        <translation>érték: %1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="134"/>
        <source>straight / on</source>
        <translation>egyenes / be</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="134"/>
        <source>curved / off</source>
        <translation>kitérő irány / ki</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="135"/>
        <source>%1 → %2</source>
        <translation>%1 → %2</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="263"/>
        <source>Lamp colours (only shown in the GUI):</source>
        <translation>Lámpaszínek (csak a programban látszanak):</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="267"/>
        <location filename="../src/mainwindow.cpp" line="273"/>
        <source>Colour of lamp %1</source>
        <translation>Lámpa %1 színe</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="660"/>
        <source>● Configure</source>
        <translation>● Konfigurálás</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="663"/>
        <source>● Simulate</source>
        <translation>● Szimuláció</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="666"/>
        <source>● Live test</source>
        <translation>● Élő teszt</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="722"/>
        <source>&amp;New board (offline)…</source>
        <translation>Ú&amp;j panel (offline)…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="723"/>
        <source>&amp;Open configuration…</source>
        <translation>Konfiguráció &amp;megnyitása…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="732"/>
        <source>Open the boards folder</source>
        <translation>A panelek mappájának megnyitása</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="737"/>
        <source>&amp;Quit</source>
        <translation>&amp;Kilépés</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="739"/>
        <source>&amp;Board</source>
        <translation>&amp;Panel</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="747"/>
        <source>Apply a preset…</source>
        <translation>Preset alkalmazása…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="749"/>
        <source>&amp;Flash firmware…</source>
        <translation>Firmware &amp;feltöltése…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="754"/>
        <source>&amp;View</source>
        <translation>&amp;Nézet</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="755"/>
        <source>&amp;Expert mode</source>
        <translation>&amp;Szakértői mód</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="757"/>
        <source>Show bytes, the serial log and protocol details</source>
        <translation>Bájtok, soros napló és protokollrészletek megjelenítése</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="759"/>
        <source>Show unused inputs and outputs</source>
        <translation>Nem használt bemenetek és kimenetek megjelenítése</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="768"/>
        <source>Show TX/RX bytes in log</source>
        <translation>TX/RX bájtok megjelenítése a naplóban</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="771"/>
        <source>Clear log</source>
        <translation>Napló törlése</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="777"/>
        <source>Theme</source>
        <translation>Téma</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="779"/>
        <source>Automatic (Windows)</source>
        <translation>Automatikus (Windows)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="780"/>
        <source>Light</source>
        <translation>Világos</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="781"/>
        <source>Dark</source>
        <translation>Sötét</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="798"/>
        <source>Language</source>
        <translation>Nyelv</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="808"/>
        <source>The language changes the next time you start the program.</source>
        <translation>A nyelv a program következő indításakor változik meg.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="811"/>
        <source>Automatic (system language)</source>
        <translation>Automatikus (rendszer nyelve)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="820"/>
        <source>&amp;Help</source>
        <translation>&amp;Súgó</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="821"/>
        <source>&amp;Getting started</source>
        <translation>&amp;Első lépések</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="822"/>
        <source>Getting started</source>
        <translation>Első lépések</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="825"/>
        <source>&amp;Configure, simulate and live test</source>
        <translation>&amp;Konfigurálás, szimuláció és élő teszt</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="826"/>
        <source>Configure, simulate and live test</source>
        <translation>Konfigurálás, szimuláció és élő teszt</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="828"/>
        <source>&amp;Board settings</source>
        <translation>&amp;Panelbeállítások</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="831"/>
        <source>Config &amp;button and teaching servos (9999)</source>
        <translation>Konfig&amp;gomb és szervók betanítása (9999)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="832"/>
        <source>Config button and teaching servos</source>
        <translation>Konfiggomb és szervók betanítása</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="835"/>
        <source>&amp;About</source>
        <translation>&amp;Névjegy</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="837"/>
        <source>&lt;b&gt;GPIOcore Configurator&lt;/b&gt; %1&lt;br&gt;&lt;br&gt;Configures GPIOcore boards (Arduino Nano / Uno, DCCnext) for model railways.&lt;br&gt;Language: %2</source>
        <translation>&lt;b&gt;GPIOcore Configurator&lt;/b&gt; %1&lt;br&gt;&lt;br&gt;GPIOcore panelek (Arduino Nano / Uno, DCCnext) beállítása modellvasutakhoz.&lt;br&gt;Nyelv: %2</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="861"/>
        <source>no serial ports found</source>
        <translation>nem található soros port</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="884"/>
        <source>Select a serial port first, or use Demo.</source>
        <translation>Először válasszon soros portot, vagy használja a Demót.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="897"/>
        <source>Could not open %1:
%2</source>
        <translation>Nem sikerült megnyitni: %1
%2</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="998"/>
        <location filename="../src/mainwindow.cpp" line="1013"/>
        <source>Disconnect</source>
        <translation>Bontás</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1017"/>
        <source>● %1 — waiting for board…</source>
        <translation>● %1 — várakozás a panelre…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1022"/>
        <source>○ Not connected</source>
        <translation>○ Nincs csatlakoztatva</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1041"/>
        <source>● %1 — GPIOcore %2</source>
        <translation>● %1 — GPIOcore %2</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1166"/>
        <location filename="../src/mainwindow.cpp" line="1663"/>
        <location filename="../src/mainwindow.cpp" line="2351"/>
        <source>input %1</source>
        <translation>bemenet %1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1176"/>
        <location filename="../src/mainwindow.cpp" line="1652"/>
        <source>output %1</source>
        <translation>kimenet %1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1183"/>
        <source>Writing %1 item(s), each is read back for verification</source>
        <translation>Elemek írása: %1, mindegyik visszaolvasásra kerül ellenőrzésként</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1206"/>
        <source>Apply preset &quot;%1&quot;?

This replaces all outputs and inputs (and their routes).</source>
        <translation>Alkalmazza a(z) „%1” előbeállítást?

Ez lecseréli az összes kimenetet és bemenetet (és a vágányútjaikat).</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1208"/>
        <source>Also clear the names of inputs and outputs (recommended)</source>
        <translation>A bemenetek és kimenetek nevét is törli (ajánlott)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1524"/>
        <source>The firmware on this board has no sound effect output yet: update the firmware (Board &gt; Flash firmware).</source>
        <translation>A panel firmware-ében még nincs hangeffekt-kimenet: frissítse a firmware-t (Panel &gt; Firmware feltöltése).</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1564"/>
        <source>The firmware on this board cannot read analog inputs yet: update the firmware (Board &gt; Flash firmware).</source>
        <translation>A panel firmware-e még nem tud analóg bemenetet olvasni: frissítse a firmware-t (Panel &gt; Firmware feltöltése).</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1898"/>
        <source>Value on A%1 (0–1023)</source>
        <translation>Érték az A%1 lábon (0–1023)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1914"/>
        <source>A%1</source>
        <translation>A%1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1920"/>
        <source>In %1: %2 %3</source>
        <translation>Be %1: %2 %3</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1925"/>
        <source>1023 = 5 V, nothing pressed. Every input on A%1 reacts: %2</source>
        <translation>1023 = 5 V, semmi sincs lenyomva. Minden bemenet az A%1 lábon reagál: %2</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2018"/>
        <source>Simulate</source>
        <translation>Szimuláció</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2018"/>
        <source>Try a configuration without a board: open a board file or start a new one, then simulate it. Nothing can break.</source>
        <translation>Próbáljon ki egy konfigurációt panel nélkül: nyisson meg egy panelfájlt vagy kezdjen újat, majd szimulálja. Semmi sem romolhat el.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2182"/>
        <source>Use the recorder (needs an I2C EEPROM on A4/A5)</source>
        <translation>Rögzítő használata (I2C EEPROM kell az A4/A5 lábon)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="2184"/>
        <source>The recorder stores its recording in an external 24LC256 EEPROM. While it is on, A4 and A5 cannot be used as inputs or outputs.</source>
        <translation>A rögzítő egy külső 24LC256 EEPROM-ban tárolja a felvételt. Amíg be van kapcsolva, az A4 és A5 nem használható bemenetként vagy kimenetként.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1261"/>
        <source>Open configuration</source>
        <translation>Konfiguráció megnyitása</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="688"/>
        <source>Load the configuration from the board (replaces what is on screen)</source>
        <translation>A konfiguráció betöltése a panelről (felváltja a képernyőn lévőt)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="689"/>
        <source>Write all</source>
        <translation>Összes írása</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="690"/>
        <source>Put the whole configuration on screen on the board</source>
        <translation>A képernyőn lévő teljes konfiguráció átvitele a panelre</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1262"/>
        <location filename="../src/mainwindow.cpp" line="1321"/>
        <source>GPIOcore configuration (*.json)</source>
        <translation>GPIOcore konfiguráció (*.json)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1319"/>
        <source>Save configuration</source>
        <translation>Konfiguráció mentése</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1336"/>
        <source>Saved %1</source>
        <translation>Mentve: %1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1381"/>
        <source>Write %1 change(s)</source>
        <translation>Változások írása (%1)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1387"/>
        <source>Saving on the board…</source>
        <translation>Mentés a panelre…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1387"/>
        <source>%1 change(s) waiting</source>
        <translation>%1 várakozó változás</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1389"/>
        <source>%1 unsent change(s)</source>
        <translation>Nem elküldött változások: %1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1472"/>
        <location filename="../src/mainwindow.cpp" line="1473"/>
        <source>Output %1 · %2</source>
        <translation>Kimenet %1 · %2</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1502"/>
        <source>%1 is not set.</source>
        <translation>%1 nincs beállítva.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1506"/>
        <source>%1 uses %2, which is the %3.</source>
        <translation>%1 használja: %2 (%3).</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1511"/>
        <source>%1 (%2) is also used by output %3 (%4).</source>
        <translation>%1 (%2) a(z) %3. kimenet (%4) is használja.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1512"/>
        <source>%1 (%2) is also used by input %3.</source>
        <translation>%1 (%2) a(z) %3. bemenet is használja.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1515"/>
        <source>%1 (%2) is not PWM capable; %3 needs PWM.</source>
        <translation>%1 (%2) nem PWM-képes; %3 PWM-et igényel.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1519"/>
        <source>This board runs old firmware, which never ends the pulse — a coil can stay energised. Update the firmware (review F9).</source>
        <translation>Ezen a panelen régi firmware fut, amely sosem fejezi be az impulzust — egy tekercs áram alatt maradhat. Frissítse a firmware-t (felülvizsgálat F9).</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1522"/>
        <source>Min is larger than max: positions are swapped.</source>
        <translation>A min nagyobb, mint a max: a helyzetek fel vannak cserélve.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1527"/>
        <source>%1 (%2) is a serial pin: it only works when no GUI is connected.</source>
        <translation>%1 (%2) soros láb: csak akkor működik, ha nincs program csatlakoztatva.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1556"/>
        <source>Pin %1 is the %2.</source>
        <translation>A(z) %1 láb foglalt: %2.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1560"/>
        <source>Pin %1 is also used by output %2.</source>
        <translation>A(z) %1 lábat a(z) %2. kimenet is használja.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1561"/>
        <source>Pin %1 is also used by input %2.</source>
        <translation>A(z) %1 lábat a(z) %2. bemenet is használja.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1566"/>
        <source>This type needs an input pin.</source>
        <translation>Ehhez a típushoz bemeneti láb kell.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1568"/>
        <source>Input %1 is a recorder too. Only one recorder per board is allowed.</source>
        <translation>A(z) %1. bemenet is rögzítő. Panelenként csak egy rögzítő engedélyezett.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1570"/>
        <source>Neither a DCC address nor a pin is set: this input never triggers.</source>
        <translation>Sem DCC-cím, sem láb nincs beállítva: ez a bemenet sosem indít.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1575"/>
        <source>Route step %1 sets output %2, which is not in use.</source>
        <translation>A(z) %1. vágányút-lépés a(z) %2. kimenetet állítja, amely nincs használatban.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1578"/>
        <source>This board stores only %1 route steps (protocol v1); steps %2 and later are lost.</source>
        <translation>Ez a panel csak %1 vágányút-lépést tárol (v1 protokoll); a(z) %2. és további lépések elvesznek.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1581"/>
        <source>All routes together use %1 steps; the board has room for %2.</source>
        <translation>Az összes vágányút együtt %1 lépést használ; a panelen %2 számára van hely.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1584"/>
        <source>Output %1 is not in use.</source>
        <translation>A(z) %1. kimenet nincs használatban.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1623"/>
        <source>%1 change(s) have not been written to the board. Quit anyway?</source>
        <translation>%1 változás nincs a panelre írva. Mégis kilép?</translation>
    </message>
</context>
<context>
    <name>OutputEditor</name>
    <message>
        <location filename="../src/outputeditor.cpp" line="80"/>
        <source>Type</source>
        <translation>Típus</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="97"/>
        <location filename="../src/outputeditor.cpp" line="478"/>
        <source>Pins</source>
        <translation>Lábak</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="99"/>
        <source>Pin A</source>
        <translation>A láb</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="99"/>
        <source>Pin B</source>
        <translation>B láb</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="99"/>
        <source>Pin C</source>
        <translation>C láb</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="99"/>
        <source>Pin D</source>
        <translation>D láb</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="116"/>
        <source>Frog polarity</source>
        <translation>Szív polaritása</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="122"/>
        <source>None (dead frog)</source>
        <translation>Nincs (szigetelt szív)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="123"/>
        <source>Electrofrog — 2 relays</source>
        <translation>Electrofrog — 2 relé</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="124"/>
        <source>Unifrog — 1 change-over relay</source>
        <translation>Unifrog — 1 váltóérintkezős relé</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="128"/>
        <source>Frog</source>
        <translation>Szív</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="129"/>
        <source>Relay straight (A)</source>
        <translation>Relé egyenes (A)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="129"/>
        <source>Relay curved (B)</source>
        <translation>Relé kitérő irány (B)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="129"/>
        <source>Unifrog relay</source>
        <translation>Unifrog relé</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="143"/>
        <source>Parameters</source>
        <translation>Paraméterek</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="149"/>
        <source>This type has no parameters.</source>
        <translation>Ennek a típusnak nincsenek paraméterei.</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="163"/>
        <source>Switch time</source>
        <translation>Kapcsolási idő</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="173"/>
        <source>LED on time</source>
        <translation>LED bekapcsolási idő</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="174"/>
        <source>Pause</source>
        <translation>Szünet</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="193"/>
        <source>Live: move the servo while adjusting</source>
        <translation>Élő: a szervó beállítás közben mozog</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="198"/>
        <source>Position 0 (curved)</source>
        <translation>0. helyzet (kitérő irány)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="199"/>
        <source>Position 1 (straight)</source>
        <translation>1. helyzet (egyenes)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="200"/>
        <source> ms/°</source>
        <translation> ms/°</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="203"/>
        <source>Speed</source>
        <translation>Sebesség</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="218"/>
        <source>Duty cycle</source>
        <translation>Kitöltési tényező</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="228"/>
        <source> Hz</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="230"/>
        <source>Flashing</source>
        <translation>Villogás</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="231"/>
        <source>How fast flashing lamps blink (1 Hz = once per second). The aspect itself is chosen with DCC extended or in a route.</source>
        <translation>Milyen gyorsan villognak a lámpák (1 Hz = másodpercenként egyszer). Magát a jelzési képet DCC extended-del vagy vágányútban választja.</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="245"/>
        <source>stop</source>
        <translation>leállítás</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="253"/>
        <source>Track when curved / off</source>
        <translation>Szám kitérő irány / ki esetén</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="254"/>
        <source>Track when straight / on</source>
        <translation>Szám egyenes / be esetén</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="255"/>
        <source>Track n is the file 000n.mp3 on the SD card (0001.mp3, 0002.mp3 …). A route step or a DCC extended command plays its own track number.</source>
        <translation>Az n. szám az SD-kártya 000n.mp3 fájlja (0001.mp3, 0002.mp3 …). Egy vágányút-lépés vagy egy DCC extended parancs a saját számát játssza le.</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="264"/>
        <source>This board runs old firmware (output opcode 0x2D, 13 bytes): the 4th data byte is not transferred. Update the firmware (review F1).</source>
        <translation>Ezen a panelen régi firmware fut (kimeneti opcode 0x2D, 13 bájt): a 4. adatbájt nem kerül átvitelre. Frissítse a firmware-t (felülvizsgálat F1).</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="271"/>
        <source>State</source>
        <translation>Állapot</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="278"/>
        <source>0  (off / curved)</source>
        <translation>0  (ki / kitérő irány)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="279"/>
        <source>1  (on / straight)</source>
        <translation>1  (be / egyenes)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="280"/>
        <source>▶ Test: curved / off</source>
        <translation>▶ Teszt: kitérő irány / ki</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="281"/>
        <source>▶ Test: straight / on</source>
        <translation>▶ Teszt: egyenes / be</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="284"/>
        <source>Writes this output to the board with the chosen state (also stored in EEPROM)</source>
        <translation>A kimenetet a választott állapottal a panelre írja (az EEPROM-ba is mentve)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="287"/>
        <source>Stored state</source>
        <translation>Tárolt állapot</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="428"/>
        <source>Output %1</source>
        <translation>Kimenet %1</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="436"/>
        <source>Both electrofrog and unifrog relays were set. Only one frog mode is allowed: the unifrog relay is removed when you change this output.</source>
        <translation>Electrofrog és Unifrog relék is be vannak állítva. Csak egy szívmód engedélyezett: az Unifrog relé törlődik, amikor módosítja ezt a kimenetet.</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="456"/>
        <source>Needs a connected board that speaks protocol v2 (SERVO_LIVE)</source>
        <translation>Csatlakoztatott, v2 protokollt beszélő panel szükséges (SERVO_LIVE)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="478"/>
        <source>Pins (PWM capable: D3 D5 D6 D9 D10 D11)</source>
        <translation>Lábak (PWM-képes: D3 D5 D6 D9 D10 D11)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="510"/>
        <source>× %1 ms  =  %2 ms</source>
        <translation>× %1 ms  =  %2 ms</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="511"/>
        <source>× %1 ms  →  on %2 ms, pause %3 ms</source>
        <translation>× %1 ms  →  be %2 ms, szünet %3 ms</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="514"/>
        <source>Full sweep ≈ %1 ms</source>
        <translation>Teljes mozgás ≈ %1 ms</translation>
    </message>
</context>
<context>
    <name>PinMapView</name>
    <message>
        <location filename="../src/pinmapview.cpp" line="45"/>
        <source>Pin usage (Arduino Uno / Nano)</source>
        <translation>Lábhasználat (Arduino Uno / Nano)</translation>
    </message>
    <message>
        <location filename="../src/pinmapview.cpp" line="52"/>
        <source>click a card to edit  ·  red = conflict</source>
        <translation>kattintson egy kártyára a szerkesztéshez  ·  piros = ütközés</translation>
    </message>
    <message>
        <location filename="../src/pinmapview.cpp" line="107"/>
        <source>Out %1 %2 · %3</source>
        <translation>Ki %1 %2 · %3</translation>
    </message>
    <message>
        <location filename="../src/pinmapview.cpp" line="110"/>
        <source>In %1 · %2</source>
        <translation>Be %1 · %2</translation>
    </message>
    <message>
        <location filename="../src/pinmapview.cpp" line="114"/>
        <source>free</source>
        <translation>szabad</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../src/connectionview.cpp" line="555"/>
        <source>DCC %1 straight</source>
        <translation>DCC %1 egyenes</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="556"/>
        <source>DCC %1 curved</source>
        <translation>DCC %1 kitérő irány</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="565"/>
        <source>no trigger</source>
        <translation>nincs indító</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="325"/>
        <source>lamp %1 flashing</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="326"/>
        <source>lamp %1 flashing, opposite phase</source>
        <translation>%1. lámpa ellenütemben villog</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="327"/>
        <source>lamp %1 on</source>
        <translation>%1. lámpa világít</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="329"/>
        <source>all lamps off</source>
        <translation>minden lámpa kialudt</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="335"/>
        <source>Signal aspect</source>
        <translation>Jelzési kép</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="337"/>
        <source>Choose per lamp. Lamp 1 is pin A, lamp 2 pin B, and so on.</source>
        <translation>Válasszon lámpánként. Az 1. lámpa az A láb, a 2. a B láb, és így tovább.</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="357"/>
        <source>Value: %1  (0x%2)</source>
        <translation>Érték: %1  (0x%2)</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="363"/>
        <source>off</source>
        <translation>ki</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="363"/>
        <source>on</source>
        <translation>be</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="363"/>
        <source>flashing</source>
        <translation>villog</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="364"/>
        <source>flashing, opposite phase</source>
        <translation>villog, ellenfázisban</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="373"/>
        <source>Lamp %1</source>
        <translation>Lámpa %1</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="409"/>
        <source>Out %1</source>
        <translation>Ki %1</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="409"/>
        <source>In %1</source>
        <translation>Be %1</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="416"/>
        <source>shared with: %1</source>
        <translation>közösen: %1</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="423"/>
        <source>in use: %1</source>
        <translation>foglalt: %1</translation>
    </message>
</context>
</TS>
