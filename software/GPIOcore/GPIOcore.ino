#include "src/date.h"
#include "src/version.h"
#include "src/macros.h"
#include "output.h"
#include "input.h"
#include "servoManager.h"
#include "src/NmraDcc.h"
#include "src/Transceiver.h"
#include "messageParser.h"
#include "routes.h"

NmraDcc         dcc ;
ServoManager    servoManager ;
Output          output[16] ;
Input           input[16] ;
Timer           debounce ;
Transceiver     transceiver ;
uint16_t        nextEEAddress = 0 ;
BoardSettings   board ;
uint16_t        boardEEAddress = EE_BOARD ; // type, flags, delay, name[16]
uint8_t         configSession  = 0 ;        // 1 after HELLO
uint8_t         serialIsIO     = 0 ;        // 1 when no HELLO came within the serial window: board link mode
uint32_t        serialWindowStart = 0 ;     // the serial window: from start-up, or from a short press on the config button
const uint16_t  SERIAL_WINDOW_MS  = 5000 ;
uint8_t         btSetupPending ;            // BT_SETUP received

void blockSignals() ;                       // defined below; also for the PC build of the simulator
void guiMode() ;
void reportOutput( uint8_t index, uint8_t state, uint8_t value ) ;
void notifyDccAccTurnoutOutput( uint16_t address, uint8_t direction, uint8_t output ) ;
void notifyDccSigOutputState( uint16_t address, uint8_t state ) ;



/* TODO
    * servo bouncing
    * combination FLASHING + FADING
    * random toggling of digital IO
    * Roco offset
    * DCC lockout (6 repeated packets cause toggle issues on MOMENTARY)
*/

bool callbackAttach(   uint8_t pin )              { return servoManager.attach( pin ) ; }
void callbackDetach(   uint8_t pin )              { servoManager.detach(   pin      ) ; }
void callbackSetServo( uint8_t pin, uint8_t pos ) { servoManager.setServo( pin, pos ) ; }


static_assert( sizeof( Settings      ) == 10, "EEPROM layout v2: output = 10 B" ) ;
static_assert( sizeof( InputSettings ) ==  4, "EEPROM layout v2: input = 4 B"   ) ;

// EEPROM layout (EEPROM.md). A board without the magic gets defaults.
void eepromBegin()
{
    if( EEPROM.read( EE_MAGIC ) == 'G' && EEPROM.read( EE_MAGIC + 1 ) == '2'
    &&  EEPROM.read( EE_MAGIC + 2 ) == EE_LAYOUT )                     // another layout: start with defaults
    {
        EEPROM.get( boardEEAddress, board ) ;
        return ;
    }

    board.type  = BOARD_NANO ;
    board.flags = INTERLOCK_AUTO << 2 ;     // timing off: 2 B per step
    board.delay = 3 ;                       // 0.3 s after each route step
    EEPROM.put( boardEEAddress, board ) ;
    for( uint8_t i = 0 ; i < BOARD_NAME_LEN ; i ++ ) EEPROM.update( boardEEAddress + sizeof( board ) + i, 0 ) ;

    for( uint8_t i = 0 ; i < 16 ; i ++ )
    {
        output[i].defaults() ;
         input[i].defaults() ;
        output[i].saveEEPROM() ;
         input[i].saveEEPROM() ;
        EEPROM.update( EE_COUNTS + i, 0 ) ;   // no routes
    }

    EEPROM.update( EE_MAGIC,     'G'       ) ;
    EEPROM.update( EE_MAGIC + 1, '2'       ) ;
    EEPROM.update( EE_MAGIC + 2, EE_LAYOUT ) ;

    routesBegin() ;
}

uint8_t outputOnPin( const Settings &o, uint8_t a, uint8_t b )   // one of its pins is a or b
{
    const uint8_t pins[7] = { o.pinA, o.pinB, o.pinC, o.pinD, o.relayPinA, o.relayPinB, o.relayPinC } ;
    for( uint8_t j = 0 ; j < 7 ; j ++ ) if( pins[j] == a || pins[j] == b ) return 1 ;
    return 0 ;
}

void setup()
{
    delay(100) ;
    Serial.begin(115200) ;
    delay(100) ;
    // nothing is sent until the GUI asks (REQ_VERSION / HELLO): without a GUI the board stays silent

    dcc.pin(2, 0) ;
    dcc.init( MAN_ID_DIY, 11, FLAGS_OUTPUT_ADDRESS_MODE | FLAGS_DCC_ACCESSORY_DECODER, 0 ) ;

    nextEEAddress = EE_OUTPUTS ;
    for( uint8_t i = 0 ; i < 16 ; i++ ) output[i].assignEEPROM() ;     // EE_OUTPUTS
    for( uint8_t i = 0 ; i < 16 ; i++ )                                 // EE_INPUTS
    {
         input[i].assignEEPROM() ;
         input[i].begin() ;
    }

    eepromBegin() ;
    routesBegin() ;

    for( uint8_t i = 0 ; i < 16 ; i++ )
    {
        output[i].loadEEPROM() ;
         input[i].loadEEPROM() ;
    }
    for( uint8_t i = 0 ; i < 16 ; i++ )     // a toggle input continues from the stored output state
    {
        input[i].syncState( output[ input[i].eedata.outputID ].eedata.state ) ;
    }

    // D13 and A7 belong to the config button / LED: an output or input that uses them is switched off
    for( uint8_t i = 0 ; i < 16 ; i++ )
    {
        Settings &o = output[i].eedata ;
        if( outputOnPin( o, CONFIG_LED_PIN, CONFIG_BUTTON_PIN ) ) { o.type = notInUse ; o.pinA = o.pinB = o.pinC = o.pinD = o.relayPinA = o.relayPinB = o.relayPinC = NA ; output[i].saveEEPROM() ; }
        const uint8_t inPin = inputPinNumber( input[i].eedata.pin ) ;
        if( inPin == CONFIG_LED_PIN || inPin == CONFIG_BUTTON_PIN )
        {
            input[i].eedata.pin = NA ; input[i].eedata.type = NOT_IN_USE ; input[i].saveEEPROM() ;
        }
    }
    pinMode( CONFIG_LED_PIN, OUTPUT ) ;
    digitalWrite( CONFIG_LED_PIN, HIGH ) ;

    debounce.set( TIMER_BLEEP, 50 ) ;
}

// ── Config mode ───────────────────────────────────────────────────────────────
// Hold the config button (D2) for 2 s: config mode on (D13 blinks). A short press selects the next servo and toggles it,
// so you can see which one it is. With loco address 9999 on the throttle the selected servo is taught:
// speed knob + direction = move it, F1 = store as curved, F2 = store as straight, F3 = toggle, F4 = invert frog relays, F5 = next.
// Hold it for 2 s again: back to normal (D13 on).
// A short press outside config mode is the GUI mode: Serial is opened again for 5 s and the board announces itself.
uint8_t  configMode ;
int8_t   selServo = -1 ;
uint8_t  learnAngle = 90 ;
int8_t   learnStep ;            // degrees per 100 ms from the throttle, signed
uint8_t  fnPrev ;               // F0-F4 of loco 9999, for edge detection
uint8_t  fnPrev5 ;              // F5-F8 of loco 9999

void selectNextServo()
{
    for( uint8_t n = 0 ; n < 16 ; n ++ )
    {
        selServo = ( selServo + 1 ) % 16 ;
        Output &o = output[selServo] ;
        if( o.eedata.type != servoType ) continue ;
        callbackInput( selServo, !o.eedata.state, 0 ) ;        // toggle: shows which servo is selected
        learnAngle = !o.eedata.state ? o.eedata.servo.servoMax : o.eedata.servo.servoMin ;
        return ;
    }
    selServo = -1 ;                                             // no servo at all
}

// D2 is the DCC input and the config button. DCC changes its level every 58-116 us; the button holds it
// at GND. Pressed = LOW for 1 ms without a single HIGH sample.
uint8_t configButtonDown()
{
    for( uint8_t i = 0 ; i < 20 ; i ++ )
    {
        if( digitalRead( CONFIG_BUTTON_PIN ) ) return 0 ;
        delayMicroseconds( 50 ) ;
    }
    return 1 ;
}

void configUpdate()                                             // every 50 ms
{
    static uint8_t held, longDone, blink ;
    if( configButtonDown() )
    {
        if( held < 255 ) held ++ ;
        if( held == 40 && !longDone )                           // 40 x 50 ms = 2 s
        {
            longDone   = 1 ;
            configMode = !configMode ;
            selServo   = -1 ;
            learnStep  = 0 ;
        }
    }
    else
    {
        if( held && held < 40 ) { if( configMode ) selectNextServo() ; else guiMode() ; }   // short press
        held = 0 ;
        longDone = 0 ;
    }

    if( configMode ) { if( ++ blink >= 5 ) { blink = 0 ; digitalWrite( CONFIG_LED_PIN, !digitalRead( CONFIG_LED_PIN ) ) ; } }
    else             digitalWrite( CONFIG_LED_PIN, HIGH ) ;

    static uint8_t tick ;
    if( ++ tick >= 2 && configMode && selServo >= 0 && learnStep )  // every 100 ms: follow the throttle
    {
        tick = 0 ;
        int16_t a = (int16_t)learnAngle + learnStep ;
        learnAngle = constrain( a, 10, 170 ) ;
        output[selServo].setPosition( learnAngle ) ;
        reportOutput( selServo, 2, learnAngle ) ;
    }
}

void notifyDccSpeed( uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps )
{
    if( Addr != 9999 || !configMode ) return ;
    int8_t step = Speed <= 1 ? 0 : (int8_t)map( Speed, 2, 127, 1, 10 ) ;   // 0 = stop, 1 = emergency stop
    learnStep = Dir ? step : -step ;
}

void notifyDccFunc( uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState )
{
    if( Addr != 9999 || !configMode ) return ;
    if( FuncGrp == FN_5_8 )                         // F5: next servo
    {
        uint8_t rose5 = FuncState & ~fnPrev5 ;
        fnPrev5 = FuncState ;
        if( rose5 & FN_BIT_05 ) selectNextServo() ;
        return ;
    }
    if( FuncGrp != FN_0_4 ) return ;
    uint8_t rose    = FuncState & ~fnPrev ;
    uint8_t changed = FuncState ^ fnPrev ;
    fnPrev = FuncState ;
    if( selServo < 0 ) return ;
    Output &o = output[selServo] ;
    if( rose & FN_BIT_01 ) { o.eedata.servo.servoMin = learnAngle ; o.saveEEPROM() ; }   // curved
    if( rose & FN_BIT_02 ) { o.eedata.servo.servoMax = learnAngle ; o.saveEEPROM() ; }   // straight
    if( rose & FN_BIT_03 ) callbackInput( selServo, !o.eedata.state, 0 ) ;               // toggle
    if( changed & FN_BIT_04 )                       // F4, every press: frog relays inverted / normal
    {
        o.eedata.invertFrog ^= 1 ;
        o.saveEEPROM() ;
        o.refreshRelays() ;
    }
}

// ── Serial window: the UART is the GUI's for 5 s after start-up or after a short press on the config button.
// Then it becomes the board link (9600 baud): frames from the previous board on RX (D0) are passed on to TX
// (D1) and run as DCC commands; board link outputs send their own; a DFPlayer on TX hears the same line.
void serialToIO()                               // no HELLO within the window: board link mode
{
    Serial.flush() ;
    Serial.end() ;
    Serial.begin( 9600 ) ;
    serialIsIO = 1 ;
}

void serialCheck()                              // every 50 ms
{
    if( configSession || serialIsIO || millis() - serialWindowStart < SERIAL_WINDOW_MS ) return ;
    serialToIO() ;
}

// ── Board link frame (PROTOCOL.md section 11): <start> <address> <value> <check>
//    start = 0xA0 | extended << 4 | hops left (0-15), check = start ^ address ^ value.
//    Basic: value = direction (1 = straight), as a DCC accessory command. Extended: value 0-255, as DCC extended.
//    The four bytes come back to back: after a gap of more than 3 ms a frame starts over.
const uint8_t LINK_START = 0xA0 ;
const uint8_t LINK_EXT   = 0x10 ;
const uint8_t LINK_HOPS  = 0x0F ;

void linkSend( uint8_t extended, uint8_t address, uint8_t value, uint8_t hops )
{
    if( !serialIsIO ) return ;                  // the GUI has the UART
    uint8_t f[4] = { (uint8_t)( LINK_START | ( extended ? LINK_EXT : 0 ) | hops ), address, value, 0 } ;
    f[3] = f[0] ^ f[1] ^ f[2] ;
    Serial.write( f, 4 ) ;
}

void linkReceive()
{
    static uint8_t  f[4], n ;
    static uint16_t last ;
    while( Serial.available() )
    {
        uint8_t  b   = Serial.read() ;
        uint16_t now = millis() ;
        if( n && (uint16_t)( now - last ) > 3 ) n = 0 ;              // the rest never came: start over
        last = now ;
        if( n == 0 && ( b & 0xE0 ) != LINK_START ) continue ;          // not a start byte (DFPlayer, noise)
        f[n ++] = b ;
        if( n < 4 ) continue ;
        n = 0 ;
        if( ( f[0] ^ f[1] ^ f[2] ) != f[3] || f[1] == 0 ) continue ;  // broken, or address 0
        uint8_t hops = f[0] & LINK_HOPS ;
        if( hops ) linkSend( f[0] & LINK_EXT, f[1], f[2], hops - 1 ) ;  // pass it on first: the next board starts at once
        if( f[0] & LINK_EXT ) notifyDccSigOutputState( f[1], f[2] ) ;
        else                  notifyDccAccTurnoutOutput( f[1], f[2] & 1, 1 ) ;
    }
}

// ── Board link output (type boardLink, on TX): data[0] = its address (0 = none), data[1] bit0 = extended.
//    A value (route step, custom value, DCC extended): extended = that value to its address; basic = bits 0-6
//    the address, bit 7 straight. On / off: basic, to its own address.
void linkOutput( uint8_t index, uint8_t isValue, uint8_t state, uint8_t value )
{
    const Settings &o = output[index].eedata ;
    uint8_t extended = o.data[1] & 1 ;
    uint8_t address  = o.data[0] ;
    uint8_t v        = isValue ? value : state ;
    if( isValue && !extended ) { address = value & 0x7F ; v = value >> 7 ; }
    reportOutput( index, 2, isValue ? value : state ) ;
    if( address ) linkSend( extended, address, v, LINK_HOPS ) ;
}

void guiMode()                                  // short press: the GUI may connect (also over Bluetooth, no reset)
{
    if( serialIsIO )                            // D0/D1 back to Serial: what is on them pauses
    {
        Serial.begin( 115200 ) ;                // also ends the DFPlayer's 9600 baud, RX on again
        serialIsIO = 0 ;
    }
    if( !configSession ) serialWindowStart = millis() ;
    sendHelloReply() ;                          // the announce: a waiting GUI sends HELLO
}

// ── Bluetooth: an HC-05 in AT mode on D0/D1 gets 115200 baud and the name GPIOcore-<board name>
uint8_t btCommand( const char *cmd, const char *arg )   // 1: "OK" within ~300 ms
{
    while( Serial.available() ) Serial.read() ;
    Serial.write( cmd ) ;
    Serial.write( arg ) ;
    Serial.write( "\r\n" ) ;
    uint8_t prev = 0 ;
    for( uint16_t t = 0 ; t < 300 ; t ++ )
    {
        delay( 1 ) ;
        while( Serial.available() )
        {
            uint8_t c = Serial.read() ;
            if( prev == 'O' && c == 'K' ) return 1 ;
            prev = c ;
        }
    }
    return 0 ;
}

void btSetup()                                  // blocks up to ~2 s; the GUI ignores the noise at other baud rates
{
    static const uint8_t bauds[] = { 16, 4, 24, 48, 8 } ;   // x 2400: 38400 (AT mode) first, then the usual ones
    uint8_t status = 1, code = 0, nameOk = 0 ;
    char    name[13] = "" ;                     // "-" + up to 11 safe characters of the board name: 20 in total
    for( uint8_t i = 0, n = 0 ; i < BOARD_NAME_LEN && n < 12 ; i ++ )
    {
        char c = EEPROM.read( boardEEAddress + sizeof( board ) + i ) ;
        if( c == 0 || c == (char)0xFF ) break ;
        if( !( ( c >= '0' && c <= '9' ) || ( ( c | 0x20 ) >= 'a' && ( c | 0x20 ) <= 'z' ) || c == '-' || c == '_' ) ) continue ;
        if( n == 0 ) name[n ++] = '-' ;
        name[n ++] = c ;
        name[n]    = 0 ;
    }
    btSetupPending = 0 ;
    Serial.flush() ;
    for( uint8_t i = 0 ; i < sizeof( bauds ) ; i ++ )
    {
        Serial.begin( 2400UL * bauds[i] ) ;
        if( !btCommand( "AT", "" ) ) continue ;
        code   = bauds[i] ;
        status = 2 ;
        if( btCommand( "AT+UART=115200,0,0", "" ) )
        {
            status = 0 ;
            nameOk = btCommand( "AT+NAME=GPIOcore", name ) ;
        }
        break ;
    }
    Serial.flush() ;
    delay( 20 ) ;                               // the rest of the HC-05's answer (CR LF) arrives
    Serial.begin( 115200 ) ;
    while( Serial.available() ) Serial.read() ;
    Message m ;
    m.command    = OPCODE_BT_RESULT ;
    m.payload[0] = status ;
    m.payload[1] = code ;
    m.payload[2] = nameOk ;
    transceiver.sendMessage( &m ) ;
}

void loop()
{
    if( !serialIsIO ) transceiver.receive() ;
    else              linkReceive() ;
    if( btSetupPending ) btSetup() ;

    static uint8_t index ;
    if( ++ index == 16 ) index = 0 ;
    output[index].update() ;

    dcc.process() ;

    if( debounce.update(1) )
    {
        for( int i = 0 ; i < 16 ; i++ )
        {
            input[i].update() ;
        }
        blockSignals() ;
        configUpdate() ;
        serialCheck() ;
    }

    // servos start one by one: each gets its stored position for ~500 ms, the next one 500 ms later
    static uint8_t  bootServo ;
    static uint16_t bootTime ;
    if( bootServo < 16 && (uint16_t)( millis() - bootTime ) >= 500 )
    {
        bootTime = millis() ;
        while( bootServo < 16 && output[bootServo].eedata.type != servoType ) bootServo ++ ;
        if( bootServo < 16 ) output[bootServo ++].engage() ;
    }
}


// ── Report an output change to the GUI (only during a configuration session) ──
void reportOutput( uint8_t index, uint8_t state, uint8_t value )
{
    if( !configSession || serialIsIO ) return ;
    uint32_t now = millis() ;
    Message m ;
    m.command    = OPCODE_OUTPUT_EVENT ;
    m.payload[0] = index ;
    m.payload[1] = state ;
    m.payload[2] = value ;
    m.payload[3] = now & 0xFF ;
    m.payload[4] = ( now >> 8 ) & 0xFF ;
    transceiver.sendMessage( &m ) ;
}

// ── Output callback — central point for all output state changes ───────────────
// ── Sound effect: DFPlayer Mini on TX (D1), 9600 baud, nothing is read back. Without a PC only:
// with the GUI connected the serial port is the GUI's, so the board only reports the track.
void playSound( uint8_t index, uint8_t track )
{
    reportOutput( index, 2, track ) ;
    if( !serialIsIO ) return ;
    // 12 plays /MP3/<track>.mp3 by its name (0001.mp3), whatever order the files were copied in; 16 stops
    uint8_t f[10] = { 0x7E, 0xFF, 0x06, (uint8_t)( track ? 0x12 : 0x16 ), 0x00, 0x00, track, 0, 0, 0xEF } ;
    uint16_t sum = 0 ;
    for( uint8_t i = 1 ; i < 7 ; i ++ ) sum += f[i] ;
    sum  = -sum ;
    f[7] = sum >> 8 ;
    f[8] = sum & 0xFF ;
    Serial.write( f, 10 ) ;
}

void callbackInput( uint8_t index, uint8_t state, uint8_t val )
{
    uint8_t type = output[index].eedata.type ;
    if( type == blockSignal ) return ;              // automatic: commands are ignored
    if( type == boardLink ) { linkOutput( index, 0, state, 0 ) ; return ; }
    if( type == soundEffect )                       // without custom values: on = track 1, off = stop
    {
        output[index].eedata.state = state ;        // not stored: no EEPROM wear for sounds
        playSound( index, val ? val : state ) ;
        return ;
    }
    reportOutput( index, ( type == fourToneSignal ) ? 2 : state, val ) ;   // a signal reports its aspect
    if( type == PWM ) output[index].eedata.state = state ;   // PWM: not stored (EEPROM wear)
    else              output[index].setState( state ) ;      // turnouts, relays, lamps, signals: kept
    if( output[index].eedata.type == PWM || output[index].eedata.type == fourToneSignal ) output[index].setVal( val ) ;   // val shares the union with servo / time settings

}

// ── Servo sweep: every degree is reported (skipped when the serial buffer is full, never the last one)
void callbackServoPos( Output *o, uint8_t pos, uint8_t done )   // servo degrees and PWM fade steps
{
    if( !configSession || serialIsIO ) return ;
    if( !done && Serial.availableForWrite() < 8 ) return ;
    reportOutput( o - output, 2, pos ) ;
}

// ── Frog relays switched: reported as OUTPUT_EVENT state 3, value = bit0 A, bit1 B, bit2 C
void callbackFrog( Output *o, uint8_t relays )
{
    reportOutput( o - output, 3, relays ) ;
}

// ── Raw route step: signal aspect, PWM value or servo angle ───────────────────
void callbackRouteValue( uint8_t index, uint8_t value )
{
    if( output[index].eedata.type == blockSignal ) return ;                                       // automatic
    if( output[index].eedata.type == soundEffect ) { playSound( index, value ) ; return ; }   // value = track, 0 = stop
    if( output[index].eedata.type == boardLink   ) { linkOutput( index, 1, 0, value ) ; return ; }
    if( output[index].eedata.type == servoType )
    {
        if( value <= 180 ) output[index].moveTo( value ) ;   // at its speed; the degrees are reported
        return ;
    }
    if( output[index].eedata.type != PWM ) reportOutput( index, 2, value ) ;   // state 2 = raw value (aspect); PWM reports its fade steps
    output[index].setVal( value ) ;
}

// ── DCC extended: a servo goes to that angle (0-180, more is ignored), others take the value
void callbackInputExt( uint8_t index, uint8_t value )
{
    if( output[index].eedata.type == servoType ) { if( value <= 180 ) output[index].moveTo( value ) ; return ; }
    if( output[index].eedata.type == soundEffect ) { playSound( index, value ) ; return ; }   // value = track, 0 = stop
    if( output[index].eedata.type == boardLink   ) { linkOutput( index, 1, 0, value ) ; return ; }
    callbackInput( index, 0, value ) ;
}

// ── Automatic signals, recalculated every 50 ms from the detector levels (Input::occupied()):
// - block signal (type blockSignal): block.aspect[0] when its own block is occupied, [1] when the next
//   block is, [2] otherwise (nextBlock == ownBlock: no next block)
// - servo with servo.follows (semaphore arm): follows input servo.followInput, occupied = curved (stop),
//   free = straight (clear); servo.followSwap swaps them
void blockSignals()
{
    for( uint8_t o = 0 ; o < 16 ; o ++ )
    {
        Output &out = output[o] ;
        const Settings &s = out.eedata ;

        if( s.type == blockSignal )
        {
            uint8_t own  = s.block.ownBlock ;
            uint8_t next = s.block.nextBlock ;
            uint8_t aspect = input[own].occupied()                    ? s.block.aspect[0]
                           : ( next != own && input[next].occupied() ) ? s.block.aspect[1]
                           :                                             s.block.aspect[2] ;
            if( out.showAspect( aspect ) ) reportOutput( o, 2, aspect ) ;
        }
        else if( s.type == servoType && s.servo.follows )
        {
            uint8_t state = !input[ s.servo.followInput ].occupied() ;
            if( s.servo.followSwap ) state = !state ;
            if( state != s.state ) callbackInput( o, state, 0 ) ;
        }
    }
}

// ── Interlocking: a turnout is an output of type servo, single pulse or double pulse
uint8_t callbackIsTurnout( uint8_t index )
{
    uint8_t type = output[index].eedata.type ;
    return type == servoType || type == singlePulse || type == doublePulse ;
}



// ── DCC callbacks ─────────────────────────────────────────────────────────────

// TODO: lockout time needed — 6 repeated DCC packets cause toggling issues on MOMENTARY
void notifyDccAccTurnoutOutput( uint16_t address, uint8_t direction, uint8_t output )
{
    if( output == 0 ) return ;

    for( int i = 0 ; i < 16 ; i++ )
    {
        input[i].setDccAddress( address, direction ) ;
    }
}

void notifyDccSigOutputState( uint16_t address, uint8_t state )
{
    for( int i = 0 ; i < 16 ; i++ )
    {
        input[i].setDccAddressExt( address, state ) ;
    }
}
