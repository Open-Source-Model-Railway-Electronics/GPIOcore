#include "Transceiver.h"


Transceiver::Transceiver()
{
}

void Transceiver::sendMessage( Message *mess2send )
{
    uint8 len = mess2send->command & 0x0F ;
    if( len == 0x0F ) len = mess2send->payload[0] ;

    Serial.write( mess2send->command ) ;

    for( int i = 0 ; i < len ; i ++ )
    {
        Serial.write( mess2send->payload[i] ) ;
    }
    uint8 checksum = mess2send->command ;
    for( int i = 0 ; i < len ; i++ )
        checksum ^= mess2send->payload[i] ;
        
    Serial.write( checksum ) ;  
}

void Transceiver::sendNack( uint8 reason )
{
    Serial.write( 0x71 ) ;
    Serial.write( reason ) ;
}

void Transceiver::sendAck()
{
    Serial.write( 0x70 ) ;
}

uint8 Transceiver::receive()
{
    if( state != 0 && millis() - lastByteTime > MAX_TIMEOUT )
    {
        sendNack( TIMEOUT ) ;
        state = 0 ;
        return 0 ;
    }

    if( Serial.available() == 0 ) return 0 ;

    uint8 b = Serial.read() ;
    lastByteTime = millis() ;

    switch( state )
    {
    case 0:
        mess.command = b ;
        len = b & 0x0F ;
        byteCounter = 0 ;
        checksum = b ;

        if( len == 0x00 )
        {
            if( messageReceived ) messageReceived( &mess, 0 ) ;
            return 1 ;
        }
        else if( len == 0x0F )
        {
            state = 1 ; // extended length volgt
        }
        else
        {
            if( len > maxLen )
            {
                sendNack( INVALID_LENGTH ) ;
                state = 0 ;
                return 0 ;
            }
            state = 2 ;
        }
        break ;

    case 1: // extended length
        if( b > maxLen )
        {
            sendNack( INVALID_LENGTH ) ;
            state = 0 ;
            return 0 ;
        }
        len = b ;
        mess.payload[ byteCounter++ ] = b ;
        checksum ^= b ;
        state = 2 ;
        break ;

    case 2:
        if( byteCounter >= maxLen )
        {
            sendNack( INVALID_LENGTH ) ;
            state = 0 ;
            return 0 ;
        }

        mess.payload[ byteCounter++ ] = b ;
        checksum ^= b ;

        if( byteCounter == len )
        {
            state = 3 ;
        }
        break ;

    case 3:
        if( b == checksum )
        {
            if( messageReceived )
                messageReceived( &mess, len ) ;

            sendAck() ;
        }
        else
        {
            sendNack( INVALID_CHECKSUM ) ;
        }

        state = 0 ;
        return 1 ;
    }

    return 0 ;
}
