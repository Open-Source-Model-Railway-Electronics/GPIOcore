#include "routes.h"
#include "output.h"
#include "input.h"

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
}

uint8_t stepSize()
{
    return ( board.flags & BOARD_TIMING ) ? 5 : 3 ;
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
    return 1 ;
}

void readStep( uint8_t route, uint8_t step, uint8_t *b )
{
    uint16_t a = routeAddress( route, step ) ;
    b[0] = EEPROM.read( a ) ;
    b[1] = EEPROM.read( a + 1 ) ;
    if( stepSize() == 5 ) { b[2] = EEPROM.read( a + 2 ) ; b[3] = EEPROM.read( a + 3 ) ; b[4] = EEPROM.read( a + 4 ) ; }
    else                  { b[2] = b[3] = 0 ;                                            b[4] = EEPROM.read( a + 2 ) ; }
}

void writeStep( uint8_t route, uint8_t step, const uint8_t *b )
{
    uint16_t a = routeAddress( route, step ) ;
    EEPROM.update( a,     b[0] ) ;
    EEPROM.update( a + 1, b[1] ) ;
    if( stepSize() == 5 ) { EEPROM.update( a + 2, b[2] ) ; EEPROM.update( a + 3, b[3] ) ; EEPROM.update( a + 4, b[4] ) ; }
    else                    EEPROM.update( a + 2, b[4] ) ;
}

// timing on: 3 -> 5 B per step, the new wait time is the fixed board delay. Timing off: 5 -> 3 B.
uint8_t setTiming( uint8_t on )
{
    uint16_t steps = usedRouteSteps() ;
    uint8_t  isOn  = ( board.flags & BOARD_TIMING ) ? 1 : 0 ;

    if( on == isOn ) return 1 ;

    if( on )
    {
        if( steps * 5 > maxRouteBytes() ) return 0 ;
        for( uint16_t s = steps ; s -- > 0 ; )          // from the top down, the pool grows
        {
            uint8_t id = EEPROM.read( EE_POOL + s * 3 ), value = EEPROM.read( EE_POOL + s * 3 + 1 ), extra = EEPROM.read( EE_POOL + s * 3 + 2 ) ;
            EEPROM.update( EE_POOL + s * 5 + 4, extra ) ;
            EEPROM.update( EE_POOL + s * 5 + 3, 0 ) ;
            EEPROM.update( EE_POOL + s * 5 + 2, board.delay ) ;
            EEPROM.update( EE_POOL + s * 5 + 1, value ) ;
            EEPROM.update( EE_POOL + s * 5,     id ) ;
        }
        board.flags |= BOARD_TIMING ;
    }
    else
    {
        for( uint16_t s = 0 ; s < steps ; s ++ )        // from the bottom up, the pool shrinks
        {
            uint8_t id = EEPROM.read( EE_POOL + s * 5 ), value = EEPROM.read( EE_POOL + s * 5 + 1 ), extra = EEPROM.read( EE_POOL + s * 5 + 4 ) ;
            EEPROM.update( EE_POOL + s * 3,     id ) ;
            EEPROM.update( EE_POOL + s * 3 + 1, value ) ;
            EEPROM.update( EE_POOL + s * 3 + 2, extra ) ;
        }
        board.flags &= ~BOARD_TIMING ;
    }

    return 1 ;
}
