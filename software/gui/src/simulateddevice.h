#pragma once
// Simulated board: a QIODevice on which the real firmware runs.
//
// open() starts gpiocore_fw_host (fwhost/, built from software/GPIOcore with an Arduino shim) and
// the DeviceLink talks to it like to a serial port. Everything the board does - the protocol, its
// replies and quirks, routes, interlocking, servo sweeps, frog relays, analog inputs, the DCC
// lockout, timing - is the firmware's own code. This class only carries the bytes, keeps a copy of
// the board's state for tests and the GUI, and plays the outside world (voltages on pins, DCC
// packets). One process per open(): every simulation starts with a fresh board.
//
// The state copy (output(), input(), board(), ...) is updated before the serial bytes of the same
// firmware loop() round arrive, so it is current when the reply / ACK to a request is seen.

#include "gpioconfig.h"

#include <QIODevice>
#include <array>
#include <optional>

class QProcess;

class SimulatedDevice : public QIODevice
{
    Q_OBJECT
public:
    explicit SimulatedDevice(QObject *parent = nullptr);
    ~SimulatedDevice() override;

    // gpiocore_fw_host next to the application (or in the build tree); empty if not found
    static QString hostProgram();
    // what a factory-fresh board contains (Output::defaults() / Input::defaults(); a test checks
    // it against the firmware), so the GUI is useful offline
    static Gpio::BoardConfig factoryDefaults();

    bool   isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return m_rx.size() + QIODevice::bytesAvailable(); }
    bool   open(OpenMode mode) override;   // starts the firmware, returns when setup() is done
    void   close() override;

    // ── The board's state, as the firmware has it ───────────────────────────
    Gpio::OutputConfig  output(int i) const { return m_out[i]; }
    Gpio::InputConfig   input(int i)  const { return m_in[i]; }
    Gpio::BoardSettings board() const       { return m_board; }
    int    servoPosition(int output) const  { return m_pos[output]; }   // Output::position()
    quint8 frogRelays(int output) const     { return m_frog[output]; }  // bit0 A, bit1 B, bit2 C
    bool   configSession() const { return m_globals[0]; }   // HELLO received
    bool   serialIsIO() const    { return m_globals[1]; }   // no HELLO within the boot window
    bool   configMode() const    { return m_globals[2]; }   // config button held 2 s
    struct Pin { quint8 mode = 0, level = 0, pwm = 0, servo = 0xFF; };   // servo 0xFF = none
    Pin    pin(int p) const { return m_pins[p]; }            // D0-D13, A0-A7 (14-21)

    // ── Setting the board up (through the firmware's own message handlers) ──
    // Before open(): applied right after the firmware booted (board settings first).
    void setOutput(int i, const Gpio::OutputConfig &c);
    void setInput(int i, const Gpio::InputConfig &c);      // with its route / custom values
    void setBoard(const Gpio::BoardSettings &b);

    // ── The outside world ───────────────────────────────────────────────────
    // Voltage on A0-A7 as analogRead() sees it (default 1023: nothing pressed, pull-up).
    // Before open(): applied right after the firmware booted, before the set-up above.
    void setAnalogValue(int channel, int value);
    int  analogValue(int channel) const { return m_analog[channel & 7]; }
    void setPinLevel(int pin, bool high);                   // a button to GND: false = pressed
    void dccAccessory(int address, int direction);          // a DCC accessory packet (output on)
    void dccExtended(int address, int value);               // a DCC extended accessory packet
    void dccLocoSpeed(int address, int speed, bool forward);   // 128 speed steps (config mode, loco 9999)
    void dccFunctions(int address, quint8 f0f4);            // F0-F4: bit4 F0, bit0 F1 ... bit3 F4

    // Line noise: the checksum of the next n frames from the host is broken (the firmware NACKs them).
    void corruptNextFrames(int n) { m_corrupt = n; }
    // A fake HC-05 in AT mode on D0/D1: answers OK to every AT command it gets at this baud rate (0 = none)
    void attachFakeHc05(int baud);
    QStringList hc05Commands() const { return m_hc05; }      // the AT commands it received, without CR LF
    void advanceClock(int ms);                              // the firmware's clock jumps ahead (max 65535 ms)

signals:
    void stateChanged();                                    // output(), input(), board(), pin() ... changed

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 size) override;

private:
    void sendFrame(char type, const QByteArray &data);
    void sendSetup(quint8 command, const QByteArray &payload);   // 'X': one message through messageReceived()
    void sendOutput(int i, const Gpio::OutputConfig &c);
    void sendInput(int i, const Gpio::InputConfig &c);
    void sendBoard(const Gpio::BoardSettings &b);
    void readHost();
    bool handleFrame(char type, const QByteArray &data);   // true: serial bytes for the DeviceLink

    QProcess  *m_proc = nullptr;
    bool       m_booted = false;
    QByteArray m_fromHost;            // not yet parsed
    QByteArray m_rx;                  // serial bytes, readable by the DeviceLink
    int        m_corrupt = 0;
    QStringList m_hc05;

    std::array<Gpio::OutputConfig, Gpio::NumOutputs> m_out;
    std::array<Gpio::InputConfig,  Gpio::NumInputs>  m_in;
    Gpio::BoardSettings m_board;
    std::array<int,    Gpio::NumOutputs> m_pos {};
    std::array<quint8, Gpio::NumOutputs> m_frog {};
    std::array<bool, 3> m_globals {};
    std::array<Pin, Gpio::NumPins> m_pins {};
    std::array<int, 8> m_analog;

    // set before open(), sent once the firmware runs
    std::optional<Gpio::BoardSettings> m_setupBoard;
    QByteArray m_setupLevels;         // 'A' frames
    std::array<std::optional<Gpio::OutputConfig>, Gpio::NumOutputs> m_setupOut;
    std::array<std::optional<Gpio::InputConfig>,  Gpio::NumInputs>  m_setupIn;
};
