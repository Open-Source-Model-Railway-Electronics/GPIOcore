#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"

// ── EEPROM layout v2, see EEPROM.md section 2 ─────────────────────────────────
const uint16_t EE_MAGIC     =    0 ;    // 'G' '2' <layout>
const uint8_t  EE_LAYOUT    =    3 ;    // 3: a step has an extra byte (2: light effect type)
const uint16_t EE_BOARD     =    3 ;    // type, flags, delay, name[16]
const uint16_t EE_OUTPUTS   =   22 ;    // 16 x 10 B
const uint16_t EE_INPUTS    =  182 ;    // 16 x  4 B
const uint16_t EE_COUNTS    =  246 ;    // steps per route, 16 x 1 B
const uint16_t EE_POOL      =  262 ;    // route steps
const uint16_t EE_END       = 1024 ;

const uint8_t  MAX_STEPS_PER_ROUTE = 16 ;

// step on the wire and in RAM (PROTOCOL.md section 4): <id | raw> <value> <wait lo> <wait hi> <extra>
// In EEPROM: <id | raw> <value> <extra> (3 B), with per-step wait times <id | raw> <value> <wait lo> <wait hi> <extra> (5 B).
// extra: the target address of a board link step (0 = the output's own address), otherwise 0.
const uint8_t  STEP_BYTES   = 5 ;
const uint8_t  STEP_ID      = 0x0F ;
const uint8_t  STEP_RAW     = 0x10 ;
const uint8_t  STEP_VALUES  = 0x20 ;   // custom values of a toggle / absolute input: 2 steps (off / curved, on / straight)

extern uint8_t routeCount[16] ;

void     routesBegin() ;
uint8_t  stepSize() ;                                       // in EEPROM: 3 = timing off, 5 = timing on
extern uint8_t routeExtra ;                                 // the extra byte of the step being run (IOcore.ino)
uint16_t routeAddress( uint8_t route, uint8_t step ) ;
uint16_t usedRouteSteps() ;
uint16_t freeRouteSteps() ;
uint16_t maxRouteBytes() ;                                 // the whole pool
uint8_t  resizeRoute( uint8_t route, uint8_t total ) ;      // 0 = does not fit
void     readStep(  uint8_t route, uint8_t step,       uint8_t *b ) ;  // STEP_BYTES, wire format
void     writeStep( uint8_t route, uint8_t step, const uint8_t *b ) ;
uint8_t  setTiming( uint8_t on ) ;                          // converts the pool, 0 = does not fit
