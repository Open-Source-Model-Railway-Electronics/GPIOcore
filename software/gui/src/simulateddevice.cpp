#include "simulateddevice.h"
#include "protocol.h"

#include <QTimer>

using namespace Gpio;

SimulatedDevice::SimulatedDevice(QObject *parent) : SimulatedDevice(Options(), parent) {}

SimulatedDevice::SimulatedDevice(const Options &options, QObject *parent)
    : QIODevice(parent), m_opt(options)
{
    m_live.fill(-1);
    factoryDefaults();
}

SimulatedDevice::Options SimulatedDevice::Options::currentFirmware()
{
    Options o;
    o.protocol = 1;
    return o;
}

SimulatedDevice::Options SimulatedDevice::Options::oldFirmware()
{
    Options o = currentFirmware();
    o.legacyFirmware = true;
    o.doubleAck      = true;
    o.oldDefaults    = true;
    return o;
}

void SimulatedDevice::factoryDefaults()
{
    // Output::defaults(): defaultSettings with pinA = 3 + index (old: index), singleSteady, switchTime 50
    for (int i = 0; i < NumOutputs; ++i)
    {
        OutputConfig c;
        c.pinA = quint8(firstPin() + i);
        c.type = SingleSteady;
        c.setSwitchTime(50);
        m_out[i] = c.normalized();
    }
    // Input::defaults(): outputID = i, address = i + 1, latching, linked = 0xFF (old: NA = 0x3F)
    for (int i = 0; i < NumInputs; ++i)
    {
        InputConfig c;
        c.outputId = quint8(i);
        c.address  = quint16(i + 1);
        c.pin      = PinNA;
        c.type     = Latching;
        c.linked.fill(m_opt.oldDefaults ? PinNA : LinkNone);
        m_in[i] = c.normalized();
    }
}

bool SimulatedDevice::open(OpenMode mode)
{
    if (!QIODevice::open(mode | QIODevice::Unbuffered))
        return false;
    m_state = 0;
    m_rx.clear();
    m_tx.clear();
    QTimer::singleShot(m_opt.bootDelayMs, this, [this] { if (isOpen()) sendVersion(); });
    return true;
}

void SimulatedDevice::close()
{
    m_rx.clear();
    m_tx.clear();
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
    for (qint64 i = 0; i < size; ++i)
        receiveByte(quint8(data[i]));
    return size;
}

void SimulatedDevice::scheduleFlush()
{
    if (m_flushScheduled)
        return;
    m_flushScheduled = true;
    QTimer::singleShot(m_opt.replyDelayMs, this, [this] {
        m_flushScheduled = false;
        if (!isOpen() || m_tx.isEmpty())
            return;
        m_rx += m_tx;
        m_tx.clear();
        emit readyRead();
    });
}

// ── Transceiver (firmware side) ─────────────────────────────────────────────
void SimulatedDevice::sendMessage(quint8 command, const quint8 *payload)
{
    int len = command & 0x0F;
    if (len == 0x0F) len = payload[0];
    m_tx.append(char(command));
    quint8 cs = command;
    for (int i = 0; i < len; ++i)
    {
        m_tx.append(char(payload[i]));
        cs ^= payload[i];
    }
    m_tx.append(char(cs));
    scheduleFlush();
}

void SimulatedDevice::sendAck()
{
    m_tx.append(char(Proto::ACK));
    scheduleFlush();
}

void SimulatedDevice::sendNack(quint8 reason)
{
    m_tx.append(char(Proto::NACK));
    m_tx.append(char(reason));
    scheduleFlush();
}

void SimulatedDevice::sendVersion()
{
    // old firmware sent a placeholder, fixed firmware sends src/version.h
    const QByteArray v = m_opt.oldDefaults ? QByteArrayLiteral("12345678") : QByteArrayLiteral("v0.0.0");
    sendMessage(quint8(0x50 | v.size()), reinterpret_cast<const quint8 *>(v.constData()));
}

void SimulatedDevice::receiveByte(quint8 b)
{
    switch (m_state)
    {
    case 0:
        m_command  = b;
        m_len      = b & 0x0F;
        m_count    = 0;
        m_checksum = b;
        if (m_len == 0)
        {
            ++m_framesReceived;
            messageReceived();             // no ACK for zero-length messages
        }
        else if (m_len == 0x0F) m_state = 1;
        else                    m_state = 2;
        break;

    case 1:
        if (b > 32) { sendNack(Proto::NACK_INVALID_LENGTH); m_state = 0; break; }
        m_len = b;
        m_payload[m_count++] = b;
        m_checksum ^= b;
        m_state = 2;
        break;

    case 2:
        if (m_count >= 32) { sendNack(Proto::NACK_INVALID_LENGTH); m_state = 0; break; }
        m_payload[m_count++] = b;
        m_checksum ^= b;
        if (m_count == m_len) m_state = 3;
        break;

    case 3:
        ++m_framesReceived;
        if (b == m_checksum && m_corrupt == 0)
        {
            messageReceived();
            sendAck();
        }
        else
        {
            if (m_corrupt > 0) --m_corrupt;
            sendNack(Proto::NACK_INVALID_CHECKSUM);
        }
        m_state = 0;
        break;
    }
}

quint8 SimulatedDevice::outCmd() const
{
    return m_opt.legacyFirmware ? Proto::SET_OUTPUT_DATA_LEGACY : Proto::SET_OUTPUT_DATA;
}

// ── messageParser.cpp (firmware side) ───────────────────────────────────────
void SimulatedDevice::messageReceived()
{
    quint8 *p = m_payload.data();
    const quint8 index = p[0];

    if (m_command == Proto::SET_INPUT_DATA)
    {
        if (index >= NumInputs) return;                 // firmware: out of bounds write
        int idx; InputConfig c;
        decodeInput(QByteArray(reinterpret_cast<const char *>(p), 14), idx, c);
        if (m_opt.protocol >= 2)
            c.route = m_in[index].route;                // v2: routes come with SET_ROUTE
        m_in[index] = c.normalized();
        if (m_opt.doubleAck) sendAck();
    }
    else if (m_opt.protocol >= 2 && m_command == Proto::HELLO)
    {
        m_session = true;                               // keep D0/D1 as serial
        const quint8 reply[5] = { quint8(m_opt.protocol), quint8(m_opt.maxRouteSteps),
                                  quint8(m_opt.stepPool & 0xFF), quint8(m_opt.stepPool >> 8),
                                  m_opt.helloFlags };
        sendMessage(quint8(Proto::HELLO_GROUP << 4 | 5), reply);
    }
    else if (m_opt.protocol >= 2 && (m_opt.helloFlags & Proto::HELLO_FLAG_BOARD) && m_command == Proto::GET_BOARD)
    {
        const QByteArray body = encodeBoard(m_board);
        quint8 msg[32] = {};
        msg[0] = quint8(body.size() + 1);
        memcpy(msg + 1, body.constData(), size_t(body.size()));
        sendMessage(Proto::SET_BOARD, msg);
    }
    else if (m_opt.protocol >= 2 && (m_opt.helloFlags & Proto::HELLO_FLAG_BOARD) && m_command == Proto::SET_BOARD)
    {
        decodeBoard(QByteArray(reinterpret_cast<const char *>(p + 1), m_len - 1), m_board);
    }
    else if (m_opt.protocol >= 2 && m_command == Proto::SET_INPUT_STATE)
    {
        if (index < NumInputs) triggerInput(index, p[1]);
    }
    else if (m_opt.protocol >= 2 && m_command == Proto::GET_OUTPUT_STATE)
    {
        if (index >= NumOutputs) return;
        const quint32 now = quint32(m_clock.isValid() ? m_clock.elapsed() : 0);
        const quint8 msg[5] = { index, m_out[index].state, m_out[index].data[0], quint8(now), quint8(now >> 8) };
        sendMessage(Proto::OUTPUT_EVENT, msg);
    }
    else if (m_opt.protocol >= 2 && m_command == Proto::GET_ROUTE)
    {
        if (index >= NumInputs) return;
        const QVector<RouteStep> &r = m_in[index].route;
        const int first = p[1];
        const int n = qBound(0, int(r.size()) - first, Proto::RouteChunkSteps);
        quint8 msg[32] = {};
        msg[0] = quint8(4 + 4 * n);
        msg[1] = index;
        msg[2] = quint8(r.size());
        msg[3] = quint8(first);
        for (int i = 0; i < n; ++i)
            memcpy(msg + 4 + 4 * i, encodeStep(r[first + i]).constData(), 4);
        sendMessage(Proto::SET_ROUTE, msg);
    }
    else if (m_opt.protocol >= 2 && m_command == Proto::SET_ROUTE)
    {
        // p[0] = length, p[1] = route, p[2] = total steps, p[3] = first step
        const int route = p[1], total = qMin(int(p[2]), m_opt.maxRouteSteps), first = p[3];
        if (route >= NumInputs) return;
        QVector<RouteStep> &r = m_in[route].route;
        if (first == 0) r.clear();
        const int n = (m_len - 4) / 4;
        for (int i = 0; i < n && r.size() < total; ++i)
            r.append(decodeStep(reinterpret_cast<const char *>(p + 4 + 4 * i)));
        m_in[route].syncLinkedFromRoute();
    }
    else if (m_opt.protocol >= 2 && m_command == Proto::SERVO_LIVE)
    {
        if (index < NumOutputs) m_live[index] = p[1];
    }
    else if (m_command == Proto::GET_INPUT_DATA)
    {
        if (index >= NumInputs) return;
        const QByteArray enc = encodeInput(index, m_in[index]);
        memcpy(p, enc.constData(), 14);
        if (m_opt.protocol < 2)                           // v1: the stored linkedOutput[] bytes as they are
            for (int i = 0; i < MaxLinked; ++i) p[6 + i] = m_in[index].linked[i];
        sendMessage(Proto::SET_INPUT_DATA, p);
    }
    else if (m_command == outCmd())
    {
        if (index >= NumOutputs) return;
        // Firmware always reads payload[1..13]; with the legacy 13-byte opcode
        // payload[13] is whatever the previous message left in the buffer.
        int idx; OutputConfig c;
        decodeOutput(QByteArray(reinterpret_cast<const char *>(p), 14), idx, c, false);
        m_out[index] = c.normalized();
        if (m_opt.doubleAck) sendAck();
    }
    else if (m_command == Proto::GET_OUTPUT_DATA)
    {
        if (index >= NumOutputs) return;
        const QByteArray enc = encodeOutput(index, m_out[index], false);
        memcpy(p, enc.constData(), 14);
        sendMessage(outCmd(), p);
    }
    else if (m_command == Proto::REQ_VERSION)
    {
        sendVersion();
    }
    else if (m_command == Proto::SET_PRESET)
    {
        applyPreset(index);
    }
    // SET/GET_*_STATE: empty handlers in the firmware
}

// ── Presets, mirroring OPCODE_SET_PRESET ────────────────────────────────────
void SimulatedDevice::presetOutput(int i, quint8 pinA, quint8 pinB, quint8 rA, quint8 rB, quint8 rC,
                                   quint8 type, std::array<quint8, 4> data, quint32 switchTime, bool isTime)
{
    OutputConfig c = m_out[i];
    c.pinA = pinA; c.pinB = pinB;
    if (!m_opt.oldDefaults) { c.pinC = 0xFF; c.pinD = 0xFF; }   // old firmware left pinC / pinD untouched
    c.relayA = rA; c.relayB = rB; c.relayC = rC;
    c.type = type; c.state = 0;
    if (isTime) c.setSwitchTime(switchTime);
    else        c.data = data;
    c = c.normalized();
    m_out[i] = c;

    // 'payload = bitfield = 0xFF' yields the truncated bitfield value (0x3F).
    if (!m_opt.oldDefaults)
    {
        // Fixed firmware: same layout as GET_OUTPUT_DATA (little endian union)
        const QByteArray enc = encodeOutput(i, c, false);
        sendMessage(outCmd(), reinterpret_cast<const quint8 *>(enc.constData()));
        return;
    }

    // Old firmware: index pinA pinB rA rB rC type state d0..d3, switch time big endian
    quint8 msg[32] = {};
    int n = 0;
    msg[n++] = quint8(i);
    msg[n++] = c.pinA; msg[n++] = c.pinB; msg[n++] = c.relayA; msg[n++] = c.relayB; msg[n++] = c.relayC;
    msg[n++] = c.type; msg[n++] = 0;
    if (isTime)
    {
        msg[n++] = quint8(switchTime >> 24); msg[n++] = quint8(switchTime >> 16);
        msg[n++] = quint8(switchTime >> 8);  msg[n++] = quint8(switchTime);
    }
    else
    {
        for (quint8 b : data) msg[n++] = b;
    }
    sendMessage(outCmd(), msg);
}

void SimulatedDevice::setupInputs(int used)
{
    for (int i = 0; i < NumInputs; ++i)
    {
        InputConfig c = m_in[i];
        c.address  = (i < used) ? quint16(m_in[0].address + i) : 0xFFFF;
        c.pin      = 0xFF;
        // old firmware left unused inputs on latching -> output 15 (0xFF in a 4-bit field)
        c.type     = (i < used || m_opt.oldDefaults) ? Latching : InputNotInUse;
        c.outputId = (i < used) ? quint8(i) : 0xFF;
        c.linked.fill(0xFF);
        c.route.clear();
        m_in[i] = c.normalized();

        const QByteArray enc = encodeInput(i, m_in[i]);
        sendMessage(Proto::SET_INPUT_DATA, reinterpret_cast<const quint8 *>(enc.constData()));
    }
}

void SimulatedDevice::clearUnusedOutputs(int from)
{
    for (int i = from; i < NumOutputs; ++i)
        presetOutput(i, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, OutputNotInUse, { { 0, 0, 0, 0 } }, 0, false);
}

void SimulatedDevice::applyPreset(quint8 preset)
{
    const std::array<quint8, 4> servo { { 80, 100, 20, 1 } };
    const quint8 X = 0xFF;
    quint8 pin = firstPin();
    int count = 0;

    switch (preset)
    {
    case 0: count = 16; for (int i = 0; i < count; ++i) { quint8 a = pin++; presetOutput(i, a, X, X, X, X, ServoType, servo, 0, false); } setupInputs(16); break;
    case 1: count = 8;  for (int i = 0; i < count; ++i) { quint8 a = pin++, c = pin++; presetOutput(i, a, X, X, X, c, ServoType, servo, 0, false); } setupInputs(8); break;
    case 2: count = 5;  for (int i = 0; i < count; ++i) { quint8 a = pin++, ra = pin++, rb = pin++; presetOutput(i, a, X, ra, rb, X, ServoType, servo, 0, false); } setupInputs(5); break;
    case 3: count = 16; for (int i = 0; i < count; ++i) { quint8 a = pin++; presetOutput(i, a, X, X, X, X, SingleSteady, {}, 0, true); } setupInputs(16); break;
    case 4: count = 8;  for (int i = 0; i < count; ++i) { quint8 a = pin++, b = pin++; presetOutput(i, a, b, X, X, X, DoublePulse, {}, 50, true); } if (!m_opt.oldDefaults) setupInputs(8); break;   // old firmware: no setupInputs, clearUnusedOutputs(uninitialised i)
    case 5: count = 5;  for (int i = 0; i < count; ++i) { quint8 a = pin++, b = pin++, c = pin++; presetOutput(i, a, b, X, X, c, DoublePulse, {}, 50, true); } setupInputs(5); break;
    case 6: count = 4;  for (int i = 0; i < count; ++i) { quint8 a = pin++, b = pin++, ra = pin++, rb = pin++; presetOutput(i, a, b, ra, rb, X, DoublePulse, {}, 50, true); } setupInputs(4); break;
    case 7: count = 8;  for (int i = 0; i < count; ++i) { quint8 a = pin++, b = pin++; presetOutput(i, a, b, X, X, X, DoubleSteady, {}, 0, true); } setupInputs(8); break;
    default: return;
    }
    clearUnusedOutputs(count);
}

// ── Live test (SET_INPUT_STATE / OUTPUT_EVENT), mirrors GPIOcore.ino + input.cpp ──
void SimulatedDevice::reportOutput(int output, int state, int value)
{
    if (!m_session)
        return;
    if (!m_clock.isValid())
        m_clock.start();
    const quint32 now = quint32(m_clock.elapsed());
    const quint8 msg[5] = { quint8(output), quint8(state), quint8(value), quint8(now), quint8(now >> 8) };
    sendMessage(Proto::OUTPUT_EVENT, msg);
}

bool SimulatedDevice::routeMovesTurnout(int input) const
{
    for (const RouteStep &s : m_in[input].route)
    {
        const quint8 t = m_out[s.output].type;
        if (t == ServoType || t == SinglePulse || t == DoublePulse)
            return true;
    }
    return false;
}

void SimulatedDevice::triggerInput(int index, int direction)
{
    InputConfig &in = m_in[index];
    if (isDirectType(in.type))
    {
        OutputConfig &o = m_out[in.outputId];
        o.state = quint8(in.type == Latching ? (direction ? 1 : 0) : (o.state ^ 1));
        reportOutput(in.outputId, o.state, o.data[0]);
        return;
    }
    if (!isRouteType(in.type) || m_routeBusy[index] || in.route.isEmpty())
        return;
    // interlocking (PROTOCOL.md section 10): a refused route is ignored
    for (int i = 0; i < NumInputs; ++i)
    {
        if (!m_routeBusy[i]) continue;
        if (m_board.interlock == InterlockOn) return;
        if (m_board.interlock == InterlockAuto && routeMovesTurnout(i) && routeMovesTurnout(index)) return;
    }
    m_routeBusy[index] = true;
    runRouteStep(index, 0);
}

void SimulatedDevice::runRouteStep(int input, int step)
{
    const QVector<RouteStep> &r = m_in[input].route;
    if (step >= r.size())
    {
        m_routeBusy[input] = false;
        return;
    }
    const RouteStep &s = r[step];
    OutputConfig &o = m_out[s.output];
    if (s.raw)
    {
        reportOutput(s.output, 2, s.value);
    }
    else
    {
        o.state = quint8(s.value ? 1 : 0);
        reportOutput(s.output, o.state, o.data[0]);
    }
    const int waitMs = 100 * (m_board.timing ? s.delay : m_board.delay);
    QTimer::singleShot(waitMs, this, [this, input, step] { runRouteStep(input, step + 1); });
}
