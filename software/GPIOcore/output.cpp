#include "output.h"
#include "servoManager.h"


extern uint16_t nextEEAddress ;

bool inline isPwmPin( uint8_t pin )
{
    if( pin ==  3 ) return true ;
    if( pin ==  5 ) return true ; 
    if( pin ==  6 ) return true ;
    if( pin ==  9 ) return true ;
    if( pin == 10 ) return true ;
    if( pin == 11 ) return true ;

    return false ;
}

Output::Output() {} 

uint8_t Output::setVal( uint8_t _val )
{
    if( eedata.type == PWM ) eedata.pwm.value   = _val ;
    else                     eedata.signal.aspect = _val ;
    if( eedata.type != PWM ) saveEEPROM() ;     // a PWM level is not kept (EEPROM wear); a signal aspect is
    return 0 ;
}

void Output::set( Settings *data )
{
    // pins this output no longer uses stop being driven
    const uint8_t oldPins[7] = { eedata.pinA, eedata.pinB, eedata.pinC, eedata.pinD,
                                 eedata.relayPinA, eedata.relayPinB, eedata.relayPinC } ;
    const uint8_t newPins[7] = { data->pinA, data->pinB, data->pinC, data->pinD,
                                 data->relayPinA, data->relayPinB, data->relayPinC } ;
    if( eedata.type == servoType && attached ) { callbackDetach( eedata.pinA ) ; attached = 0 ; live = 0 ; }
    for( uint8_t i = 0 ; i < 7 ; i ++ )
    {
        uint8_t p = oldPins[i] ;
        if( p == NA || p <= 2 || p >= 20 ) continue ;   // D0/D1 belong to the UART, D2 to DCC
        uint8_t kept = 0 ;
        for( uint8_t j = 0 ; j < 7 ; j ++ ) kept |= ( newPins[j] == p ) ;
        if( !kept ) { digitalWrite( p, LOW ) ; pinMode( p, INPUT ) ; }
    }
    relays = 0 ;

    eedata = *data ;
    oldPos = !eedata.state ;    // new settings: apply the state once, also when unchanged
    custom = 0 ;

    uint8_t lo = min( eedata.servo.servoMin, eedata.servo.servoMax ) ;
    uint8_t hi = max( eedata.servo.servoMin, eedata.servo.servoMax ) ;
    if( eedata.type == servoType && ( pos < lo || pos > hi ) )      // position unknown (new servo): start at the end position
        pos = eedata.state ? eedata.servo.servoMax : eedata.servo.servoMin ;
    setPinModes() ;     // new pins are outputs right away (not only after a reset)
    saveEEPROM() ;
}

// Sweep to an angle at the servo's speed, waiting for a free servo slot like a state change.
// The frog relay is left alone: an angle is for semaphores, barriers and the like, not for a turnout.
void Output::moveTo( uint8_t angle )
{
    if( eedata.type != servoType ) return ;
    target = angle ;
    custom = 1 ;
    if( sm.getState() == idle ) sm.nextState( moveActuator ) ;   // a running sweep just follows the new target
}

void Output::setPosition( uint8_t angle )
{
    if( eedata.type != servoType ) return ;
    if( !callbackAttach( eedata.pinA ) ) return ;   // no free servo slot

    callbackSetServo( eedata.pinA, angle ) ;
    pos      = angle ;                  // the next state change sweeps from here
    live     = 1 ;
    liveTime = millis() ;
}

Settings* Output::get()
{
    return &eedata ; 
}

uint8_t Output::setState( uint8_t _state)
{
    custom = 0 ;                        // a state change ends a custom angle
    eedata.state = _state ;
    saveEEPROM() ;
    return 0 ;
}


void Output::defaults( )
{
    static byte index = 0 ;
    eedata = defaultSettings ;
    eedata.pinA = index < 10 ? 3 + index : 4 + index ;   // D3-D12, A0-A5: skip D0/D1 (Serial), D2 (DCC), D13 (LED)
    index ++ ;
}

void Output::assignEEPROM()
{
    eeAddress = nextEEAddress;
    nextEEAddress += sizeof(eedata);
    // Serial.print("nextEEAddress: "); Serial.println( nextEEAddress);
}

void Output::saveEEPROM()
{
    EEPROM.put(eeAddress, eedata);
    
}

static void outputPin( uint8_t pin )    // never D0/D1: the UART (GUI, board link, DFPlayer)
{
    if( pin != NA && pin > 1 ) pinMode( pin, OUTPUT ) ;
}

void Output::setPinModes()
{
    outputPin( eedata.pinA      ) ;
    outputPin( eedata.pinB      ) ;
    outputPin( eedata.pinC      ) ;
    outputPin( eedata.pinD      ) ;
    outputPin( eedata.relayPinA ) ;
    outputPin( eedata.relayPinB ) ;
    outputPin( eedata.relayPinC ) ;
}

void Output::loadEEPROM()
{
    EEPROM.get(eeAddress, eedata);
    setPinModes() ;

    // apply the stored state once after boot, except for coils (no burst of pulses at power up)
    // servos start one by one from loop() (engage()), coils are not pulsed at all
    oldPos = ( eedata.type == singlePulse || eedata.type == doublePulse || eedata.type == servoType ) ? eedata.state : !eedata.state ;

    // a servo starts where it was left (its stored end position), not at 0 degrees: no sweep at power up (F15)
    if( eedata.type == servoType ) pos = eedata.state ? eedata.servo.servoMax : eedata.servo.servoMin ;

    // coil turnouts are not pulsed at power up, so set their frog relays for the stored position here
    // (otherwise an electrofrog stays dead and a unifrog can have the wrong polarity)
    if( eedata.type == singlePulse || eedata.type == doublePulse )
    {
        if( eedata.relayPinA != NA && eedata.relayPinB != NA )
        {
            relayWrite( 0,  eedata.state ) ;
            relayWrite( 1, !eedata.state ) ;
        }
        relayWrite( 2, eedata.state ) ;
    }
}

// all frog relay writes go through here, so a change can be reported
void Output::relayWrite( uint8_t which, uint8_t level )
{
    uint8_t pin = which == 0 ? eedata.relayPinA : which == 1 ? eedata.relayPinB : eedata.relayPinC ;
    if( pin == NA ) return ;
    digitalWrite( pin, level ^ eedata.invertFrog ) ;       // 'relays' keeps the logical level
    uint8_t bits = level ? ( relays | ( 1 << which ) ) : ( relays & ~( 1 << which ) ) ;
    if( bits == relays ) return ;
    relays = bits ;
    if( callbackFrog ) callbackFrog( this, relays ) ;
}

void Output::refreshRelays()
{
    const uint8_t pins[3] = { eedata.relayPinA, eedata.relayPinB, eedata.relayPinC } ;
    for( uint8_t i = 0 ; i < 3 ; i ++ )
        if( pins[i] != NA ) digitalWrite( pins[i], ( ( relays >> i ) & 1 ) ^ eedata.invertFrog ) ;
}

uint8_t Output::sweep()
{
    // Wait for a free servo slot — implicit queue via round-robin update
    if( !attached )
    {
        attached = callbackAttach( eedata.pinA ) ? 1 : 0 ;
        if( !attached ) return 0 ;  // no slot yet, try again next cycle
    }

    if( timer.update( 1 ) )
    {
        uint8_t setPoint ;

        if(      custom       ) { setPoint = target ; }
        else if( eedata.state ) { setPoint = eedata.servo.servoMax ; }
        else                    { setPoint = eedata.servo.servoMin ; }

        uint8_t before = pos ;
        if( pos < setPoint ) { pos ++ ; }
        if( pos > setPoint ) { pos -- ; }
        if( pos != before && callbackServoPos ) callbackServoPos( this, pos, pos == setPoint ) ;

        uint8_t servoDelay = 500 / ( timer.PT > 2 ? timer.PT : 2 ) ;   // speed 0 would divide by zero

        if( pos == setPoint && counter < servoDelay ) { counter ++ ; }
        else                                          { counter = 0 ; }

        stopTrigger.update( counter == servoDelay ) ;

        if( stopTrigger.rose() )
        {
            callbackDetach( eedata.pinA ) ;
            attached = 0 ;
            return 1 ;
        }

        callbackSetServo( eedata.pinA, pos ) ;

        uint8_t middlePos = ( eedata.servo.servoMin + eedata.servo.servoMax ) / 2 ;
        uint8_t straightSide = eedata.servo.servoMin <= eedata.servo.servoMax ? pos >= middlePos : pos <= middlePos ;   // min > max is allowed
        if( !custom ) relayWrite( 2, straightSide ? HIGH : LOW ) ;   // unifrog only for curved / straight
    }

    return 0 ;
}


#define StateFunction( x ) uint8_t Output::x##F()
StateFunction( idle )
{
    if( live && (uint16_t)millis() - liveTime > 1000 )   // setPosition() done: release the servo
    {
        callbackDetach( eedata.pinA ) ;
        live     = 0 ;
        attached = 0 ;
    }
    return 1 ;      // the idle state body in update() must run every time: it starts the actions
}

// PWM: pwm.value = brightness, pwm.fadeTime = s for a full 0-255 change (0 = at once).
// 'pos' (the servo angle) holds the brightness of the moment; it moves one step per tick.
StateFunction( pwm )
{
    if( sm.entryState() )
    {
        timer.type  = TIMER_BLEEP ;
    }

    timer.PT = eedata.pwm.fadeTime ? eedata.pwm.fadeTime * 1000UL / 255 : 100 ;
    timer.update( 1 ) ;                                // a BLEEP timer only runs with its input on
    if( timer.Q )
    {
        uint8_t before = pos ;
        if(      !eedata.pwm.fadeTime        ) pos = eedata.pwm.value ;
        else if( pos < eedata.pwm.value      ) pos ++ ;
        else if( pos > eedata.pwm.value      ) pos -- ;
        if( pos != before && callbackServoPos ) callbackServoPos( this, pos, pos == eedata.pwm.value ) ;   // every fade step
        if( isPwmPin( eedata.pinA ) ) { analogWrite( eedata.pinA, pos ) ; }
    }

    if( eedata.type != PWM ) return 1 ;

    return  0 ;
}


StateFunction( turnOnRelays )
{
    if( sm.entryState() )
    {
        timer.type  = TIMER_ON ;
        timer.PT    = 100 ;
        timer.update( 0 ) ;         // re-arm TIMER_ON, it only restarts after IN was low

        relayWrite( 0,  eedata.state ) ;
        relayWrite( 1, !eedata.state ) ;
    }
    
    if( timer.update( 1 ) ) { return 1 ; }

    return 0 ;
}

StateFunction( turnOffRelays )
{
    if( sm.entryState() )
    {
        timer.type  = TIMER_ON ;
        timer.PT    = 100 ;
        timer.update( 0 ) ;         // re-arm TIMER_ON

        relayWrite( 0, LOW ) ;
        relayWrite( 1, LOW ) ;
    }
    
    if( timer.update( 1 ) ) { return 1 ; }

    return 0 ;
}

StateFunction( moveActuator )
{
    if( sm.entryState() )
    {
        switch( eedata.type )
        {
        case servoType:
            timer.type = TIMER_BLEEP ;
            timer.PT   = eedata.servo.servoSpeed ;
            attached   = 0 ;
            break ;

        case singlePulse :
            timer.type = TIMER_ON ;
            timer.PT   = eedata.switchTime * 100 ;
            timer.update( 0 ) ;     // re-arm TIMER_ON
            digitalWrite( eedata.pinA, HIGH ) ;
            break ;

        case singleSteady :
            digitalWrite( eedata.pinA, eedata.state ) ;
            break ;

        case doublePulse:
            timer.type = TIMER_ON ;
            timer.PT   = eedata.switchTime * 10 ;
            timer.update( 0 ) ;     // re-arm TIMER_ON
        case doubleSteady:
            digitalWrite( eedata.pinA,  eedata.state ) ;
            digitalWrite( eedata.pinB, !eedata.state ) ;
            break ;
        }
    }

    if( sm.onState() )
    {
        switch( eedata.type )
        {
        case singleSteady:
        case doubleSteady:  // these modes may just exit
            sm.exit() ;
            break ;

        case singlePulse:
        case doublePulse:
            if( timer.update( 1 ) ) // pulse expired, -> exit
            {
                relayWrite( 2, eedata.state ) ;     // toggle the unifrog relay
                sm.exit() ;
            } 
            break ;

        case servoType:
            {
                uint8_t status = sweep() ;  // servo is in position
                if( status == 1 ) sm.exit() ;
            }
            break ;
        }
    }
    if( sm.exitState() && ( eedata.type == singlePulse || eedata.type == doublePulse ) )   // end of pulse only
    {
        digitalWrite( eedata.pinA, LOW ) ;
        if( eedata.pinB != NA ) digitalWrite( eedata.pinB, LOW ) ;
    }
    
    return sm.endState() ;
}

StateFunction( swingLightState )
{
    if( sm.entryState() )
    {
        counter    = 0 ;
        timer.type = TIMER_BLEEP ;
        timer.PT   = swingOnTime() * 10UL ;
        digitalWrite( eedata.pinA, eedata.state ? HIGH : LOW ) ;
        digitalWrite( eedata.pinB, LOW ) ;
    }

    if( eedata.state && timer.update(1) )
    {
        if( counter == 0 )          // A was on → switch to B
        {
            digitalWrite( eedata.pinA, LOW  ) ;
            digitalWrite( eedata.pinB, HIGH ) ;
            timer.PT = swingOnTime() * 10UL ;
            counter  = 1 ;
        }
        else if( counter == 1 )     // B was on → pause
        {
            digitalWrite( eedata.pinB, LOW ) ;
            timer.PT = swingPause() * 10UL ;
            counter  = 2 ;
        }
        else                        // pause done → A on
        {
            digitalWrite( eedata.pinA, HIGH ) ;
            timer.PT = swingOnTime() * 10UL ;
            counter  = 0 ;
        }
    }

    if( !eedata.state )
    {
        digitalWrite( eedata.pinA, LOW ) ;
        digitalWrite( eedata.pinB, LOW ) ;
    }

    if( !isEffect( LIGHT_SWING ) ) return 1 ;
    return 0 ;
}


StateFunction( flashing )
{
    if( sm.entryState() )
    {
        timer.type = TIMER_BLEEP ;
        timer.PT   = eedata.switchTime * 10 ;
    }

    if( timer.update(1) )
    {
        if( eedata.state ) // BLINK
        {
            digitalWrite( eedata.pinA, !digitalRead( eedata.pinA )) ;
            if( eedata.pinB != NA ) digitalWrite( eedata.pinB, !digitalRead( eedata.pinA ) ); // B is the inverse of A
        }
        else                // OFF
        {
            digitalWrite( eedata.pinA, LOW ) ;
            if( eedata.pinB != NA ) digitalWrite( eedata.pinB, LOW ) ;
        }
    }

    if( eedata.type != singleFlashing && eedata.type != doubleFlashing ) return 1 ;

    return  0 ;
}


StateFunction( firePlaceLightState )
{
    if( sm.entryState() )
    {
        randomSeed( analogRead(0) ) ;
        timer.type = TIMER_BLEEP ;
        timer.PT   = random( 75, 90 ) ;
    }

    if( eedata.state && timer.update(1) )
    {
        if( isPwmPin( eedata.pinA ) ) analogWrite( eedata.pinA, random( 120 ) + 135 ) ;
        if( isPwmPin( eedata.pinB ) ) analogWrite( eedata.pinB, random( 120 ) + 135 ) ;
        timer.PT = random( 75, 90 ) ;
    }

    if( !eedata.state )
    {
        if( eedata.pinA != NA ) digitalWrite( eedata.pinA, LOW ) ;
        if( eedata.pinB != NA ) digitalWrite( eedata.pinB, LOW ) ;
    }

    if( !isEffect( LIGHT_FIRE ) ) return 1 ;
    return 0 ;
}


StateFunction( tubeLightState )
{
    if( sm.entryState() )
    {
        randomSeed( analogRead(0) ) ;
        counter    = 0 ;
        timer.type = TIMER_BLEEP ;
        timer.PT   = random( 10, 30 ) ;
    }

    if( eedata.state && timer.update(1) )
    {
        if( counter < 75 )                          // flickering phase
        {
            if( isPwmPin( eedata.pinA ) ) analogWrite( eedata.pinA, random(255) / 10 * 10 ) ;
            timer.PT = random( 10, 30 ) ;
            counter ++ ;
        }
        else                                        // on phase
        {
            if( isPwmPin( eedata.pinA ) ) analogWrite( eedata.pinA, 255 ) ;
            if( random( 0, 150 ) == 0 ) counter = 0 ; // occasionally flicker again
            timer.PT = 75 ;
        }
    }

    if( !eedata.state )
    {
        if( eedata.pinA != NA ) digitalWrite( eedata.pinA, LOW ) ;
    }

    if( !isEffect( LIGHT_TUBE ) ) return 1 ;
    return 0 ;
}


StateFunction( welderLightState )
{
    if( sm.entryState() )
    {
        randomSeed( analogRead(0) ) ;
        counter    = 0 ;
        timer.type = TIMER_BLEEP ;
        timer.PT   = random( 50, 800 ) ;
    }

    if( eedata.state && timer.update(1) )
    {
        counter ^= 1 ;

        if( counter )                               // flash on
        {
            if( eedata.pinA != NA ) digitalWrite( eedata.pinA, HIGH ) ;
            timer.PT = random( 20, 70 ) ;
        }
        else                                        // flash off
        {
            if( eedata.pinA != NA ) digitalWrite( eedata.pinA, LOW ) ;

            uint8_t chance = random( 0, 75 ) ;
            if( chance <= 1  ) timer.PT = 15000 ;  // short break  ~15s
            else if( chance == 2 ) timer.PT = 60000 ; // medium break ~1min
            else               timer.PT = random( 50, 800 ) ;
        }
    }

    if( !eedata.state )
    {
        if( eedata.pinA != NA ) digitalWrite( eedata.pinA, LOW ) ;
    }

    if( !isEffect( LIGHT_WELDER ) ) return 1 ;
    return 0 ;
}


// Photographer: a short bright flash at a random moment, about one in three with a pre-flash
// (red-eye reduction); a few seconds of darkness in between.
// counter: 0 = dark, 1 = pre-flash, 2 = gap after the pre-flash, 3 = flash
StateFunction( photoLightState )
{
    if( sm.entryState() )
    {
        randomSeed( analogRead(0) ) ;
        counter    = 0 ;
        timer.type = TIMER_BLEEP ;
        timer.PT   = random( 500, 3000 ) ;
    }

    if( eedata.state && timer.update(1) )
    {
        if( counter == 0 )                          // dark is over: pre-flash or straight to the flash
        {
            if( eedata.pinA != NA ) digitalWrite( eedata.pinA, HIGH ) ;
            if( random( 0, 3 ) == 0 ) { timer.PT = 15 ; counter = 1 ; }
            else                      { timer.PT = 40 ; counter = 3 ; }
        }
        else if( counter == 1 )                     // pre-flash done
        {
            if( eedata.pinA != NA ) digitalWrite( eedata.pinA, LOW ) ;
            timer.PT = random( 120, 200 ) ;
            counter  = 2 ;
        }
        else if( counter == 2 )                     // the flash
        {
            if( eedata.pinA != NA ) digitalWrite( eedata.pinA, HIGH ) ;
            timer.PT = 40 ;
            counter  = 3 ;
        }
        else                                        // flash done: dark again
        {
            if( eedata.pinA != NA ) digitalWrite( eedata.pinA, LOW ) ;
            timer.PT = random( 800, 6000 ) ;
            counter  = 0 ;
        }
    }

    if( !eedata.state )
    {
        if( eedata.pinA != NA ) digitalWrite( eedata.pinA, LOW ) ;
    }

    if( !isEffect( LIGHT_PHOTO ) ) return 1 ;
    return 0 ;
}


StateFunction( fourToneSignalState )
{
    // val byte layout:
    // bits 0-3 (LSB nibble) : LED on/off  (1=on,  0=off)
    // bits 4-7 (MSB nibble) : LED blink   (1=blink, 0=steady)
    // if both nibble bits are set: blink in opposite phase

    if( sm.entryState() )
    {
        timer.type = TIMER_BLEEP ;
        timer.PT   = 500 ;          // blink interval 500ms
    }

    const uint8_t aspect = eedata.type == blockSignal ? pos : eedata.signal.aspect ;   // block signal: computed, not stored
    uint8_t onMask   = aspect & 0x0F ;
    uint8_t blinkMask = ( aspect >> 4 ) & 0x0F ;

    const uint8_t pins[4] = { eedata.pinA, eedata.pinB, eedata.pinC, eedata.pinD } ;

    // half a blink period: a signal has its own rate (x 0.1 Hz), block signals share the board's
    timer.PT = eedata.type == blockSignal ? blockBlinkHalfMs()
             : eedata.signal.blinkRate    ? 5000 / eedata.signal.blinkRate : 500 ;
    if( timer.update(1) ) phase ^= 1 ;     // timer.Q is high for one call only: keep our own phase

    for( uint8_t i = 0 ; i < 4 ; i++ )
    {
        if( pins[i] == NA ) continue ;

        uint8_t on    = ( onMask    >> i ) & 1 ;
        uint8_t blink = ( blinkMask >> i ) & 1 ;

        if( on && blink )                   // blink in phase
        {
            digitalWrite( pins[i], phase ? HIGH : LOW ) ;
        }
        else if( !on && blink )             // blink in opposite phase
        {
            digitalWrite( pins[i], phase ? LOW : HIGH ) ;
        }
        else                                // steady on or off
        {
            digitalWrite( pins[i], on ? HIGH : LOW ) ;
        }
    }

    if( eedata.type != fourToneSignal && eedata.type != blockSignal ) return 1 ;
    return 0 ;
}


uint8_t Output::update()
{
    STATE_MACHINE_BEGIN( sm )
    {
        State( idle ) {
            if(      eedata.type == PWM          )  { sm.nextState( pwm                ) ; }
            else if( isEffect( LIGHT_SWING  ) )     { sm.nextState( swingLightState    ) ; }
            else if( isEffect( LIGHT_FIRE   ) )     { sm.nextState( firePlaceLightState) ; }
            else if( isEffect( LIGHT_TUBE   ) )     { sm.nextState( tubeLightState     ) ; }
            else if( isEffect( LIGHT_WELDER ) )     { sm.nextState( welderLightState   ) ; }
            else if( isEffect( LIGHT_PHOTO  ) )     { sm.nextState( photoLightState    ) ; }
            else if( eedata.type == lightEffect  )  { }               // unknown effect: nothing
            else if( eedata.type == fourToneSignal
                 ||  eedata.type == blockSignal  )  { sm.nextState( fourToneSignalState    ) ; }
            else if( eedata.type == singleFlashing
            ||       eedata.type == doubleFlashing) { sm.nextState( flashing           ) ; }
            else if( eedata.state == oldPos                                         // only act on a state change
                 ||  eedata.type  == notInUse
                 ||  eedata.type  == soundEffect                    // sound and board link: GPIOcore.ino
                 ||  eedata.type  == boardLink   )  { }
            else if( eedata.relayPinA != NA
                 &&  eedata.relayPinB != NA )       { oldPos = eedata.state ; sm.nextState( turnOffRelays ) ; }
            else {                                    oldPos = eedata.state ; sm.nextState( moveActuator  ) ; } }

        State( turnOffRelays ) {
            sm.nextState( moveActuator ) ; }

        State( moveActuator ) {
            if( eedata.relayPinA != NA
            &&  eedata.relayPinB != NA ) { sm.nextState( turnOnRelays ) ; }
            else                         { sm.nextState( idle ) ; } }

        State( flashing ) {
            sm.nextState( idle ) ; }

        State( swingLightState ) {
            sm.nextState( idle ) ; }

        State( firePlaceLightState ) {
            sm.nextState( idle ) ; }

        State( tubeLightState ) {
            sm.nextState( idle ) ; }

        State( welderLightState ) {
            sm.nextState( idle ) ; }

        State( photoLightState ) {
            sm.nextState( idle ) ; }

        State( fourToneSignalState ) {
            sm.nextState( idle ) ; }

        State( pwm ) {                  // was missing: PWM never ran, so it never wrote, faded or reported
            sm.nextState( idle ) ; }

        State( turnOnRelays ) {
            sm.nextState( idle ) ; }
    }
    STATE_MACHINE_END( sm ) ;
}

