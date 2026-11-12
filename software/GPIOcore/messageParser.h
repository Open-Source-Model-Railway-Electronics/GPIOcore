#pragma once
#include <Arduino.h>

extern void sendMyVersion() ;

// custom messsage protocol, project specific, not so much linked to this librayy
const int OPCODE_SET_INPUT_DATA    = 0x1E ;   // command=1, length=14
// <OPCODE_SET_INPUT_DATA> <index> <addr H> <addr L> <pin> <type> <output> <linked 0> <linked 1> <linked 2> <linked 3> <linked 4> <linked 5> <linked 6> <linked 7>

const int OPCODE_GET_INPUT_DATA    = 0x11 ;   // command=1, length=1
const int OPCODE_SET_OUTPUT_DATA   = 0x2E ;   // command=2, length=14  <index> <pinA> <pinB> <pinC> <pinD> <relayPinA> <relayPinB> <relayPinC> <type> <state> <data0> <data1> <data2> <data3>
const int OPCODE_GET_OUTPUT_DATA   = 0x21 ;   // command=2, length=1

const int OPCODE_SET_INPUT_STATE   = 0x32 ;   // command=3, length=2 (index + state)
const int OPCODE_GET_INPUT_STATE   = 0x31 ;   // command=3, length=1 (index)
const int OPCODE_SET_OUTPUT_STATE  = 0x42 ;   // command=4, length=2 (index + state)
const int OPCODE_GET_OUTPUT_STATE  = 0x41 ;   // command=4, length=1 (index)

const int OPCODE_REQ_VERSION       = 0x50 ;
const int OPCODE_SEND_VERSION      = 0x50 ;   // atleasyt 1 byte, but is is variable in length

const int OPCODE_SET_PRESET        = 0x61 ;
const int OPCODE_CLEAR_OUTPUT      = 0xb1 ;   // clear unsued outputs in FUI

// protocol v2, see PROTOCOL.md
const int OPCODE_HELLO             = 0x90 ;   // no payload -> reply 0x95
const int OPCODE_HELLO_REPLY       = 0x95 ;   // <protocol> <max steps> <pool lo> <pool hi> <flags>
const int OPCODE_GET_ROUTE         = 0x82 ;   // <route> <first> -> reply SET_ROUTE
const int OPCODE_SET_ROUTE         = 0x8F ;   // <len> <route> <total> <first> <step ...>
const int OPCODE_SERVO_LIVE        = 0xA2 ;   // <output> <angle>
const int OPCODE_DCC_EXT          = 0x33 ;   // <addr H> <addr L> <value>: as if a DCC extended accessory packet arrived
const int OPCODE_OUTPUT_EVENT      = 0x45 ;   // device -> host: <output> <state> <value> <ms lo> <ms hi>
const int OPCODE_GET_BOARD         = 0xC0 ;   // no payload -> reply SET_BOARD
const int OPCODE_SET_BOARD         = 0xCF ;   // <len> <type> <flags> <delay> <name 0..16>

const uint8_t NACK_FULL            = 4 ;
const uint8_t NACK_INVALID_PIN     = 5 ;

const uint8_t PROTOCOL_VERSION     = 2 ;
const uint8_t MAX_ROUTE_STEPS      = 16 ;
const uint8_t HELLO_IO_ON_SERIAL   = 0x01 ;   // D0/D1 usable as I/O outside a config session
const uint8_t HELLO_SERVO_LIVE     = 0x02 ;
const uint8_t HELLO_EXT_EEPROM     = 0x04 ;
const uint8_t HELLO_BOARD          = 0x08 ;

extern uint16_t freeRouteSteps() ;
