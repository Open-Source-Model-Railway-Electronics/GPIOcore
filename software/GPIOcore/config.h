#pragma once

// ── Sentinel value for "not assigned" pins / IDs ──────────────────────────────
const int  NA               = 0x3F ;

// ── Input types ───────────────────────────────────────────────────────────────
// Input pins use INPUT_PULLUP: button / contact to GND, pressed = LOW = falling edge.
const int  MOMENTARY        = 0 ;   // toggle the output on the falling edge (or on every DCC command)
const int  LATCHING         = 1 ;   // output follows the input level (or the DCC direction)
const int  ROUTE            = 2 ;   // set the route on the falling edge (press) or on any DCC command (0 and 1)
const int  NOT_IN_USE       = 3 ;
const int  SENSOR           = 4 ;   // occupancy detector: only reports its state (automatic block signals)
//              5                     was the recorder's feedback input (removed): read as not in use
const int  ROUTE_STRAIGHT   = 6 ;   // route on a button press, or on a DCC 'straight' (1) command only
const int  ROUTE_CURVED     = 7 ;   // route on a button press, or on a DCC 'curved' (0) command only

inline bool isRoute( uint8_t type ) { return type == ROUTE || type == ROUTE_STRAIGHT || type == ROUTE_CURVED ; }

// ── Analog inputs (resistor ladder, sensor) ───────────────────────────────────
// The input pin field holds 0x20 | mode << 3 | channel (A0-A7) instead of a pin number.
// The threshold is stored in 7 bits as value / 8 (0-1016). The condition counts as a pressed
// button (LOW), so toggle, absolute and route work as with a digital pin.
const uint8_t ANALOG_INPUT      = 0x20 ;
const uint8_t ANALOG_BELOW      = 0 ;    // pressed below the threshold, released at threshold + 20
const uint8_t ANALOG_ABOVE      = 1 ;    // pressed above the threshold, released at threshold - 20
const uint8_t ANALOG_AT         = 2 ;    // pressed within threshold +/- 10: one button of a resistor ladder
const uint8_t ANALOG_HYSTERESIS = 20 ;
inline bool    isAnalogInput( uint8_t pin ) { return pin != NA && ( pin & ANALOG_INPUT ) && ( ( pin >> 3 ) & 3 ) <= ANALOG_AT ; }
inline uint8_t inputPinNumber( uint8_t pin ) { return isAnalogInput( pin ) ? 14 + ( pin & 7 ) : pin ; }   // the real pin

// A command station repeats an accessory command a few times: a toggle input or a route
// takes the first one and ignores the same address for this long.
const uint16_t DCC_LOCKOUT_MS = 500 ;

// ── Board pins with a fixed job: never an input or output ──────────────────────
const uint8_t  CONFIG_LED_PIN    = 13 ;  // D13: on = normal, blinking = config mode
const uint8_t  CONFIG_BUTTON_PIN = 2 ;   // D2: the DCC input is also the config button (a button to GND)
inline bool isReservedPin( uint8_t pin ) { return pin == 2 || pin == CONFIG_LED_PIN ; }   // A7 is a normal analog input


// ── Sizes ─────────────────────────────────────────────────────────────────────
const int  OUTPUT_DATA_BYTES = 4 ;

// ── Board settings (PROTOCOL.md section 10) ───────────────────────────────────
enum boardTypes { BOARD_UNO, BOARD_NANO, BOARD_DCCNEXT, BOARD_JACK } ;

const uint8_t  BOARD_RESERVED     = 0x01 ;  // flags bit0: was 'recorder enabled', ignored
const uint8_t  BOARD_TIMING       = 0x02 ;  // flags bit1: per-step wait times stored
const uint8_t  BOARD_INTERLOCK    = 0x0C ;  // flags bit2-3: 0 = off, 1 = on, 2 = auto
const uint8_t  INTERLOCK_OFF      = 0 ;
const uint8_t  INTERLOCK_ON       = 1 ;
const uint8_t  INTERLOCK_AUTO     = 2 ;
const uint8_t  BOARD_BLINK        = 0xF0 ;  // flags bit4-7: block signal blink rate x 0.25 Hz, 0 = 1 Hz
const uint8_t  BOARD_NAME_LEN     = 16 ;

typedef struct                      // name[16] is not kept in RAM, it is read from EEPROM when needed
{
    uint8_t type ;
    uint8_t flags ;
    uint8_t delay ;                 // fixed wait after a route step when timing is off, x 0.1 s
} BoardSettings ;

extern BoardSettings board ;

// Half a blink period of the block signals in ms: 1 Hz by default, else n x 0.25 Hz
inline uint16_t blockBlinkHalfMs() { uint8_t n = board.flags >> 4 ; return n ? 2000 / n : 500 ; }
extern uint16_t      boardEEAddress ;
extern uint8_t       configSession ;    // set by HELLO
extern uint8_t       serialIsIO ;       // 1: no GUI, the UART runs the board link (9600 baud)

bool    isValidPin( uint8_t pin, uint8_t isOutput ) ;   // pin rules, PROTOCOL.md section 9
