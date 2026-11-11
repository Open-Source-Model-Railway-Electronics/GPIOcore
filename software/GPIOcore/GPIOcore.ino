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

// Recorder — uses the top of the shared EEPROM pool, the range is set by updateRecorderRange()
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
    board.flags = BOARD_RECORDER | ( INTERLOCK_AUTO << 2 ) ;   // timing off: 2 B per step
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
    recorder.clearProgram() ;           // the old recorder space is now part of the pool
}

void setup()
{
    delay(100) ;
    Serial.begin(115200) ;
    delay(100) ;
    sendMyVersion() ;

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

    debounce.set( TIMER_BLEEP, 50 ) ;
}

void loop()
{
    transceiver.receive() ;
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
    }
}


// ── Output callback — central point for all output state changes ───────────────
void callbackInput( uint8_t index, uint8_t state, uint8_t val )
{
    output[index].setState( state ) ;
    if( output[index].eedata.type == PWM || output[index].eedata.type == fourToneSignal ) output[index].setVal( val ) ;   // val shares the union with servo / time settings

    recorder.storeEvent( OUTPUT_EVENT, index, state ) ; // logged when recording
}

// ── Raw route step: signal aspect, PWM value or servo angle ───────────────────
void callbackRouteValue( uint8_t index, uint8_t value )
{
    if( output[index].eedata.type == servoType ) output[index].setPosition( value ) ;
    else                                         output[index].setVal( value ) ;
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
