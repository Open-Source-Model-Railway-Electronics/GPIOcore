<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="da_DK">
<context>
    <name>ConnectionView</name>
    <message>
        <location filename="../src/connectionview.cpp" line="276"/>
        <source>In %1</source>
        <translation>Ind %1</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="329"/>
        <source>Out %1</source>
        <translation>Ud %1</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="390"/>
        <source>Inputs</source>
        <translation>Indgange</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="391"/>
        <source>Outputs</source>
        <translation>Udgange</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="398"/>
        <source>drag an output onto an input</source>
        <translation>træk en udgang hen på en indgang</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="539"/>
        <source>Remove connection In %1 → Out %2</source>
        <translation>Fjern forbindelse Ind %1 → Ud %2</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="544"/>
        <source>Set curved (0)</source>
        <translation>Sæt til afvigende (0)</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="544"/>
        <source>Set straight (1)</source>
        <translation>Sæt til lige (1)</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="547"/>
        <source>Edit value in the input panel</source>
        <translation>Rediger værdien i indgangspanelet</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="549"/>
        <source>Move earlier in the route</source>
        <translation>Tidligere i togvejen</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="552"/>
        <source>Move later in the route</source>
        <translation>Senere i togvejen</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="559"/>
        <source>Remove all connections of In %1</source>
        <translation>Fjern alle forbindelser fra Ind %1</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="582"/>
        <source>In %1 → Out %2 (%3)</source>
        <translation>Ind %1 → Ud %2 (%3)</translation>
    </message>
    <message>
        <location filename="../src/connectionview.cpp" line="584"/>
        <source>
step %1: %2, then wait %3</source>
        <translation>
trin %1: %2, vent derefter %3</translation>
    </message>
</context>
<context>
    <name>DeviceLink</name>
    <message>
        <location filename="../src/devicelink.cpp" line="56"/>
        <source>Serial port lost: %1</source>
        <translation>Seriel port mistet: %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="94"/>
        <source>Opened %1</source>
        <translation>%1 åbnet</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="100"/>
        <source>Waiting for boot message…</source>
        <translation>Venter på opstartsbesked…</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="129"/>
        <source>Closed %1</source>
        <translation>%1 lukket</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="218"/>
        <source>Firmware uses legacy SET_OUTPUT_DATA (0x2D, 13 bytes): the 4th data byte (servo pattern / high byte of times) is not transmitted.</source>
        <translation>Firmwaren bruger det gamle SET_OUTPUT_DATA (0x2D, 13 bytes): den 4. databyte (servomønster / høj byte af tider) sendes ikke.</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="278"/>
        <source>This firmware speaks protocol v1: routes are stored in the 8 linked bytes of SET_INPUT_DATA (state only, fixed 0.5 s per step).</source>
        <translation>Denne firmware taler protokol v1: togveje gemmes i de 8 sammenkædede bytes i SET_INPUT_DATA (kun tilstand, fast 0,5 s pr. trin).</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="324"/>
        <source>No answer to %1, retrying</source>
        <translation>Intet svar på %1, prøver igen</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="330"/>
        <source>No HELLO reply: firmware speaks protocol v1</source>
        <translation>Intet HELLO-svar: firmwaren taler protokol v1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="342"/>
        <source>No answer to %1</source>
        <translation>Intet svar på %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="357"/>
        <source>No boot message, requesting version</source>
        <translation>Ingen opstartsbesked, beder om version</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="372"/>
        <source>Incomplete frame discarded</source>
        <translation>Ufuldstændig ramme kasseret</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="400"/>
        <source>NACK (%1) on %2</source>
        <translation>NACK (%1) på %2</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="409"/>
        <source>Device rejected %1: %2</source>
        <translation>Enheden afviste %1: %2</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="477"/>
        <source>Protocol v%1: %2 steps per route, %3 steps in total%4</source>
        <translation>Protokol v%1: %2 trin pr. togvej, %3 trin i alt%4</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="479"/>
        <source>, external EEPROM</source>
        <translation>, ekstern EEPROM</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="548"/>
        <source>Preset broadcast ignored (old payload layout)</source>
        <translation>Preset-broadcast ignoreret (gammelt format)</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="573"/>
        <source>Unsolicited %1 ignored</source>
        <translation>Uopfordret %1 ignoreret</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="580"/>
        <source>GPIOcore version &apos;%1&apos;</source>
        <translation>GPIOcore-version &apos;%1&apos;</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="591"/>
        <source>version request</source>
        <translation>versionsforespørgsel</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="592"/>
        <source>read input %1</source>
        <translation>læs indgang %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="593"/>
        <source>read output %1</source>
        <translation>læs udgang %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="594"/>
        <source>write input %1</source>
        <translation>skriv indgang %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="595"/>
        <source>write output %1</source>
        <translation>skriv udgang %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="596"/>
        <source>preset %1</source>
        <translation>preset %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="597"/>
        <source>hello</source>
        <translation>hello</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="598"/>
        <source>read route %1</source>
        <translation>læs togvej %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="599"/>
        <source>write route %1</source>
        <translation>skriv togvej %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="600"/>
        <source>live servo %1</source>
        <translation>live servo %1</translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="601"/>
        <source>read board settings</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/devicelink.cpp" line="602"/>
        <source>write board settings</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>FlashDialog</name>
    <message>
        <location filename="../src/flasher.cpp" line="387"/>
        <source>Update firmware</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="394"/>
        <source>Puts the GPIOcore program (firmware) on your Arduino. Plug the board in with USB, check the settings below and click &lt;b&gt;Flash&lt;/b&gt;. Your settings on the board are normally kept.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="407"/>
        <source>Refresh</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="408"/>
        <source>Look again for connected boards</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="412"/>
        <source>Port:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="416"/>
        <source>Browse…</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="420"/>
        <source>Firmware file:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="424"/>
        <source>Arduino Uno / Nano (new bootloader)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="425"/>
        <source>Arduino Nano (old bootloader)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="426"/>
        <source>Via ArduinoISP programmer (advanced)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="432"/>
        <source>Board type:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="435"/>
        <source>&lt;b&gt;Advanced:&lt;/b&gt; choose the port of the &lt;i&gt;programmer&lt;/i&gt; Arduino (running ArduinoISP), wired to the ICSP pins of your board. This also sets the fuses and &lt;b&gt;removes the USB bootloader&lt;/b&gt;: afterwards the board can only be updated with the programmer again. The settings stored on the board are erased.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="444"/>
        <source>Flash</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="483"/>
        <source>Close</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="508"/>
        <source>Trying again with the old Nano bootloader…</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="509"/>
        <location filename="../src/flasher.cpp" line="573"/>
        <source>Uploading… do not unplug the board.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="529"/>
        <source>(no board found – plug it in and click Refresh)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="544"/>
        <source>Choose firmware file</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="545"/>
        <source>Firmware (*.hex);;All files (*)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="560"/>
        <source>The firmware file was not found.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="565"/>
        <source>No board found.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="599"/>
        <source>Done! The new firmware is on the board.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="602"/>
        <source>Your board has the old Nano bootloader. Next time choose &quot;Arduino Nano (old bootloader)&quot;.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="610"/>
        <source>Uploading did not work.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="624"/>
        <source>The folder &lt;i&gt;tools/avrdude&lt;/i&gt; next to GPIOcoreGUI is missing. Install the program again.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="626"/>
        <source>Click &lt;b&gt;Browse…&lt;/b&gt; and choose the GPIOcore &lt;i&gt;.hex&lt;/i&gt; file.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="628"/>
        <source>Close other programs that use the board (Arduino IDE serial monitor, other configuration tools) and try again.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="631"/>
        <source>Check the USB cable, click &lt;b&gt;Refresh&lt;/b&gt; and choose the right port.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="634"/>
        <source>Check that the programmer Arduino runs ArduinoISP, that the right port is chosen and that the wires to the board are correct. A 10 µF capacitor between RESET and GND of the programmer often helps.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="637"/>
        <source>Choose another &lt;b&gt;board type&lt;/b&gt; (for example &quot;Arduino Nano (old bootloader)&quot;) and try again. Also check that the right port is chosen and close other programs that use the board.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="640"/>
        <source>Check the six wires between the programmer and the board, and that the board has power.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="641"/>
        <source>This board does not have an ATmega328P chip. GPIOcore needs an Arduino Uno or Nano.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="643"/>
        <source>Try again. If it keeps failing, try another USB cable or USB port.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="647"/>
        <source>Click &lt;b&gt;Show details&lt;/b&gt; to see what happened.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="673"/>
        <source>Hide details</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="673"/>
        <source>Show details</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>Flasher</name>
    <message>
        <location filename="../src/flasher.cpp" line="167"/>
        <source>The upload program (avrdude) was not found next to GPIOcoreGUI (tools/avrdude).</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="170"/>
        <source>The firmware file was not found: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="173"/>
        <source>No port selected.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="308"/>
        <source>Cancelled.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="312"/>
        <source>Firmware written and checked.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="319"/>
        <source>No answer at 115200 baud. Trying again at 57600 baud (old Nano bootloader)...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="326"/>
        <source>The port could not be opened: another program is using it.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="327"/>
        <source>The port could not be opened: the board was not found.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="328"/>
        <source>The firmware file could not be read.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="329"/>
        <source>The board did not answer.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="330"/>
        <source>The chip on the board was not recognised.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="331"/>
        <source>Writing failed: the check after writing found differences.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="333"/>
        <source>Uploading failed (avrdude exit code %1).</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="334"/>
        <source>The upload program stopped unexpectedly.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/flasher.cpp" line="344"/>
        <source>The upload program (avrdude) could not be started: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>Gpio</name>
    <message>
        <location filename="../src/gpioconfig.cpp" line="13"/>
        <source>Single steady</source>
        <translation>Enkelt fast</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="14"/>
        <source>One pin follows the state.</source>
        <translation>Én pin følger tilstanden.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="15"/>
        <source>Single pulse</source>
        <translation>Enkelt puls</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="16"/>
        <source>One pin is pulsed for the switch time (firmware: value x 100 ms).</source>
        <translation>Én pin får en puls i omstillingstiden (firmware: værdi x 100 ms).</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="17"/>
        <source>Single flashing</source>
        <translation>Enkelt blink</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="18"/>
        <source>Pin A blinks while the state is on (value x 10 ms per half period).</source>
        <translation>Pin A blinker, så længe tilstanden er tændt (værdi x 10 ms pr. halv periode).</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="19"/>
        <source>Double steady</source>
        <translation>Dobbelt fast</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="20"/>
        <source>Pin A = state, pin B = inverted state.</source>
        <translation>Pin A = tilstand, pin B = omvendt tilstand.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="21"/>
        <source>Double pulse</source>
        <translation>Dobbelt puls</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="22"/>
        <source>Turnout coil pair: pin A or B is pulsed (value x 10 ms).</source>
        <translation>Spolepar i et sporskifte: pin A eller B får en puls (værdi x 10 ms).</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="23"/>
        <source>Double flashing</source>
        <translation>Dobbelt blink</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="24"/>
        <source>Pins A and B blink alternately (value x 10 ms per half period).</source>
        <translation>Pin A og B blinker skiftevis (værdi x 10 ms pr. halv periode).</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="25"/>
        <source>Swing light</source>
        <translation>Vekselblink</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="26"/>
        <source>Level crossing style: A, B, pause (values x 10 ms).</source>
        <translation>Som ved en overkørsel: A, B, pause (værdier x 10 ms).</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="27"/>
        <source>Fireplace</source>
        <translation>Pejs</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="28"/>
        <source>Random flicker on PWM pins A and B.</source>
        <translation>Tilfældig flimren på PWM-pin A og B.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="29"/>
        <source>TL tube</source>
        <translation>Lysstofrør</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="30"/>
        <source>Fluorescent tube start-up flicker on PWM pin A.</source>
        <translation>Opstartsflimren fra et lysstofrør på PWM-pin A.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="31"/>
        <source>Welder</source>
        <translation>Svejser</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="32"/>
        <source>Random welding flashes on pin A.</source>
        <translation>Tilfældige svejseglimt på pin A.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="33"/>
        <source>4-aspect signal</source>
        <translation>Signal med 4 signalbilleder</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="34"/>
        <source>Four LEDs A-D. The aspect comes from DCC extended (signal) packets or a route; the board keeps the last aspect in EEPROM.</source>
        <translation>Fire LED&apos;er A-D. Signalbilledet kommer fra udvidede DCC-kommandoer (signal) eller en togvej; printet husker det sidste signalbillede i EEPROM.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="35"/>
        <source>Servo</source>
        <translation>Servo</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="36"/>
        <source>Servo on pin A, sweeps between min and max.</source>
        <translation>Servo på pin A, bevæger sig mellem min og max.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="37"/>
        <source>PWM</source>
        <translation>PWM</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="38"/>
        <source>Fixed PWM value on pin A.</source>
        <translation>Fast PWM-værdi på pin A.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="39"/>
        <location filename="../src/gpioconfig.cpp" line="56"/>
        <source>Not in use</source>
        <translation>Ikke i brug</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="40"/>
        <source>Output disabled.</source>
        <translation>Udgang deaktiveret.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="41"/>
        <source>Unknown type</source>
        <translation>Ukendt type</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="42"/>
        <location filename="../src/gpioconfig.cpp" line="59"/>
        <source>Type value not known by this GUI.</source>
        <translation>Typeværdien kendes ikke af dette program.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="53"/>
        <source>Toggle</source>
        <translation>Skift</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="53"/>
        <source>Each press (falling edge) or DCC command toggles the output.</source>
        <translation>Hvert tryk (faldende flanke) eller DCC-kommando skifter udgangen.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="54"/>
        <source>Absolute</source>
        <translation>Absolut</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="54"/>
        <source>The output follows the input level or the DCC direction.</source>
        <translation>Udgangen følger indgangens niveau eller DCC-retningen.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="55"/>
        <source>Route</source>
        <translation>Togvej</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="55"/>
        <source>Sets up to 16 outputs one after another, each step with its own wait time. Starts when the button is pressed, or on any DCC command for its address (straight and curved).</source>
        <translation>Stiller op til 16 udgange efter hinanden, hvert trin med sin egen ventetid. Starter når knappen trykkes, eller ved enhver DCC-kommando til dens adresse (lige og afvigende).</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="56"/>
        <source>Input disabled.</source>
        <translation>Indgang deaktiveret.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="57"/>
        <source>Recorder</source>
        <translation>Optager</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="57"/>
        <source>Long press = record on/off, short press = play on/off.</source>
        <translation>Langt tryk = optagelse til/fra, kort tryk = afspilning til/fra.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="58"/>
        <source>Feedback</source>
        <translation>Tilbagemelding</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="58"/>
        <source>Sensor for the recorder; &apos;output&apos; is used as feedback id.</source>
        <translation>Sensor til optageren; &apos;udgang&apos; bruges som tilbagemeldings-id.</translation>
    </message>
    <message>
        <location filename="../src/gpioconfig.cpp" line="59"/>
        <source>Unknown</source>
        <translation>Ukendt</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1035"/>
        <source>Pin A</source>
        <translation>Pin A</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1035"/>
        <source>Pin B</source>
        <translation>Pin B</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1036"/>
        <source>Pin C</source>
        <translation>Pin C</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1036"/>
        <source>Pin D</source>
        <translation>Pin D</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1037"/>
        <source>Electrofrog A</source>
        <translation>Electrofrog A</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1037"/>
        <source>Electrofrog B</source>
        <translation>Electrofrog B</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1038"/>
        <source>Unifrog</source>
        <translation>Unifrog</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="183"/>
        <source>A</source>
        <translation>A</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="183"/>
        <source>B</source>
        <translation>B</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="183"/>
        <source>C</source>
        <translation>C</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="184"/>
        <source>D</source>
        <translation>D</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="184"/>
        <source>relay A</source>
        <translation>relæ A</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="185"/>
        <source>relay B</source>
        <translation>relæ B</translation>
    </message>
    <message>
        <location filename="../src/uihelpers.cpp" line="185"/>
        <source>relay C</source>
        <translation>relæ C</translation>
    </message>
</context>
<context>
    <name>Info</name>
    <message>
        <location filename="../src/infotexts.cpp" line="43"/>
        <source>No help text for &apos;%1&apos; yet.</source>
        <translation>Endnu ingen forklaring til &apos;%1&apos;.</translation>
    </message>
    <message>
        <location filename="../src/infotexts.cpp" line="51"/>
        <source>What is this? Click for an explanation.</source>
        <translation>Hvad er dette? Klik for en forklaring.</translation>
    </message>
</context>
<context>
    <name>InputEditor</name>
    <message>
        <location filename="../src/inputeditor.cpp" line="51"/>
        <source>Output %1</source>
        <translation>Udgang %1</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="64"/>
        <location filename="../src/inputeditor.cpp" line="307"/>
        <source>Out %1 · %2</source>
        <translation>Ud %1 · %2</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="80"/>
        <source>Direct control</source>
        <translation>Direkte styring</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="81"/>
        <source>Route</source>
        <translation>Togvej</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="82"/>
        <source>Not in use</source>
        <translation>Ikke i brug</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="83"/>
        <location filename="../src/inputeditor.cpp" line="152"/>
        <source>Recorder</source>
        <translation>Optager</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="84"/>
        <source>Feedback</source>
        <translation>Tilbagemelding</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="104"/>
        <source>Behaviour</source>
        <translation>Opførsel</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="118"/>
        <source>Triggered by</source>
        <translation>Udløses af</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="120"/>
        <source>DCC accessory address</source>
        <translation>DCC-tilbehørsadresse</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="130"/>
        <source>DCC</source>
        <translation>DCC</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="134"/>
        <source>Input pin</source>
        <translation>Indgangspin</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="138"/>
        <source>Controls</source>
        <translation>Styrer</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="143"/>
        <location filename="../src/inputeditor.cpp" line="290"/>
        <location filename="../src/inputeditor.cpp" line="488"/>
        <source>Output</source>
        <translation>Udgang</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="154"/>
        <source>Long press: record on / off.  Short press: play on / off.</source>
        <translation>Langt tryk: optagelse til / fra.  Kort tryk: afspilning til / fra.</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="162"/>
        <source>Route steps</source>
        <translation>Togvejens trin</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="170"/>
        <source>This board speaks protocol v1: max. 8 steps, curved / straight only, fixed 0.5 s per step.</source>
        <translation>Dette print taler protokol v1: maks. 8 trin, kun afvigende / lige, fast 0,5 s pr. trin.</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="179"/>
        <source>+ Add step</source>
        <translation>+ Tilføj trin</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="180"/>
        <source>Or drag an output onto this input in the canvas</source>
        <translation>Eller træk en udgang hen på denne indgang i lærredet</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="249"/>
        <source>There already is a recorder input; only one is allowed</source>
        <translation>Der findes allerede en optager-indgang; kun én er tilladt</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="290"/>
        <source>#</source>
        <translation>#</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="290"/>
        <source>Value</source>
        <translation>Værdi</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="290"/>
        <source>Wait after</source>
        <translation>Vent efter</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="304"/>
        <source>— choose an output —</source>
        <translation>— vælg en udgang —</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="308"/>
        <source>or drag an output onto this input</source>
        <translation>eller træk en udgang hen på denne indgang</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="344"/>
        <source> s</source>
        <translation> s</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="347"/>
        <source>Earlier</source>
        <translation>Tidligere</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="348"/>
        <source>Later</source>
        <translation>Senere</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="352"/>
        <source>Delete step %1</source>
        <translation>Slet trin %1</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="409"/>
        <source>Curved (0)</source>
        <translation>Afvigende (0)</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="410"/>
        <source>Straight (1)</source>
        <translation>Lige (1)</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="412"/>
        <source>Angle</source>
        <translation>Vinkel</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="430"/>
        <source>aspect </source>
        <translation>signalbillede </translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="437"/>
        <source>PWM </source>
        <translation>PWM </translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="455"/>
        <source>%1 / %2 steps  ·  board total %3 / %4</source>
        <translation>%1 / %2 trin  ·  print i alt %3 / %4</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="466"/>
        <source>Input %1</source>
        <translation>Indgang %1</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="474"/>
        <source>Toggle — every press switches the output over</source>
        <translation>Skift — hvert tryk skifter udgangen</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="475"/>
        <source>Absolute — the output follows the input</source>
        <translation>Absolut — udgangen følger indgangen</translation>
    </message>
    <message>
        <location filename="../src/inputeditor.cpp" line="488"/>
        <source>Feedback id</source>
        <translation>Tilbagemeldings-id</translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <location filename="../src/mainwindow.cpp" line="80"/>
        <location filename="../src/mainwindow.cpp" line="1257"/>
        <location filename="../src/mainwindow.cpp" line="1356"/>
        <source>GPIOcore Configurator</source>
        <translation>GPIOcore Configurator</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="130"/>
        <source>Done with %1 error(s)</source>
        <translation>Færdig med %1 fejl</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="140"/>
        <source>Preset %1 applied, reading back</source>
        <translation>Preset %1 anvendt, læser tilbage</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="183"/>
        <source>Close (Esc)</source>
        <translation>Luk (Esc)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="186"/>
        <source>Remove</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="187"/>
        <source>Set this to &apos;not in use&apos; and take it off the canvas</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="194"/>
        <source>Name, e.g. &quot;Turnout station left&quot; (kept on this computer)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="207"/>
        <source>INPUT SETTINGS</source>
        <translation>INDGANGSINDSTILLINGER</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="208"/>
        <source>OUTPUT SETTINGS</source>
        <translation>UDGANGSINDSTILLINGER</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="231"/>
        <source>+ Add input</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="232"/>
        <source>+ Add output</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="233"/>
        <source>Take the next free input and open its settings</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="234"/>
        <source>Take the next free output and open its settings</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="251"/>
        <source>Configuration</source>
        <translation>Konfiguration</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="258"/>
        <source>Pin map</source>
        <translation>Pinoversigt</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="270"/>
        <source>Serial log</source>
        <translation>Seriel log</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="279"/>
        <source>Legacy output format 0x2D</source>
        <translation>Gammelt udgangsformat 0x2D</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="281"/>
        <source>The firmware sends/receives output data with 13 bytes; the 4th data byte (servo pattern, high bytes of times) is lost. See review: OPCODE_SET_OUTPUT_DATA.</source>
        <translation>Firmwaren sender/modtager udgangsdata med 13 bytes; den 4. databyte (servomønster, høje bytes af tider) går tabt. Se review: OPCODE_SET_OUTPUT_DATA.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="306"/>
        <source>Input %1 → output %2</source>
        <translation>Indgang %1 → udgang %2</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="309"/>
        <source>Input %1 already drives output %2</source>
        <translation>Indgang %1 styrer allerede udgang %2</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="312"/>
        <source>The route of input %1 already has %2 steps (maximum)</source>
        <translation>Togvejen for indgang %1 har allerede %2 trin (maksimum)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="316"/>
        <source>The board has no room for more route steps (%1 in total)</source>
        <translation>Printet har ikke plads til flere togvejstrin (%1 i alt)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="320"/>
        <source>Input %1 is a recorder / feedback input and cannot drive outputs</source>
        <translation>Indgang %1 er en optager- / tilbagemeldingsindgang og kan ikke styre udgange</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="452"/>
        <source>protocol v%1 · %2 steps/route · %3 in total</source>
        <translation>protokol v%1 · %2 trin/togvej · %3 i alt</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="453"/>
        <source>protocol v1 · routes: 8 steps, no times</source>
        <translation>protokol v1 · togveje: 8 trin, ingen tider</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="486"/>
        <source>Main</source>
        <translation>Hoved</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="495"/>
        <source>  Port </source>
        <translation>  Port </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="498"/>
        <source>Refresh port list</source>
        <translation>Opdater portliste</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="499"/>
        <location filename="../src/mainwindow.cpp" line="695"/>
        <source>Connect</source>
        <translation>Forbind</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="500"/>
        <source>Demo</source>
        <translation>Demo</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="501"/>
        <source>Connect to a built-in simulation of the GPIOcore firmware</source>
        <translation>Forbind til en indbygget simulering af GPIOcore-firmwaren</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="504"/>
        <source>Read board</source>
        <translation>Læs print</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="505"/>
        <source>Read everything from the board again</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="506"/>
        <location filename="../src/mainwindow.cpp" line="931"/>
        <source>Write changes</source>
        <translation>Skriv ændringer</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="508"/>
        <source>Revert</source>
        <translation>Fortryd ændringer</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="514"/>
        <source> Preset </source>
        <translation> Preset </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="516"/>
        <source>Apply</source>
        <translation>Anvend</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="524"/>
        <source>Which board this is: decides which pins exist</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="531"/>
        <source> Board </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="534"/>
        <source>Board settings…</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="860"/>
        <source>Send this configuration to the connected board now?</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="878"/>
        <source>Loaded %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1077"/>
        <source>%1 (%2): conflict with the servo(s). The Servo library uses timer 1, so PWM on D9 and D10 stops working as soon as at least one servo output exists. Use D3, D5, D6 or D11 for PWM, or remove the servo.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1198"/>
        <source>&lt;h2&gt;GPIOcore Configurator&lt;/h2&gt;&lt;p&gt;Plug a GPIOcore board into a USB port, choose its port at the top and press &lt;b&gt;Connect&lt;/b&gt;. Everything you change is sent to the board straight away.&lt;/p&gt;</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1214"/>
        <source>Connect to the board</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1214"/>
        <source>Uses the port selected at the top.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1215"/>
        <source>Try the demo</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1215"/>
        <source>A simulated board, nothing can break.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1216"/>
        <source>Open a board file…</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1216"/>
        <source>Look at or prepare the settings of a board without connecting it.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1217"/>
        <source>New board (offline)…</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1217"/>
        <source>Prepare a new board on this computer and send it later.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1218"/>
        <source>Flash firmware…</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1218"/>
        <source>Put the GPIOcore firmware on an Arduino.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1252"/>
        <source>Names loaded from %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1257"/>
        <location filename="../src/mainwindow.cpp" line="1356"/>
        <source>%1 — GPIOcore Configurator</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1258"/>
        <source>● %1 — %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1258"/>
        <source>board without a name</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1278"/>
        <source>Name this board</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1279"/>
        <source>This board has no name yet. Give it a short, unique name (max. 16 letters, e.g. &quot;Station-left&quot;).
The name is stored on the board; your names for inputs and outputs are kept in a file with this name.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1294"/>
        <source>Board settings</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1301"/>
        <source>Use the recorder</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1303"/>
        <source>Off: the recorder&apos;s memory is used for routes instead</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1304"/>
        <source>Own wait time per route step</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1306"/>
        <source>Off: every step waits the same time and routes need half the memory</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1311"/>
        <source> s</source>
        <translation type="unfinished"> s</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1314"/>
        <source>Off — routes may run at the same time</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1315"/>
        <source>On — one route at a time</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1316"/>
        <source>Automatic — turnout routes one at a time, lights in parallel</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1318"/>
        <source>Name</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1319"/>
        <source>Board type</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1322"/>
        <source>Wait time per step</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1323"/>
        <source>Route interlocking</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1325"/>
        <source>This board&apos;s firmware cannot store these settings yet; they are only kept in the file.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1364"/>
        <source>New board</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1365"/>
        <source>Name of the new board (max. 16 letters). When you later connect a board and give it this name, these settings belong to it.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1394"/>
        <source>All 16 outputs are in use.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1406"/>
        <source>All 16 inputs are in use.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="537"/>
        <source>&amp;File</source>
        <translation>&amp;Filer</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="538"/>
        <source>&amp;New board (offline)…</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="539"/>
        <source>&amp;Open configuration…</source>
        <translation>Å&amp;bn konfiguration…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="542"/>
        <source>&amp;Save configuration…</source>
        <translation>&amp;Gem konfiguration…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="545"/>
        <source>Open the boards folder</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="550"/>
        <source>&amp;Quit</source>
        <translation>&amp;Afslut</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="552"/>
        <source>&amp;Board</source>
        <translation>&amp;Print</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="561"/>
        <source>Apply a preset…</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="563"/>
        <source>&amp;Flash firmware…</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="567"/>
        <source>&amp;View</source>
        <translation>&amp;Vis</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="568"/>
        <source>&amp;Expert mode</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="570"/>
        <source>Show bytes, the serial log, protocol details and presets</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="572"/>
        <source>Show unused inputs and outputs</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="581"/>
        <source>Show TX/RX bytes in log</source>
        <translation>Vis TX/RX-bytes i log</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="584"/>
        <source>Clear log</source>
        <translation>Ryd log</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="590"/>
        <source>Language</source>
        <translation>Sprog</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="600"/>
        <source>The language changes the next time you start the program.</source>
        <translation>Sproget skifter næste gang programmet startes.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="603"/>
        <source>Automatic (system language)</source>
        <translation>Automatisk (systemsprog)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="612"/>
        <source>&amp;Help</source>
        <translation>&amp;Hjælp</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="613"/>
        <source>&amp;Getting started</source>
        <translation>&amp;Kom godt i gang</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="614"/>
        <source>Getting started</source>
        <translation>Kom godt i gang</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="617"/>
        <source>&amp;About</source>
        <translation>&amp;Om</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="619"/>
        <source>&lt;b&gt;GPIOcore Configurator&lt;/b&gt; %1&lt;br&gt;&lt;br&gt;Configures GPIOcore boards (Arduino Nano / Uno, DCCnext) for model railways.&lt;br&gt;Language: %2</source>
        <translation>&lt;b&gt;GPIOcore Configurator&lt;/b&gt; %1&lt;br&gt;&lt;br&gt;Konfigurerer GPIOcore-print (Arduino Nano / Uno, DCCnext) til modeljernbaner.&lt;br&gt;Sprog: %2</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="643"/>
        <source>no serial ports found</source>
        <translation>ingen serielle porte fundet</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="663"/>
        <source>Select a serial port first, or use Demo.</source>
        <translation>Vælg først en seriel port, eller brug Demo.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="672"/>
        <source>Could not open %1:
%2</source>
        <translation>Kunne ikke åbne %1:
%2</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="695"/>
        <source>Disconnect</source>
        <translation>Afbryd</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="699"/>
        <source>● %1 — waiting for board…</source>
        <translation>● %1 — venter på print…</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="704"/>
        <source>○ Not connected</source>
        <translation>○ Ikke forbundet</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="716"/>
        <source>● %1 — GPIOcore %2</source>
        <translation>● %1 — GPIOcore %2</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="776"/>
        <source>Discard %1 unsaved change(s) and reload everything from the board?</source>
        <translation>Kassér %1 ikke-gemte ændring(er) og læs alt igen fra printet?</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="798"/>
        <source>Writing %1 item(s), each is read back for verification</source>
        <translation>Skriver %1 element(er), hvert læses tilbage til kontrol</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="814"/>
        <source>Apply preset &quot;%1&quot;?

This overwrites the outputs (and for most presets the inputs) on the board. It takes a few seconds.</source>
        <translation>Anvend preset &quot;%1&quot;?

Dette overskriver udgangene (og for de fleste presets også indgangene) på printet. Det tager et par sekunder.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="833"/>
        <source>Open configuration</source>
        <translation>Åbn konfiguration</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="834"/>
        <location filename="../src/mainwindow.cpp" line="888"/>
        <source>GPIOcore configuration (*.json)</source>
        <translation>GPIOcore-konfiguration (*.json)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="887"/>
        <source>Save configuration</source>
        <translation>Gem konfiguration</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="898"/>
        <source>Saved %1</source>
        <translation>%1 gemt</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="931"/>
        <source>Write %1 change(s)</source>
        <translation>Skriv %1 ændring(er)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="937"/>
        <source>Saving on the board…</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="937"/>
        <source>%1 change(s) waiting</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="939"/>
        <source>%1 unsent change(s)</source>
        <translation>%1 ikke-sendte ændring(er)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1016"/>
        <location filename="../src/mainwindow.cpp" line="1017"/>
        <source>Output %1 · %2</source>
        <translation>Udgang %1 · %2</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1046"/>
        <source>%1 is not set.</source>
        <translation>%1 er ikke sat.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1050"/>
        <source>%1 uses %2, which is the %3.</source>
        <translation>%1 bruger %2 (%3).</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1054"/>
        <source>%1 (%2) is also used by output %3 (%4).</source>
        <translation>%1 (%2) bruges også af udgang %3 (%4).</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1055"/>
        <source>%1 (%2) is also used by input %3.</source>
        <translation>%1 (%2) bruges også af indgang %3.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1058"/>
        <source>%1 (%2) is not PWM capable; %3 needs PWM.</source>
        <translation>%1 (%2) kan ikke PWM; %3 kræver PWM.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1062"/>
        <source>This board runs old firmware, which never ends the pulse — a coil can stay energised. Update the firmware (review F9).</source>
        <translation>Dette print har gammel firmware, som aldrig afslutter pulsen — en spole kan forblive strømførende. Opdater firmwaren (review F9).</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1065"/>
        <source>Min is larger than max: positions are swapped.</source>
        <translation>Min er større end max: positionerne er byttet om.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1068"/>
        <source>%1 (%2) is a serial pin: it only works when no GUI is connected.</source>
        <translation>%1 (%2) er en seriel pin: den virker kun, når intet program er forbundet.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1097"/>
        <source>Pin %1 is the %2.</source>
        <translation>Pin %1 er reserveret: %2.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1101"/>
        <source>Pin %1 is also used by output %2.</source>
        <translation>Pin %1 bruges også af udgang %2.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1102"/>
        <source>Pin %1 is also used by input %2.</source>
        <translation>Pin %1 bruges også af indgang %2.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1105"/>
        <source>This type needs an input pin.</source>
        <translation>Denne type kræver en indgangspin.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1107"/>
        <source>Input %1 is a recorder too. Only one recorder per board is allowed.</source>
        <translation>Indgang %1 er også en optager. Kun én optager pr. print er tilladt.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1109"/>
        <source>Neither a DCC address nor a pin is set: this input never triggers.</source>
        <translation>Hverken DCC-adresse eller pin er sat: denne indgang udløses aldrig.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1114"/>
        <source>Route step %1 sets output %2, which is not in use.</source>
        <translation>Togvejstrin %1 stiller udgang %2, som ikke er i brug.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1117"/>
        <source>This board stores only %1 route steps (protocol v1); steps %2 and later are lost.</source>
        <translation>Dette print gemmer kun %1 togvejstrin (protokol v1); trin %2 og derefter går tabt.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1120"/>
        <source>All routes together use %1 steps; the board has room for %2.</source>
        <translation>Alle togveje bruger tilsammen %1 trin; printet har plads til %2.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1123"/>
        <source>Output %1 is not in use.</source>
        <translation>Udgang %1 er ikke i brug.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1151"/>
        <source>%1 change(s) have not been written to the board. Quit anyway?</source>
        <translation>%1 ændring(er) er ikke skrevet til printet. Afslut alligevel?</translation>
    </message>
</context>
<context>
    <name>OutputEditor</name>
    <message>
        <location filename="../src/outputeditor.cpp" line="90"/>
        <location filename="../src/outputeditor.cpp" line="399"/>
        <source>Pins</source>
        <translation>Pins</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="92"/>
        <source>Pin A</source>
        <translation>Pin A</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="92"/>
        <source>Pin B</source>
        <translation>Pin B</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="92"/>
        <source>Pin C</source>
        <translation>Pin C</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="92"/>
        <source>Pin D</source>
        <translation>Pin D</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="109"/>
        <source>Frog polarity</source>
        <translation>Hjertestykke-polarisering</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="115"/>
        <source>None (dead frog)</source>
        <translation>Ingen (isoleret hjertestykke)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="116"/>
        <source>Electrofrog — 2 relays</source>
        <translation>Electrofrog — 2 relæer</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="117"/>
        <source>Unifrog — 1 change-over relay</source>
        <translation>Unifrog — 1 skifterelæ</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="121"/>
        <source>Frog</source>
        <translation>Hjertestykke</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="122"/>
        <source>Relay straight (A)</source>
        <translation>Relæ lige (A)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="122"/>
        <source>Relay curved (B)</source>
        <translation>Relæ afvigende (B)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="122"/>
        <source>Unifrog relay</source>
        <translation>Unifrog-relæ</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="136"/>
        <source>Parameters</source>
        <translation>Parametre</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="142"/>
        <source>This type has no parameters.</source>
        <translation>Denne type har ingen parametre.</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="156"/>
        <source>Switch time</source>
        <translation>Omstillingstid</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="166"/>
        <source>LED on time</source>
        <translation>LED-tændtid</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="167"/>
        <source>Pause</source>
        <translation>Pause</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="186"/>
        <source>Live: move the servo while adjusting</source>
        <translation>Live: flyt servoen under justering</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="191"/>
        <source>Position 0 (curved)</source>
        <translation>Position 0 (afvigende)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="192"/>
        <source>Position 1 (straight)</source>
        <translation>Position 1 (lige)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="193"/>
        <source> ms/°</source>
        <translation> ms/°</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="196"/>
        <source>Speed</source>
        <translation>Hastighed</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="211"/>
        <source>Duty cycle</source>
        <translation>Duty cycle</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="216"/>
        <source>This board runs old firmware (output opcode 0x2D, 13 bytes): the 4th data byte is not transferred. Update the firmware (review F1).</source>
        <translation>Dette print har gammel firmware (udgangs-opcode 0x2D, 13 bytes): den 4. databyte overføres ikke. Opdater firmwaren (review F1).</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="223"/>
        <source>State</source>
        <translation>Tilstand</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="230"/>
        <source>0  (off / curved)</source>
        <translation>0  (fra / afvigende)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="231"/>
        <source>1  (on / straight)</source>
        <translation>1  (til / lige)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="232"/>
        <source>▶ Test: curved / off</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="233"/>
        <source>▶ Test: straight / on</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="236"/>
        <source>Writes this output to the board with the chosen state (also stored in EEPROM)</source>
        <translation>Skriver denne udgang til printet med den valgte tilstand (gemmes også i EEPROM)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="239"/>
        <source>Stored state</source>
        <translation>Gemt tilstand</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="353"/>
        <source>Output %1</source>
        <translation>Udgang %1</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="361"/>
        <source>Both electrofrog and unifrog relays were set. Only one frog mode is allowed: the unifrog relay is removed when you change this output.</source>
        <translation>Både Electrofrog- og Unifrog-relæer er sat. Kun én hjertestykke-tilstand er tilladt: Unifrog-relæet fjernes, når du ændrer denne udgang.</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="378"/>
        <source>Needs a connected board that speaks protocol v2 (SERVO_LIVE)</source>
        <translation>Kræver et forbundet print, der taler protokol v2 (SERVO_LIVE)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="399"/>
        <source>Pins (PWM capable: D3 D5 D6 D9 D10 D11)</source>
        <translation>Pins (PWM-egnede: D3 D5 D6 D9 D10 D11)</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="431"/>
        <source>× %1 ms  =  %2 ms</source>
        <translation>× %1 ms  =  %2 ms</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="432"/>
        <source>× %1 ms  →  on %2 ms, pause %3 ms</source>
        <translation>× %1 ms  →  tændt %2 ms, pause %3 ms</translation>
    </message>
    <message>
        <location filename="../src/outputeditor.cpp" line="435"/>
        <source>Full sweep ≈ %1 ms</source>
        <translation>Fuld bevægelse ≈ %1 ms</translation>
    </message>
</context>
<context>
    <name>PinMapView</name>
    <message>
        <location filename="../src/pinmapview.cpp" line="45"/>
        <source>Pin usage (Arduino Uno / Nano)</source>
        <translation>Pinbrug (Arduino Uno / Nano)</translation>
    </message>
    <message>
        <location filename="../src/pinmapview.cpp" line="52"/>
        <source>click a card to edit  ·  red = conflict</source>
        <translation>klik på et kort for at redigere  ·  rød = konflikt</translation>
    </message>
    <message>
        <location filename="../src/pinmapview.cpp" line="107"/>
        <source>Out %1 %2 · %3</source>
        <translation>Ud %1 %2 · %3</translation>
    </message>
    <message>
        <location filename="../src/pinmapview.cpp" line="110"/>
        <source>In %1 · %2</source>
        <translation>Ind %1 · %2</translation>
    </message>
    <message>
        <location filename="../src/pinmapview.cpp" line="114"/>
        <source>free</source>
        <translation>fri</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../src/connectionview.cpp" line="242"/>
        <source>no trigger</source>
        <translation>ingen udløser</translation>
    </message>
</context>
</TS>
