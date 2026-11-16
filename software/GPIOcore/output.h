#pragma once
#include "src/stateMachineClass.h"
#include "src/ST.h"
#include "config.h"
#include <EEPROM.h>

// <OPCODE_SET_INPUT_DATA> <pinA> <pinB> <relayPinA> <relayPinB> <relayPinC> <type> <state> <union byte1> <union byte2> <union byte3> <union byte4>
// <OPCODE_SET_INPUT_DATA> <index> <addr H> <addr L> <pin> <type> <output> <threshold/4> <unused x7 7>

typedef struct eeOutputSettings 
{
    uint8_t     pinA      : 6  ; // primary IO pin
    uint8_t     pinB      : 6  ; // secondary pin
    uint8_t     pinC      : 6  ; // tertiary pin
    uint8_t     pinD      : 6  ; // quaternary pin
    uint8_t     relayPinA : 6  ; // electrofrog
    uint8_t     relayPinB : 6  ; // electrofrog
    uint8_t     relayPinC : 6  ; // unifrog
    uint8_t     type      : 4  ;
    uint8_t     state     : 1  ;

    union {
        uint32_t switchTime ;  // switch time for flashing or pulsed IO

        struct {               // servo settings
            uint8_t servoMin ;
            uint8_t servoMax ;
            uint8_t servoSpeed ;
            uint8_t servoPattern ;
        } servo ;

        struct {               // swing light settings
            uint16_t pulseTime ;   // how long each LED is on (ms)
            uint16_t pauseTime ;   // dark pause between cycles (ms)
        } swing ;

        uint8_t data[4] ;

        uint8_t pwmVal ;    // PWM: brightness; data[1] = fade time in s for 0-255 (0 = at once)
    };

} Settings ;


enum types
{
    singleSteady,
    singlePulse,
    singleFlashing,
    doubleSteady,
    doublePulse,
    doubleFlashing,
    swingLight,
    firePlaceLight,
    tubeLight,
    welderLight,
    fourToneSignal,
    servoType,
    PWM,
    notInUse,
    soundEffect         // DFPlayer Mini on TX (D1): the input chooses the track (custom values, route step, DCC extended)
} ;

const Settings defaultSettings = {
    1,      // pinA
    NA,     // pinB
    NA,     // pinC
    NA,     // pinD
    NA,     // relayPinA
    NA,     // relayPinB
    NA,     // relayPinC
    singleSteady, // type
    0,      // state
    50      // switchTime
};

enum state
{
    idle,
    turnOffRelays,
    moveActuator,
    flashing,
    swingLightState,
    firePlaceLightState,
    tubeLightState,
    welderLightState,
    fourToneSignalState,
    pwm,
    turnOnRelays,
} ;


class Output ;
extern void callbackServoPos( Output *o, uint8_t pos, uint8_t done ) __attribute__((weak)) ;   // every degree of a sweep
extern void callbackFrog(     Output *o, uint8_t relays )             __attribute__((weak)) ;   // frog relays changed: bit0 A, bit1 B, bit2 C

class Output
{
public:
    Output() ;
    uint8_t     update() ;

    void        set( Settings *data ) ;
    Settings   *get() ;
    
    uint8_t     setVal(   uint8_t   _val ) ;
    uint8_t     setState( uint8_t _state ) ;
    void        setPosition( uint8_t angle ) ;  // servo: move now, nothing stored, detach after ~1 s (live)
    void        moveTo( uint8_t angle ) ;       // servo: sweep to an angle at its speed (route / DCC extended)
    void        defaults() ;
    void        engage() { oldPos = !eedata.state ; }   // apply the stored state now (servo start-up)
    void        saveEEPROM() ;
    void        loadEEPROM() ;
    void        assignEEPROM() ;
    void        setPinModes() ;

    Settings    eedata ;
    uint8_t     position() { return pos ; }   // servo: current angle
    uint8_t     frogRelays() { return relays ; }   // bit0 relay A, bit1 relay B, bit2 relay C (unifrog)

private:
    StateMachine sm ;

    uint8_t     idleF() ;
    uint8_t     turnOffRelaysF() ;
    uint8_t     moveActuatorF() ;
    uint8_t     flashingF() ;
    uint8_t     swingLightStateF() ;
    uint8_t     firePlaceLightStateF() ;
    uint8_t     tubeLightStateF() ;
    uint8_t     welderLightStateF() ;
    uint8_t     fourToneSignalStateF() ;
    uint8_t     pwmF() ;
    uint8_t     turnOnRelaysF() ;
    uint8_t     sweep() ;
    
    uint16_t    eeAddress ;
    uint8_t     oldPos ;
    uint8_t     pos ;
    uint8_t     counter ;

    Trigger     stopTrigger ;
    Timer       timer ;
    uint8_t     attached : 1 ;
    uint8_t     live     : 1 ;              // moved by setPosition(), waiting to detach
    uint8_t     relays   : 3 ;              // last level written to relay A, B, C
    uint8_t     phase    : 1 ;              // signal: blink phase
    uint8_t     custom   : 1 ;              // servo: sweeping to 'target' instead of the state's end position
    uint8_t     target ;
    void        relayWrite( uint8_t which, uint8_t level ) ;   // 0 = A, 1 = B, 2 = C
    uint16_t    liveTime ;
} ;