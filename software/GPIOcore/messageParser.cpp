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
    if( isAnalogInput( pin ) )                      // analog compare: inputs on A0-A7 only, the same rules as that pin
    {
        if( isOutput ) return false ;
        pin = inputPinNumber( pin ) ;
    }
    if( isReservedPin( pin ) ) return false ;       // D2 = DCC, D13 = config LED, A7 = config button
    if( pin >= 20 ) return !isOutput && pin <= 21 && board.type == BOARD_NANO ;   // A6/A7: analog inputs, Nano only
    return true ;
}

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
    data.level      = msg->payload[ p   ] >> 1 ;   // payload 6: analog threshold (0-1016) / 4, see PROTOCOL.md
    // payload 7-13 are unused, the route comes with SET_ROUTE

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

    for( uint8_t i = 0 ; i < 8 ; i++ ) msg->payload[p++] = 0xFF ;          // payload 6-13: unused, the route comes with GET_ROUTE
    if( isAnalogInput( data->pin ) ) msg->payload[6] = data->level << 1 ;   // payload 6 = analog threshold / 4
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
    data.state      = msg->payload[ p   ] & 1 ;
    data.invertFrog = ( msg->payload[ p++ ] >> 1 ) & 1 ;   // bit1: frog relays active low

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
    msg->payload[p++] = data->state | ( data->invertFrog << 1 ) ;

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

OPCODE( OPCODE_GET_ANALOG )             // live reading of an analog pin, for setting up analog inputs
{
    uint8_t  ch = msg->payload[0] & 7 ;
    uint16_t v  = analogRead( 14 + ch ) ;
    msg->command    = OPCODE_ANALOG_VALUE ;
    msg->payload[0] = ch ;
    msg->payload[1] = v >> 8 ;
    msg->payload[2] = v & 0xFF ;
    transceiver.sendMessage( msg ) ;
}

// <output> -> OUTPUT_EVENT with the current state
OPCODE( OPCODE_GET_OUTPUT_STATE )
{
    uint8_t i = msg->payload[0] ;
    if( i >= 16 ) return ;
    uint32_t now = millis() ;
    msg->command    = OPCODE_OUTPUT_EVENT ;
    const uint8_t type = output[i].eedata.type ;
    msg->payload[1] = ( type == fourToneSignal || type == blockSignal ) ? 2 : output[i].eedata.state ;   // signals: the aspect
    msg->payload[2] = ( type == servoType || type == blockSignal ) ? output[i].position() : output[i].eedata.data[0] ;
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



// HELLO reply; sent unasked (without a session) after a short press on the config button: the announce
void sendHelloReply()
{
    uint16_t pool = freeRouteSteps() ;
    Message m ;
    m.command    = OPCODE_HELLO_REPLY ;
    m.payload[0] = PROTOCOL_VERSION ;
    m.payload[1] = MAX_ROUTE_STEPS ;
    m.payload[2] = pool & 0xFF ;
    m.payload[3] = pool >> 8 ;
    m.payload[4] = HELLO_BOARD | HELLO_SERVO_LIVE | HELLO_IO_ON_SERIAL | HELLO_ANALOG_SOUND | HELLO_BLUETOOTH ;
    transceiver.sendMessage( &m ) ;
}

OPCODE( OPCODE_HELLO )
{
    configSession = 1 ;
    sendHelloReply() ;
}

OPCODE( OPCODE_BT_SETUP )               // done by loop() after this message: it changes the baud rate
{
    btSetupPending = 1 ;
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

    flags &= ~BOARD_RESERVED ;
    board.flags = ( flags & ~BOARD_TIMING ) | ( oldFlags & BOARD_TIMING ) ;   // other flags first, then convert the pool
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
        process( OPCODE_GET_ANALOG       ) ;
        process( OPCODE_BT_SETUP         ) ;
    }
}
#undef process
