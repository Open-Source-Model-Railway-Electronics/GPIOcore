#include "routes.h"
#include "src/event.h"
#include "output.h"
#include "input.h"

extern EventHandler recorder ;
extern Output       output[16] ;
extern Input        input[16] ;

uint8_t routeCount[16] ;    // cached copy of EE_COUNTS


void routesBegin()
{
    for( uint8_t i = 0 ; i < 16 ; i ++ )
    {
        routeCount[i] = EEPROM.read( EE_COUNTS + i ) ;
        if( routeCount[i] > MAX_STEPS_PER_ROUTE ) routeCount[i] = 0 ;
    }
    updateRecorderRange() ;
}

uint8_t stepSize()
{
    return ( board.flags & BOARD_TIMING ) ? 4 : 2 ;
}

uint16_t routeAddress( uint8_t route, uint8_t step )   // route 16 = end of all routes
{
    uint16_t steps = step ;
    for( uint8_t i = 0 ; i < route ; i ++ ) steps += routeCount[i] ;
    return EE_POOL + steps * stepSize() ;
}

uint16_t usedRouteSteps()
{
    uint16_t steps = 0 ;
    for( uint8_t i = 0 ; i < 16 ; i ++ ) steps += routeCount[i] ;
    return steps ;
}

uint16_t maxRouteBytes()
{
    return EE_END - EE_POOL ;
}

uint16_t freeRouteSteps()
{
    uint16_t maxSteps = maxRouteBytes() / stepSize() ;
    uint16_t used     = usedRouteSteps() ;
    return used < maxSteps ? maxSteps - used : 0 ;
}

uint8_t resizeRoute( uint8_t route, uint8_t total )
{
    if( route >= 16 || total > MAX_STEPS_PER_ROUTE ) return 0 ;
    if( total > routeCount[route] && total - routeCount[route] > freeRouteSteps() ) return 0 ;

    uint16_t from  = routeAddress( route + 1, 0 ) ;    // first byte of the routes behind this one
    uint16_t end   = routeAddress( 16, 0 ) ;
    int16_t  delta = ( (int16_t)total - routeCount[route] ) * stepSize() ;

    if( delta > 0 ) { for( uint16_t a = end ;  a -- > from ; )  EEPROM.update( a + delta, EEPROM.read( a ) ) ; }
    if( delta < 0 ) { for( uint16_t a = from ; a < end ; a ++ ) EEPROM.update( a + delta, EEPROM.read( a ) ) ; }

    routeCount[route] = total ;
    EEPROM.update( EE_COUNTS + route, total ) ;
    updateRecorderRange() ;
    return 1 ;
}

void readStep( uint8_t route, uint8_t step, uint8_t *b )
{
    uint16_t a = routeAddress( route, step ) ;
    b[2] = b[3] = 0 ;
    for( uint8_t i = 0 ; i < stepSize() ; i ++ ) b[i] = EEPROM.read( a + i ) ;
}

void writeStep( uint8_t route, uint8_t step, const uint8_t *b )
{
    uint16_t a = routeAddress( route, step ) ;
    for( uint8_t i = 0 ; i < stepSize() ; i ++ ) EEPROM.update( a + i, b[i] ) ;
}

// timing on: 2 -> 4 B per step, the new wait time is the fixed board delay. Timing off: 4 -> 2 B.
uint8_t setTiming( uint8_t on )
{
    uint16_t steps = usedRouteSteps() ;
    uint8_t  isOn  = ( board.flags & BOARD_TIMING ) ? 1 : 0 ;

    if( on == isOn ) return 1 ;

    if( on )
    {
        if( steps * 4 > maxRouteBytes() ) return 0 ;
        for( uint16_t s = steps ; s -- > 0 ; )          // from the top down, the pool grows
        {
            EEPROM.update( EE_POOL + s * 4 + 3, 0 ) ;
            EEPROM.update( EE_POOL + s * 4 + 2, board.delay ) ;
            EEPROM.update( EE_POOL + s * 4 + 1, EEPROM.read( EE_POOL + s * 2 + 1 ) ) ;
            EEPROM.update( EE_POOL + s * 4,     EEPROM.read( EE_POOL + s * 2     ) ) ;
        }
        board.flags |= BOARD_TIMING ;
    }
    else
    {
        for( uint16_t s = 0 ; s < steps ; s ++ )        // from the bottom up, the pool shrinks
        {
            EEPROM.update( EE_POOL + s * 2,     EEPROM.read( EE_POOL + s * 4     ) ) ;
            EEPROM.update( EE_POOL + s * 2 + 1, EEPROM.read( EE_POOL + s * 4 + 1 ) ) ;
        }
        board.flags &= ~BOARD_TIMING ;
    }

    updateRecorderRange() ;
    return 1 ;
}

// the recorder uses the external I2C EEPROM; I2C (A4/A5) only runs while the recorder is enabled
void updateRecorderRange()
{
    if( board.flags & BOARD_RECORDER )
    {
        recorder.begin() ;
        recorder.setRange( 0, EXT_EEPROM_SIZE ) ;
    }
    else
    {
        recorder.end() ;
        recorder.setRange( 0, 0 ) ;
    }
}

uint8_t pinInUse( uint8_t pin )
{
    for( uint8_t i = 0 ; i < 16 ; i ++ )
    {
        const Settings &o = output[i].eedata ;
        if( o.pinA == pin || o.pinB == pin || o.pinC == pin || o.pinD == pin
        ||  o.relayPinA == pin || o.relayPinB == pin || o.relayPinC == pin ) return 1 ;
        if( input[i].eedata.pin == pin && input[i].eedata.type != NOT_IN_USE ) return 1 ;
    }
    return 0 ;
}
