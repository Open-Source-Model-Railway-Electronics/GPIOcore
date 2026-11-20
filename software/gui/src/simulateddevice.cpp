#include "simulateddevice.h"
#include "protocol.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDeadlineTimer>
#include <QFileInfo>
#include <QProcess>

using namespace Gpio;

// The channel to gpiocore_fw_host, both ways: <type> <length lo> <length hi> <payload>
// (see fwhost/main.cpp for the frame types).

static QByteArray bytes(std::initializer_list<int> values)
{
    QByteArray b;
    for (int v : values)
        b.append(char(v & 0xFF));
    return b;
}

SimulatedDevice::SimulatedDevice(QObject *parent) : QIODevice(parent)
{
    const BoardConfig d = factoryDefaults();
    for (int i = 0; i < NumOutputs; ++i) m_out[i] = d.outputs[i];
    for (int i = 0; i < NumInputs; ++i)  m_in[i]  = d.inputs[i];
    m_board = d.board;
    m_analog.fill(AnalogMaxValue);       // pull-up, no button pressed
}

SimulatedDevice::~SimulatedDevice()
{
    close();
}

QString SimulatedDevice::hostProgram()
{
#ifdef Q_OS_WIN
    const QString exe = QStringLiteral("gpiocore_fw_host.exe");
#else
    const QString exe = QStringLiteral("gpiocore_fw_host");
#endif
    QStringList candidates;
    const QString env = qEnvironmentVariable("GPIOCORE_FW_HOST");
    if (!env.isEmpty())
        candidates << env;
    candidates << QCoreApplication::applicationDirPath() + QLatin1Char('/') + exe;
#ifdef GPIOCORE_FW_HOST_PATH
    candidates << QStringLiteral(GPIOCORE_FW_HOST_PATH);   // the build tree
#endif
    for (const QString &c : candidates)
        if (QFileInfo(c).isExecutable())
            return c;
    return QString();
}

BoardConfig SimulatedDevice::factoryDefaults()
{
    BoardConfig b;
    // Output::defaults(): pinA = D3-D12, A0-A5 (D13 is the config LED), single steady, switch time 50
    for (int i = 0; i < NumOutputs; ++i)
    {
        OutputConfig c;
        c.pinA = quint8(i < 10 ? 3 + i : 4 + i);
        c.type = SingleSteady;
        c.setSwitchTime(50);
        b.outputs[i] = c.normalized();
    }
    // Input::defaults(): output i, DCC address i + 1, absolute, no pin
    for (int i = 0; i < NumInputs; ++i)
    {
        InputConfig c;
        c.outputId = quint8(i);
        c.address  = quint16(i + 1);
        c.pin      = PinNA;
        c.type     = Latching;
        b.inputs[i] = c.normalized();
    }
    return b;   // board settings: the BoardSettings defaults (Nano, timing off, interlock auto, 0.3 s)
}

bool SimulatedDevice::open(OpenMode mode)
{
    if (isOpen())
        return true;
    const QString program = hostProgram();
    if (program.isEmpty())
    {
        setErrorString(tr("The firmware simulator (gpiocore_fw_host) is missing next to the program."));
        return false;
    }
    if (!QIODevice::open(mode | QIODevice::Unbuffered))
        return false;

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &SimulatedDevice::readHost);
    connect(m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (isOpen() && m_proc)
            qWarning() << "firmware simulator stopped:" << code << status;
    });
    m_booted = false;
    m_fromHost.clear();
    m_rx.clear();
    m_proc->start(program, QStringList(), QIODevice::ReadWrite);
    if (!m_proc->waitForStarted(5000))
    {
        setErrorString(tr("Could not start the firmware simulator: %1").arg(m_proc->errorString()));
        close();
        return false;
    }

    // the levels and the configuration that were set before: board settings first (they decide the route format)
    m_proc->write(m_setupLevels);
    m_setupLevels.clear();
    if (m_setupBoard) sendBoard(*m_setupBoard);
    for (int i = 0; i < NumOutputs; ++i) if (m_setupOut[i]) sendOutput(i, *m_setupOut[i]);
    for (int i = 0; i < NumInputs; ++i)  if (m_setupIn[i])  sendInput(i, *m_setupIn[i]);
    m_setupBoard.reset();
    for (auto &o : m_setupOut) o.reset();
    for (auto &in : m_setupIn) in.reset();

    const QDeadlineTimer deadline(5000);
    while (!m_booted && m_proc && m_proc->state() == QProcess::Running && !deadline.hasExpired())
        m_proc->waitForReadyRead(50);
    if (!m_booted)
    {
        setErrorString(tr("The firmware simulator did not start up."));
        close();
        return false;
    }
    return true;
}

void SimulatedDevice::close()
{
    if (m_proc)
    {
        QProcess *p = m_proc;
        m_proc = nullptr;                // no more reads or warnings from it
        p->disconnect(this);
        p->closeWriteChannel();          // the host program ends when its stdin closes
        if (!p->waitForFinished(1000))
            p->kill(), p->waitForFinished(1000);
        delete p;
    }
    m_rx.clear();
    if (isOpen())
        QIODevice::close();
}

qint64 SimulatedDevice::readData(char *data, qint64 maxSize)
{
    const qint64 n = qMin<qint64>(maxSize, m_rx.size());
    memcpy(data, m_rx.constData(), size_t(n));
    m_rx.remove(0, int(n));
    return n;
}

qint64 SimulatedDevice::writeData(const char *data, qint64 size)
{
    QByteArray bytes(data, int(size));
    if (m_corrupt > 0 && bytes.size() > 1)   // a frame with a checksum: break it
    {
        --m_corrupt;
        bytes[bytes.size() - 1] = char(bytes.at(bytes.size() - 1) ^ 0x5A);
    }
    sendFrame('S', bytes);
    return size;
}

void SimulatedDevice::sendFrame(char type, const QByteArray &data)
{
    QByteArray f;
    f.append(type);
    f.append(char(data.size() & 0xFF));
    f.append(char(data.size() >> 8));
    f += data;
    if (m_proc)
        m_proc->write(f);
    else if (type == 'A')                // a level set before open(): sent first, before the set-up
        m_setupLevels += f;
}

// ── Setting the board up ─────────────────────────────────────────────────────
void SimulatedDevice::sendSetup(quint8 command, const QByteArray &payload)
{
    sendFrame('X', QByteArray(1, char(command)) + payload);
}

void SimulatedDevice::sendOutput(int i, const OutputConfig &c)
{
    sendSetup(Proto::SET_OUTPUT_DATA, encodeOutput(i, c));
}

void SimulatedDevice::sendInput(int i, const InputConfig &c)
{
    sendSetup(Proto::SET_INPUT_DATA, encodeInput(i, c));
    // the route (or the custom values) in chunks, like DeviceLink::writeRoute(); 0 steps clears it
    const int total = qMin(int(c.route.size()), MaxRouteSteps);
    int first = 0;
    do
    {
        const int n = qMin(Proto::RouteChunkSteps, total - first);
        QByteArray p;
        p.append(char(4 + StepBytes * n));
        p.append(char(i));
        p.append(char(total));
        p.append(char(first));
        for (int k = 0; k < n; ++k)
            p += encodeStep(c.route[first + k]);
        sendSetup(Proto::SET_ROUTE, p);
        first += n;
    } while (first < total);
}

void SimulatedDevice::sendBoard(const BoardSettings &b)
{
    const QByteArray body = encodeBoard(b);
    sendSetup(Proto::SET_BOARD, QByteArray(1, char(body.size() + 1)) + body);
}

void SimulatedDevice::setOutput(int i, const OutputConfig &c)
{
    m_out[i] = c.normalized();           // the firmware's report follows
    if (m_proc) sendOutput(i, c);
    else        m_setupOut[i] = c;
}

void SimulatedDevice::setInput(int i, const InputConfig &c)
{
    m_in[i] = c.normalized();
    if (m_proc) sendInput(i, c);
    else        m_setupIn[i] = c;
}

void SimulatedDevice::setBoard(const BoardSettings &b)
{
    m_board = b;
    if (m_proc) sendBoard(b);
    else        m_setupBoard = b;
}

// ── The outside world ────────────────────────────────────────────────────────
void SimulatedDevice::setAnalogValue(int channel, int value)
{
    channel &= 7;
    m_analog[channel] = qBound(0, value, AnalogMaxValue);
    sendFrame('A', bytes({ 14 + channel, m_analog[channel] >> 8, m_analog[channel] }));
}

void SimulatedDevice::setPinLevel(int pin, bool high)
{
    if (pin >= 14 && pin < NumPins)
    {
        setAnalogValue(pin - 14, high ? AnalogMaxValue : 0);
        return;
    }
    const int v = high ? AnalogMaxValue : 0;
    sendFrame('A', bytes({ pin, v >> 8, v & 0xFF }));
}

void SimulatedDevice::dccAccessory(int address, int direction)
{
    sendFrame('D', bytes({ address >> 8, address & 0xFF, direction ? 1 : 0, 1 }));
}

void SimulatedDevice::dccExtended(int address, int value)
{
    sendFrame('E', bytes({ address >> 8, address & 0xFF, value }));
}

void SimulatedDevice::dccLocoSpeed(int address, int speed, bool forward)
{
    sendFrame('L', bytes({ address >> 8, address & 0xFF, speed, forward ? 1 : 0 }));
}

void SimulatedDevice::attachFakeHc05(int baud)
{
    sendFrame('H', bytes({ baud / 2400 }));
}

void SimulatedDevice::linkReceive(const QByteArray &bytes)
{
    sendFrame('S', bytes);                   // the UART receiver: at 9600 baud the firmware reads link frames
}

void SimulatedDevice::advanceClock(int ms)
{
    ms = qBound(0, ms, 0xFFFF);
    sendFrame('T', bytes({ ms & 0xFF, ms >> 8 }));
}

void SimulatedDevice::dccFunctions(int address, quint8 f0f4)
{
    sendFrame('F', bytes({ address >> 8, address & 0xFF, f0f4 }));
}

// ── From the firmware ────────────────────────────────────────────────────────
void SimulatedDevice::readHost()
{
    if (!m_proc)
        return;
    m_fromHost += m_proc->readAllStandardOutput();
    bool serial = false, state = false;
    int pos = 0;
    while (m_fromHost.size() - pos >= 3)
    {
        const int n = quint8(m_fromHost.at(pos + 1)) | quint8(m_fromHost.at(pos + 2)) << 8;
        if (m_fromHost.size() - pos < 3 + n)
            break;
        const char type = m_fromHost.at(pos);
        if (handleFrame(type, m_fromHost.mid(pos + 3, n))) serial = true;
        else if (type != 'R' && type != 'x')              state  = true;
        pos += 3 + n;
    }
    m_fromHost.remove(0, pos);
    if (state)
        emit stateChanged();
    if (serial && isOpen())
        emit readyRead();
}

bool SimulatedDevice::handleFrame(char type, const QByteArray &d)
{
    auto at = [&d](int i) { return i < d.size() ? quint8(d.at(i)) : quint8(0); };
    switch (type)
    {
    case 'R':
        m_booted = true;
        break;
    case 'S':
        m_rx += d;
        return true;
    case 'x':
        if (at(1))
            qWarning() << "firmware simulator: set-up message" << Proto::commandName(at(0))
                       << "refused:" << Proto::nackReason(at(1));
        break;
    case 'O':                            // GET_OUTPUT_DATA layout + servo angle + frog relays
    {
        int idx = -1;
        OutputConfig c;
        if (d.size() >= 16 && decodeOutput(d.left(14), idx, c))
        {
            c.name       = m_out[idx].name;
            c.lampColors = m_out[idx].lampColors;
            m_out[idx]  = c;
            m_pos[idx]  = at(14);
            m_frog[idx] = at(15);
        }
        break;
    }
    case 'I':                            // <index> <addr H> <addr L> <pin> <type> <output> <level> <steps> <steps x 4>
    {
        QByteArray p = d.left(6);
        p.append(char(quint8(at(6) << 1)));               // GET_INPUT_DATA: threshold / 4
        p.append(QByteArray(7, char(0xFF)));
        int idx = -1;
        InputConfig c;
        if (d.size() >= 8 && decodeInput(p, idx, c))
        {
            for (int k = 0; k < at(7) && 8 + StepBytes * k + StepBytes <= d.size(); ++k)
                c.route.append(decodeStep(d.constData() + 8 + StepBytes * k));
            c.name = m_in[idx].name;
            m_in[idx] = c;
        }
        break;
    }
    case 'B':
        decodeBoard(d, m_board);
        break;
    case 'h':
        m_hc05 << QString::fromLatin1(d);
        break;
    case 'K':
        m_link += d;
        break;
    case 'G':
        for (int i = 0; i < 3; ++i) m_globals[i] = at(i) != 0;
        break;
    case 'P':
        for (int p = 0; p < NumPins && 4 * p + 3 < d.size(); ++p)
            m_pins[p] = Pin { at(4 * p), at(4 * p + 1), at(4 * p + 2), at(4 * p + 3) };
        break;
    default:
        break;
    }
    return false;
}
