#include "input.h"
#include "routes.h"

extern uint16_t nextEEAddress ;
extern Input    input[16] ;

Input::Input() {}

void Input::begin()
{
    outputTimer.type = TIMER_BLEEP ;
    outputTimer.PT   = 500 ;
}

void Input::assignEEPROM()
{
    eeAddress      = nextEEAddress ;
    nextEEAddress += sizeof(eedata) ;
}

void Input::saveEEPROM() { EEPROM.put(eeAddress, eedata) ; }

void Input::loadEEPROM()
{
    EEPROM.get(eeAddress, eedata) ;
    setPinMode() ;

    if( eedata.type == 5 ) eedata.type = NOT_IN_USE ;   // old feedback input
}

void Input::defaults()
{
    static uint8_t address = 0 ;

    eedata.outputID  = address ++ ;     // input[n] → output[n] by default
    eedata.myAddress = address ;        // DCC addresses 1-16 by default
    eedata.pin       = NA ;
    eedata.type      = LATCHING ;
}

void Input::set( InputSettings *data )
{
    eedata = *data ;
    setPinMode() ;
    saveEEPROM() ;
    rearm() ;                           // as at start-up: an absolute input follows its switch once
}

InputSettings* Input::get()
{
    return &eedata ;
}


void Input::setDccAddress( uint16_t address, uint8_t direction )
{
    if( address         != eedata.myAddress ) return ;
    if( direction       >                 1 ) direction = 1 ;

    if( eedata.type == ROUTE_STRAIGHT && direction != 1 ) return ;   // this route only reacts to 'straight'
    if( eedata.type == ROUTE_CURVED   && direction != 0 ) return ;   // this route only reacts to 'curved'

    if( eedata.type == MOMENTARY || isRoute( eedata.type ) )         // repeated packets: take the first one
    {
        uint16_t now = millis() ;
        if( dccSeen && (uint16_t)( now - lastDcc ) < DCC_LOCKOUT_MS ) return ;
        lastDcc = now ;
        dccSeen = 1 ;
    }

    if( isRoute( eedata.type ) ) { startRoute() ; return ; }
    if( eedata.type == MOMENTARY ) state  ^= 1 ;
    if( eedata.type == LATCHING  ) state   = direction ;

    if( eedata.type == MOMENTARY || eedata.type == LATCHING ) switchOutput() ;
}

// Custom values: the input's route slot holds 2 steps marked STEP_VALUES, one per state, each
// a curved / straight or a raw value (servo angle, aspect, PWM, sound track) like a route step.
void Input::switchOutput()
{
    uint8_t me = this - input ;
    uint8_t step[4] ;
    if( routeCount[ me ] == 2 )
    {
        readStep( me, state, step ) ;
        if( step[0] & STEP_VALUES )
        {
            uint8_t id = step[0] & STEP_ID ;
            if( step[0] & STEP_RAW ) { if( callbackRouteValue ) callbackRouteValue( id, step[1] ) ; }
            else                     { if( callbackInput      ) callbackInput( id, step[1], 0 ) ; }
            return ;
        }
    }
    if( callbackInput ) callbackInput( eedata.outputID, state, 0 ) ;
}

void Input::setDccAddressExt( uint16_t address, uint8_t val )
{
    if( address != eedata.myAddress ) return ;
    if( eedata.type != MOMENTARY && eedata.type != LATCHING ) return ;   // routes have no single output
    if( callbackInputExt ) callbackInputExt( eedata.outputID, val ) ;
    else if( callbackInput ) callbackInput( eedata.outputID, 0, val ) ;
}

void Input::update()
{
    // ── Normal inputs ─────────────────────────────────────────────────────────
    if( eedata.type != NOT_IN_USE && eedata.pin != NA )
    {
        uint8_t first = !trigger.armed ;
        trigger.update( readPin() ) ;

        // the first sample is the starting level: an absolute input applies it once, when the output differs
        if( first && eedata.type == LATCHING && trigger.state != state ) { state = trigger.state ; switchOutput() ; }

        if( trigger.toggled() )         // trigger.state is the new level: 1 = rising, 0 = falling edge
        {
            if( isRoute( eedata.type ) && !trigger.state ) startRoute() ;           // on press only (any route kind)
            if( eedata.type == MOMENTARY && !trigger.state ) state ^= 1 ;           // toggle on press only
            if( eedata.type == LATCHING  ) state   = trigger.state ;

            if( eedata.type == LATCHING || ( eedata.type == MOMENTARY && !trigger.state ) ) switchOutput() ;
        }
    }
    triggerOutputs() ;
}

// Nano A6/A7 (pin 20/21) are analog only, external pull-up: analogRead() < 512 = pressed (LOW)
void Input::setPinMode()            // button / contact to GND. A6/A7 (20/21) are analog only, D0/D1 only after the boot window
{
    if( isAnalogInput( eedata.pin ) ) { if( ( eedata.pin & 7 ) < 6 ) pinMode( inputPinNumber( eedata.pin ), INPUT ) ; return ; }
    if( eedata.pin < 20 && ( eedata.pin > 1 || serialPinsFree() ) ) pinMode( eedata.pin, INPUT_PULLUP ) ;
}

// Analog condition as a button level: LOW = pressed. It must be the same in two samples in a row
// (2 x 50 ms) before it counts: a resistor ladder passes other values while a button moves.
uint8_t Input::readAnalog()
{
    int16_t v    = analogRead( inputPinNumber( eedata.pin ) ) ;
    int16_t t    = eedata.level << 3 ;
    uint8_t mode = ( eedata.pin >> 3 ) & 3 ;
    uint8_t held = !trigger.state ;                  // pressed now: the hysteresis keeps it pressed
    uint8_t hit ;

    if(      mode == ANALOG_BELOW ) hit = v < t + ( held ? ANALOG_HYSTERESIS : 0 ) ;
    else if( mode == ANALOG_ABOVE ) hit = v > t - ( held ? ANALOG_HYSTERESIS : 0 ) ;
    else                            hit = abs( v - t ) <= ANALOG_HYSTERESIS / 2 ;

    if( hit != analogHit ) { analogHit = hit ; return trigger.state ; }   // not stable yet: keep the old level
    return !hit ;
}

uint8_t Input::readPin()
{
    if( isAnalogInput( eedata.pin ) ) return readAnalog() ;
    if( eedata.pin < 2 && !serialPinsFree() ) return HIGH ;              // D0/D1 still Serial: not pressed
    if( eedata.pin >= 20 ) return analogRead( eedata.pin ) >= 512 ? HIGH : LOW ;
    return digitalRead( eedata.pin ) ;
}

uint8_t Input::movesTurnout()
{
    uint8_t route = this - input ;
    uint8_t step[4] ;

    for( uint8_t i = 0 ; i < routeCount[ route ] ; i ++ )
    {
        readStep( route, i, step ) ;
        if( callbackIsTurnout && callbackIsTurnout( step[0] & STEP_ID ) ) return 1 ;
    }
    return 0 ;
}

void Input::startRoute()
{
    if( settingOutputs ) return ;           // already running

    uint8_t mode = ( board.flags & BOARD_INTERLOCK ) >> 2 ;
    uint8_t mine = mode == INTERLOCK_AUTO && movesTurnout() ;

    for( uint8_t i = 0 ; i < 16 ; i ++ )
    {
        if( !input[i].settingOutputs ) continue ;
        if( mode == INTERLOCK_ON ) return ;                         // one route at a time
        if( mine && input[i].movesTurnout() ) return ;              // auto: one turnout route at a time
    }

    settingOutputs = 1 ;
    index          = 0 ;
    outputTimer.PT = 1 ;                    // first step right away
}

void Input::triggerOutputs()
{
    if( !isRoute( eedata.type ) ) return ;

    if( outputTimer.update( settingOutputs ) )
    {
        uint8_t route = this - input ;

        if( index < routeCount[ route ] )
        {
            uint8_t step[4] ;
            readStep( route, index, step ) ;

            uint8_t id = step[0] & STEP_ID ;
            if( step[0] & STEP_RAW ) { if( callbackRouteValue ) callbackRouteValue( id, step[1] ) ; }  // aspect, angle, PWM
            else                     { callbackInput( id, step[1], 0 ) ; }                            // curved / straight

            if( board.flags & BOARD_TIMING ) outputTimer.PT = ( ( (uint16_t)( step[3] & 0x3F ) << 8 ) | step[2] ) * 100UL ;
            else                             outputTimer.PT = board.delay * 100UL ;   // wait after this step, x 0.1 s
            index ++ ;
        }
        else
        {
            settingOutputs  = false ;
            index           =     0 ;
        }
    }
}
