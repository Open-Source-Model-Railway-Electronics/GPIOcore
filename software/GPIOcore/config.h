#pragma once

// ── Sentinel value for "not assigned" pins / IDs ──────────────────────────────
const int  NA               = 0x3F ;

// ── Input types ───────────────────────────────────────────────────────────────
// Input pins use INPUT_PULLUP: button / contact to GND, pressed = LOW = falling edge.
const int  MOMENTARY        = 0 ;   // toggle the output on the falling edge (or on every DCC command)
const int  LATCHING         = 1 ;   // output follows the input level (or the DCC direction)
const int  ROUTE            = 2 ;   // set the route on the falling edge (press) or on any DCC command (0 and 1)
const int  NOT_IN_USE       = 3 ;
const int  RECORDER         = 4 ;   // long press = record toggle, short press = play toggle
const int  FEEDBACK_INPUT   = 5 ;   // feeds back into EventHandler (pause / step)

// ── Recorder callback actions ─────────────────────────────────────────────────
const int  RECORD_TOGGLE    = 0 ;
const int  PLAY_TOGGLE      = 1 ;

// ── Timing ────────────────────────────────────────────────────────────────────
const uint16_t LONG_PRESS_MS = 1500 ;   // threshold between short and long press

// ── Sizes ─────────────────────────────────────────────────────────────────────
const int  MAX_OUTPUTS      = 8 ;
const int  OUTPUT_DATA_BYTES = 4 ;

// ── Board settings (PROTOCOL.md section 10) ───────────────────────────────────
enum boardTypes { BOARD_UNO, BOARD_NANO, BOARD_DCCNEXT, BOARD_JACK } ;

const uint8_t  BOARD_RECORDER     = 0x01 ;  // flags bit0: recorder enabled
const uint8_t  BOARD_TIMING       = 0x02 ;  // flags bit1: per-step wait times stored
const uint8_t  BOARD_INTERLOCK    = 0x0C ;  // flags bit2-3: 0 = off, 1 = on, 2 = auto
const uint8_t  INTERLOCK_OFF      = 0 ;
const uint8_t  INTERLOCK_ON       = 1 ;
const uint8_t  INTERLOCK_AUTO     = 2 ;
const uint8_t  BOARD_NAME_LEN     = 16 ;

typedef struct                      // name[16] is not kept in RAM, it is read from EEPROM when needed
{
    uint8_t type ;
    uint8_t flags ;
    uint8_t delay ;                 // fixed wait after a route step when timing is off, x 0.1 s
} BoardSettings ;

extern BoardSettings board ;
extern uint16_t      boardEEAddress ;
extern uint8_t       configSession ;    // set by HELLO

bool    isValidPin( uint8_t pin, uint8_t isOutput ) ;   // pin rules, PROTOCOL.md section 9
