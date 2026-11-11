#pragma once
#include "src/ST.h"
#include "config.h"
#include <EEPROM.h>

typedef struct eeInputSettings
{
    uint16_t myAddress : 12 ;   // DCC adres of ID
    uint8_t  pin       : 6  ;   // GPIO pin (of NA)
    uint8_t  type      : 3  ;   // input type (3 bits → 0-7)
    uint8_t  outputID  : 4  ;   // gekoppeld output object (0-15)
} InputSettings ;               // 4 bytes, the route is stored in the route pool (routes.h)

class Input
{
public:
    Input() ;
    void    update() ;

    void    set( InputSettings *data ) ;
    InputSettings *get() ;

    void    begin() ;
    void    defaults() ;
    void    assignEEPROM() ;
    void    saveEEPROM() ;
    void    loadEEPROM() ;
    void    setDccAddress(    uint16_t address, uint8_t direction ) ;
    void    setDccAddressExt( uint16_t address, uint8_t       val ) ;

    InputSettings   eedata ;

private:
    void    triggerOutputs() ;

    Trigger         trigger ;
    uint16_t        eeAddress ;
    uint8_t         settingOutputs : 1 ;
    uint8_t         index          : 6 ;
    uint8_t         state          : 1 ;
    Timer           outputTimer ;
} ;

void callbackInput(    uint8_t outputID, uint8_t state, uint8_t value ) __attribute__((weak)) ;
void callbackRecorder( uint8_t action                                 ) __attribute__((weak)) ;
void callbackFeedback( uint8_t id,       uint8_t state                ) __attribute__((weak)) ;
