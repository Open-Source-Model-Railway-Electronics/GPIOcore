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
