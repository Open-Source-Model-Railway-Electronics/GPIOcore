#include "input.h"

extern uint16_t nextEEAddress ;

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
    if( eedata.pin != NA ) pinMode( eedata.pin, INPUT_PULLUP ) ;   // button / contact to GND

    if( eedata.type == RECORDER )       // configure timer for long/short press detection
    {
        outputTimer.type = TIMER_ON ;
        outputTimer.PT   = LONG_PRESS_MS ;
    }
}

void Input::defaults()
{
    static uint8_t address = 0 ;

    eedata.outputID  = address ++ ;     // input[n] → output[n] by default
    eedata.myAddress = address ;        // DCC addresses 1-16 by default
    eedata.pin       = NA ;
    eedata.type      = LATCHING ;

    for( int i = 0 ; i < MAX_OUTPUTS ; i ++ )
    {
        eedata.linkedOutput[i] = 0xFF ;   // 0x7F in bits 6-0 = end of route
    }
}

void Input::set( InputSettings *data )
{
    eedata = *data ;
    if( eedata.pin != NA ) pinMode( eedata.pin, INPUT_PULLUP ) ;
    saveEEPROM() ;
}

InputSettings* Input::get()
{
    return &eedata ;
}


void Input::setDccAddress( uint16_t address, uint8_t direction )
{
    if( settingOutputs  ==                1 ) return ;
    if( address         != eedata.myAddress ) return ;
    if( direction       >                 1 ) direction = 1 ;

    if( eedata.type ==     ROUTE ) settingOutputs = 1 ;   // any DCC command, direction 0 and 1
    if( eedata.type == MOMENTARY ) state  ^= 1 ;
    if( eedata.type == LATCHING  ) state   = direction ;

    if( callbackInput && ( eedata.type == MOMENTARY || eedata.type == LATCHING ) )
    {
        callbackInput( eedata.outputID, state, 0 ) ;
    }
}

void Input::setDccAddressExt( uint16_t address, uint8_t val )
{
    if( address != eedata.myAddress ) return ;
    if( callbackInput ) callbackInput( eedata.outputID, 0, val ) ;
}

void Input::update()
{
    // ── FEEDBACK input ────────────────────────────────────────────────────────
    if( eedata.type == FEEDBACK_INPUT && eedata.pin != NA )
    {
        trigger.update( digitalRead( eedata.pin ) ) ;
        if( trigger.toggled() )
        {
            if( callbackFeedback ) callbackFeedback( eedata.outputID, trigger.state ) ;
        }
        return ;
    }

    // ── RECORDER input (long press = record toggle, short press = play toggle) ─
    if( eedata.type == RECORDER && eedata.pin != NA )
    {
        trigger.update( digitalRead( eedata.pin ) ) ;
        outputTimer.update( !trigger.state ) ;  // count while button is held (active low)

        if( trigger.rose() )    // button released → determine press duration
        {
            if( outputTimer.Q ) { if( callbackRecorder ) callbackRecorder( RECORD_TOGGLE ) ; }
            else                { if( callbackRecorder ) callbackRecorder( PLAY_TOGGLE   ) ; }
        }
        return ;
    }

    // ── Normal inputs ─────────────────────────────────────────────────────────
    if( eedata.type != NOT_IN_USE && eedata.pin != NA )
    {
        trigger.update( digitalRead( eedata.pin ) ) ;

        if( trigger.toggled() )         // trigger.state is the new level: 1 = rising, 0 = falling edge
        {
            if( eedata.type ==     ROUTE && !trigger.state ) settingOutputs = 1 ;   // on press only
            if( eedata.type == MOMENTARY && !trigger.state ) state ^= 1 ;           // toggle on press only
            if( eedata.type == LATCHING  ) state   = trigger.state ;

            if( callbackInput && ( eedata.type == LATCHING || ( eedata.type == MOMENTARY && !trigger.state ) ) )
            {
                callbackInput( eedata.outputID, state, 0 ) ;
            }
        }
    }
    triggerOutputs() ;
}

void Input::triggerOutputs()
{
    if( eedata.type != ROUTE ) return ;

    if( outputTimer.update( settingOutputs ) )
    {
        uint8_t outID = eedata.linkedOutput[index] & 0x7F ;    // bits 6-0 = ID
        uint8_t state = eedata.linkedOutput[index] >>    7 ;   // bit  7   = state

        if( outID < 16 )    // 0x7F = end of route, anything else >= 16 is invalid
        {
            callbackInput( outID, state, 0 ) ;

            if( ++ index == MAX_OUTPUTS )
            {
                settingOutputs  = false ;
                index           =     0 ;
            }
        }
        else
        {
            settingOutputs  = false ;
            index           =     0 ;
        }
    }
}
