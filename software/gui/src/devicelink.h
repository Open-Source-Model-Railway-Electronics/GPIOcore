#pragma once
// Request/response link to a GPIOcore board over any QIODevice
// (QSerialPort for real hardware, SimulatedDevice for demo and tests).
//
// One transaction is in flight at a time. After each transaction a short
// drain period swallows stray ACKs (e.g. the ACK after a GET reply).

#include "gpioconfig.h"
#include "protocol.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QTimer>

class QIODevice;
class QSerialPort;

// What the connected decoder can do, from its HELLO reply.
// A board that does not answer HELLO is no GPIOcore board (or has very old firmware).
struct DeviceCaps
{
    bool known         = false;
    int  protocol      = 2;
    int  maxRouteSteps = Gpio::MaxRouteSteps;
    int  stepPool      = Gpio::NumInputs * Gpio::MaxRouteSteps;
    bool serialPins    = false;   // D0/D1 usable as I/O outside a config session
    bool servoLive     = false;
    bool boardSettings = false;   // GET_BOARD / SET_BOARD (name, type, ...)
    bool analogSound   = false;   // analog inputs (GET_ANALOG) and the sound effect output
};

class DeviceLink : public QObject
{
    Q_OBJECT
public:
    explicit DeviceLink(QObject *parent = nullptr);
    ~DeviceLink() override;

    bool openSerial(const QString &portName, QString *error);
    // Use an already constructed device (takes ownership). bootsOnOpen:
    // the device sends its version by itself shortly after opening.
    bool openDevice(QIODevice *device, bool bootsOnOpen, QString *error);
    void close();

    bool    isOpen()  const { return m_device != nullptr; }
    bool    isReady() const { return m_ready; }
    bool    isBusy()  const;           // live readings in the background do not count
    QString version() const { return m_version; }
    QString portName() const { return m_portName; }

    void requestVersion();
    void readAll();
    void readInput(int index);
    void readOutput(int index);
    void writeInput(int index, const Gpio::InputConfig &c);
    void writeOutput(int index, const Gpio::OutputConfig &c);
    void readRoute(int route);
    void writeRoute(int route, const QVector<Gpio::RouteStep> &steps);
    void servoLive(int output, int angle);   // coalesced: only the latest angle is sent
    void readBoard();                        // boards with board settings only
    void triggerInput(int index, int direction);   // as if a DCC command for this input arrived
    void readOutputState(int output);              // answered with an outputEvent
    void dccExt(int address, int value);           // as if a DCC extended accessory packet arrived
    void writeBoard(const Gpio::BoardSettings &b);
    void readAnalog(int channel);                  // analog boards: answered with analogValue; not in progress / finished

    const DeviceCaps &caps() const { return m_caps; }

    // Timings (ms), shortened in unit tests.
    int bootWaitMs   = 2500;
    int timeoutMs    = 400;
    int eepromShiftMs = 4000;   // SET_ROUTE / SET_BOARD may move the whole route pool
    int drainMs      = 15;
    int retries      = 2;

signals:
    void opened();
    void closed();
    void ready(const QString &version);
    void inputReceived(int index, const Gpio::InputConfig &c);
    void outputReceived(int index, const Gpio::OutputConfig &c);
    void inputWritten(int index);
    void outputWritten(int index);
    void capabilitiesChanged(const DeviceCaps &caps);
    void routeReceived(int route, const QVector<Gpio::RouteStep> &steps);
    void boardReceived(const Gpio::BoardSettings &board);
    // An output changed on the board. state 0/1, or 2 = raw value (aspect / angle / PWM) in 'value'.
    void outputEvent(int output, int state, int value);
    void analogValue(int channel, int value);   // ANALOG_VALUE: analogRead() of A0-A7, 0-1023
    void progress(int done, int total);
    void finished(int failures);            // queue became empty
    void errorOccurred(const QString &message);
    void noBoard(const QString &port);       // the port opened, but no GPIOcore board answered HELLO
    void logMessage(const QString &kind, const QString &text);   // TX, RX, INFO, WARN, ERR

private:
    enum class Kind { Version, Hello, GetInput, GetOutput, SetInput, SetOutput,
                      GetRoute, SetRoute, ServoLive, GetBoard, SetBoard,
                      TriggerInput, GetOutputState, DccExt, GetAnalog };

    struct Txn
    {
        Kind       kind    = Kind::Version;
        quint8     command = 0;
        QByteArray payload;
        int        index   = -1;
        int        timeout = 0;
        int        triesLeft = 0;
        bool       active  = false;
    };

    void enqueue(Kind kind, quint8 command, const QByteArray &payload, int index, int timeout,
                 bool front = false);
    // Background requests (live readings) are not part of a batch: no progress, no finished().
    static bool isBackground(Kind k) { return k == Kind::GetAnalog; }
    void setCaps(const DeviceCaps &caps);
    void sendNext();
    void transmit();
    void complete(bool ok);
    void onReadyRead();
    void onTimeout();
    void onBootTimeout();
    void handleFrame(const Proto::Frame &f);
    void handleUnsolicited(const Proto::Frame &f);
    void setReady(const QString &version);
    QString describe(const Txn &t) const;

    QPointer<QIODevice> m_device;
    QString             m_portName;
    Proto::FrameParser  m_parser;
    QElapsedTimer       m_lastByte;
    QQueue<Txn>         m_queue;
    Txn                 m_current;
    QTimer              m_timer;
    QTimer              m_bootTimer;
    bool                m_draining     = false;
    bool                m_booting      = false;
    bool                m_ready        = false;
    QString             m_version;
    DeviceCaps          m_caps;
    QVector<Gpio::RouteStep> m_routeBuf[Gpio::NumInputs];
    int                 m_batchTotal   = 0;
    int                 m_batchDone    = 0;
    int                 m_failures     = 0;
};
