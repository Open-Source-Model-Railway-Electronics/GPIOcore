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
    uint8_t     invertFrog : 1 ; // frog relays active low (relay modules that switch on LOW)

    union {                    // the 4 settings bytes, one struct per output type
        uint32_t switchTime ;  // switch time for flashing or pulsed IO

        struct {               // servoType
            uint8_t servoMin ;
            uint8_t servoMax ;
            uint8_t servoSpeed ;
            uint8_t followInput : 4 ;   // semaphore arm: the input (block detector) it follows
            uint8_t             : 2 ;
            uint8_t followSwap  : 1 ;   // occupied = straight instead of curved
            uint8_t follows     : 1 ;   // follows followInput instead of DCC
        } servo ;

        struct {               // lightEffect
            uint8_t time[3] ;      // swing: on time (12 bits) and pause (12 bits), x 10 ms; other effects: unused
            uint8_t effect ;       // LIGHT_SWING, LIGHT_FIRE, ...
        } light ;

        struct {               // PWM
            uint8_t value ;        // brightness 0-255
            uint8_t fadeTime ;     // s for a full 0-255 change, 0 = at once
        } pwm ;

        struct {               // fourToneSignal
            uint8_t aspect ;       // lamps A-D: bits 0-3 on, bits 4-7 blink (both: blink in opposite phase)
            uint8_t blinkRate ;    // x 0.1 Hz, 0 = 1 Hz
        } signal ;

        struct {               // blockSignal (the rate: board flags, blockBlinkHalfMs())
            uint8_t aspect[3] ;    // own block occupied / next block occupied / both free
            uint8_t ownBlock  : 4 ;    // detector inputs; nextBlock == ownBlock: no next block
            uint8_t nextBlock : 4 ;
        } block ;

        uint8_t data[4] ;      // raw, as the protocol sends them
    };

} Settings ;

#ifdef __AVR__
static_assert( sizeof( Settings ) == 10, "EEPROM layout: an output is 10 bytes" ) ;   // the 6-bit pins are packed
#endif


enum types
{
    singleSteady,
    singlePulse,
    singleFlashing,
    doubleSteady,
    doublePulse,
    doubleFlashing,
    lightEffect,        // one type for all light effects, the effect in Settings.light.effect
    unusedType7,        // free (were the fireplace, tube and welder types)
    unusedType8,
    unusedType9,
    fourToneSignal,
    servoType,
    PWM,
    notInUse,
    soundEffect,        // DFPlayer Mini on TX (D1): the input chooses the track (custom values, route step, DCC extended)
    blockSignal         // automatic block signal, lamps A-D, settings in Settings.block; the current aspect
                        // is only in RAM
} ;

enum lightEffects     // Settings.light.effect
{
    LIGHT_SWING,        // A and B alternate, then a pause (level crossing)
    LIGHT_FIRE,         // random flicker on PWM pins A and B
    LIGHT_TUBE,         // fluorescent tube start-up flicker on PWM pin A
    LIGHT_WELDER,       // random welding flashes on pin A
    LIGHT_PHOTO,        // photographer: a flash now and then, sometimes with a pre-flash
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
    0,      // invertFrog
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
    photoLightState,
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
    uint8_t     position() { return pos ; }   // servo: current angle, block signal: current aspect
    uint8_t     showAspect( uint8_t a ) { if( a == pos ) return 0 ; pos = a ; return 1 ; }   // block signal: 1 = changed
    uint8_t     frogRelays() { return relays ; }   // bit0 relay A, bit1 relay B, bit2 relay C (unifrog)
    void        refreshRelays() ;                  // write the relay levels again (after invertFrog changed)

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
    uint8_t     photoLightStateF() ;
    uint8_t     fourToneSignalStateF() ;
    uint8_t     isEffect( uint8_t e ) { return eedata.type == lightEffect && eedata.light.effect == e ; }
    uint16_t    swingOnTime() { return eedata.light.time[0] | ( eedata.light.time[1] & 0x0F ) << 8 ; }   // x 10 ms
    uint16_t    swingPause()  { return eedata.light.time[1] >> 4 | eedata.light.time[2] << 4 ; }        // x 10 ms
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