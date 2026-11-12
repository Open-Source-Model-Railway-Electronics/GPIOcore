#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"

// ── EEPROM layout v2, see EEPROM.md section 2 ─────────────────────────────────
const uint16_t EE_MAGIC     =    0 ;    // 'G' '2' <layout>
const uint8_t  EE_LAYOUT    =    1 ;
const uint16_t EE_BOARD     =    3 ;    // type, flags, delay, name[16]
const uint16_t EE_OUTPUTS   =   22 ;    // 16 x 10 B
const uint16_t EE_INPUTS    =  182 ;    // 16 x  4 B
const uint16_t EE_COUNTS    =  246 ;    // steps per route, 16 x 1 B
const uint16_t EE_POOL      =  262 ;    // route steps (the recorder is in the I2C EEPROM)
const uint16_t EE_END       = 1024 ;

const uint8_t  MAX_STEPS_PER_ROUTE = 16 ;

// step on the wire (PROTOCOL.md section 4): <id | raw> <value> <wait lo> <wait hi>
const uint8_t  STEP_ID      = 0x0F ;
const uint8_t  STEP_RAW     = 0x10 ;

extern uint8_t routeCount[16] ;

void     routesBegin() ;
uint8_t  stepSize() ;                                       // 2 = timing off, 4 = timing on
uint16_t routeAddress( uint8_t route, uint8_t step ) ;
uint16_t usedRouteSteps() ;
uint16_t freeRouteSteps() ;
uint16_t maxRouteBytes() ;                                 // the whole pool
uint8_t  resizeRoute( uint8_t route, uint8_t total ) ;      // 0 = does not fit
void     readStep(  uint8_t route, uint8_t step,       uint8_t *b ) ;  // 4 bytes, wire format
void     writeStep( uint8_t route, uint8_t step, const uint8_t *b ) ;
uint8_t  setTiming( uint8_t on ) ;                          // converts the pool, 0 = does not fit
void     updateRecorderRange() ;                            // starts / stops I2C with the recorder flag
uint8_t  pinInUse( uint8_t pin ) ;
