#include "messageParser.h"
#include "src/Transceiver.h"
#include "output.h"
#include "input.h"
#include "routes.h"

#include "src/NmraDcc.h"     // notifyDccSigOutputState() (extern "C")

extern Output      output[16] ;
extern Input       input[16]  ;
extern Transceiver transceiver ;


// callback from the transceiver, most likely we need to process configuration data, or it is a request.

#define OPCODE(x) void inline x##F( Message *msg )

// pin rules, PROTOCOL.md section 9. NA (0x3F, or 0xFF masked) = not used
bool isValidPin( uint8_t pin, uint8_t isOutput )
{
    pin &= 0x3F ;
    if( pin == NA ) return true ;
    if( isReservedPin( pin ) ) return false ;       // D2 = DCC, D13 = config LED, A7 = config button
    if( pin >= 20 ) return !isOutput && pin <= 21 && board.type == BOARD_NANO ;   // A6/A7: analog inputs, Nano only
    if( ( pin == 18 || pin == 19 ) && ( board.flags & BOARD_RECORDER ) ) return false ;  // A4/A5 = I2C for the recorder
    return true ;
}


// To avoid compilation errors, this version uses the Input::set() public method instead of directly accessing eedata.
// Adjust 'latching' definition if needed to match your project context.

// Laat me weten als je direct de definitie van 'latching' of je MAX_OUTPUTS context hiernaast wilt integreren en centraliseren.




OPCODE( OPCODE_SET_INPUT_DATA )
{
    uint8_t index = msg->payload[0] ;
    if( index >= 16 ) return ;
    if( !isValidPin( msg->payload[3], 0 ) ) { transceiver.nack = NACK_INVALID_PIN ; return ; }
    InputSettings data ;

    int p = 1 ; // payload index

    data.myAddress  = msg->payload[ p++ ] << 8 ;
    data.myAddress |= msg->payload[ p++ ] ;

    data.pin        = msg->payload[ p++ ] ;
    data.type       = msg->payload[ p++ ] ;
    data.outputID   = msg->payload[ p++ ] ;
    // linked[8] (payload 6-13) is ignored, the route comes with SET_ROUTE

    input[index].set( &data ) ;

    // ACK is sent by Transceiver::receive()
}

OPCODE( OPCODE_GET_INPUT_DATA )
{
    uint8_t index = msg->payload[0] ;
    if( index >= 16 ) return ;

    InputSettings *data = input[index].get() ;

    
    uint8_t p = 1 ;
    msg->payload[p++] = (uint8_t)((data->myAddress >> 8) & 0xFF) ;
    msg->payload[p++] = (uint8_t)(data->myAddress & 0xFF) ;

    msg->payload[p++] = data->pin ;
    msg->payload[p++] = data->type ;
    msg->payload[p++] = data->outputID ;

    for( uint8_t i = 0 ; i < MAX_OUTPUTS ; i++ )   // v1 linked[8]: first 8 steps of the route, state only
    {
        uint8_t step[4] = { 0xFF, 0, 0, 0 } ;
        if( i < routeCount[ index ] ) readStep( index, i, step ) ;
        msg->payload[p++] = step[0] == 0xFF ? 0xFF : ( step[0] & STEP_ID ) | ( step[1] ? 0x80 : 0 ) ;
    }
    msg->command = OPCODE_SET_INPUT_DATA ;
    transceiver.sendMessage( msg ) ;
}


OPCODE( OPCODE_SET_OUTPUT_DATA )
{
    uint8_t index = msg->payload[0] ;
    if( index >= 16 ) return ;
    for( uint8_t i = 1 ; i <= 7 ; i ++ )            // pinA-D, relayA-C
    {
        if( !isValidPin( msg->payload[i], 1 ) ) { transceiver.nack = NACK_INVALID_PIN ; return ; }
    }
    Settings data ;

    int p = 1 ;

    data.pinA      = msg->payload[ p++ ] ;
    data.pinB      = msg->payload[ p++ ] ;
    data.pinC      = msg->payload[ p++ ] ;
    data.pinD      = msg->payload[ p++ ] ;
    data.relayPinA = msg->payload[ p++ ] ;
    data.relayPinB = msg->payload[ p++ ] ;
    data.relayPinC = msg->payload[ p++ ] ;
    data.type      = msg->payload[ p++ ] ;
    data.state     = msg->payload[ p++ ] ;

    for( int i=0 ; i<4 ; i++ )
    {
        data.data[i] = msg->payload[p++];
    }

    output[index].set( &data ) ;

    // ACK is sent by Transceiver::receive()
}


OPCODE( OPCODE_GET_OUTPUT_DATA )
{

    uint8_t index = msg->payload[0] ;
    if( index >= 16 ) return ;

    //Message *toSend ;
    
    Settings *data = output[index].get() ;

    uint8_t p = 1 ;
    msg->payload[p++] = data->pinA ;
    msg->payload[p++] = data->pinB ;
    msg->payload[p++] = data->pinC ;
    msg->payload[p++] = data->pinD ;
    msg->payload[p++] = data->relayPinA ;
    msg->payload[p++] = data->relayPinB ;
    msg->payload[p++] = data->relayPinC ;
    msg->payload[p++] = data->type ;
    msg->payload[p++] = data->state ;

    for( int i = 0 ; i < OUTPUT_DATA_BYTES ; i++ )
    {
        msg->payload[p++] = data->data[i] ;
    }
    msg->command = OPCODE_SET_OUTPUT_DATA ;

    transceiver.sendMessage( msg ) ;
}


// <input> <direction>: acts as if a DCC command for this input's address arrived (toggle, absolute, route)
OPCODE( OPCODE_SET_INPUT_STATE )
{
    uint8_t i = msg->payload[0] ;
    if( i >= 16 ) return ;
    input[i].setDccAddress( input[i].eedata.myAddress, msg->payload[1] ) ;
}

OPCODE( OPCODE_GET_INPUT_STATE )
{
}

OPCODE( OPCODE_DCC_EXT )
{
    notifyDccSigOutputState( ( msg->payload[0] << 8 ) | msg->payload[1], msg->payload[2] ) ;
}

OPCODE( OPCODE_SET_OUTPUT_STATE )
{
}

// <output> -> OUTPUT_EVENT with the current state
OPCODE( OPCODE_GET_OUTPUT_STATE )
{
    uint8_t i = msg->payload[0] ;
    if( i >= 16 ) return ;
    uint32_t now = millis() ;
    msg->command    = OPCODE_OUTPUT_EVENT ;
    msg->payload[1] = output[i].eedata.state ;
    msg->payload[2] = output[i].eedata.type == servoType ? output[i].position() : output[i].eedata.data[0] ;
    msg->payload[3] = now & 0xFF ;
    msg->payload[4] = ( now >> 8 ) & 0xFF ;
    transceiver.sendMessage( msg ) ;

    const Settings &s = output[i].eedata ;
    if( s.relayPinA != NA || s.relayPinB != NA || s.relayPinC != NA )   // frog relays as a second report
    {
        msg->command    = OPCODE_OUTPUT_EVENT ;
        msg->payload[0] = i ;
        msg->payload[1] = 3 ;
        msg->payload[2] = output[i].frogRelays() ;
        transceiver.sendMessage( msg ) ;
    }
}

OPCODE( OPCODE_REQ_VERSION )
{
    //Message *toSend ;
    sendMyVersion() ;
}


// OPCODE_SET_PRESET (0x61) moved to the GUI (2026-11-13): the GUI writes the preset as normal
// SET_OUTPUT_DATA / SET_INPUT_DATA messages. An old GUI still gets an ACK, nothing changes.


OPCODE( OPCODE_HELLO )
{
    uint16_t pool = freeRouteSteps() ;

    configSession = 1 ;

    msg->command    = OPCODE_HELLO_REPLY ;
    msg->payload[0] = PROTOCOL_VERSION ;
    msg->payload[1] = MAX_ROUTE_STEPS ;
    msg->payload[2] = pool & 0xFF ;
    msg->payload[3] = pool >> 8 ;
    msg->payload[4] = HELLO_BOARD | HELLO_SERVO_LIVE | HELLO_IO_ON_SERIAL
                    | ( ( board.flags & BOARD_RECORDER ) ? HELLO_EXT_EEPROM : 0 ) ;
    transceiver.sendMessage( msg ) ;
}

OPCODE( OPCODE_GET_BOARD )
{
    uint8_t n ;
    for( n = 0 ; n < BOARD_NAME_LEN ; n ++ )
    {
        uint8_t c = EEPROM.read( boardEEAddress + sizeof( board ) + n ) ;
        if( c == 0 || c == 0xFF ) break ;
        msg->payload[ 4 + n ] = c ;
    }

    msg->command    = OPCODE_SET_BOARD ;
    msg->payload[0] = 4 + n ;
    msg->payload[1] = board.type ;
    msg->payload[2] = board.flags ;
    msg->payload[3] = board.delay ;
    transceiver.sendMessage( msg ) ;
}

OPCODE( OPCODE_SET_BOARD )
{
    if( msg->payload[0] < 4 ) return ;
    uint8_t n = msg->payload[0] - 4 ;
    if( n > BOARD_NAME_LEN ) n = BOARD_NAME_LEN ;

    uint8_t oldFlags = board.flags ;
    uint8_t flags    = msg->payload[2] ;

    if( ( flags & BOARD_RECORDER ) && !( oldFlags & BOARD_RECORDER )      // recorder needs A4/A5 for I2C
    &&  ( pinInUse( 18 ) || pinInUse( 19 ) ) )
    {
        transceiver.nack = NACK_INVALID_PIN ;
        return ;
    }

    board.flags = ( flags & ~BOARD_TIMING ) | ( oldFlags & BOARD_TIMING ) ;   // recorder first, then convert the pool
    if( usedRouteSteps() * stepSize() > maxRouteBytes()
    ||  !setTiming( ( flags & BOARD_TIMING ) ? 1 : 0 ) )
    {
        board.flags     = oldFlags ;        // does not fit: keep the old settings
        transceiver.nack = NACK_FULL ;
        return ;
    }

    board.type  = msg->payload[1] ;
    board.flags = flags ;
    board.delay = msg->payload[3] ;
    updateRecorderRange() ;
    EEPROM.put( boardEEAddress, board ) ;

    for( uint8_t i = 0 ; i < BOARD_NAME_LEN ; i ++ )
    {
        EEPROM.update( boardEEAddress + sizeof( board ) + i, i < n ? msg->payload[ 4 + i ] : 0 ) ;
    }
}

OPCODE( OPCODE_GET_ROUTE )
{
    uint8_t route = msg->payload[0] ;
    uint8_t first = msg->payload[1] ;
    if( route >= 16 ) return ;

    uint8_t total = routeCount[ route ] ;
    uint8_t n     = first < total ? total - first : 0 ;
    if( n > 7 ) n = 7 ;                 // max 7 steps per message

    msg->command    = OPCODE_SET_ROUTE ;
    msg->payload[0] = 4 + 4 * n ;
    msg->payload[1] = route ;
    msg->payload[2] = total ;
    msg->payload[3] = first ;
    for( uint8_t i = 0 ; i < n ; i ++ ) readStep( route, first + i, &msg->payload[ 4 + 4 * i ] ) ;
    transceiver.sendMessage( msg ) ;
}

// <len> <route> <total> <first> <step ...>: first = 0 (re)sizes the route, the steps are written in place
OPCODE( OPCODE_SET_ROUTE )
{
    uint8_t route = msg->payload[1] ;
    uint8_t total = msg->payload[2] ;
    uint8_t first = msg->payload[3] ;
    if( route >= 16 || msg->payload[0] < 4 ) return ;

    if( first == 0 && !resizeRoute( route, total ) ) { transceiver.nack = NACK_FULL ; return ; }

    uint8_t n = ( msg->payload[0] - 4 ) / 4 ;
    for( uint8_t i = 0 ; i < n && first + i < routeCount[ route ] ; i ++ )
    {
        writeStep( route, first + i, &msg->payload[ 4 + 4 * i ] ) ;
    }
}

OPCODE( OPCODE_SERVO_LIVE )             // <output> <angle>: move now, nothing stored
{
    if( msg->payload[0] < 16 ) output[ msg->payload[0] ].setPosition( msg->payload[1] ) ;
}

#undef OPCODE

extern const char * version ;   // src/version.h, defined in GPIOcore.ino

void sendMyVersion()
{
    uint8_t len = 0 ;  // keep under 15
    Message toSend ;

    for( len = 0 ; version[len] != 0 && len < 14 ; len ++ ) toSend.payload[ len ] = version[ len ] ;

    toSend.command = OPCODE_SEND_VERSION | len ;
    transceiver.sendMessage( &toSend) ;
}

#define process(x) case x: x##F( msg ) ;/* Serial.print(#x);Serial.print("  ") ; Serial.println(x, HEX);*/ break ;

void messageReceived( Message *msg, uint8 len )  // call back message
{
    switch( msg->command )
    {
        process( OPCODE_SET_INPUT_DATA   ) ;
        process( OPCODE_GET_INPUT_DATA   ) ;
        process( OPCODE_SET_OUTPUT_DATA  ) ;
        process( OPCODE_GET_OUTPUT_DATA  ) ;
        process( OPCODE_SET_INPUT_STATE  ) ;
        process( OPCODE_GET_INPUT_STATE  ) ;
        process( OPCODE_DCC_EXT          ) ;
        process( OPCODE_SET_OUTPUT_STATE ) ;
        process( OPCODE_GET_OUTPUT_STATE ) ;
        process( OPCODE_REQ_VERSION      ) ;
        process( OPCODE_HELLO            ) ;
        process( OPCODE_GET_BOARD        ) ;
        process( OPCODE_SET_BOARD        ) ;
        process( OPCODE_GET_ROUTE        ) ;
        process( OPCODE_SET_ROUTE        ) ;
        process( OPCODE_SERVO_LIVE       ) ;
    }
}
#undef process
