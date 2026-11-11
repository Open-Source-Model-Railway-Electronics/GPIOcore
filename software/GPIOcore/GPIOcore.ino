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

NmraDcc         dcc ;
ServoManager    servoManager ;
Output          output[16] ;
Input           input[16] ;
Timer           debounce ;
Transceiver     transceiver ;
uint16_t        nextEEAddress = 0 ;
BoardSettings   board ;
uint16_t        boardEEAddress = 1004 ;     // type, flags, delay, name[16]
uint8_t         configSession  = 0 ;        // 1 after HELLO

// Recorder — uses internal EEPROM from address 420 up to 999 (~72 events of 8 bytes)
const uint16_t  RECORDER_EE_START = 420 ;
const uint16_t  RECORDER_EE_END   = 999 ;
const uint8_t   OUTPUT_EVENT      =   1 ;   // custom event type for output state changes

EventHandler    recorder( RECORDER_EE_START, RECORDER_EE_END ) ;


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


// number of route steps that still fit in EEPROM (HELLO)
uint16_t freeRouteSteps()
{
    return 16 * MAX_OUTPUTS ;           // v1 storage: 8 linked outputs per input
}

void loadBoard()
{
    EEPROM.get( boardEEAddress, board ) ;
    if( board.type <= BOARD_JACK ) return ;

    board.type  = BOARD_NANO ;          // erased EEPROM: defaults
    board.flags = BOARD_RECORDER | ( INTERLOCK_AUTO << 2 ) ;
    board.delay = 3 ;
    EEPROM.put( boardEEAddress, board ) ;
    for( uint8_t i = 0 ; i < BOARD_NAME_LEN ; i ++ ) EEPROM.update( boardEEAddress + sizeof( board ) + i, 0 ) ;
}

bool deadbeef()
{
    const int DEADBEEF_EE_ADDRESS = 1000 ;

    uint32_t deadbeef_ ;
    EEPROM.get(DEADBEEF_EE_ADDRESS, deadbeef_) ;

    if( deadbeef_ != 0xDEADBEEF )
    {
        deadbeef_ = 0xDEADBEEF ;
        EEPROM.put(DEADBEEF_EE_ADDRESS, deadbeef_) ;
        return true ;
    }

    return false ;
}

void setup()
{
    delay(100) ;
    Serial.begin(115200) ;
    delay(100) ;
    sendMyVersion() ;

    loadBoard() ;

    dcc.pin(2, 0) ;
    dcc.init( MAN_ID_DIY, 11, FLAGS_OUTPUT_ADDRESS_MODE | FLAGS_DCC_ACCESSORY_DECODER, 0 ) ;

    for( uint8_t i = 0 ; i < 16 ; i++ )
    {
        output[i].assignEEPROM() ;
         input[i].assignEEPROM() ;
         input[i].begin() ;
    }

    if( deadbeef() )
    {
        for( uint8_t i = 0 ; i < 16 ; i++ )
        {
            output[i].defaults() ;
            output[i].saveEEPROM() ;
             input[i].defaults() ;
             input[i].saveEEPROM() ;
        }
    }

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
