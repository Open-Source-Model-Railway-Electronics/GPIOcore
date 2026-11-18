#include "devicelink.h"

#include <QSerialPort>

using namespace Proto;

static QByteArray frameBytes(const Frame &f)
{
    if (f.isAck())  return QByteArray(1, char(ACK));
    if (f.isNack()) return QByteArray(1, char(NACK)) + f.payload;
    QByteArray b(1, char(f.command));
    b += f.payload;
    quint8 cs = f.command;
    for (char c : f.payload) cs ^= quint8(c);
    b.append(char(cs));
    return b;
}

DeviceLink::DeviceLink(QObject *parent) : QObject(parent)
{
    m_timer.setSingleShot(true);
    m_bootTimer.setSingleShot(true);
    m_announceTimer.setSingleShot(true);
    connect(&m_timer,     &QTimer::timeout, this, &DeviceLink::onTimeout);
    connect(&m_bootTimer, &QTimer::timeout, this, &DeviceLink::onBootTimeout);
    connect(&m_announceTimer, &QTimer::timeout, this, &DeviceLink::onAnnounceTimeout);
}

DeviceLink::~DeviceLink()
{
    if (m_device)
        m_device->close();
}

bool DeviceLink::openSerial(const QString &portName, QString *error)
{
    auto *port = new QSerialPort(portName);
    port->setBaudRate(QSerialPort::Baud115200);
    port->setDataBits(QSerialPort::Data8);
    port->setParity(QSerialPort::NoParity);
    port->setStopBits(QSerialPort::OneStop);
    port->setFlowControl(QSerialPort::NoFlowControl);

    if (!port->open(QIODevice::ReadWrite))
    {
        if (error) *error = port->errorString();
        delete port;
        return false;
    }

    // DTR high resets an Uno / Nano (auto-reset), the firmware then sends its
    // version on boot. Boards without auto-reset get a REQ_VERSION later.
    port->setDataTerminalReady(true);

    connect(port, &QSerialPort::errorOccurred, this, [this, port](QSerialPort::SerialPortError e) {
        if (e == QSerialPort::ResourceError || e == QSerialPort::PermissionError)
        {
            emit errorOccurred(tr("Serial port lost: %1").arg(port->errorString()));
            close();
        }
    });

    m_portName = portName;
    return openDevice(port, true, error);
}

bool DeviceLink::openDevice(QIODevice *device, bool bootsOnOpen, QString *error)
{
    if (m_device)
        close();

    if (!device->isOpen() && !device->open(QIODevice::ReadWrite))
    {
        if (error) *error = device->errorString();
        delete device;
        return false;
    }

    device->setParent(this);
    m_device = device;
    if (m_portName.isEmpty())
        m_portName = QStringLiteral("simulator");
    connect(device, &QIODevice::readyRead, this, &DeviceLink::onReadyRead);

    m_parser.reset();
    m_lastByte.start();
    m_ready        = false;
    m_waiting      = false;
    m_btBuf.clear();
    m_version.clear();
    m_caps         = DeviceCaps();
    m_batchTotal = m_batchDone = m_failures = 0;

    emit opened();
    emit logMessage(QStringLiteral("INFO"), tr("Opened %1").arg(m_portName));

    if (bootsOnOpen)
    {
        m_booting = true;
        m_bootTimer.start(bootWaitMs);
        emit logMessage(QStringLiteral("INFO"), tr("Waiting for boot message…"));
    }
    else
    {
        requestVersion();
    }
    return true;
}

void DeviceLink::close()
{
    if (!m_device)
        return;

    m_timer.stop();
    m_bootTimer.stop();
    m_announceTimer.stop();
    m_queue.clear();
    m_current      = Txn();
    m_draining     = false;
    m_booting      = false;
    m_ready        = false;
    m_waiting      = false;
    m_btBuf.clear();
    m_batchTotal = m_batchDone = m_failures = 0;

    QIODevice *dev = m_device;
    m_device = nullptr;
    disconnect(dev, nullptr, this, nullptr);
    dev->close();
    dev->deleteLater();

    emit logMessage(QStringLiteral("INFO"), tr("Closed %1").arg(m_portName));
    m_portName.clear();
    emit closed();
}

bool DeviceLink::isBusy() const
{
    if (m_current.active && !isBackground(m_current.kind))
        return true;
    for (const Txn &t : m_queue)
        if (!isBackground(t.kind))
            return true;
    return false;
}

// ── Public requests ─────────────────────────────────────────────────────────
void DeviceLink::requestVersion()
{
    enqueue(Kind::Version, REQ_VERSION, QByteArray(), -1, timeoutMs);
}

void DeviceLink::readAll()
{
    for (int i = 0; i < Gpio::NumInputs; ++i)  readInput(i);
    for (int i = 0; i < Gpio::NumOutputs; ++i) readOutput(i);
}

void DeviceLink::readInput(int index)
{
    enqueue(Kind::GetInput, GET_INPUT_DATA, QByteArray(1, char(index)), index, timeoutMs);
    readRoute(index);
}

void DeviceLink::readRoute(int route)
{
    QByteArray p;
    p.append(char(route));
    p.append(char(0));                   // first step
    enqueue(Kind::GetRoute, GET_ROUTE, p, route, timeoutMs);
}

void DeviceLink::writeRoute(int route, const QVector<Gpio::RouteStep> &steps)
{
    const int total = qMin(steps.size(), Gpio::MaxRouteSteps);
    int first = 0;
    do
    {
        const int n = qMin(RouteChunkSteps, total - first);
        QByteArray p;
        p.append(char(4 + 4 * n));      // extended length, includes itself
        p.append(char(route));
        p.append(char(total));
        p.append(char(first));
        for (int i = 0; i < n; ++i)
            p += Gpio::encodeStep(steps[first + i]);
        // a route that changes length shifts the routes behind it in EEPROM (~3.4 ms per byte, up to ~2.6 s)
        enqueue(Kind::SetRoute, SET_ROUTE, p, route, qMax(timeoutMs, eepromShiftMs));
        first += n;
    } while (first < total);
}

void DeviceLink::servoLive(int output, int angle)
{
    const QByteArray p = QByteArray(1, char(output)) + QByteArray(1, char(qBound(0, angle, 180)));
    for (Txn &t : m_queue)
        if (t.kind == Kind::ServoLive && t.index == output)
        {
            t.payload = p;               // still waiting: just send the newest angle
            return;
        }
    enqueue(Kind::ServoLive, SERVO_LIVE, p, output, 150);
}

void DeviceLink::triggerInput(int index, int direction)
{
    QByteArray p;
    p.append(char(index));
    p.append(char(direction ? 1 : 0));
    enqueue(Kind::TriggerInput, SET_INPUT_STATE, p, index, timeoutMs);
}

void DeviceLink::dccExt(int address, int value)
{
    QByteArray p;
    p.append(char(address >> 8));
    p.append(char(address & 0xFF));
    p.append(char(value));
    for (Txn &t : m_queue)
        if (t.kind == Kind::DccExt && t.index == address)
        {
            t.payload = p;               // still waiting: just send the newest value
            return;
        }
    enqueue(Kind::DccExt, DCC_EXT, p, address, timeoutMs);
}

void DeviceLink::readAnalog(int channel)
{
    if (m_current.active && m_current.kind == Kind::GetAnalog && m_current.index == (channel & 7))
        return;                          // being read now
    for (const Txn &t : m_queue)
        if (t.kind == Kind::GetAnalog && t.index == (channel & 7))
            return;                      // still waiting: one reading per channel is enough
    enqueue(Kind::GetAnalog, GET_ANALOG, QByteArray(1, char(channel & 7)), channel & 7, timeoutMs);
}

// One long task: the board talks to the HC-05 at other baud rates, so nothing else is sent meanwhile
// (one transaction at a time) and everything but a valid BT_RESULT is ignored (scanBluetooth).
void DeviceLink::bluetoothSetup()
{
    enqueue(Kind::BtSetup, BT_SETUP, QByteArray(), -1, bluetoothMs);
}

void DeviceLink::readOutputState(int output)
{
    enqueue(Kind::GetOutputState, GET_OUTPUT_STATE, QByteArray(1, char(output)), output, timeoutMs);
}

void DeviceLink::readBoard()
{
    enqueue(Kind::GetBoard, GET_BOARD, QByteArray(), -1, timeoutMs);
}

void DeviceLink::writeBoard(const Gpio::BoardSettings &b)
{
    const QByteArray body = Gpio::encodeBoard(b);
    // switching timing converts the whole route pool in EEPROM
    enqueue(Kind::SetBoard, SET_BOARD, QByteArray(1, char(body.size() + 1)) + body, -1, qMax(timeoutMs, eepromShiftMs));
}

void DeviceLink::readOutput(int index)
{
    enqueue(Kind::GetOutput, GET_OUTPUT_DATA, QByteArray(1, char(index)), index, timeoutMs);
}

void DeviceLink::writeInput(int index, const Gpio::InputConfig &c)
{
    enqueue(Kind::SetInput, SET_INPUT_DATA, Gpio::encodeInput(index, c), index, timeoutMs);
}

void DeviceLink::writeOutput(int index, const Gpio::OutputConfig &c)
{
    enqueue(Kind::SetOutput, SET_OUTPUT_DATA, Gpio::encodeOutput(index, c), index, timeoutMs);
}

// ── Queue ───────────────────────────────────────────────────────────────────
void DeviceLink::enqueue(Kind kind, quint8 command, const QByteArray &payload, int index, int timeout,
                         bool front)
{
    if (!m_device)
        return;

    Txn t;
    t.kind      = kind;
    t.command   = command;
    t.payload   = payload;
    t.index     = index;
    t.timeout   = timeout;
    t.triesLeft = (kind == Kind::ServoLive || kind == Kind::GetAnalog || kind == Kind::BtSetup) ? 0
                : (kind == Kind::Hello ? 1 : retries);

    // Version / hello requests and follow-up route chunks go first.
    if (front || kind == Kind::Version) m_queue.prepend(t);
    else                                m_queue.enqueue(t);

    if (!isBackground(kind))
    {
        ++m_batchTotal;
        emit progress(m_batchDone, m_batchTotal);
    }
    sendNext();
}

void DeviceLink::sendNext()
{
    if (!m_device || m_current.active || m_draining || m_booting || m_waiting)
        return;

    if (m_queue.isEmpty())
    {
        if (m_batchTotal > 0)
        {
            const int failures = m_failures;
            m_batchTotal = m_batchDone = m_failures = 0;
            emit progress(0, 0);
            emit finished(failures);
        }
        return;
    }

    m_current = m_queue.dequeue();
    m_current.active = true;
    transmit();
}

void DeviceLink::transmit()
{
    const QByteArray bytes = encode(m_current.command, m_current.payload);
    m_device->write(bytes);
    emit logMessage(QStringLiteral("TX"),
                    QStringLiteral("%1  %2").arg(commandName(m_current.command), -30).arg(toHex(bytes)));
    m_timer.start(m_current.timeout);
}

void DeviceLink::complete(bool ok)
{
    m_timer.stop();
    if (!ok)
        ++m_failures;
    m_current.active = false;
    if (!isBackground(m_current.kind))
    {
        ++m_batchDone;
        emit progress(m_batchDone, m_batchTotal);
    }

    m_draining = true;
    QTimer::singleShot(drainMs, this, [this] {
        m_draining = false;
        sendNext();
    });
}

void DeviceLink::onTimeout()
{
    if (!m_current.active)
        return;

    m_parser.reset();
    if (m_current.triesLeft > 0)
    {
        --m_current.triesLeft;
        emit logMessage(QStringLiteral("WARN"), tr("No answer to %1, retrying").arg(describe(m_current)));
        transmit();
        return;
    }
    if (m_current.kind == Kind::Hello || (m_current.kind == Kind::Version && !m_ready))
    {
        startWaiting();                  // the board's serial port may be closed: wait for its announce
        return;
    }
    if (m_current.kind == Kind::ServoLive || m_current.kind == Kind::GetAnalog)
    {
        complete(true);                  // live positions and readings are best effort
        return;
    }
    emit errorOccurred(tr("No answer to %1").arg(describe(m_current)));
    complete(false);
    if (m_current.kind == Kind::BtSetup)
    {
        m_btBuf.clear();
        emit bluetoothResult(-1, 0, false);
    }
}

// No answer when connecting. The board may have closed its serial port: it has run for more than 5 s
// without the GUI (over Bluetooth connecting does not reset it). A short press on its config button opens
// the port again for 5 s and the board announces itself (the HELLO reply, unasked): wait for that.
void DeviceLink::startWaiting()
{
    m_timer.stop();
    m_current = Txn();
    m_queue.clear();
    m_ready = false;
    m_batchTotal = m_batchDone = m_failures = 0;
    emit progress(0, 0);
    m_waiting = true;
    m_announceTimer.start(announceWaitMs);
    emit logMessage(QStringLiteral("INFO"), tr("No answer: waiting for the board (short press on its config button)"));
    emit waitingForBoard();
}

void DeviceLink::onAnnounceTimeout()
{
    if (!m_waiting)
        return;
    m_waiting = false;
    // every GPIOcore firmware answers HELLO: this is another device or very old firmware
    emit errorOccurred(tr("No answer to HELLO: this is no GPIOcore board, or its firmware is too old. "
                          "Flash the firmware (Board > Flash firmware)."));
    const QString port = m_portName;
    close();
    emit noBoard(port);
}

void DeviceLink::setCaps(const DeviceCaps &caps)
{
    m_caps = caps;
    emit capabilitiesChanged(m_caps);
}

void DeviceLink::onBootTimeout()
{
    m_booting = false;
    if (!m_ready)
    {
        emit logMessage(QStringLiteral("INFO"), tr("No boot message, requesting version"));
        requestVersion();
    }
    sendNext();
}

// ── Receive ─────────────────────────────────────────────────────────────────
void DeviceLink::onReadyRead()
{
    if (!m_device)
        return;

    const QByteArray data = m_device->readAll();
    if (m_current.active && m_current.kind == Kind::BtSetup && scanBluetooth(data))
        return;
    if (!m_parser.idle() && m_lastByte.elapsed() > 100)
    {
        emit logMessage(QStringLiteral("WARN"), tr("Incomplete frame discarded"));
        m_parser.reset();
    }
    m_lastByte.restart();

    FrameParser::Result r;
    m_parser.feed(data, r);
    for (const QString &e : r.errors)
        emit logMessage(QStringLiteral("ERR"), e);
    for (const Frame &f : r.frames)
        handleFrame(f);
}

// During BT_SETUP the board talks to the HC-05 at other baud rates: noise here, maybe NACKs from the HC-05's
// replies. Only a valid BT_RESULT counts. Returns true: the bytes are taken.
bool DeviceLink::scanBluetooth(const QByteArray &data)
{
    m_lastByte.restart();
    m_btBuf += data;
    for (int i = 0; i + 4 < m_btBuf.size(); ++i)
    {
        const auto b = [this, i](int k) { return quint8(m_btBuf.at(i + k)); };
        if (b(0) != BT_RESULT || b(1) > 2 || quint8(b(0) ^ b(1) ^ b(2) ^ b(3)) != b(4))
            continue;
        emit logMessage(QStringLiteral("RX"), QStringLiteral("%1  %2").arg(commandName(BT_RESULT), -30)
                                                  .arg(toHex(m_btBuf.mid(i, 5))));
        if (i > 0)
            emit logMessage(QStringLiteral("INFO"), tr("%1 byte(s) of noise ignored during the Bluetooth set-up").arg(i));
        const int  status = b(1), baud = b(2) * 2400;
        const bool nameOk = b(3);
        m_btBuf.clear();
        m_parser.reset();
        complete(true);                  // the dialog shows the result
        emit bluetoothResult(status, baud, nameOk);
        return true;
    }
    if (m_btBuf.size() > 4096)
        m_btBuf = m_btBuf.right(4);
    return true;
}

void DeviceLink::handleFrame(const Frame &f)
{
    if (!f.isAck())
        emit logMessage(QStringLiteral("RX"),
                        QStringLiteral("%1  %2").arg(commandName(f.command), -30).arg(toHex(frameBytes(f))));

    if (f.command == OUTPUT_EVENT && f.payload.size() >= 3)
    {
        emit outputEvent(f.byte(0), f.byte(1), f.byte(2));    // may arrive at any time
        return;
    }
    if (f.command == ANALOG_VALUE && f.payload.size() >= 3)
    {
        emit analogValue(f.byte(0) & 7, f.byte(1) << 8 | f.byte(2));
        if (m_current.active && m_current.kind == Kind::GetAnalog)
            complete(true);              // the trailing ACK is swallowed as unsolicited
        return;
    }

    if (!m_current.active)
    {
        handleUnsolicited(f);
        return;
    }

    if (f.isNack())
    {
        emit logMessage(QStringLiteral("WARN"),
                        tr("NACK (%1) on %2").arg(nackReason(f.byte(0)), describe(m_current)));
        if (m_current.triesLeft > 0)
        {
            --m_current.triesLeft;
            m_timer.stop();
            QTimer::singleShot(drainMs, this, [this] { if (m_current.active) transmit(); });
        }
        else
        {
            emit errorOccurred(tr("Device rejected %1: %2").arg(describe(m_current), nackReason(f.byte(0))));
            complete(false);
        }
        return;
    }

    switch (m_current.kind)
    {
    case Kind::Version:
        if (f.group() == VERSION_GROUP)
        {
            setReady(QString::fromLatin1(f.payload));
            complete(true);
            return;
        }
        break;

    case Kind::GetInput:
        if (f.command == SET_INPUT_DATA && f.byte(0) == m_current.index)
        {
            int idx = -1;
            Gpio::InputConfig c;
            if (Gpio::decodeInput(f.payload, idx, c))
                emit inputReceived(idx, c);
            complete(true);
            return;
        }
        break;

    case Kind::GetOutput:
        if (f.command == SET_OUTPUT_DATA && f.byte(0) == m_current.index)
        {
            int idx = -1;
            Gpio::OutputConfig c;
            if (Gpio::decodeOutput(f.payload, idx, c))
                emit outputReceived(idx, c);
            complete(true);
            return;
        }
        break;

    case Kind::SetInput:
        if (f.isAck()) { emit inputWritten(m_current.index);  complete(true); return; }
        break;

    case Kind::SetOutput:
        if (f.isAck()) { emit outputWritten(m_current.index); complete(true); return; }
        break;

    case Kind::Hello:
        if (f.group() == HELLO_GROUP && f.payload.size() >= 5)
        {
            DeviceCaps c;
            c.known         = true;
            c.protocol      = f.byte(0);
            c.maxRouteSteps = qBound(0, int(f.byte(1)), Gpio::MaxRouteSteps);
            c.stepPool      = f.byte(2) | f.byte(3) << 8;
            c.serialPins    = f.byte(4) & HELLO_FLAG_SERIAL_PINS;
            c.servoLive     = f.byte(4) & HELLO_FLAG_SERVO_LIVE;
            c.boardSettings = f.byte(4) & HELLO_FLAG_BOARD;
            c.analogSound   = f.byte(4) & HELLO_FLAG_ANALOG_SOUND;
            c.bluetooth     = f.byte(4) & HELLO_FLAG_BLUETOOTH;
            emit logMessage(QStringLiteral("INFO"),
                            tr("Protocol v%1: %2 steps per route, %3 steps in total")
                                .arg(c.protocol).arg(c.maxRouteSteps).arg(c.stepPool));
            setCaps(c);
            complete(true);
            if (c.boardSettings)
                enqueue(Kind::GetBoard, GET_BOARD, QByteArray(), -1, timeoutMs, true);   // name first
            return;
        }
        break;

    case Kind::GetBoard:
        if (f.command == SET_BOARD)
        {
            Gpio::BoardSettings b;
            if (Gpio::decodeBoard(f.payload.mid(1), b))
            {
                complete(true);
                emit boardReceived(b);
                return;
            }
        }
        break;

    case Kind::SetBoard:
    case Kind::TriggerInput:
    case Kind::GetOutputState:
    case Kind::DccExt:
        if (f.isAck()) { complete(true); return; }
        break;

    case Kind::GetRoute:
        if (f.command == SET_ROUTE && f.byte(1) == m_current.index)
        {
            const int route = m_current.index;
            const int total = qMin(int(f.byte(2)), Gpio::MaxRouteSteps);
            const int first = f.byte(3);
            const int n     = (f.payload.size() - 4) / 4;
            if (first == 0)
                m_routeBuf[route].clear();
            for (int i = 0; i < n && m_routeBuf[route].size() < total; ++i)
                m_routeBuf[route].append(Gpio::decodeStep(f.payload.constData() + 4 + 4 * i));
            complete(true);
            if (m_routeBuf[route].size() < total && n > 0)
            {
                QByteArray q;
                q.append(char(route));
                q.append(char(first + n));
                enqueue(Kind::GetRoute, GET_ROUTE, q, route, timeoutMs, true);   // next chunk first
            }
            else
            {
                emit routeReceived(route, m_routeBuf[route]);
            }
            return;
        }
        break;

    case Kind::SetRoute:
    case Kind::ServoLive:
        if (f.isAck()) { complete(true); return; }
        break;

    case Kind::GetAnalog:
        break;                           // ANALOG_VALUE is handled above

    case Kind::BtSetup:
        break;                           // BT_RESULT is found by scanBluetooth()
    }

    if (f.group() == VERSION_GROUP)
        handleUnsolicited(f);     // board was reset while we were waiting
}

void DeviceLink::handleUnsolicited(const Frame &f)
{
    if (f.isAck())
        return;                   // trailing ACK of the previous transaction

    if (m_waiting && (f.group() == VERSION_GROUP || (f.group() == HELLO_GROUP && f.payload.size() >= 5)))
    {
        m_waiting = false;        // the board is there (the announce, or it was reset): sign on as usual
        m_announceTimer.stop();
        emit logMessage(QStringLiteral("INFO"), tr("The board announced itself"));
        if (f.group() == HELLO_GROUP)
        {
            requestVersion();     // then HELLO, within the 5 s the board keeps its port open
            return;
        }
    }
    if (f.group() == VERSION_GROUP)
    {
        const bool wasBooting = m_booting;
        m_bootTimer.stop();
        m_booting = false;
        setReady(QString::fromLatin1(f.payload));
        if (wasBooting)
            sendNext();
        return;
    }
    emit logMessage(QStringLiteral("INFO"), tr("Unsolicited %1 ignored").arg(commandName(f.command)));
}

void DeviceLink::setReady(const QString &version)
{
    m_version = version;
    m_ready   = true;
    emit logMessage(QStringLiteral("INFO"), tr("GPIOcore version '%1'").arg(version));
    // Sign on: starts a configuration session (the decoder keeps D0/D1 as serial)
    // and tells us what it can do. No reply = no GPIOcore board.
    enqueue(Kind::Hello, HELLO, QByteArray(), -1, 300, true);
    emit ready(version);
}

QString DeviceLink::describe(const Txn &t) const
{
    switch (t.kind)
    {
    case Kind::Version:   return tr("version request");
    case Kind::GetInput:  return tr("read input %1").arg(t.index + 1);
    case Kind::GetOutput: return tr("read output %1").arg(t.index + 1);
    case Kind::SetInput:  return tr("write input %1").arg(t.index + 1);
    case Kind::SetOutput: return tr("write output %1").arg(t.index + 1);
    case Kind::Hello:     return tr("hello");
    case Kind::GetRoute:  return tr("read route %1").arg(t.index + 1);
    case Kind::SetRoute:  return tr("write route %1").arg(t.index + 1);
    case Kind::ServoLive: return tr("live servo %1").arg(t.index + 1);
    case Kind::GetBoard:  return tr("read board settings");
    case Kind::SetBoard:  return tr("write board settings");
    case Kind::TriggerInput:   return tr("trigger input %1").arg(t.index + 1);
    case Kind::GetOutputState: return tr("read state of output %1").arg(t.index + 1);
    case Kind::DccExt:         return tr("DCC extended to address %1").arg(t.index);
    case Kind::GetAnalog:      return tr("read analog pin A%1").arg(t.index);
    case Kind::BtSetup:        return tr("Bluetooth set-up");
    }
    return QString();
}
