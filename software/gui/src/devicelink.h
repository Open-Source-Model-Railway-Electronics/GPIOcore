#pragma once
// Request/response link to a GPIOcore board over any QIODevice
// (QSerialPort for real hardware, SimulatedDevice for demo and tests).
//
// One transaction is in flight at a time. After each transaction a short
// drain period swallows stray ACKs: the current firmware sends two ACKs for
// every SET (handler + transceiver) and an ACK after every GET reply.

#include "gpioconfig.h"
#include "protocol.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QTimer>

class QIODevice;
class QSerialPort;

// What the connected decoder can do, from its HELLO reply (protocol v2).
// A board that does not answer HELLO is the current firmware: protocol v1.
struct DeviceCaps
{
    bool known         = false;
    int  protocol      = 1;
    int  maxRouteSteps = Gpio::MaxRouteStepsV1;
    int  stepPool      = Gpio::NumInputs * Gpio::MaxRouteStepsV1;
    bool serialPins    = false;   // D0/D1 usable as I/O outside a config session
    bool servoLive     = false;
    bool extEeprom     = false;
    bool boardSettings = false;   // GET_BOARD / SET_BOARD (name, type, ...)
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
    bool    isBusy()  const { return m_current.active || !m_queue.isEmpty(); }
    QString version() const { return m_version; }
    QString portName() const { return m_portName; }

    // Firmware before the review fixes uses 0x2D (13 bytes) for output data,
    // which drops data[3]. Detected from the first GET_OUTPUT_DATA reply.
    bool legacyOutputFormat() const { return m_legacyOutput; }

    void requestVersion();
    void readAll();
    void readInput(int index);
    void readOutput(int index);
    void writeInput(int index, const Gpio::InputConfig &c);
    void writeOutput(int index, const Gpio::OutputConfig &c);
    void applyPreset(int preset);
    void readRoute(int route);
    void writeRoute(int route, const QVector<Gpio::RouteStep> &steps);
    void servoLive(int output, int angle);   // coalesced: only the latest angle is sent
    void readBoard();                        // v2 with board settings only
    void writeBoard(const Gpio::BoardSettings &b);

    const DeviceCaps &caps() const { return m_caps; }

    // Timings (ms), shortened in unit tests.
    int bootWaitMs   = 2500;
    int timeoutMs    = 400;
    int presetMs     = 10000;
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
    void presetApplied(int preset);
    void legacyOutputFormatChanged(bool legacy);
    void capabilitiesChanged(const DeviceCaps &caps);
    void routeReceived(int route, const QVector<Gpio::RouteStep> &steps);
    void boardReceived(const Gpio::BoardSettings &board);
    void progress(int done, int total);
    void finished(int failures);            // queue became empty
    void errorOccurred(const QString &message);
    void logMessage(const QString &kind, const QString &text);   // TX, RX, INFO, WARN, ERR

private:
    enum class Kind { Version, Hello, GetInput, GetOutput, SetInput, SetOutput, Preset,
                      GetRoute, SetRoute, ServoLive, GetBoard, SetBoard };

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
    static bool needsV2(Kind k) { return k == Kind::GetRoute || k == Kind::SetRoute || k == Kind::ServoLive
                                      || k == Kind::GetBoard || k == Kind::SetBoard; }
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
    bool                m_legacyOutput = false;
    bool                m_legacyWarned = false;
    QString             m_version;
    DeviceCaps          m_caps;
    bool                m_v2Warned     = false;
    QVector<Gpio::RouteStep> m_routeBuf[Gpio::NumInputs];
    int                 m_batchTotal   = 0;
    int                 m_batchDone    = 0;
    int                 m_failures     = 0;
};
