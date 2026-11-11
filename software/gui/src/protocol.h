#pragma once
// GPIOcore serial protocol (host side).
//
// Frame:  <command> <payload ...> <checksum>
//   command  : high nibble = message group, low nibble = payload length
//              (0x0F = extended: payload[0] holds the length, incl. itself)
//   checksum : XOR of command and all payload bytes
//
// Exceptions, mirrored from src/Transceiver.cpp in the firmware:
//   ACK  = 0x70           single byte, no checksum
//   NACK = 0x71 <reason>  two bytes,   no checksum
//   A host->device frame with length 0 (e.g. REQ_VERSION) is dispatched by
//   the firmware immediately, so it must be sent WITHOUT a checksum byte.

#include <QByteArray>
#include <QString>
#include <QVector>

namespace Proto {

constexpr quint8 SET_INPUT_DATA         = 0x1E;  // 14 bytes
constexpr quint8 GET_INPUT_DATA         = 0x11;  //  1 byte  (index)
constexpr quint8 SET_OUTPUT_DATA        = 0x2E;  // 14 bytes (fixed firmware)
constexpr quint8 SET_OUTPUT_DATA_LEGACY = 0x2D;  // 13 bytes (current firmware, data[3] is lost)
constexpr quint8 GET_OUTPUT_DATA        = 0x21;  //  1 byte  (index)
constexpr quint8 SET_INPUT_STATE        = 0x32;
constexpr quint8 GET_INPUT_STATE        = 0x31;
constexpr quint8 SET_OUTPUT_STATE       = 0x42;
constexpr quint8 GET_OUTPUT_STATE       = 0x41;
constexpr quint8 REQ_VERSION            = 0x50;  //  0 bytes
constexpr quint8 VERSION_GROUP          = 0x5;   // reply: 0x5n, n = string length
constexpr quint8 SET_PRESET             = 0x61;  //  1 byte  (preset number)
constexpr quint8 ACK                    = 0x70;
constexpr quint8 NACK                   = 0x71;

// ── Protocol v2 (see PROTOCOL.md) ──────────────────────────────────────────
// HELLO: host -> device, 0 bytes, sent right after the serial port is opened.
// Starts a configuration session (the decoder keeps D0/D1 as serial).
// Reply 0x95: <protocol> <max steps per route> <step pool lo> <step pool hi> <flags>
constexpr quint8 HELLO                  = 0x90;
constexpr quint8 HELLO_GROUP            = 0x9;
constexpr quint8 HELLO_FLAG_SERIAL_PINS = 0x01;  // D0/D1 usable as I/O outside a config session
constexpr quint8 HELLO_FLAG_SERVO_LIVE  = 0x02;  // SERVO_LIVE supported
constexpr quint8 HELLO_FLAG_EXT_EEPROM  = 0x04;  // routes / recorder in external EEPROM
constexpr quint8 HELLO_FLAG_BOARD       = 0x08;  // GET_BOARD / SET_BOARD supported

// Board settings and name (PROTOCOL.md section 10)
// GET_BOARD (0 bytes) -> reply SET_BOARD <len> <type> <flags> <delay> <name 0..16>
constexpr quint8 GET_BOARD              = 0xC0;
constexpr quint8 SET_BOARD              = 0xCF;

// Routes: up to 16 steps of 4 bytes, transferred in chunks of up to 7 steps.
// GET_ROUTE <route> <first step>            -> reply SET_ROUTE
// SET_ROUTE <len> <route> <total> <first> <n x 4 step bytes>   (extended length)
constexpr quint8 GET_ROUTE              = 0x82;
constexpr quint8 SET_ROUTE              = 0x8F;
constexpr int    RouteChunkSteps        = 7;     // 4 header + 7 * 4 = 32 = MaxLen

// SERVO_LIVE <output> <angle>: move a servo now, nothing is stored.
constexpr quint8 SERVO_LIVE             = 0xA2;

constexpr int    MaxLen                 = 32;

constexpr quint8 NACK_INVALID_CHECKSUM  = 1;
constexpr quint8 NACK_INVALID_LENGTH    = 2;
constexpr quint8 NACK_TIMEOUT           = 3;
constexpr quint8 NACK_FULL              = 4;     // v2: route / step pool full
constexpr quint8 NACK_INVALID_PIN       = 5;     // v2: pin 2 (DCC) or a pin that does not exist

struct Frame
{
    quint8     command = 0;
    QByteArray payload;

    bool   isAck()   const { return command == ACK;  }
    bool   isNack()  const { return command == NACK; }
    quint8 group()   const { return command >> 4;    }
    quint8 byte(int i) const { return i < payload.size() ? quint8(payload.at(i)) : 0; }
};

// Payload length encoded in a command byte (0x0F means extended).
inline int nibbleLength(quint8 command) { return command & 0x0F; }

// Build the bytes for one host->device frame. The payload is padded or
// truncated to the length the command byte announces.
QByteArray encode(quint8 command, const QByteArray &payload = QByteArray());

QString toHex(const QByteArray &bytes);
QString nackReason(quint8 reason);
QString commandName(quint8 command);

// Incremental parser for device->host bytes.
class FrameParser
{
public:
    struct Result
    {
        QVector<Frame>   frames;
        QVector<QString> errors;
    };

    void feed(const QByteArray &bytes, Result &out);
    void reset();
    bool idle() const { return m_state == Idle; }

private:
    enum State { Idle, NackReason, ExtLength, Payload, Checksum };

    State      m_state    = Idle;
    quint8     m_command  = 0;
    int        m_length   = 0;
    quint8     m_checksum = 0;
    QByteArray m_payload;
};

} // namespace Proto
