/*
 * Copyright (C) 2024 Sebastiaan Knippels, Train-Science
 *
 * To the extent possible under law, the person who associated CC0 with this work
 * has waived all copyright and related or neighboring rights to this work.
 *
 * This work is published from: The Netherlands
 *
 * You can copy, modify, distribute and perform the work, even for commercial purposes,
 * all without asking permission. See the full license for details at:
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

#include <Wire.h>
#include "event.h"

static uint8_t extActive = 0 ;          // 1 after begin(): I2C runs on A4/A5

// write n bytes (n <= 30, must not cross a 64-byte page; events are 8 B and 8-aligned)
static void extWrite( uint16_t address, const uint8_t *data, uint8_t n )
{
    if( !extActive ) return ;
    Wire.beginTransmission( EXT_EEPROM_ADDR ) ;
    Wire.write( (uint8_t)( address >> 8 ) ) ;
    Wire.write( (uint8_t)( address & 0xFF ) ) ;
    Wire.write( data, n ) ;
    Wire.endTransmission() ;
    for( uint8_t i = 0 ; i < 10 ; i ++ )       // ACK polling: the write cycle takes up to 5 ms
    {
        Wire.beginTransmission( EXT_EEPROM_ADDR ) ;
        if( Wire.endTransmission() == 0 ) break ;
        delay( 1 ) ;
    }
}

static uint8_t extRead( uint16_t address, uint8_t *data, uint8_t n )
{
    if( !extActive ) return 0 ;
    Wire.beginTransmission( EXT_EEPROM_ADDR ) ;
    Wire.write( (uint8_t)( address >> 8 ) ) ;
    Wire.write( (uint8_t)( address & 0xFF ) ) ;
    if( Wire.endTransmission() != 0 ) return 0 ;     // no EEPROM on the bus
    Wire.requestFrom( EXT_EEPROM_ADDR, n ) ;
    for( uint8_t i = 0 ; i < n ; i ++ ) data[i] = Wire.available() ? Wire.read() : 0xFF ;
    return 1 ;
}

EventHandler::EventHandler( uint32_t _beginAddress, uint32_t _maxAddress )
{
    recordingDevice = evStandby ;
    beginAddress    = _beginAddress ;
    maxAddress      = _maxAddress ;
    eepromType      = INTERNAL_EEPROM ;
}

// I2C EEPROM constructor — uncomment when hardware is available
// EventHandler::EventHandler( uint32_t _beginAddress, uint32_t _maxAddress, uint8_t _i2cAddress )
// {
//     recordingDevice = evStandby ;
//     beginAddress    = _beginAddress ;
//     maxAddress      = _maxAddress ;
//     eepromType      = I2C_EEPROM ;
//     i2cAddress      = _i2cAddress ;
// }

void EventHandler::begin()
{
    if( extActive ) return ;
    Wire.begin() ;
#ifdef WIRE_HAS_TIMEOUT
    Wire.setWireTimeout( 3000, true ) ;     // a missing / broken EEPROM must not hang the decoder
#endif
    extActive = 1 ;
}

void EventHandler::end()
{
    if( !extActive ) return ;
    recordingDevice = evStandby ;
    Wire.end() ;
    extActive = 0 ;
}

uint8_t EventHandler::isActive()
{
    return extActive ;
}

void EventHandler::startRecording()
{
    if( recordingDevice == evStandby )
    {
        eeAddress       = beginAddress ;
        recordingDevice = evRecording ;
        prevTime        = millis() ;
        storeEvent( START, 1, 1 ) ;
    }
}

void EventHandler::stopRecording()
{
    if( recordingDevice == evRecording )
    {
        storeEvent( STOP, 1, 1 ) ;
        recordingDevice = evStandby ;
    }
}

Event EventHandler::getEvent()
{
    Event localEvent ;
    if( eeAddress + sizeof( localEvent ) > maxAddress )     // past the end of the recorder space: stop
    {
        localEvent.data1          = STOP ;
        localEvent.time2nextEvent = 1 ;
        return localEvent ;
    }
    if( !extRead( eeAddress, (uint8_t *)&localEvent, sizeof( localEvent ) ) )
    {
        localEvent.data1          = STOP ;  // no EEPROM: behave like an empty program
        localEvent.time2nextEvent = 1 ;
        recordingDevice           = evFinishing ;
    }

    eeAddress += sizeof( localEvent ) ;
    return localEvent ;
}

void EventHandler::startPlaying()
{
    if( recordingDevice == evStandby )
    {
        eeAddress             = beginAddress ;
        event                 = getEvent() ;    // loads START event
        event.time2nextEvent  = 1 ;             // fire immediately
        prevTime              = millis() ;
        recordingDevice       = evPlaying ;
    }
    else if( recordingDevice == evFinishing )
    {
        recordingDevice = evPlaying ;
    }
}

void EventHandler::stopPlaying()
{
    if( recordingDevice == evFinishing )
    {
        recordingDevice = evStandby ;
    }
    else if( recordingDevice == evPlaying )
    {
        recordingDevice = evFinishing ;         // finish current loop, then stop
    }
}

void EventHandler::resetProgram()
{
    recordingDevice = evStandby ;
}

void EventHandler::storeEvent( uint8_t _data1, uint16_t _data2, uint8_t _data3 )
{
    if( recordingDevice != evRecording ) return ;
    if( eeAddress + ( _data1 == STOP ? 1 : 2 ) * sizeof( Event ) > maxAddress ) // always keep room for STOP
    {
        if( memoryFull ) memoryFull() ;
        return ;
    }

    Event    localEvent ;
    uint32_t currTime = millis() ;

    localEvent.data1 = _data1 ;
    localEvent.data2 = _data2 ;
    localEvent.data3 = _data3 ;

    if(      _data1 == FEEDBACK_EVENT ) { localEvent.time2nextEvent = 0 ; }         // wait for sensor
    else if( _data1 == START          ) { localEvent.time2nextEvent = 1 ; }         // immediate
    else                                { localEvent.time2nextEvent = currTime - prevTime ; }

    prevTime = currTime ;

    extWrite( eeAddress, (const uint8_t *)&localEvent, sizeof( localEvent ) ) ;

    if( displayFreeMemory )
    {
        uint16_t freeSlots = ( maxAddress - eeAddress ) / sizeof( localEvent ) ;
        displayFreeMemory( freeSlots ) ;
    }

    if( eeAddress >= maxAddress && memoryFull ) memoryFull() ;

    eeAddress += sizeof( localEvent ) ;
}

void EventHandler::sendFeedbackEvent( uint16_t number, uint8_t state )
{
    if( recordingDevice == evPlaying || recordingDevice == evFinishing )
    {
        newSensor = number ;
        newState  = state ;
    }
}

void EventHandler::update()
{
    uint32_t currTime = millis() ;

    if( ( recordingDevice == evPlaying || recordingDevice == evFinishing )
    &&  ( currTime - prevTime ) >= event.time2nextEvent )
    {
        if( event.time2nextEvent == 0 )         // FEEDBACK event — wait for sensor
        {
            if( newSensor == event.data2 && newState == event.data3 )
                event.time2nextEvent = 1 ;
            return ;
        }

        if( notifyEvent ) notifyEvent( event.data1, event.data2, event.data3 ) ;

        prevTime = currTime ;

        if( event.data1 == STOP )
        {
            if( recordingDevice == evFinishing )
            {
                recordingDevice = evStandby ;
            }
            else
            {
                recordingDevice = evStandby ;
                startPlaying() ;                // loop
            }
            return ;
        }

        event = getEvent() ;
        newSensor = 0 ;
    }
}

// the recorder space moved (routes grew or shrank, recorder switched on/off): the old program is lost
void EventHandler::setRange( uint32_t _beginAddress, uint32_t _endAddress )
{
    uint8_t moved = beginAddress != 0 && beginAddress != _beginAddress ;

    beginAddress = _beginAddress ;
    maxAddress   = _endAddress ;

    if( moved ) clearProgram() ;
}

void EventHandler::clearProgram()
{
    recordingDevice = evStandby ;
    if( beginAddress + sizeof( Event ) <= maxAddress )
    {
        Event stop = { STOP, 1, 1, 1 } ;        // empty program
        extWrite( beginAddress, (const uint8_t *)&stop, sizeof( stop ) ) ;
    }
}

uint8_t EventHandler::getState()
{
    return recordingDevice ;
}
