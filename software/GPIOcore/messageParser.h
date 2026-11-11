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
