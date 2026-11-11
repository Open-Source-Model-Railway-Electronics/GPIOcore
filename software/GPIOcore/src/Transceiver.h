#include <Arduino.h>
#include "macros.h"


static const uint8 INVALID_CHECKSUM =  1 ;
static const uint8 INVALID_LENGTH   =  2 ;
static const uint8 TIMEOUT          =  3 ;


static const uint8 MAX_TIMEOUT      =  5 ;
static const int   maxLen           = 32 ;

typedef struct mess
{
    uint8 command ;
    uint8 payload[maxLen] ;
} Message ;


class Transceiver
{
public:
    Transceiver(/* args */);
    uint8_t receive() ;
    //void    processMessage() ;
    void    sendMessage(Message *) ;
    void    sendAck() ;
    void    sendNack( uint8 reason ) ;
    uint8_t nack ;          // set by messageReceived() to answer with NACK <nack> instead of ACK
    
    private:
    Message     mess ;
    Message     toSend ;
    uint8_t     state ;
    uint8_t     len ;
    uint8_t     receiving : 1 ;
    uint8_t     transmitting : 1 ;
    uint8_t     byteCounter ;
    uint8_t     checksum ;
    uint8_t     transmittAttempts = 0 ;
    uint32_t    lastByteTime ;
} ;

extern void messageReceived( Message *, uint8 ) __attribute__ ((weak)) ; 



