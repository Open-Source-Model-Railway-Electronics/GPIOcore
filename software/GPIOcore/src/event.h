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

#pragma once
#include <Arduino.h>

// I2C EEPROM support — uncomment when hardware is available
// #include "i2cEeprom.h"

enum eventModes
{
    evStandby,      // renamed from 'idle' to avoid conflict with output state machine
    evPlaying,
    evRecording,
    evFinishing,
} ;

enum defaultEvents
{
    FEEDBACK_EVENT = 250,
    START,
    STOP,
} ;

enum eepromTypes
{
    INTERNAL_EEPROM,
    // I2C_EEPROM,  // uncomment when I2C EEPROM hardware is available
} ;

typedef struct                  // 8 bytes per event
{
    uint8_t  data1 ;
    uint16_t data2 ;
    uint8_t  data3 ;
    uint32_t time2nextEvent ;
} Event ;


class EventHandler
{
public:
    EventHandler( uint32_t beginAddress, uint32_t maxAddress ) ;

    // I2C EEPROM constructor — uncomment when hardware is available
    // EventHandler( uint32_t beginAddress, uint32_t maxAddress, uint8_t i2cAddress ) ;

    void    begin() ;
    void    startRecording() ;
    void    stopRecording() ;
    void    startPlaying() ;
    void    stopPlaying() ;
    void    resetProgram() ;
    void    sendFeedbackEvent( uint16_t number, uint8_t state ) ;
    void    update() ;
    void    storeEvent( uint8_t data1, uint16_t data2, uint8_t data3 ) ;
    uint8_t getState() ;
    void    clearProgram() ;
    void    setRange( uint32_t beginAddress, uint32_t endAddress ) ;  // end is exclusive

private:
    Event       event ;
    Event       getEvent() ;

    // I2cEeprom   i2cEeprom ;  // uncomment when I2C EEPROM hardware is available

    uint32_t    beginAddress ;
    uint32_t    eeAddress ;
    uint32_t    maxAddress ;
    uint32_t    prevTime ;
    uint16_t    newSensor ;
    uint8_t     newState ;
    uint8_t     recordingDevice ;
    uint8_t     eepromType ;
    // uint8_t  i2cAddress ;    // uncomment when I2C EEPROM hardware is available
} ;


// Callbacks
extern void notifyEvent(        uint8_t, uint16_t, uint8_t ) __attribute__(( weak )) ;
extern void displayFreeMemory(  uint16_t                   ) __attribute__(( weak )) ;
extern void memoryFull()                                     __attribute__(( weak )) ;
