#include "src/date.h"
#include "src/version.h"
#include "src/macros.h"
#include "output.h"
#include "input.h"
#include "servoManager.h"
#include "src/NmraDcc.h"
#include "src/Transceiver.h"
#include "src/event.h"
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
uint8_t         serialIsIO     = 0 ;        // 1 when no HELLO came within the boot window
const uint16_t  BOOT_WINDOW_MS = 5000 ;

// Recorder — in the external I2C EEPROM (A4/A5), started by updateRecorderRange() when enabled
const uint8_t   OUTPUT_EVENT      =   1 ;   // custom event type for output state changes

EventHandler    recorder( 0, 0 ) ;


/* TODO
    * servo bouncing
    * pwm fading/ramping — RAMP_GEN available in ST.h
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

// EEPROM layout v2 (EEPROM.md). A v1 board (DEADBEEF at 1000) is migrated, an empty one gets defaults.
void eepromBegin()
{
    if( EEPROM.read( EE_MAGIC ) == 'G' && EEPROM.read( EE_MAGIC + 1 ) == '2' )
    {
        EEPROM.get( boardEEAddress, board ) ;
        return ;
    }

    // v1: output i at i * 22, input i at i * 22 + 10 (4 B settings + linked[8])
    const uint8_t V1_RECORD = 22 ;
    uint32_t marker ;
    uint8_t  linked[16][MAX_OUTPUTS] ;
    EEPROM.get( 1000, marker ) ;
    uint8_t  v1 = marker == 0xDEADBEEF ;

    for( uint8_t i = 0 ; i < 16 ; i ++ )
    {
        linked[i][0] = 0xFF ;
        if( v1 )
        {
            EEPROM.get( i * V1_RECORD,      output[i].eedata ) ;
            EEPROM.get( i * V1_RECORD + 10,  input[i].eedata ) ;
            for( uint8_t j = 0 ; j < MAX_OUTPUTS ; j ++ ) linked[i][j] = EEPROM.read( i * V1_RECORD + 14 + j ) ;
        }
        else
        {
            output[i].defaults() ;
             input[i].defaults() ;
        }
    }

    board.type  = BOARD_NANO ;
    board.flags = INTERLOCK_AUTO << 2 ;     // recorder off (needs the I2C EEPROM), timing off: 2 B per step
    board.delay = v1 ? 5 : 3 ;          // v1 routes waited 0.5 s per step
    EEPROM.put( boardEEAddress, board ) ;
    for( uint8_t i = 0 ; i < BOARD_NAME_LEN ; i ++ ) EEPROM.update( boardEEAddress + sizeof( board ) + i, 0 ) ;

    uint16_t address = EE_POOL ;
    for( uint8_t i = 0 ; i < 16 ; i ++ )
    {
        output[i].saveEEPROM() ;
         input[i].saveEEPROM() ;

        uint8_t n = 0 ;                 // linked[]: bit 7 = state, bits 0-6 = output id, 0x7F = end
        while( n < MAX_OUTPUTS && ( linked[i][n] & 0x7F ) < 16 )
        {
            EEPROM.update( address ++, linked[i][n] & 0x7F ) ;
            EEPROM.update( address ++, linked[i][n] >> 7   ) ;
            n ++ ;
        }
        EEPROM.update( EE_COUNTS + i, n ) ;
    }

    EEPROM.update( EE_MAGIC,     'G'       ) ;
    EEPROM.update( EE_MAGIC + 1, '2'       ) ;
    EEPROM.update( EE_MAGIC + 2, EE_LAYOUT ) ;

    routesBegin() ;
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
        const uint8_t pins[7] = { o.pinA, o.pinB, o.pinC, o.pinD, o.relayPinA, o.relayPinB, o.relayPinC } ;
        uint8_t bad = 0 ;
        for( uint8_t j = 0 ; j < 7 ; j ++ ) bad |= ( pins[j] == CONFIG_LED_PIN || pins[j] == CONFIG_BUTTON_PIN ) ;
        if( bad ) { o.type = notInUse ; o.pinA = o.pinB = o.pinC = o.pinD = o.relayPinA = o.relayPinB = o.relayPinC = NA ; output[i].saveEEPROM() ; }
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
// Hold A7 for 2 s: config mode on (D13 blinks). A short press selects the next servo and toggles it,
// so you can see which one it is. With loco address 9999 on the throttle the selected servo is taught:
// speed knob + direction = move it, F1 = store as curved, F2 = store as straight, F3 = toggle, F4 = next.
// Hold A7 for 2 s again: back to normal (D13 on).
uint8_t  configMode ;
int8_t   selServo = -1 ;
uint8_t  learnAngle = 90 ;
int8_t   learnStep ;            // degrees per 100 ms from the throttle, signed
uint8_t  fnPrev ;               // F1-F4 of loco 9999, for edge detection

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

void configUpdate()                                             // every 50 ms
{
    static uint8_t held, longDone, blink, down ;
    int level = analogRead( A7 ) ;                             // external pull-up, button to GND
    if( level < 200 ) down = 1 ;                               // hysteresis: a floating pin between
    if( level > 800 ) down = 0 ;                               // 200 and 800 does not count as a press
    if( down )
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
        if( held && held < 40 && configMode ) selectNextServo() ;   // short press
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
    if( Addr != 9999 || FuncGrp != FN_0_4 || !configMode ) return ;
    uint8_t rose = FuncState & ~fnPrev ;
    fnPrev = FuncState ;
    if( rose & FN_BIT_04 ) { selectNextServo() ; return ; }
    if( selServo < 0 ) return ;
    Output &o = output[selServo] ;
    if( rose & FN_BIT_01 ) { o.eedata.servo.servoMin = learnAngle ; o.saveEEPROM() ; }   // curved
    if( rose & FN_BIT_02 ) { o.eedata.servo.servoMax = learnAngle ; o.saveEEPROM() ; }   // straight
    if( rose & FN_BIT_03 ) callbackInput( selServo, !o.eedata.state, 0 ) ;               // toggle
}

void loop()
{
    if( !serialIsIO ) transceiver.receive() ;

    if( !configSession && !serialIsIO && millis() >= BOOT_WINDOW_MS )   // no HELLO: D0/D1 become I/O
    {
        Serial.flush() ;
        Serial.end() ;
        serialIsIO = 1 ;
        for( uint8_t i = 0 ; i < 16 ; i++ )      // a sound effect output: TX (D1) at 9600 baud for the DFPlayer,
        {                                        // RX stays off so D0 can be an input
            if( output[i].eedata.type != soundEffect ) continue ;
            Serial.begin( 9600 ) ;
            UCSR0B &= ~_BV( RXEN0 ) ;
            break ;
        }
        for( uint8_t i = 0 ; i < 16 ; i++ )
        {
            output[i].setPinModes() ;
             input[i].setPinMode() ;
        }
    }
    recorder.update() ;

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
        configUpdate() ;
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
    uint8_t f[10] = { 0x7E, 0xFF, 0x06, (uint8_t)( track ? 0x03 : 0x16 ), 0x00, 0x00, track, 0, 0, 0xEF } ;   // 03 play track, 16 stop
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
    if( type == soundEffect )                       // without custom values: on = track 1, off = stop
    {
        output[index].setState( state ) ;
        playSound( index, val ? val : state ) ;
        return ;
    }
    reportOutput( index, ( type == fourToneSignal ) ? 2 : state, val ) ;   // a signal reports its aspect
    output[index].setState( state ) ;
    if( output[index].eedata.type == PWM || output[index].eedata.type == fourToneSignal ) output[index].setVal( val ) ;   // val shares the union with servo / time settings

    recorder.storeEvent( OUTPUT_EVENT, index, state ) ; // logged when recording
}

// ── Servo sweep: every degree is reported (skipped when the serial buffer is full, never the last one)
void callbackServoPos( Output *o, uint8_t pos, uint8_t done )
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
    if( output[index].eedata.type == soundEffect ) { playSound( index, value ) ; return ; }   // value = track, 0 = stop
    if( output[index].eedata.type == servoType )
    {
        if( value <= 180 ) output[index].moveTo( value ) ;   // at its speed; the degrees are reported
        return ;
    }
    reportOutput( index, 2, value ) ;   // state 2 = raw value (aspect / PWM)
    output[index].setVal( value ) ;
}

// ── DCC extended: a servo goes to that angle (0-180, more is ignored), others take the value
void callbackInputExt( uint8_t index, uint8_t value )
{
    if( output[index].eedata.type == servoType ) { if( value <= 180 ) output[index].moveTo( value ) ; return ; }
    if( output[index].eedata.type == soundEffect ) { playSound( index, value ) ; return ; }   // value = track, 0 = stop
    callbackInput( index, 0, value ) ;
}

// ── Interlocking: a turnout is an output of type servo, single pulse or double pulse
uint8_t callbackIsTurnout( uint8_t index )
{
    uint8_t type = output[index].eedata.type ;
    return type == servoType || type == singlePulse || type == doublePulse ;
}

// ── Recorder playback — fires for each stored event ───────────────────────────
void notifyEvent( uint8_t data1, uint16_t data2, uint8_t data3 )
{
    if( data1 == OUTPUT_EVENT )
    {
        output[data2].setState( data3 ) ;
    }
}

// ── Recorder control — called by Input with RECORDER type ─────────────────────
void callbackRecorder( uint8_t action )
{
    if( !( board.flags & BOARD_RECORDER ) ) return ;   // recorder disabled: the input does nothing

    if( action == RECORD_TOGGLE )
    {
        if( recorder.getState() == evRecording ) recorder.stopRecording() ;
        else                                     recorder.startRecording() ;
    }
    else if( action == PLAY_TOGGLE )
    {
        if( recorder.getState() == evPlaying  ||
            recorder.getState() == evFinishing ) recorder.stopPlaying() ;
        else                                     recorder.startPlaying() ;
    }
}

// ── Feedback input — pauses playback until sensor fires ───────────────────────
void callbackFeedback( uint8_t id, uint8_t state )
{
    recorder.storeEvent(       FEEDBACK_EVENT, id, state ) ; // log tijdens opname
    recorder.sendFeedbackEvent( id, state ) ;                 // trigger tijdens playback
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
