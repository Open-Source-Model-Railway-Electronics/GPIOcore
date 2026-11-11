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
    eedata.pwmVal = _val ;
    saveEEPROM() ;
    return 0 ;
}

void Output::set( Settings *data )
{
    eedata = *data ;
    oldPos = !eedata.state ;    // new settings: apply the state once, also when unchanged
    saveEEPROM() ; // Opslaan indien gewenst
}

Settings* Output::get()
{
    return &eedata ; 
}

uint8_t Output::setState( uint8_t _state)
{
    eedata.state = _state ;
    saveEEPROM() ;
    return 0 ;
}


void Output::defaults( )
{
    static byte index = 0 ;
    eedata = defaultSettings ;
    eedata.pinA = 3 + index ++  ;   // skip D0/D1 (Serial) and D2 (DCC)
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

void Output::loadEEPROM()
{
    EEPROM.get(eeAddress, eedata);
    if( eedata.pinA      != NA ) pinMode( eedata.pinA,      OUTPUT ) ;
    if( eedata.pinB      != NA ) pinMode( eedata.pinB,      OUTPUT ) ;
    if( eedata.pinC      != NA ) pinMode( eedata.pinC,      OUTPUT ) ;
    if( eedata.pinD      != NA ) pinMode( eedata.pinD,      OUTPUT ) ;
    if( eedata.relayPinA != NA ) pinMode( eedata.relayPinA, OUTPUT ) ;
    if( eedata.relayPinB != NA ) pinMode( eedata.relayPinB, OUTPUT ) ;
    if( eedata.relayPinC != NA ) pinMode( eedata.relayPinC, OUTPUT ) ;

    // apply the stored state once after boot, except for coils (no burst of pulses at power up)
    oldPos = ( eedata.type == singlePulse || eedata.type == doublePulse ) ? eedata.state : !eedata.state ;
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

        if( eedata.state ) { setPoint = eedata.servo.servoMax ; }
        else               { setPoint = eedata.servo.servoMin ; }

        if( pos < setPoint ) { pos ++ ; }
        if( pos > setPoint ) { pos -- ; }

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
        if( eedata.relayPinC != NA ) digitalWrite( eedata.relayPinC, pos < middlePos ? LOW : HIGH ) ;
    }

    return 0 ;
}


#define StateFunction( x ) uint8_t Output::x##F()
StateFunction( idle )
{    
    return 0 ;
}

StateFunction( pwm )
{
    if( sm.entryState() )
    {
        timer.type  = TIMER_BLEEP ;
        timer.PT    =         100 ;
    }

    timer.update() ;
    if( timer.Q )                                      
    {
        if( isPwmPin( eedata.pinA ) ) { analogWrite( eedata.pinA, eedata.pwmVal ) ; }
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

        if( eedata.relayPinA != NA ) digitalWrite( eedata.relayPinA,  eedata.state ) ;
        if( eedata.relayPinB != NA ) digitalWrite( eedata.relayPinB, !eedata.state ) ;
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

        if( eedata.relayPinA != NA ) digitalWrite( eedata.relayPinA, LOW ) ;
        if( eedata.relayPinB != NA ) digitalWrite( eedata.relayPinB, LOW ) ;
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
                if( eedata.relayPinC != NA ) digitalWrite( eedata.relayPinC, eedata.state ) ; // toggle the unifrog relay
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
        timer.PT   = eedata.swing.pulseTime * 10 ;
        digitalWrite( eedata.pinA, eedata.state ? HIGH : LOW ) ;
        digitalWrite( eedata.pinB, LOW ) ;
    }

    if( eedata.state && timer.update(1) )
    {
        if( counter == 0 )          // A was on → switch to B
        {
            digitalWrite( eedata.pinA, LOW  ) ;
            digitalWrite( eedata.pinB, HIGH ) ;
            timer.PT = eedata.swing.pulseTime * 10 ;
            counter  = 1 ;
        }
        else if( counter == 1 )     // B was on → pause
        {
            digitalWrite( eedata.pinB, LOW ) ;
            timer.PT = eedata.swing.pauseTime * 10 ;
            counter  = 2 ;
        }
        else                        // pause done → A on
        {
            digitalWrite( eedata.pinA, HIGH ) ;
            timer.PT = eedata.swing.pulseTime * 10 ;
            counter  = 0 ;
        }
    }

    if( !eedata.state )
    {
        digitalWrite( eedata.pinA, LOW ) ;
        digitalWrite( eedata.pinB, LOW ) ;
    }

    if( eedata.type != swingLight ) return 1 ;
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
            if( eedata.pinB != NA ) digitalWrite( eedata.pinB, !digitalRead( eedata.pinA ) ); // maken B inverted from A
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

    if( eedata.type != firePlaceLight ) return 1 ;
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

    if( eedata.type != tubeLight ) return 1 ;
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

    if( eedata.type != welderLight ) return 1 ;
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

    uint8_t onMask   = eedata.pwmVal & 0x0F ;
    uint8_t blinkMask = ( eedata.pwmVal >> 4 ) & 0x0F ;

    const uint8_t pins[4] = { eedata.pinA, eedata.pinB, eedata.pinC, eedata.pinD } ;

    timer.update(1) ;

    for( uint8_t i = 0 ; i < 4 ; i++ )
    {
        if( pins[i] == NA ) continue ;

        uint8_t on    = ( onMask    >> i ) & 1 ;
        uint8_t blink = ( blinkMask >> i ) & 1 ;

        if( on && blink )                   // blink in phase
        {
            digitalWrite( pins[i], timer.Q ? HIGH : LOW ) ;
        }
        else if( !on && blink )             // blink in opposite phase
        {
            digitalWrite( pins[i], timer.Q ? LOW : HIGH ) ;
        }
        else                                // steady on or off
        {
            digitalWrite( pins[i], on ? HIGH : LOW ) ;
        }
    }

    if( eedata.type != fourToneSignal ) return 1 ;
    return 0 ;
}


uint8_t Output::update()
{
    STATE_MACHINE_BEGIN( sm )
    {
        State( idle ) {
            if(      eedata.type == PWM          )  { sm.nextState( pwm                ) ; }
            else if( eedata.type == swingLight   )  { sm.nextState( swingLightState    ) ; }
            else if( eedata.type == firePlaceLight) { sm.nextState( firePlaceLightState) ; }
            else if( eedata.type == tubeLight    )  { sm.nextState( tubeLightState     ) ; }
            else if( eedata.type == welderLight  )  { sm.nextState( welderLightState   ) ; }
            else if( eedata.type == fourToneSignal   )  { sm.nextState( fourToneSignalState    ) ; }
            else if( eedata.type == singleFlashing
            ||       eedata.type == doubleFlashing) { sm.nextState( flashing           ) ; }
            else if( eedata.state == oldPos                                         // only act on a state change
                 ||  eedata.type  == notInUse )     { }
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

        State( fourToneSignalState ) {
            sm.nextState( idle ) ; }

        State( turnOnRelays ) {
            sm.nextState( idle ) ; }
    }
    STATE_MACHINE_END( sm ) ;
}

