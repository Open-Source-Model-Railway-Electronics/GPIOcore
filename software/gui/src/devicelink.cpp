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
    connect(&m_timer,     &QTimer::timeout, this, &DeviceLink::onTimeout);
    connect(&m_bootTimer, &QTimer::timeout, this, &DeviceLink::onBootTimeout);
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
    m_version.clear();
    m_legacyOutput = false;
    m_legacyWarned = false;
    m_v2Warned     = false;
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
    m_queue.clear();
    m_current      = Txn();
    m_draining     = false;
    m_booting      = false;
    m_ready        = false;
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
    readRoute(index);                    // v2 only, skipped on v1 boards
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
    if (m_legacyOutput && !m_legacyWarned)
    {
        m_legacyWarned = true;
        emit logMessage(QStringLiteral("WARN"),
                        tr("Firmware uses legacy SET_OUTPUT_DATA (0x2D, 13 bytes): the 4th data byte "
                           "(servo pattern / high byte of times) is not transmitted."));
    }
    const quint8 cmd = m_legacyOutput ? SET_OUTPUT_DATA_LEGACY : SET_OUTPUT_DATA;
    enqueue(Kind::SetOutput, cmd, Gpio::encodeOutput(index, c, m_legacyOutput), index, timeoutMs);
}

void DeviceLink::applyPreset(int preset)
{
    enqueue(Kind::Preset, SET_PRESET, QByteArray(1, char(preset)), preset, presetMs);
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
    t.triesLeft = (kind == Kind::ServoLive) ? 0 : (kind == Kind::Hello ? 1 : retries);

    // Version / hello requests and follow-up route chunks go first.
    if (front || kind == Kind::Version) m_queue.prepend(t);
    else                                m_queue.enqueue(t);

    ++m_batchTotal;
    emit progress(m_batchDone, m_batchTotal);
    sendNext();
}

void DeviceLink::sendNext()
{
    if (!m_device || m_current.active || m_draining || m_booting)
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
    if (needsV2(m_current.kind) && m_caps.protocol < 2)
    {
        if (m_current.kind == Kind::SetRoute && !m_v2Warned)
        {
            m_v2Warned = true;
            emit logMessage(QStringLiteral("WARN"),
                            tr("This firmware speaks protocol v1: routes are stored in the 8 linked bytes "
                               "of SET_INPUT_DATA (state only, fixed 0.5 s per step)."));
        }
        ++m_batchDone;                   // nothing to send on a v1 board
        emit progress(m_batchDone, m_batchTotal);
        sendNext();
        return;
    }
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
    ++m_batchDone;
    emit progress(m_batchDone, m_batchTotal);

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
    if (m_current.kind == Kind::Hello)
    {
        emit logMessage(QStringLiteral("INFO"), tr("No HELLO reply: firmware speaks protocol v1"));
        DeviceCaps v1;
        v1.known = true;
        setCaps(v1);
        complete(true);
        return;
    }
    if (m_current.kind == Kind::ServoLive)
    {
        complete(true);                  // live positions are best effort
        return;
    }
    emit errorOccurred(tr("No answer to %1").arg(describe(m_current)));
    complete(false);
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
        if ((f.command == SET_OUTPUT_DATA || f.command == SET_OUTPUT_DATA_LEGACY) && f.byte(0) == m_current.index)
        {
            const bool legacy = (f.command == SET_OUTPUT_DATA_LEGACY);
            if (legacy != m_legacyOutput)
            {
                m_legacyOutput = legacy;
                emit legacyOutputFormatChanged(legacy);
            }
            int idx = -1;
            Gpio::OutputConfig c;
            if (Gpio::decodeOutput(f.payload, idx, c, legacy))
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
            c.extEeprom     = f.byte(4) & HELLO_FLAG_EXT_EEPROM;
            c.boardSettings = f.byte(4) & HELLO_FLAG_BOARD;
            emit logMessage(QStringLiteral("INFO"),
                            tr("Protocol v%1: %2 steps per route, %3 steps in total%4")
                                .arg(c.protocol).arg(c.maxRouteSteps).arg(c.stepPool)
                                .arg(c.extEeprom ? tr(", external EEPROM") : QString()));
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

    case Kind::Preset:
        if (f.isAck())
        {
            const int preset = m_current.index;
            complete(true);
            emit presetApplied(preset);
            readAll();     // preset broadcasts use an old layout, read back properly
            return;
        }
        if (f.group() != VERSION_GROUP)
        {
            emit logMessage(QStringLiteral("INFO"), tr("Preset broadcast ignored (old payload layout)"));
            return;
        }
        break;
    }

    if (f.group() == VERSION_GROUP)
        handleUnsolicited(f);     // board was reset while we were waiting
}

void DeviceLink::handleUnsolicited(const Frame &f)
{
    if (f.isAck())
        return;                   // trailing ACK of the previous transaction

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
    // and tells us what it can do. No reply = protocol v1.
    enqueue(Kind::Hello, HELLO, QByteArray(), -1, 300, true);
    emit ready(version);
}

QString DeviceLink::describe(const Txn &t) const
{
    switch (t.kind)
    {
    case Kind::Version:   return tr("version request");
    case Kind::GetInput:  return tr("read input %1").arg(t.index);
    case Kind::GetOutput: return tr("read output %1").arg(t.index);
    case Kind::SetInput:  return tr("write input %1").arg(t.index);
    case Kind::SetOutput: return tr("write output %1").arg(t.index);
    case Kind::Preset:    return tr("preset %1").arg(t.index);
    case Kind::Hello:     return tr("hello");
    case Kind::GetRoute:  return tr("read route %1").arg(t.index);
    case Kind::SetRoute:  return tr("write route %1").arg(t.index);
    case Kind::ServoLive: return tr("live servo %1").arg(t.index);
    case Kind::GetBoard:  return tr("read board settings");
    case Kind::SetBoard:  return tr("write board settings");
    case Kind::TriggerInput:   return tr("trigger input %1").arg(t.index);
    case Kind::GetOutputState: return tr("read state of output %1").arg(t.index);
    case Kind::DccExt:         return tr("DCC extended to address %1").arg(t.index);
    }
    return QString();
}
