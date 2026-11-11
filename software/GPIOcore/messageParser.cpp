#include "messageParser.h"
#include "src/Transceiver.h"
#include "output.h"
#include "input.h"
#include "routes.h"

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
    if( pin <=  2 ) return false ;                  // D0/D1 = Serial, D2 = DCC input
    if( pin >= 20 ) return !isOutput && pin <= 21 && board.type == BOARD_NANO ;   // A6/A7: analog inputs, Nano only
    return true ;
}

void clearUnusedOutputs( uint8_t index )
{
    for( int i = index; i < 16; i++ )
    {
        Message toSend;
        toSend.command = OPCODE_SET_OUTPUT_DATA;
        int p = 0;

        toSend.payload[p++] = i; // index

        toSend.payload[p++] = output[i].eedata.pinA      = 0xFF;
        toSend.payload[p++] = output[i].eedata.pinB      = 0xFF;
        toSend.payload[p++] = output[i].eedata.pinC      = 0xFF;
        toSend.payload[p++] = output[i].eedata.pinD      = 0xFF;
        toSend.payload[p++] = output[i].eedata.relayPinA = 0xFF;
        toSend.payload[p++] = output[i].eedata.relayPinB = 0xFF;
        toSend.payload[p++] = output[i].eedata.relayPinC = 0xFF;
        toSend.payload[p++] = output[i].eedata.type      = notInUse; 
        toSend.payload[p++] = output[i].eedata.state     = 0;

        // Union data (4 bytes) op nul zetten met dubbele assignment
        toSend.payload[p++] = output[i].eedata.data[0] = 0;
        toSend.payload[p++] = output[i].eedata.data[1] = 0;
        toSend.payload[p++] = output[i].eedata.data[2] = 0;
        toSend.payload[p++] = output[i].eedata.data[3] = 0;

        transceiver.sendMessage(&toSend);
        delay(100);
    }
}

void setupInputs(uint8_t usedInputs)
{
    for(uint8_t i = 0; i < 16; i++)
    {
        Message toSend;
        toSend.command = OPCODE_SET_INPUT_DATA;
        int p = 0;
        toSend.payload[p++] = i; // index

        if(i < usedInputs) { input[i].eedata.myAddress = input[0].eedata.myAddress + i; }
        else               { input[i].eedata.myAddress = 0xFFFF; }

        // Payload vullen volgens OPCODE_SET_INPUT_DATA structuur
        toSend.payload[p++] = (input[i].eedata.myAddress >> 8) & 0xFF; // 12 bit MSB
        toSend.payload[p++] = input[i].eedata.myAddress & 0xFF;        // LSB
        toSend.payload[p++] = input[i].eedata.pin = 0xFF;
        toSend.payload[p++] = input[i].eedata.type = ( i < usedInputs ) ? LATCHING : NOT_IN_USE ; // outputID 0xFF would become 15
        if(i < usedInputs)  toSend.payload[p++] = input[i].eedata.outputID = i ;
        else                toSend.payload[p++] = input[i].eedata.outputID = 0xFF ;

        for(int j = 0; j < MAX_OUTPUTS; j++) { toSend.payload[p++] = 0xFF; }

        transceiver.sendMessage(&toSend);
        input[i].saveEEPROM() ;
        delay(100);
    }
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


OPCODE( OPCODE_SET_INPUT_STATE )
{
}

OPCODE( OPCODE_GET_INPUT_STATE )
{
}

OPCODE( OPCODE_SET_OUTPUT_STATE )
{
}

OPCODE( OPCODE_GET_OUTPUT_STATE )
{
}

OPCODE( OPCODE_REQ_VERSION )
{
    //Message *toSend ;
    sendMyVersion() ;
}


OPCODE( OPCODE_SET_PRESET )
{
    uint8_t preset = msg->payload[0];
    uint8_t pinIndex = 3;   // skip D0/D1 (Serial) and D2 (DCC)
    uint8_t i ;

    switch( preset )
    {
    case 0: // just servos
        for( i=0; i<16; i++ )
        {
            Message toSend;
            toSend.command = OPCODE_SET_OUTPUT_DATA;
            int p = 0;
            toSend.payload[p++] = i; // index
            toSend.payload[p++] = output[i].eedata.pinA      = pinIndex++;
            toSend.payload[p++] = output[i].eedata.pinB      = 0xFF;
            toSend.payload[p++] = output[i].eedata.pinC      = 0xFF;
            toSend.payload[p++] = output[i].eedata.pinD      = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinA = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinB = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinC = 0xFF;
            toSend.payload[p++] = output[i].eedata.type      = servoType;
            toSend.payload[p++] = output[i].eedata.state     = 0;

            toSend.payload[p++] = output[i].eedata.servo.servoMin     = 80;
            toSend.payload[p++] = output[i].eedata.servo.servoMax     = 100;
            toSend.payload[p++] = output[i].eedata.servo.servoSpeed   = 20;
            toSend.payload[p++] = output[i].eedata.servo.servoPattern = 1;

            transceiver.sendMessage(&toSend);
            delay(50);
        }
        setupInputs(16) ;
        break;

    case 1: // servo unifrog relay
        for( i=0; i<8; i++ )
        {
            Message toSend;
            toSend.command = OPCODE_SET_OUTPUT_DATA;
            int p = 0;
            toSend.payload[p++] = i; // index
            toSend.payload[p++] = output[i].eedata.pinA      = pinIndex++;
            toSend.payload[p++] = output[i].eedata.pinB      = 0xFF;
            toSend.payload[p++] = output[i].eedata.pinC      = 0xFF;
            toSend.payload[p++] = output[i].eedata.pinD      = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinA = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinB = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinC = pinIndex++;
            toSend.payload[p++] = output[i].eedata.type      = servoType;
            toSend.payload[p++] = output[i].eedata.state     = 0;

            toSend.payload[p++] = output[i].eedata.servo.servoMin     = 80;
            toSend.payload[p++] = output[i].eedata.servo.servoMax     = 100;
            toSend.payload[p++] = output[i].eedata.servo.servoSpeed   = 20;
            toSend.payload[p++] = output[i].eedata.servo.servoPattern = 1;

            transceiver.sendMessage(&toSend);
            delay(100);
        }
        setupInputs(8) ;
        break;

    case 2: // servo electrofrog relays
        for( i=0; i<5; i++ )
        {
            Message toSend;
            toSend.command = OPCODE_SET_OUTPUT_DATA;
            int p = 0;
            toSend.payload[p++] = i; // index
            toSend.payload[p++] = output[i].eedata.pinA      = pinIndex++;
            toSend.payload[p++] = output[i].eedata.pinB      = 0xFF;
            toSend.payload[p++] = output[i].eedata.pinC      = 0xFF;
            toSend.payload[p++] = output[i].eedata.pinD      = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinA = pinIndex++;
            toSend.payload[p++] = output[i].eedata.relayPinB = pinIndex++;
            toSend.payload[p++] = output[i].eedata.relayPinC = 0xFF;
            toSend.payload[p++] = output[i].eedata.type      = servoType;
            toSend.payload[p++] = output[i].eedata.state     = 0;

            toSend.payload[p++] = output[i].eedata.servo.servoMin     = 80;
            toSend.payload[p++] = output[i].eedata.servo.servoMax     = 100;
            toSend.payload[p++] = output[i].eedata.servo.servoSpeed   = 20;
            toSend.payload[p++] = output[i].eedata.servo.servoPattern = 1;

            transceiver.sendMessage(&toSend);
            delay(100);
        }
        setupInputs(5) ;
        break;

    case 3: // single steady
        for( i=0; i<16; i++ )
        {
            Message toSend;
            toSend.command = OPCODE_SET_OUTPUT_DATA;
            int p = 0;
            toSend.payload[p++] = i; // index
            toSend.payload[p++] = output[i].eedata.pinA      = pinIndex++;
            toSend.payload[p++] = output[i].eedata.pinB      = 0xFF;
            toSend.payload[p++] = output[i].eedata.pinC      = 0xFF;
            toSend.payload[p++] = output[i].eedata.pinD      = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinA = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinB = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinC = 0xFF;
            toSend.payload[p++] = output[i].eedata.type      = singleSteady;
            toSend.payload[p++] = output[i].eedata.state     = 0;

            uint32_t switchTime = 0;
            output[i].eedata.switchTime = switchTime ;
            toSend.payload[p++] = (switchTime)       & 0xFF;
            toSend.payload[p++] = (switchTime >> 8)  & 0xFF;
            toSend.payload[p++] = (switchTime >> 16) & 0xFF;
            toSend.payload[p++] = (switchTime >> 24) & 0xFF;

            transceiver.sendMessage(&toSend);
            delay(100);
        }
        setupInputs(16) ;
        break;

    case 4: // double pulse
        for( i=0; i<8; i++ )
        {
            Message toSend;
            toSend.command = OPCODE_SET_OUTPUT_DATA;
            int p = 0;
            toSend.payload[p++] = i; // index
            toSend.payload[p++] = output[i].eedata.pinA      = pinIndex++;
            toSend.payload[p++] = output[i].eedata.pinB      = pinIndex++;
            toSend.payload[p++] = output[i].eedata.pinC      = 0xFF;
            toSend.payload[p++] = output[i].eedata.pinD      = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinA = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinB = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinC = 0xFF;
            toSend.payload[p++] = output[i].eedata.type      = doublePulse;
            toSend.payload[p++] = output[i].eedata.state     = 0;

            uint32_t switchTime = 50;
            output[i].eedata.switchTime = switchTime ;
            toSend.payload[p++] = (switchTime)       & 0xFF;
            toSend.payload[p++] = (switchTime >> 8)  & 0xFF;
            toSend.payload[p++] = (switchTime >> 16) & 0xFF;
            toSend.payload[p++] = (switchTime >> 24) & 0xFF;

            transceiver.sendMessage(&toSend);
            delay(100);
        }
        setupInputs(8) ;
        break;

    case 5: // double pulse w/ unifrog relay
        for( i=0; i<5; i++ )
        {
            Message toSend;
            toSend.command = OPCODE_SET_OUTPUT_DATA;
            int p = 0;
            toSend.payload[p++] = i; // index
            toSend.payload[p++] = output[i].eedata.pinA      = pinIndex++;
            toSend.payload[p++] = output[i].eedata.pinB      = pinIndex++;
            toSend.payload[p++] = output[i].eedata.pinC      = 0xFF;
            toSend.payload[p++] = output[i].eedata.pinD      = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinA = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinB = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinC = pinIndex++;
            toSend.payload[p++] = output[i].eedata.type      = doublePulse;
            toSend.payload[p++] = output[i].eedata.state     = 0;

            uint32_t switchTime = 50;
            output[i].eedata.switchTime = switchTime ;
            toSend.payload[p++] = (switchTime)       & 0xFF;
            toSend.payload[p++] = (switchTime >> 8)  & 0xFF;
            toSend.payload[p++] = (switchTime >> 16) & 0xFF;
            toSend.payload[p++] = (switchTime >> 24) & 0xFF;

            transceiver.sendMessage(&toSend);
            delay(100);
        }
        setupInputs(5) ;
        break;

    case 6: // double pulse w/ electrofrog relay
        for( i=0; i<4; i++ )
        {
            Message toSend;
            toSend.command = OPCODE_SET_OUTPUT_DATA;
            int p = 0;
            toSend.payload[p++] = i; // index
            toSend.payload[p++] = output[i].eedata.pinA      = pinIndex++;
            toSend.payload[p++] = output[i].eedata.pinB      = pinIndex++;
            toSend.payload[p++] = output[i].eedata.pinC      = 0xFF;
            toSend.payload[p++] = output[i].eedata.pinD      = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinA = pinIndex++;
            toSend.payload[p++] = output[i].eedata.relayPinB = pinIndex++;
            toSend.payload[p++] = output[i].eedata.relayPinC = 0xFF;
            toSend.payload[p++] = output[i].eedata.type      = doublePulse;
            toSend.payload[p++] = output[i].eedata.state     = 0;

            uint32_t switchTime = 50;
            output[i].eedata.switchTime = switchTime ;
            toSend.payload[p++] = (switchTime)       & 0xFF;
            toSend.payload[p++] = (switchTime >> 8)  & 0xFF;
            toSend.payload[p++] = (switchTime >> 16) & 0xFF;
            toSend.payload[p++] = (switchTime >> 24) & 0xFF;

            transceiver.sendMessage(&toSend);
            delay(100);
        }
        setupInputs(4) ;
        break;

    case 7: // double steady
        for( i=0; i<8; i++ )
        {
            Message toSend;
            toSend.command = OPCODE_SET_OUTPUT_DATA;
            int p = 0;
            toSend.payload[p++] = i; // index
            toSend.payload[p++] = output[i].eedata.pinA      = pinIndex++;
            toSend.payload[p++] = output[i].eedata.pinB      = pinIndex++;
            toSend.payload[p++] = output[i].eedata.pinC      = 0xFF;
            toSend.payload[p++] = output[i].eedata.pinD      = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinA = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinB = 0xFF;
            toSend.payload[p++] = output[i].eedata.relayPinC = 0xFF;
            toSend.payload[p++] = output[i].eedata.type      = doubleSteady;
            toSend.payload[p++] = output[i].eedata.state     = 0;

            uint32_t switchTime = 0;
            output[i].eedata.switchTime = switchTime ;
            toSend.payload[p++] = (switchTime)       & 0xFF;
            toSend.payload[p++] = (switchTime >> 8)  & 0xFF;
            toSend.payload[p++] = (switchTime >> 16) & 0xFF;
            toSend.payload[p++] = (switchTime >> 24) & 0xFF;

            transceiver.sendMessage(&toSend);
            delay(100);
        }
        setupInputs(8) ;
        break;
    }

    clearUnusedOutputs( i ) ;
    for( i = 0 ; i < 16 ; i ++ ) { output[i].saveEEPROM(); }
}


OPCODE( OPCODE_HELLO )
{
    uint16_t pool = freeRouteSteps() ;

    configSession = 1 ;

    msg->command    = OPCODE_HELLO_REPLY ;
    msg->payload[0] = PROTOCOL_VERSION ;
    msg->payload[1] = MAX_ROUTE_STEPS ;
    msg->payload[2] = pool & 0xFF ;
    msg->payload[3] = pool >> 8 ;
    msg->payload[4] = HELLO_BOARD ;
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

    board.type  = msg->payload[1] ;
    board.flags = msg->payload[2] ;
    board.delay = msg->payload[3] ;
    EEPROM.put( boardEEAddress, board ) ;

    for( uint8_t i = 0 ; i < BOARD_NAME_LEN ; i ++ )
    {
        EEPROM.update( boardEEAddress + sizeof( board ) + i, i < n ? msg->payload[ 4 + i ] : 0 ) ;
    }
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
        process( OPCODE_SET_OUTPUT_STATE ) ;
        process( OPCODE_GET_OUTPUT_STATE ) ;
        process( OPCODE_REQ_VERSION      ) ;
        process( OPCODE_SET_PRESET       ) ;
        process( OPCODE_HELLO            ) ;
        process( OPCODE_GET_BOARD        ) ;
        process( OPCODE_SET_BOARD        ) ;
    }
}
#undef process
