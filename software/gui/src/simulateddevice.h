#pragma once
// Firmware simulator: a QIODevice that behaves like a GPIOcore board on the
// other end of the serial line. It mirrors src/Transceiver.cpp and
// messageParser.cpp of the firmware, including its quirks:
//   - two ACKs after every SET_*_DATA (handler + transceiver)
//   - an ACK after every GET reply
//   - legacy output opcode 0x2D (13 bytes): data[3] comes from a stale buffer
//   - bitfield truncation (6-bit pins, 4-bit outputID, 12-bit address, ...)
//   - preset broadcasts in the old 12-byte output layout
//   - a version message on boot, REQ_VERSION gets no ACK

#include "gpioconfig.h"

#include <QElapsedTimer>
#include <QIODevice>
#include <array>

class SimulatedDevice : public QIODevice
{
    Q_OBJECT
public:
    // Defaults = fixed firmware (review F1–F16 applied, 2026-11-09).
    struct Options
    {
        bool legacyFirmware = false;  // true: OPCODE_SET_OUTPUT_DATA = 0x2D, 13 bytes (old firmware)
        bool doubleAck      = false;  // true: handlers call sendAck() too (old firmware)
        bool oldDefaults    = false;  // true: pins from D0, linked = NA, old preset layout, version "12345678"
        int  protocol       = 2;      // 1 = current firmware (no HELLO / routes / live servo)
        int  maxRouteSteps  = Gpio::MaxRouteSteps;
        int  stepPool       = 256;    // external EEPROM: 16 routes x 16 steps
        quint8 helloFlags   = 0x1F;   // serial pins, servo live, external EEPROM, board settings, analog + sound
        int  bootDelayMs    = 150;
        int  replyDelayMs   = 1;

        static Options currentFirmware(); // firmware as in the repo now (protocol v1)
        static Options oldFirmware();     // firmware before the review fixes
    };

    explicit SimulatedDevice(QObject *parent = nullptr);
    SimulatedDevice(const Options &options, QObject *parent = nullptr);

    bool   isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return m_rx.size() + QIODevice::bytesAvailable(); }
    bool   open(OpenMode mode) override;
    void   close() override;

    // Device state (for tests and the demo)
    Gpio::OutputConfig output(int i) const { return m_out[i]; }
    Gpio::InputConfig  input(int i)  const { return m_in[i]; }
    void setOutput(int i, const Gpio::OutputConfig &c) { m_out[i] = c.normalized(); }
    void setInput(int i, const Gpio::InputConfig &c)   { m_in[i] = c.normalized(); m_in[i].syncLinkedFromRoute(); }
    int  liveAngle(int output) const { return m_live[output]; }   // last SERVO_LIVE, -1 = none
    bool configSession() const { return m_session; }                // HELLO received
    Gpio::BoardSettings board() const { return m_board; }
    void setBoard(const Gpio::BoardSettings &b) { m_board = b; }
    // Voltage on A0-A7 as analogRead() sees it (default 1023: nothing pressed). Every analog input
    // on that channel reacts like the firmware: a press or release of a button to GND.
    void setAnalogValue(int channel, int value);
    int  analogValue(int channel) const { return m_analog[channel & 7]; }

    // Reply with NACK(checksum) to the next n complete frames.
    void corruptNextFrames(int n) { m_corrupt = n; }
    int  framesReceived() const { return m_framesReceived; }

    void factoryDefaults();

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 size) override;

private:
    void   receiveByte(quint8 b);
    void   messageReceived();
    void   sendMessage(quint8 command, const quint8 *payload);
    void   sendAck();
    void   sendNack(quint8 reason);
    void   sendVersion();
    void   scheduleFlush();
    void   reportOutput(int output, int state, int value);   // OUTPUT_EVENT after HELLO
    void   triggerInput(int index, int direction);           // like the firmware's setDccAddress()
    void   startRoute(int index);                            // Input::startRoute(), with interlocking
    void   pinLevel(int index, bool pressed);                // Input::update(): a button edge
    void   switchOutput(int index, int state);               // Input::switchOutput(): custom value or state
    void   applyStep(const Gpio::RouteStep &s);              // a route step / custom value on its output
    void   evaluateAnalog(int index);                        // Input::readAnalog()
    void   runRouteStep(int input, int step);
    bool   routeMovesTurnout(int input) const;
    void   setOutputState(int output, quint8 state);          // state change + report, servos sweep
    void   sweepServo(int output);
    void   setFrog(int output, int bit, bool on);

    quint8 outCmd() const;
    quint8 firstPin() const { return m_opt.oldDefaults ? 0 : 3; }
    void   presetOutput(int i, quint8 pinA, quint8 pinB, quint8 rA, quint8 rB, quint8 rC,
                        quint8 type, std::array<quint8, 4> data, quint32 switchTime, bool isTime);
    void   setupInputs(int used);
    void   clearUnusedOutputs(int from);
    void   applyPreset(quint8 preset);

    Options m_opt;
    std::array<Gpio::OutputConfig, Gpio::NumOutputs> m_out;
    std::array<Gpio::InputConfig,  Gpio::NumInputs>  m_in;

    // Transceiver receive state; 'mess' is persistent like in the firmware.
    quint8 m_command = 0;
    std::array<quint8, 32> m_payload {};
    int    m_state   = 0;
    int    m_len     = 0;
    int    m_count   = 0;
    quint8 m_checksum = 0;

    QByteArray m_tx;          // produced, not yet delivered
    QByteArray m_rx;          // delivered, readable by host
    bool       m_flushScheduled = false;
    int        m_corrupt = 0;
    int        m_framesReceived = 0;
    std::array<int, Gpio::NumOutputs> m_live;
    bool       m_session = false;
    Gpio::BoardSettings m_board;
    QElapsedTimer       m_clock;
    std::array<bool, Gpio::NumInputs> m_routeBusy {};
    std::array<int,  Gpio::NumOutputs> m_servoPos;            // -1 = unknown
    std::array<bool, Gpio::NumOutputs> m_sweeping {};
    std::array<int,  Gpio::NumOutputs> m_servoTarget;         // -1 = the state's end position
    void   servoTo(int output, int angle);
    std::array<quint8, Gpio::NumOutputs> m_frog {};
    std::array<int,  8> m_analog;                              // analogRead() per channel
    std::array<bool, Gpio::NumInputs> m_pressed {};            // analog inputs: condition met
    std::array<quint8, Gpio::NumInputs> m_inState {};          // Input::state (toggle / absolute)
};
