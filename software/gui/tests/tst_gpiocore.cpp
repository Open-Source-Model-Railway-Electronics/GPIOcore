// Unit tests: protocol framing, config model and DeviceLink against the
// simulated board, which runs the real firmware (gpiocore_fw_host).

#include "boardtools.h"
#include "connections.h"
#include "devicelink.h"
#include "gpioconfig.h"
#include "protocol.h"
#include "simulateddevice.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QtTest>

using namespace Gpio;

Q_DECLARE_METATYPE(Gpio::InputConfig)
Q_DECLARE_METATYPE(Gpio::OutputConfig)

// A board with a few outputs and inputs in use (partial export / import)
static BoardConfig sampleBoard()
{
    BoardConfig b;                                   // everything not in use
    b.board.name = QStringLiteral("Station");
    b.board.type = BoardJackOfAllTrades;
    b.board.timing = true;
    b.outputs[0].type = ServoType; b.outputs[0].pinA = 3; b.outputs[0].name = QStringLiteral("Turnout");
    b.outputs[0].data = { { 70, 110, 20, 0 } };
    b.outputs[2].type = FourToneSignal; b.outputs[2].pinA = 4; b.outputs[2].pinB = 5;
    b.outputs[2].lampColors = QStringList{ "#ff0000", "#00ff00", "#0000ff", "#ffffff" };
    b.inputs[0].type = Latching; b.inputs[0].address = 7; b.inputs[0].outputId = 0; b.inputs[0].name = QStringLiteral("Button");
    b.inputs[3].type = Route; b.inputs[3].address = 9;
    b.inputs[3].route = { defaultStep(0, ServoType), defaultStep(2, FourToneSignal) };
    b.inputs[5].type = Latching; b.inputs[5].pin = 14; b.inputs[5].analog = AnalogAt; b.inputs[5].threshold = 512;
    b.inputs[5].outputId = 2; b.inputs[5].setCustomValues(true, FourToneSignal);
    return b;
}

// A device that answers everything except HELLO: no GPIOcore board (or very old firmware)
class NoHelloDevice : public SimulatedDevice
{
public:
    using SimulatedDevice::SimulatedDevice;

protected:
    qint64 writeData(const char *data, qint64 size) override
    {
        if (size == 1 && quint8(data[0]) == Proto::HELLO)
            return size;
        return SimulatedDevice::writeData(data, size);
    }
};

// A board whose HELLO reply has no 'analog + sound' flag (firmware before analog inputs): the
// flags byte of the reply is rewritten on its way to the host.
class NoAnalogDevice : public SimulatedDevice
{
public:
    using SimulatedDevice::SimulatedDevice;

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 n = SimulatedDevice::readData(data, maxSize);
        for (qint64 i = 0; i + 6 < n; ++i)
            if (quint8(data[i]) == (Proto::HELLO_GROUP << 4 | 5) && quint8(data[i + 1]) == 2)
            {
                const quint8 old = quint8(data[i + 5]);
                data[i + 5] = char(old & ~Proto::HELLO_FLAG_ANALOG_SOUND);
                data[i + 6] = char(quint8(data[i + 6]) ^ old ^ quint8(data[i + 5]));   // checksum
                break;
            }
        return n;
    }
};

// Three lamps A green, B yellow, C red (lamp D not used), aspects: own block occupied = red,
// next block occupied = yellow, free = green
static OutputConfig makeBlockSignal(int own, int next, quint8 a, quint8 b, quint8 c)
{
    OutputConfig s;
    s.type = BlockSignal;
    s.pinA = a; s.pinB = b; s.pinC = c;
    s.data = { { 0x04, 0x02, 0x01, 0 } };
    s.setBlockInputs(own, next);
    return s;
}

class TestGpioCore : public QObject
{
    Q_OBJECT

private:
    struct Rig
    {
        DeviceLink       *link = nullptr;
        SimulatedDevice  *sim  = nullptr;
        QVector<OutputConfig> outputs = QVector<OutputConfig>(NumOutputs);
        QVector<InputConfig>  inputs  = QVector<InputConfig>(NumInputs);
        QVector<QVector<RouteStep>> routes = QVector<QVector<RouteStep>>(NumInputs);
        int outputsSeen = 0, inputsSeen = 0, routesSeen = 0;
        int errors = 0;

        ~Rig() { delete link; }   // owns the simulator; drops the lambdas capturing this Rig
    };

    // Link + simulated board with short timings, connected and version received.
    void makeRig(Rig &r, SimulatedDevice *device = nullptr)
    {
        r.link = new DeviceLink(this);
        r.link->bootWaitMs = 200;
        r.link->timeoutMs  = 100;
        r.link->drainMs    = 3;
        r.sim = device ? device : new SimulatedDevice;
        connect(r.link, &DeviceLink::outputReceived, this, [&r](int i, const OutputConfig &c) { r.outputs[i] = c; ++r.outputsSeen; });
        connect(r.link, &DeviceLink::inputReceived,  this, [&r](int i, const InputConfig &c)  { r.inputs[i]  = c; ++r.inputsSeen;  });
        connect(r.link, &DeviceLink::errorOccurred,  this, [&r](const QString &) { ++r.errors; });
        connect(r.link, &DeviceLink::routeReceived,  this, [&r](int i, const QVector<RouteStep> &s) { r.routes[i] = s; ++r.routesSeen; });

        QSignalSpy ready(r.link, &DeviceLink::ready);
        QString err;
        QVERIFY2(r.link->openDevice(r.sim, false, &err), qPrintable(err));   // the firmware is silent until asked
        QVERIFY(ready.wait(1000));
        QCOMPARE(ready.first().first().toString(), QStringLiteral("v0.0.0"));
        waitFinished(r);                 // HELLO
        QVERIFY(r.link->caps().known);
    }

    void waitFinished(Rig &r, int timeoutMs = 3000)
    {
        QSignalSpy done(r.link, &DeviceLink::finished);
        if (r.link->isBusy())
            QVERIFY(done.wait(timeoutMs));
    }

private slots:
    void initTestCase()
    {
        qRegisterMetaType<Gpio::InputConfig>();
        qRegisterMetaType<Gpio::OutputConfig>();
    }

    // ── Framing ─────────────────────────────────────────────────────────────
    void encodeAddsChecksum()
    {
        const QByteArray f = Proto::encode(Proto::GET_INPUT_DATA, QByteArray(1, 5));
        QCOMPARE(Proto::toHex(f), QStringLiteral("11 05 14"));
    }

    void encodeZeroLengthHasNoChecksum()
    {
        QCOMPARE(Proto::encode(Proto::REQ_VERSION).size(), 1);
    }

    void parserHandlesAckNackAndFrames()
    {
        Proto::FrameParser p;
        Proto::FrameParser::Result r;
        QByteArray bytes;
        bytes.append(char(0x70));                         // ACK
        bytes.append(char(0x71)); bytes.append(char(2));  // NACK invalid length
        bytes += Proto::encode(0x58, "12345678");          // version
        bytes.append(char(0x70));
        p.feed(bytes.left(5), r);                         // split mid-frame
        p.feed(bytes.mid(5), r);
        QCOMPARE(r.errors.size(), 0);
        QCOMPARE(r.frames.size(), 4);
        QVERIFY(r.frames[0].isAck());
        QVERIFY(r.frames[1].isNack());
        QCOMPARE(r.frames[1].byte(0), quint8(2));
        QCOMPARE(r.frames[2].payload, QByteArray("12345678"));
        QVERIFY(r.frames[3].isAck());
        QVERIFY(p.idle());
    }

    void parserDetectsChecksumError()
    {
        Proto::FrameParser p;
        Proto::FrameParser::Result r;
        QByteArray f = Proto::encode(Proto::GET_INPUT_DATA, QByteArray(1, 5));
        f[2] = char(f[2] ^ 1);
        p.feed(f, r);
        QCOMPARE(r.frames.size(), 0);
        QCOMPARE(r.errors.size(), 1);
    }

    void parserExtendedLength()
    {
        QByteArray payload;
        payload.append(char(4)); payload.append("abc");   // length byte counts itself
        const QByteArray f = Proto::encode(0x9F, payload);
        QCOMPARE(f.size(), 1 + 4 + 1);
        Proto::FrameParser p;
        Proto::FrameParser::Result r;
        p.feed(f, r);
        QCOMPARE(r.frames.size(), 1);
        QCOMPARE(r.frames[0].payload, payload);
    }

    // ── Model ───────────────────────────────────────────────────────────────
    void switchTimeIsLittleEndian()
    {
        OutputConfig c;
        c.setSwitchTime(0x01020304);
        QCOMPARE(c.data[0], quint8(0x04));
        QCOMPARE(c.data[3], quint8(0x01));
        QCOMPARE(c.switchTime(), quint32(0x01020304));
    }

    void outputRoundTrip()
    {
        OutputConfig c;
        c.type = ServoType; c.pinA = 9; c.relayC = 4; c.state = 1;
        c.data = { { 70, 110, 15, 2 } };
        const QByteArray p = encodeOutput(3, c);
        QCOMPARE(p.size(), 14);
        int idx = -1; OutputConfig d;
        QVERIFY(decodeOutput(p, idx, d));
        QCOMPARE(idx, 3);
        QVERIFY(d == c);
        QVERIFY(!d.invertFrog);
        c.invertFrog = true;                       // state byte bit1, bit0 stays the state
        const QByteArray q = encodeOutput(3, c);
        QCOMPARE(quint8(q.at(9)), quint8(0x03));
        QVERIFY(decodeOutput(q, idx, d));
        QVERIFY(d.invertFrog);
        QCOMPARE(int(d.state), 1);
        QVERIFY(d == c);
        c.state = 0;
        QVERIFY(decodeOutput(encodeOutput(3, c), idx, d));
        QCOMPARE(int(d.state), 0);
        QVERIFY(d.invertFrog);
    }

    void inputBitfieldsTruncate()
    {
        InputConfig c;
        c.address = 0xFFFF; c.pin = 0xFF; c.outputId = 0xFF; c.type = Route;
        const InputConfig n = c.normalized();
        QCOMPARE(n.address, AddrNone);
        QCOMPARE(n.pin, PinNA);
        QCOMPARE(n.outputId, quint8(15));          // 'no output' is not representable
    }

    void jsonRoundTrip()
    {
        BoardConfig b;
        b.outputs[2].type = LightEffect; b.outputs[2].pinA = 5; b.outputs[2].pinB = 6;
        b.outputs[2].setLightEffect(LightSwing);
        b.outputs[2].setSwing(40, 120);
        b.inputs[1].type = Route; b.inputs[1].address = 12;
        RouteStep a; a.output = 2; a.value = 1;
        RouteStep c2; c2.output = 7; c2.value = 120; c2.raw = true; c2.delay = 16383;
        b.inputs[1].route = { a, c2 };
        b.outputs[3].type = ServoType; b.outputs[3].relayC = 8; b.outputs[3].invertFrog = true;
        const QByteArray json = QJsonDocument(b.toJson()).toJson();
        BoardConfig c; QString err;
        QVERIFY(BoardConfig::fromJson(QJsonDocument::fromJson(json).object(), c, &err));
        QVERIFY(c.outputs[2] == b.outputs[2]);
        QVERIFY(c.inputs[1] == b.inputs[1]);
        QVERIFY(c.outputs[3].invertFrog);
        QVERIFY(c.outputs[3] == b.outputs[3]);
        // only written when set: older files (without the key) load as 'not inverted'
        QVERIFY(b.outputs[3].toJson().value(QStringLiteral("invertFrog")).toBool());
        QVERIFY(!b.outputs[2].toJson().contains(QStringLiteral("invertFrog")));
        QVERIFY(!OutputConfig::fromJson(b.outputs[2].toJson()).invertFrog);
    }

    // ── Device link vs simulator ────────────────────────────────────────────
    void readAllFromFixedFirmware()
    {
        Rig r; makeRig(r);
        r.link->readAll();
        waitFinished(r);
        QCOMPARE(r.errors, 0);
        QCOMPARE(r.inputsSeen, 16);
        QCOMPARE(r.outputsSeen, 16);
        QCOMPARE(r.outputs[0].pinA, quint8(3));     // F7: D0..D2 are skipped
        QCOMPARE(r.outputs[15].pinA, quint8(19));          // D3-D12, A0-A5: D13 is the config LED
        QCOMPARE(r.outputs[7].switchTime(), quint32(50));
    }

    void writeAndReadBackFixedFirmware()
    {
        Rig r; makeRig(r);
        OutputConfig c;
        c.type = ServoType; c.pinA = 10; c.relayA = 11; c.relayB = 12;
        c.data = { { 60, 120, 25, 3 } };
        r.link->writeOutput(5, c);
        r.link->readOutput(5);
        waitFinished(r);
        QCOMPARE(r.errors, 0);
        QVERIFY(r.outputs[5] == c);
        QVERIFY(r.sim->output(5) == c);
    }

    void nackIsRetried()
    {
        Rig r; makeRig(r);
        r.sim->corruptNextFrames(1);
        r.link->readInput(2);
        waitFinished(r);
        QCOMPARE(r.errors, 0);
        QCOMPARE(r.inputsSeen, 1);
    }

    // ── Connections ─────────────────────────────────────────────────────────
    void connectDirectInput()
    {
        BoardConfig b;
        b.inputs[2].type = InputNotInUse;
        QVERIFY(!inputConnected(b.inputs[2]));
        QCOMPARE(connectOutput(b, 2, 5), ConnectResult::Connected);
        QCOMPARE(int(b.inputs[2].type), int(Latching));       // not in use -> absolute
        QCOMPARE(b.inputs[2].outputId, quint8(5));
        QCOMPARE(connectOutput(b, 2, 5), ConnectResult::AlreadyConnected);
        QCOMPARE(connectOutput(b, 2, 7), ConnectResult::Replaced);   // one output only
        QCOMPARE(outputSources(b, 7), QVector<int>({ 2 }));
        QVERIFY(outputSources(b, 5).isEmpty());
        Gpio::disconnect(b, 2, -1);
        QCOMPARE(int(b.inputs[2].type), int(InputNotInUse));
    }

    void connectRouteInput()
    {
        BoardConfig b;
        b.inputs[1].type = Route;
        for (int o = 0; o < 15; ++o)
            QCOMPARE(connectOutput(b, 1, o), ConnectResult::Connected);
        QCOMPARE(connectOutput(b, 1, 3), ConnectResult::Connected);   // the same output again is allowed
        QCOMPARE(connectOutput(b, 1, 9), ConnectResult::RouteFull);   // 16 steps
        QCOMPARE(routeSteps(b.inputs[1]).size(), 16);
        QCOMPARE(int(routeSteps(b.inputs[1]).first().delay), int(DefaultStepDelay));   // 0.3 s

        Gpio::disconnect(b, 1, 0);                                   // remove step 1, rest shifts up
        QCOMPARE(routeSteps(b.inputs[1]).first().output, 1);
        toggleStepState(b, 1, 0);
        QCOMPARE(int(routeSteps(b.inputs[1]).first().value), 0);
        moveStep(b, 1, 0, +1);
        QCOMPARE(routeSteps(b.inputs[1])[1].output, 1);
        QCOMPARE(int(routeSteps(b.inputs[1])[1].value), 0);
    }

    void routeLimitsAndPool()
    {
        BoardConfig b;
        b.inputs[1].type = Route;
        RouteLimits eight; eight.maxSteps = 8;              // a board with 8 steps per route
        for (int o = 0; o < 8; ++o)
            QCOMPARE(connectOutput(b, 1, o, eight), ConnectResult::Connected);
        QCOMPARE(connectOutput(b, 1, 9, eight), ConnectResult::RouteFull);

        RouteLimits pool; pool.pool = 9;
        b.inputs[2].type = Route;
        QCOMPARE(connectOutput(b, 2, 0, pool), ConnectResult::Connected);   // 9th step on the board
        QCOMPARE(connectOutput(b, 2, 1, pool), ConnectResult::PoolFull);
    }

    void stepValueDependsOnOutputType()
    {
        BoardConfig b;
        b.outputs[4].type = FourToneSignal;
        b.outputs[5].type = ServoType;
        b.inputs[0].type = Route;
        connectOutput(b, 0, 4);
        connectOutput(b, 0, 5);
        QVERIFY(b.inputs[0].route[0].raw);                  // signal: aspect number
        QVERIFY(!b.inputs[0].route[1].raw);                 // servo: curved / straight by default
        QCOMPARE(stepKind(ServoType), StepKind::ServoStateOrAngle);
    }

    void stepEncoding()
    {
        RouteStep s; s.output = 13; s.value = 200; s.raw = true; s.delay = MaxStepDelay;
        const QByteArray b = encodeStep(s);
        QCOMPARE(Proto::toHex(b), QStringLiteral("1D C8 FF 3F"));
        QVERIFY(decodeStep(b.constData()) == s);
    }

    void removedRecorderAndFeedback()
    {
        // input type 5 (feedback) is reserved: from the wire or an old file = not in use
        for (quint8 type : { quint8(5) })
        {
            InputConfig c;
            c.type = Momentary; c.pin = 8; c.outputId = 2;
            QByteArray p = encodeInput(7, c);
            p[4] = char(type);
            int index = -1;
            InputConfig d;
            QVERIFY(decodeInput(p, index, d));
            QCOMPARE(int(d.type), int(InputNotInUse));
        }
        QCOMPARE(int(InputConfig::fromJson(QJsonObject{ { "type", "recorder" }, { "pin", 20 } }).type), int(InputNotInUse));   // 4 was the recorder
        QCOMPARE(int(InputConfig::fromJson(QJsonObject{ { "type", "feedback" }, { "pin", 8 } }).type), int(InputNotInUse));
        BoardSettings b;
        const char board[3] = { 1, 1, 3 };               // Nano, flags bit0 set, delay 3
        QVERIFY(decodeBoard(QByteArray(board, 3), b));   // flags bit0 (was 'recorder on') is ignored
        QCOMPARE(int(quint8(encodeBoard(b).at(1))), 0);
        QVERIFY(pinReservedReason(18).isEmpty() && pinReservedReason(19).isEmpty());   // A4/A5 always usable
    }

    void sensorInputModel()
    {
        // input type 4 = sensor (occupancy detector): no address, output or route
        QCOMPARE(int(Sensor), 4);
        QVERIFY(!inputTypeInfo(Sensor).usesAddress && !inputTypeInfo(Sensor).usesOutput && !inputTypeInfo(Sensor).usesRoute);
        QCOMPARE(inputTypeName(Sensor), QStringLiteral("Sensor (occupancy)"));
        InputConfig c;
        c.type = Sensor; c.pin = 0;                      // D0: e.g. the next board's block via an optocoupler
        const QByteArray p = encodeInput(5, c);
        QCOMPARE(int(quint8(p.at(3))), 0);
        QCOMPARE(int(quint8(p.at(4))), 4);
        int index = -1;
        InputConfig d;
        QVERIFY(decodeInput(p, index, d));
        QCOMPARE(int(d.type), int(Sensor));
        QCOMPARE(c.normalized().type, quint8(Sensor));
        QCOMPARE(c.toJson()["type"].toString(), QStringLiteral("sensor"));
        QVERIFY(InputConfig::fromJson(c.toJson()) == c);
        InputConfig a;                                   // analog sensor: a condition on A1
        a.type = Sensor; a.analog = AnalogAbove; a.pin = 15; a.threshold = 600;
        QVERIFY(InputConfig::fromJson(a.toJson()) == a);
        QVERIFY(!inputConnected(c));                     // no wire of its own
        BoardConfig board;
        board.inputs[2] = c;
        QCOMPARE(connectOutput(board, 2, 4, RouteLimits()), ConnectResult::NotConnectable);
        QVERIFY(board.inputs[2] == c);
    }

    void blockSignalModel()
    {
        // output type 15: data[0] / data[1] / data[2] = aspect when its own block / the next block is occupied /
        // both are free, data[3] = own block input | next block input << 4 (equal = no next block)
        QCOMPARE(int(BlockSignal), 15);
        QCOMPARE(QString::fromUtf8(outputTypeInfo(BlockSignal).key), QStringLiteral("blockSignal"));
        QCOMPARE(outputTypeInfo(BlockSignal).pinCount, 4);
        OutputConfig s;
        s.type = BlockSignal; s.data = { { 0x04, 0x02, 0x01, 0 } };
        s.setBlockInputs(3, 9);
        QCOMPARE(int(s.data[3]), 0x93);
        QCOMPARE(s.blockOwn(), 3);
        QCOMPARE(s.blockNext(), 9);
        QCOMPARE(s.watchedInputs(), (QVector<int>{ 3, 9 }));
        const QByteArray p = encodeOutput(6, s);
        QCOMPARE(Proto::toHex(p.mid(8)), QStringLiteral("0F 00 04 02 01 93"));   // type, state, data[0-3]
        int idx = -1; OutputConfig d;
        QVERIFY(decodeOutput(p, idx, d));
        QVERIFY(d == s);
        QVERIFY(OutputConfig::fromJson(s.toJson()) == s);
        QCOMPARE(s.toJson()["type"].toString(), QStringLiteral("blockSignal"));
        QCOMPARE(int(s.normalized().type), 15);
        s.setBlockInputs(5, -1);                         // no next block: stored as next = own
        QCOMPARE(int(s.data[3]), 0x55);
        QCOMPARE(s.blockNext(), -1);
        QCOMPARE(s.watchedInputs(), QVector<int>{ 5 });

        // not a target: inputs, routes and DCC cannot set it
        BoardConfig board;
        board.outputs[2] = s;
        QCOMPARE(connectOutput(board, 0, 2, RouteLimits()), ConnectResult::NotConnectable);
        QCOMPARE(int(board.inputs[0].type), int(InputNotInUse));
        board.inputs[1].type = Route;
        QCOMPARE(connectOutput(board, 1, 2, RouteLimits()), ConnectResult::NotConnectable);
        QVERIFY(board.inputs[1].route.isEmpty());

        // a board file with the removed automatic mode of the 4-aspect signal: a manual signal
        QJsonObject old = OutputConfig().toJson();
        old["type"] = QStringLiteral("fourToneSignal");
        old["data"] = QJsonArray{ 1, 10, 0x93, 0xC6 };
        const OutputConfig manual = OutputConfig::fromJson(old);
        QCOMPARE(int(manual.type), int(FourToneSignal));
        QCOMPARE(manual.data, (std::array<quint8, 4>{ { 1, 10, 0, 0 } }));
        QVERIFY(manual.watchedInputs().isEmpty());

        // servo as semaphore arm: data[3] = 0x80 | 0x40 (swap) | detector input
        OutputConfig servo;
        servo.type = ServoType; servo.data = { { 80, 100, 20, 0 } };
        QCOMPARE(servo.servoFollows(), -1);
        QVERIFY(servo.watchedInputs().isEmpty());
        servo.data[3] = 0x80 | 0x40 | 7;
        QCOMPARE(servo.servoFollows(), 7);
        QVERIFY(servo.servoFollowSwap());
        QCOMPARE(servo.watchedInputs(), QVector<int>{ 7 });
        servo.type = Pwm;                                // only a servo follows a detector
        QCOMPARE(servo.servoFollows(), -1);
    }

    void dccNextPins()
    {
        boardOptions().dccNext = true;
        QCOMPARE(dccNextPort(3), 1);
        QCOMPARE(dccNextPort(12), 10);
        QCOMPARE(dccNextPort(14), 11);
        QCOMPARE(dccNextPort(19), 16);
        QVERIFY(!pinReservedReason(13).isEmpty());          // D13 is not on a DCCnext
        boardOptions().dccNext = false;
        QVERIFY(!pinReservedReason(13).isEmpty());          // config LED on every board
        QVERIFY(!pinReservedReason(21).isEmpty());          // A7 = config button
        boardOptions().serialPinsUsable = false;
        QVERIFY(!pinReservedReason(0).isEmpty());
        boardOptions().serialPinsUsable = true;
        QVERIFY(pinReservedReason(0).isEmpty());
        QVERIFY(!pinNote(0).isEmpty());
    }

    void dccNextNamesAndPinRules()
    {
        // DCCnext: the ports P1-P16 printed on the board are the names the user sees; nothing else is usable
        setBoardType(BoardDccNext);
        QCOMPARE(pinName(3), QStringLiteral("P1"));
        QCOMPARE(pinName(12), QStringLiteral("P10"));
        QCOMPARE(pinName(14), QStringLiteral("P11"));
        QCOMPARE(pinName(19), QStringLiteral("P16"));
        QCOMPARE(pinLabel(3), QStringLiteral("P1 (D3)"));
        QCOMPARE(pinLabel(19), QStringLiteral("P16 (A5)"));
        QCOMPARE(arduinoPinName(3), QStringLiteral("D3"));
        QCOMPARE(pinName(13), QStringLiteral("D13"));                 // not a port: its Arduino name
        QCOMPARE(pwmPinNames(), QStringList({ "P1", "P3", "P4", "P7", "P8", "P9" }));
        for (quint8 p : { 0, 1, 2, 13, 20, 21 })
            QVERIFY2(!pinReservedReason(p).isEmpty(), qPrintable(pinName(p)));
        for (quint8 p = 3; p <= 19; ++p)
            QCOMPARE(pinReservedReason(p).isEmpty(), p != 13);
        // Jack of all trades: no A6 / A7, D-names
        setBoardType(BoardJackOfAllTrades);
        QCOMPARE(pinName(3), QStringLiteral("D3"));
        QCOMPARE(pinLabel(3), QStringLiteral("D3"));
        QVERIFY(!pinReservedReason(20).isEmpty() && !pinReservedReason(21).isEmpty());
        QVERIFY(pinReservedReason(0).isEmpty() && pinReservedReason(12).isEmpty());
        // Uno (type 0, old files and boards): no longer offered, it is a Nano
        setBoardType(BoardUno);
        QCOMPARE(int(boardOptions().type), int(BoardNano));
        QVERIFY(pinReservedReason(20).isEmpty());
        BoardSettings uno;
        QVERIFY(decodeBoard(QByteArray("\x00\x08\x03", 3), uno));
        QCOMPARE(int(uno.type), int(BoardNano));
        QJsonObject j = BoardSettings().toJson();
        j["type"] = QStringLiteral("uno");
        QCOMPARE(int(BoardSettings::fromJson(j).type), int(BoardNano));
        // Nano: A6 analog input, A7 the config button
        setBoardType(BoardNano);
        QVERIFY(pinReservedReason(20).isEmpty());
        QCOMPARE(pinReservedReason(21), QStringLiteral("config button"));
        QCOMPARE(pinName(14), QStringLiteral("A0"));
    }

    // ── Partial export / import ─────────────────────────────────────────────
    void exportOnlyInUseItems()
    {
        const BoardConfig b = sampleBoard();
        const BoardConfig outs = exportPart(b, PartOutputs);
        QCOMPARE(int(outs.board.type), int(BoardJackOfAllTrades));   // the type always: it gives the pins their meaning
        QVERIFY(outs.board.name.isEmpty() && !outs.board.timing);
        QVERIFY(outs.outputs[0] == b.outputs[0] && outs.outputs[0].name == QStringLiteral("Turnout"));
        QCOMPARE(outs.outputs[2].lampColors, b.outputs[2].lampColors);
        for (const InputConfig &i : outs.inputs)
            QCOMPARE(int(i.type), int(InputNotInUse));
        const BoardConfig all = exportPart(b, PartOutputs | PartInputs | PartBoard);
        QVERIFY(all.board == b.board);
        QVERIFY(all.inputs[3].route == b.inputs[3].route);
        QVERIFY(all.inputs[5].isAnalog() && all.inputs[5].hasCustomValues());
        QCOMPARE(all.inputs[0].name, QStringLiteral("Button"));
        // a normal board file: the other slots are written as 'notInUse'
        const QJsonObject json = exportPart(b, PartInputs).toJson();
        BoardConfig back;
        QVERIFY(BoardConfig::fromJson(json, back, nullptr));
        QCOMPARE(json["outputs"].toArray().at(0).toObject()["type"].toString(), QStringLiteral("notInUse"));
        QVERIFY(back.inputs[5] == b.inputs[5]);
        QCOMPARE(int(back.outputs[0].type), int(OutputNotInUse));
    }

    void importSameNumbers()
    {
        const BoardConfig file = sampleBoard();
        BoardConfig mine;                                // the user's board: output 2 and input 3 in use
        mine.outputs[2].type = SingleSteady; mine.outputs[2].pinA = 9;
        mine.inputs[3].type = Momentary;
        const ImportResult plan = planImport(mine, file, PartOutputs | PartInputs, true);
        QVERIFY(plan.error.isEmpty());
        QCOMPARE(plan.outputs.value(0), 0);
        QCOMPARE(plan.outputs.value(2), 2);
        QCOMPARE(plan.outputs.value(1), -1);             // not in use in the file
        QCOMPARE(plan.inputs.value(3), 3);
        QVERIFY(importedInput(file, 3, plan).route == file.inputs[3].route);   // same numbers: nothing renumbered
        QVERIFY(planImport(mine, file, PartInputs, true).outputs.count(-1) == NumOutputs);   // outputs not ticked

        // a file without outputs in use (e.g. a resistor ladder): ticking Outputs touches none of mine
        BoardConfig ladder;
        ladder.inputs[0].type = Route;
        for (bool same : { true, false })
        {
            const ImportResult p = planImport(mine, ladder, PartOutputs | PartInputs, same);
            QVERIFY(p.error.isEmpty());
            QVERIFY(p.outputs.count(-1) == NumOutputs);
        }
    }

    void importIntoFreeSlotsRenumbers()
    {
        BoardConfig file;                                // a level crossing: 2 detectors, a signal, a route
        file.outputs[0].type = ServoType; file.outputs[0].pinA = 3;
        file.outputs[0].data = { { 80, 100, 20, 0x80 | 1 } };             // semaphore arm: follows input 1
        file.outputs[1].type = BlockSignal; file.outputs[1].pinA = 4;
        file.outputs[1].setBlockInputs(0, 1);
        file.inputs[0].type = Sensor; file.inputs[0].pin = 5;
        file.inputs[1].type = Sensor; file.inputs[1].pin = 6;
        file.inputs[2].type = Route;
        file.inputs[2].route = { defaultStep(0, ServoType), defaultStep(1, BlockSignal), defaultStep(9, SingleSteady) };
        file.inputs[3].type = Latching; file.inputs[3].outputId = 0;
        file.inputs[3].setCustomValues(true, ServoType);

        BoardConfig mine;                                // outputs 0-2 and inputs 0-1 in use, output 3 driven
        for (int o = 0; o < 3; ++o) { mine.outputs[o].type = SingleSteady; mine.outputs[o].pinA = quint8(7 + o); }
        mine.inputs[0].type = Latching; mine.inputs[0].outputId = 0;
        mine.inputs[1].type = Route; mine.inputs[1].route = { defaultStep(3, OutputNotInUse) };
        const ImportResult plan = planImport(mine, file, PartOutputs | PartInputs, false);
        QVERIFY(plan.error.isEmpty());
        QCOMPARE(plan.outputs.value(0), 4);              // output 3 is not in use, but a route drives it
        QCOMPARE(plan.outputs.value(1), 5);
        QCOMPARE(plan.inputs.mid(0, 4), QVector<int>({ 2, 3, 4, 5 }));

        const OutputConfig arm = importedOutput(file, 0, plan);
        QCOMPARE(arm.servoFollows(), 3);                 // input 1 -> 3
        const OutputConfig sig = importedOutput(file, 1, plan);
        QCOMPARE(sig.blockOwn(), 2);
        QCOMPARE(sig.blockNext(), 3);
        const InputConfig route = importedInput(file, 2, plan);
        QCOMPARE(route.route.size(), 3);
        QCOMPARE(route.route[0].output, 4);
        QCOMPARE(route.route[1].output, 5);
        QCOMPARE(route.route[2].output, 9);              // output 9 was not imported: the user's own
        const InputConfig direct = importedInput(file, 3, plan);
        QCOMPARE(int(direct.outputId), 4);
        QVERIFY(direct.hasCustomValues());
        QCOMPARE(direct.route[0].output, 4);             // the custom values follow their output

        // only the inputs: their references stay on the user's own outputs
        const ImportResult inputsOnly = planImport(mine, file, PartInputs, false);
        QCOMPARE(importedInput(file, 2, inputsOnly).route[0].output, 0);
        QCOMPARE(int(inputsOnly.outputs.count(-1)), NumOutputs);

        // not enough free slots: an error and nothing planned
        for (int i = 2; i < NumInputs - 1; ++i) mine.inputs[i].type = Momentary;
        const ImportResult full = planImport(mine, file, PartOutputs | PartInputs, false);
        QVERIFY(full.error.contains(QLatin1String("Not enough free slots")));
        QVERIFY(full.inputs.count(-1) == NumInputs && full.outputs.count(-1) == NumOutputs);
    }

    void oldNextStepsKeyIgnored()
    {
        // board files of the former new board wizard have a "nextSteps" list: it is ignored
        QJsonObject json = sampleBoard().toJson();
        json["nextSteps"] = QJsonArray{ QJsonObject{ { "text", "Try it in Simulate" }, { "done", true } } };
        BoardConfig back;
        QVERIFY(BoardConfig::fromJson(json, back, nullptr));
        QVERIFY(back.outputs == sampleBoard().outputs);
        QVERIFY(!back.toJson().contains(QLatin1String("nextSteps")));
    }

    // ── Protocol v2 ─────────────────────────────────────────────────────────
    void helloV2Capabilities()
    {
        Rig r; makeRig(r);
        QCOMPARE(r.link->caps().protocol, 2);
        QCOMPARE(r.link->caps().maxRouteSteps, 16);
        QCOMPARE(r.link->caps().stepPool, (1024 - 262) / 2);   // the EEPROM pool, timing off: 2 B per step
        QVERIFY(r.link->caps().servoLive);
        QVERIFY(r.sim->configSession());
        QCOMPARE(r.errors, 0);
    }

    void boardSettingsRoundTrip()
    {
        BoardSettings b;
        b.name = QStringLiteral("Station/left:1234567890");   // path characters dropped, max 16
        b.type = BoardDccNext; b.timing = false; b.interlock = InterlockOn; b.delay = 7;
        b.blockBlink = BoardSettings::blockBlinkFromHz(1.5);   // 90 a minute: flags bits 4-7 = 6
        QCOMPARE(int(b.blockBlink), 6);
        QCOMPARE(int(quint8(encodeBoard(b).at(1))), 0x60 | (InterlockOn << 2));
        QCOMPARE(int(BoardSettings::blockBlinkFromHz(1.0)), 0);   // 1 Hz: the default, as older firmware
        QCOMPARE(int(BoardSettings::blockBlinkFromHz(9.0)), 15);
        BoardSettings d;
        QVERIFY(decodeBoard(encodeBoard(b), d));
        QCOMPARE(d.blockBlinkHz(), 1.5);
        QCOMPARE(d.blockBlinkHalfMs(), 333);
        QCOMPARE(d.name, QStringLiteral("Stationleft12345"));
        QCOMPARE(int(d.type), int(BoardDccNext));
        QVERIFY(!d.timing);
        QCOMPARE(int(d.interlock), int(InterlockOn));
        QCOMPARE(int(d.delay), 7);
        QCOMPARE(BoardSettings::fromJson(d.toJson()), d);

        Rig r; makeRig(r);
        QVERIFY(r.link->caps().boardSettings);
        BoardSettings got;
        QObject::connect(r.link, &DeviceLink::boardReceived, r.link, [&got](const BoardSettings &x) { got = x; });
        r.link->writeBoard(d);
        r.link->readBoard();
        waitFinished(r);
        QCOMPARE(r.errors, 0);
        QCOMPARE(r.sim->board(), d);
        QCOMPARE(got, d);
    }

    void namesStayInTheFile()
    {
        BoardConfig b;
        b.outputs[2].name = QStringLiteral("Turnout left");
        b.inputs[5].name  = QStringLiteral("Button A");
        b.outputs[3].lampColors = { QStringLiteral("#ff0000"), QStringLiteral("#00ff00") };
        b.board.name = QStringLiteral("Test");
        BoardConfig c;
        QVERIFY(BoardConfig::fromJson(b.toJson(), c, nullptr));
        QCOMPARE(c.outputs[2].name, b.outputs[2].name);
        QCOMPARE(c.inputs[5].name, b.inputs[5].name);
        QCOMPARE(c.outputs[3].lampColors, b.outputs[3].lampColors);
        QCOMPARE(c.board.name, b.board.name);
        // names are not part of the board data: a named output equals the board's copy
        OutputConfig plain = b.outputs[2]; plain.name.clear();
        QVERIFY(plain == b.outputs[2]);
    }

    void helloTimeoutIsNoBoard()
    {
        DeviceLink link;
        link.bootWaitMs = 200;
        link.timeoutMs  = 100;
        link.drainMs    = 3;
        link.announceWaitMs = 300;
        QSignalSpy closed(&link, &DeviceLink::closed);
        QSignalSpy errors(&link, &DeviceLink::errorOccurred);
        QSignalSpy noBoard(&link, &DeviceLink::noBoard);
        QSignalSpy waiting(&link, &DeviceLink::waitingForBoard);
        QString err;
        QVERIFY(link.openDevice(new NoHelloDevice, true, &err));
        QVERIFY(waiting.wait(3000));                        // no HELLO reply: first wait for an announce
        QVERIFY(link.isOpen() && link.isWaitingForBoard());
        QCOMPARE(errors.size(), 0);
        QVERIFY(closed.wait(3000));                         // none came: not a GPIOcore board, link closed
        QCOMPARE(errors.size(), 1);
        QTRY_COMPARE(noBoard.size(), 1);                    // the GUI shows a clear message for this one
        QVERIFY(!link.isOpen());
        QVERIFY(!link.isWaitingForBoard());
        QVERIFY(!link.caps().known);
    }

    void disconnectCancelsTheWait()
    {
        DeviceLink link;
        link.timeoutMs  = 100;
        link.drainMs    = 3;
        link.announceWaitMs = 400;
        QSignalSpy noBoard(&link, &DeviceLink::noBoard);
        QSignalSpy waiting(&link, &DeviceLink::waitingForBoard);
        QString err;
        QVERIFY(link.openDevice(new NoHelloDevice, false, &err));
        QVERIFY(waiting.wait(3000));
        link.close();
        QTest::qWait(700);
        QCOMPARE(noBoard.size(), 0);
        QVERIFY(!link.isWaitingForBoard());
    }

    // The board ran for more than 5 s without the GUI: D0/D1 are I/O, its serial port is closed. A short press
    // on the config button (A7) opens it again for 5 s and the board announces itself (the HELLO reply, unasked).
    void shortPressOpensTheSerialPort()
    {
        SimulatedDevice sim;
        InputConfig d0;                                      // D0 in use: the port closes after the window
        d0.type = Sensor; d0.pin = 0;
        sim.setInput(0, d0);
        QVERIFY(sim.open(QIODevice::ReadWrite));
        sim.advanceClock(6000);
        QTRY_VERIFY(sim.serialIsIO());
        sim.write(QByteArray(1, char(Proto::HELLO)));       // lost: the port is closed
        QTest::qWait(100);
        QVERIFY(sim.readAll().isEmpty());
        QVERIFY(!sim.configSession());

        auto shortPress = [&sim] {
            sim.setAnalogValue(7, 0);
            QTest::qWait(150);
            sim.setAnalogValue(7, AnalogMaxValue);
        };
        shortPress();
        QByteArray rx;
        QTRY_VERIFY((rx += sim.readAll()).size() >= 7);
        QCOMPARE(quint8(rx.at(0)), quint8(Proto::HELLO_GROUP << 4 | 5));    // the announce
        QVERIFY(quint8(rx.at(5)) & Proto::HELLO_FLAG_BLUETOOTH);
        QVERIFY(!sim.serialIsIO());
        QVERIFY(!sim.configMode());                          // a short press, not config mode
        QVERIFY(!sim.configSession());                       // the announce starts no session

        sim.advanceClock(5100);                              // no HELLO within 5 s: D0/D1 are I/O again
        QTRY_VERIFY(sim.serialIsIO());

        shortPress();                                        // again, now the GUI answers
        rx.clear();
        QTRY_VERIFY((rx += sim.readAll()).size() >= 7);
        sim.write(QByteArray(1, char(Proto::HELLO)));
        QTRY_VERIFY(sim.configSession());
        sim.advanceClock(6000);
        QTest::qWait(100);
        QVERIFY(!sim.serialIsIO());                          // the session keeps the port
    }

    // Connecting to a board whose port is closed (over Bluetooth, nothing resets it): the link waits for the
    // announce instead of giving up, then signs on as usual.
    void connectWaitsForTheAnnounce()
    {
        auto *sim = new SimulatedDevice;
        InputConfig d0;                                      // D0 in use: the port closes after the window
        d0.type = Sensor; d0.pin = 0;
        sim->setInput(0, d0);
        QVERIFY(sim->open(QIODevice::ReadWrite));
        sim->advanceClock(6000);
        QTRY_VERIFY(sim->serialIsIO());
        DeviceLink link;
        link.timeoutMs = 100;
        link.drainMs   = 3;
        link.announceWaitMs = 5000;
        QSignalSpy waiting(&link, &DeviceLink::waitingForBoard);
        QSignalSpy noBoard(&link, &DeviceLink::noBoard);
        QSignalSpy errors(&link, &DeviceLink::errorOccurred);
        QString err;
        QVERIFY(link.openDevice(sim, false, &err));
        QVERIFY(waiting.wait(3000));
        QVERIFY(link.isWaitingForBoard() && !link.isReady());
        sim->setAnalogValue(7, 0);                           // short press on the config button
        QTest::qWait(150);
        sim->setAnalogValue(7, AnalogMaxValue);
        QTRY_VERIFY(link.caps().known);
        QVERIFY(link.isReady() && !link.isWaitingForBoard());
        QVERIFY(link.caps().bluetooth);
        QTRY_VERIFY(sim->configSession());
        QCOMPARE(noBoard.size(), 0);
        QCOMPARE(errors.size(), 0);
    }

    // GPIOcore.ino serialCheck(): after the window the serial port only closes when the configuration uses D0/D1
    // (an input, output or relay pin, or a sound effect on TX). Checked every 50 ms, so a new configuration counts.
    void serialStaysOpenWithoutD0D1()
    {
        SimulatedDevice sim;
        QVERIFY(sim.open(QIODevice::ReadWrite));
        sim.advanceClock(6000);
        QTest::qWait(200);
        QVERIFY(!sim.serialIsIO());                          // factory board: D0/D1 unused, the port stays open

        auto shortPress = [&sim] {
            sim.setAnalogValue(7, 0);
            QTest::qWait(150);
            sim.setAnalogValue(7, AnalogMaxValue);
            QTRY_VERIFY(!sim.serialIsIO());
            QByteArray announce;
            QTRY_VERIFY((announce += sim.readAll()).size() >= 7);
        };
        InputConfig d0;
        d0.type = Sensor; d0.pin = 0;
        sim.setInput(0, d0);                                 // the configuration now uses D0
        QTRY_VERIFY(sim.serialIsIO());

        shortPress();
        sim.setInput(0, SimulatedDevice::factoryDefaults().inputs[0]);
        OutputConfig sound;
        sound.type = SoundEffect; sound.pinA = SoundPin;
        sim.setOutput(5, sound);
        QTest::qWait(200);
        QVERIFY(!sim.serialIsIO());                          // the window still runs
        sim.advanceClock(5100);
        QTRY_VERIFY(sim.serialIsIO());                       // the DFPlayer needs TX

        shortPress();
        sim.setOutput(5, SimulatedDevice::factoryDefaults().outputs[5]);
        sim.advanceClock(5100);
        QTest::qWait(200);
        QVERIFY(!sim.serialIsIO());                          // nothing on D0/D1 any more: it stays open
        sim.write(QByteArray(1, char(Proto::HELLO)));        // a GUI connects later, without a reset
        QByteArray rx;
        QTRY_VERIFY((rx += sim.readAll()).size() >= 7);
        QCOMPARE(quint8(rx.at(0)), quint8(Proto::HELLO_GROUP << 4 | 5));
        QTRY_VERIFY(sim.configSession());
    }

    // BT_SETUP: the board finds the HC-05 (AT mode) at one of five baud rates and sets 115200 baud and its name.
    // The noise it makes at the other rates is ignored.
    void bluetoothSetupFindsTheHc05()
    {
        Rig r; makeRig(r);
        QVERIFY(r.link->caps().bluetooth);
        BoardSettings b;
        b.name = QStringLiteral("Yard 2 (east)");
        r.sim->setBoard(b);
        r.sim->attachFakeHc05(38400);                        // AT mode: always 38400
        QSignalSpy result(r.link, &DeviceLink::bluetoothResult);
        r.link->bluetoothSetup();
        QVERIFY(r.link->isBusy());
        QVERIFY(result.wait(5000));
        QCOMPARE(result.first().at(0).toInt(), 0);
        QCOMPARE(result.first().at(1).toInt(), 38400);
        QVERIFY(result.first().at(2).toBool());
        QTRY_COMPARE(r.sim->hc05Commands(),
                     (QStringList{ "AT", "AT+UART=115200,0,0", "AT+NAME=GPIOcore-Yard2east" }));
        QCOMPARE(r.errors, 0);
        r.link->readOutput(0);                               // the session goes on at 115200
        waitFinished(r);
        QCOMPARE(r.errors, 0);
        QVERIFY(r.sim->configSession());
    }

    void bluetoothSetupOtherBaudRate()
    {
        Rig r; makeRig(r);
        r.sim->attachFakeHc05(57600);                        // tried third, after noise at 38400 and 9600
        QSignalSpy result(r.link, &DeviceLink::bluetoothResult);
        r.link->bluetoothSetup();
        QVERIFY(result.wait(5000));
        QCOMPARE(result.first().at(0).toInt(), 0);
        QCOMPARE(result.first().at(1).toInt(), 57600);
        QTRY_COMPARE(r.sim->hc05Commands(), (QStringList{ "AT", "AT+UART=115200,0,0", "AT+NAME=GPIOcore" }));
        QCOMPARE(r.errors, 0);
    }

    void bluetoothSetupWithoutHc05()
    {
        Rig r; makeRig(r);
        QSignalSpy result(r.link, &DeviceLink::bluetoothResult);
        r.link->bluetoothSetup();
        QVERIFY(result.wait(5000));
        QCOMPARE(result.first().at(0).toInt(), 1);           // no HC-05 answered
        QCOMPARE(result.first().at(1).toInt(), 0);
        QVERIFY(!result.first().at(2).toBool());
        QVERIFY(r.sim->hc05Commands().isEmpty());
        r.link->readOutput(0);
        waitFinished(r);
        QCOMPARE(r.errors, 0);
    }

    void routeRoundTripV2()
    {
        Rig r; makeRig(r);
        BoardSettings timed; timed.timing = true;
        r.sim->setBoard(timed);                             // keep per-step wait times
        InputConfig in;
        in.type = Route; in.address = 40;
        for (int i = 0; i < 16; ++i)
        {
            RouteStep s;
            s.output = i % 16;
            s.raw    = (i % 3 == 0);
            s.value  = s.raw ? quint8(10 + i * 10) : quint8(i & 1);
            s.delay  = quint16(3 + i * 1000);
            in.route.append(s);
        }
        r.link->writeInput(3, in);
        r.link->writeRoute(3, in.route);                    // 3 chunks: 7 + 7 + 2
        r.link->readInput(3);
        waitFinished(r);
        QCOMPARE(r.errors, 0);
        QCOMPARE(r.sim->input(3).route, in.route);
        QCOMPARE(r.routes[3], in.route);
    }

    void servoLiveV2()
    {
        Rig r; makeRig(r);
        OutputConfig servo = r.sim->output(2);               // SERVO_LIVE moves servos only
        servo.type = ServoType;
        servo.data = { { 60, 120, 20, 0 } };
        r.sim->setOutput(2, servo);
        for (int a = 60; a <= 120; a += 5)
            r.link->servoLive(2, a);                         // a burst, as from a slider
        waitFinished(r);
        QCOMPARE(r.sim->servoPosition(2), 120);
        QCOMPARE(r.errors, 0);
    }

    void directInputsColourFirst()
    {
        BoardConfig b;
        for (auto &in : b.inputs) in.type = InputNotInUse;
        b.inputs[4].type = Route;
        connectOutput(b, 4, 3);
        b.inputs[9].type = InputNotInUse;
        connectOutput(b, 9, 3);
        QCOMPARE(outputSources(b, 3), QVector<int>({ 9, 4 })); // direct before route
        QCOMPARE(connectOutput(b, 9, 3), ConnectResult::AlreadyConnected);
        QCOMPARE(connectOutput(b, NumInputs, 3), ConnectResult::NotConnectable);   // no such input
    }

    // ── Analog inputs and the sound effect output ──────────────────────────
    void analogInputEncoding()
    {
        InputConfig c;
        c.type      = Latching;
        c.analog    = AnalogAt;
        c.pin       = 14 + 3;                                   // A3
        c.threshold = 515;
        QCOMPARE(int(c.pinByte()), 0x20 | 2 << 3 | 3);
        const QByteArray p = encodeInput(5, c);
        QCOMPARE(int(quint8(p.at(3))), 0x33);
        QCOMPARE(int(quint8(p.at(6))), 512 / 4);               // rounded to a step of 8, sent / 4
        int idx = -1;
        InputConfig d;
        QVERIFY(decodeInput(p, idx, d));
        QCOMPARE(idx, 5);
        QCOMPARE(int(d.analog), int(AnalogAt));
        QCOMPARE(int(d.pin), 17);
        QCOMPARE(int(d.threshold), 512);
        QVERIFY(d.route.isEmpty());                             // the route comes with GET_ROUTE
        QVERIFY(d == c.normalized());

        c.analog = AnalogBelow; c.pin = 14;
        QCOMPARE(int(c.pinByte()), 0x20);
        c.analog = AnalogAbove; c.pin = 20;
        QCOMPARE(int(c.pinByte()), 0x20 | 1 << 3 | 6);
        QCOMPARE(InputConfig::roundThreshold(1023), 1016);
        QCOMPARE(InputConfig::roundThreshold(3), 0);
        QCOMPARE(InputConfig::roundThreshold(4), 8);
        QCOMPARE(InputConfig::roundThreshold(-5), 0);
        c.threshold = 2000;
        QCOMPARE(int(c.normalized().threshold), 1016);

        d.setPinByte(PinNA);                                    // NA and mode 3 are never analog
        QVERIFY(!d.isAnalog());
        QCOMPARE(int(d.pin), int(PinNA));
        d.setPinByte(0x3A);
        QVERIFY(!d.isAnalog());
        d.setPinByte(7);
        QVERIFY(!d.isAnalog());
        QCOMPARE(int(d.pin), 7);

        // a digital input sends 0xFF in payload 6, payload 7-13 are always 0xFF; its threshold does not matter
        InputConfig e;
        e.type  = Route;
        e.pin   = 5;
        e.route = { RouteStep{ 4, 1, false, 3 } };
        const QByteArray ep = encodeInput(0, e);
        QCOMPARE(ep.size(), 14);
        for (int i = 6; i < 14; ++i)
            QCOMPARE(int(quint8(ep.at(i))), 0xFF);
        for (int i = 7; i < 14; ++i)
            QCOMPARE(int(quint8(p.at(i))), 0xFF);
        InputConfig f = e;
        f.threshold = 100;
        QVERIFY(f == e);
        f.analog = AnalogBelow;                                 // pin 5 cannot be analog
        QVERIFY(!f.normalized().isAnalog());
    }

    void analogAndSoundJson()
    {
        BoardConfig b;
        b.inputs[2].type      = Momentary;
        b.inputs[2].analog    = AnalogBelow;
        b.inputs[2].pin       = 15;                             // A1
        b.inputs[2].threshold = 296;
        b.inputs[3].type      = Latching;
        b.inputs[3].pin       = 7;
        b.outputs[1].type     = SoundEffect;
        b.outputs[1].pinA     = SoundPin;
        b.outputs[1].data     = { { 3, 4, 0, 0 } };
        const QJsonObject j = b.toJson();
        const QJsonObject in2 = j["inputs"].toArray().at(2).toObject();
        QVERIFY(in2["pin"].isNull());
        QCOMPARE(in2["analog"].toObject()["mode"].toString(), QStringLiteral("below"));
        QCOMPARE(in2["analog"].toObject()["channel"].toInt(), 1);
        QCOMPARE(in2["analog"].toObject()["threshold"].toInt(), 296);
        const QJsonObject in3 = j["inputs"].toArray().at(3).toObject();
        QVERIFY(!in3.contains("analog"));                       // digital: as before
        QCOMPARE(in3["pin"].toInt(), 7);
        QCOMPARE(j["outputs"].toArray().at(1).toObject()["type"].toString(), QStringLiteral("soundEffect"));

        BoardConfig back;
        QString err;
        QVERIFY(BoardConfig::fromJson(QJsonDocument::fromJson(QJsonDocument(j).toJson()).object(), back, &err));
        QVERIFY(back.inputs[2] == b.inputs[2]);
        QCOMPARE(int(back.inputs[2].analog), int(AnalogBelow));
        QCOMPARE(int(back.inputs[2].pin), 15);
        QVERIFY(!back.inputs[3].isAnalog());
        QCOMPARE(int(back.inputs[3].pin), 7);
        QVERIFY(back.outputs[1] == b.outputs[1]);
        QCOMPARE(stepKind(SoundEffect), StepKind::Track);
        const RouteStep s = defaultStep(1, SoundEffect);
        QVERIFY(s.raw);
        QCOMPARE(int(s.value), 1);
    }

    void simulatorAnalogSharedChannel()
    {
        Rig r; makeRig(r);
        QVERIFY(r.link->caps().analogSound);
        auto analog = [](quint8 type, quint8 mode, int threshold, int output) {
            InputConfig c;
            c.type = type; c.analog = mode; c.pin = 14; c.threshold = quint16(threshold);
            c.outputId = quint8(output); c.address = AddrNone;
            return c;
        };
        // four inputs on A0, like a resistor ladder plus a sensor. A0 starts at 700, where no
        // condition holds (at 1023 'above 800' would hold from the start: no edge, nothing happens).
        r.sim->setAnalogValue(0, 700);
        r.sim->setInput(0, analog(Latching,  AnalogBelow, 512, 0));
        r.sim->setInput(1, analog(Momentary, AnalogAt,    296, 1));
        InputConfig route = analog(Route, AnalogAbove, 800, 0);
        route.route = { RouteStep{ 2, 1, false, 0 } };
        r.sim->setInput(2, route);
        r.sim->setInput(3, analog(Latching, AnalogAbove, 800, 3));
        for (int o : { 0, 3 })
        {
            OutputConfig out = r.sim->output(o);
            out.state = 1;
            r.sim->setOutput(o, out);
        }
        QSignalSpy events(r.link, &DeviceLink::outputEvent);
        QTest::qWait(150);                       // the inputs read 'released' first (their first edge only arms them)
        // the firmware samples every 50 ms and wants two equal samples: a change counts after 50-100 ms
        const int settle = 250;

        r.sim->setAnalogValue(0, 1000);          // above 800: the route and input 3
        QTRY_COMPARE(int(r.sim->output(2).state), 1);
        QTRY_COMPARE(int(r.sim->output(3).state), 0);    // pressed = LOW, like a button
        QCOMPARE(int(r.sim->output(0).state), 1);    // not below 512
        r.sim->setAnalogValue(0, 790);           // still above 800 - 20
        QTest::qWait(settle);
        QCOMPARE(int(r.sim->output(3).state), 0);
        r.sim->setAnalogValue(0, 779);           // released
        QTRY_COMPARE(int(r.sim->output(3).state), 1);

        r.sim->setAnalogValue(0, 300);           // 'at 296' and 'below 512'
        QTRY_COMPARE(int(r.sim->output(1).state), 1);    // toggled
        QTRY_COMPARE(int(r.sim->output(0).state), 0);
        r.sim->setAnalogValue(0, 520);           // below 512 + 20: still pressed
        QTest::qWait(settle);
        QCOMPARE(int(r.sim->output(0).state), 0);
        r.sim->setAnalogValue(0, 540);           // released
        QTRY_COMPARE(int(r.sim->output(0).state), 1);
        r.sim->setAnalogValue(0, 505);           // pressed again (no hysteresis on the way in)
        QTRY_COMPARE(int(r.sim->output(0).state), 0);
        r.sim->setAnalogValue(0, 290);           // 'at' window: 286-306
        QTRY_COMPARE(int(r.sim->output(1).state), 0);    // toggled back
        r.sim->setAnalogValue(1, 1000);          // another channel: nothing on A0 changes
        QTest::qWait(settle);
        QCOMPARE(int(r.sim->output(0).state), 0);
        QCOMPARE(int(r.sim->output(1).state), 0);
        QTRY_VERIFY(events.size() >= 6);         // the board reports every change

        // live reading: a background request, no progress or 'finished'
        QSignalSpy values(r.link, &DeviceLink::analogValue);
        QSignalSpy done(r.link, &DeviceLink::finished);
        r.link->readAnalog(0);
        r.link->readAnalog(0);                   // merged with the first one
        QVERIFY(!r.link->isBusy());
        QVERIFY(values.wait(1000));
        QCOMPARE(values.first().at(0).toInt(), 0);
        QCOMPARE(values.first().at(1).toInt(), 290);
        QTest::qWait(50);
        QCOMPARE(values.size(), 1);
        QCOMPARE(done.size(), 0);
        QCOMPARE(r.errors, 0);
    }

    void analogNeedsHelloFlag()
    {
        Rig r; makeRig(r, new NoAnalogDevice);   // firmware before analog inputs
        QVERIFY(!r.link->caps().analogSound);
    }

    void soundOutputEvents()
    {
        Rig r; makeRig(r);
        OutputConfig snd;
        snd.type = SoundEffect;
        snd.pinA = SoundPin;
        r.sim->setOutput(5, snd);
        InputConfig toggle;
        toggle.type = Momentary; toggle.address = 50; toggle.outputId = 5;
        r.sim->setInput(5, toggle);
        InputConfig route;
        route.type = Route; route.address = 51;
        route.route = { RouteStep{ 5, 9, true, 1 }, RouteStep{ 5, 0, true, 1 } };
        r.sim->setInput(6, route);
        InputConfig absolute;                    // custom values: off = track 3, on = track 7
        absolute.type = Latching; absolute.address = 52; absolute.outputId = 5;
        absolute.setCustomValues(true, SoundEffect);
        absolute.route[0].value = 3;
        absolute.route[1].value = 7;
        r.sim->setInput(7, absolute);

        QSignalSpy events(r.link, &DeviceLink::outputEvent);
        auto seen = [&events](int o, int state, int value) {
            for (const QList<QVariant> &e : events)
                if (e.at(0).toInt() == o && e.at(1).toInt() == state && e.at(2).toInt() == value)
                    return true;
            return false;
        };
        r.link->triggerInput(5, 1);              // no custom values: on = track 1
        QTRY_VERIFY_WITH_TIMEOUT(seen(5, 2, 1), 2000);
        events.clear();
        QTest::qWait(550);                       // a toggle input ignores its address for 500 ms (DCC repeats)
        r.link->triggerInput(5, 1);              // off = stop
        QTRY_VERIFY_WITH_TIMEOUT(seen(5, 2, 0), 2000);
        r.link->triggerInput(7, 1);              // the input chooses the track
        QTRY_VERIFY_WITH_TIMEOUT(seen(5, 2, 7), 2000);
        r.link->triggerInput(7, 0);
        QTRY_VERIFY_WITH_TIMEOUT(seen(5, 2, 3), 2000);
        events.clear();
        r.link->triggerInput(6, 1);              // route: track 9, then stop
        QTRY_VERIFY_WITH_TIMEOUT(seen(5, 2, 9), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(seen(5, 2, 0), 2000);
        r.link->dccExt(50, 12);                  // DCC extended: the value is the track
        QTRY_VERIFY_WITH_TIMEOUT(seen(5, 2, 12), 2000);
        waitFinished(r);
        QCOMPARE(r.errors, 0);
    }

    // ── Custom values of toggle / absolute inputs ──────────────────────────
    void customValuesModel()
    {
        InputConfig c;
        c.type = Momentary; c.outputId = 3;
        c.setCustomValues(true, ServoType);
        QVERIFY(c.hasCustomValues());
        QCOMPARE(c.route.size(), 2);
        QVERIFY(!c.route[0].raw && c.route[0].value == 0);      // curved / straight to start with
        QVERIFY(!c.route[1].raw && c.route[1].value == 1);
        c.route[0].raw = true; c.route[0].value = 45;
        c.route[1].raw = true; c.route[1].value = 135;
        const QByteArray step = encodeStep(c.route[0]);
        QCOMPARE(int(quint8(step.at(0))), 0x03 | 0x10 | 0x20);  // id | raw | STEP_VALUES
        QVERIFY(decodeStep(step.constData()) == c.route[0]);

        BoardConfig b;
        b.inputs[2] = c;
        const QJsonObject j = b.toJson();
        const QJsonObject in = j["inputs"].toArray().at(2).toObject();
        QCOMPARE(in["values"].toArray().size(), 2);
        QCOMPARE(in["values"].toArray().at(1).toObject()["value"].toInt(), 135);
        QVERIFY(in["route"].toArray().isEmpty());
        BoardConfig back;
        QString err;
        QVERIFY(BoardConfig::fromJson(j, back, &err));
        QVERIFY(back.inputs[2].hasCustomValues());
        QVERIFY(back.inputs[2] == c);
        QCOMPARE(totalRouteSteps(back), 2);                      // the pool counts them

        InputConfig d = c;                                       // another output: the steps follow
        d.outputId = 7;
        d.syncCustomValues();
        QCOMPARE(d.route[0].output, 7);
        QCOMPARE(d.route[1].output, 7);
        d.type = Route;                                          // not a direct input: no values
        d.syncCustomValues();
        QVERIFY(d.route.isEmpty());
        InputConfig e;                                           // a direct input never keeps a stale route
        e.type = Latching;
        e.route = { RouteStep{ 4, 1, false, 3 } };
        e.syncCustomValues();
        QVERIFY(e.route.isEmpty());

        InputConfig snd;
        snd.type = Latching; snd.outputId = 1;
        snd.setCustomValues(true, SoundEffect);                  // off = stop, on = track 1
        QVERIFY(snd.route[0].raw && snd.route[0].value == 0);
        QVERIFY(snd.route[1].raw && snd.route[1].value == 1);
        InputConfig pwm;
        pwm.type = Latching;
        pwm.setCustomValues(true, Pwm);
        QCOMPARE(int(pwm.route[0].value), 0);
        QCOMPARE(int(pwm.route[1].value), 255);

        BoardConfig s;                                           // dragging onto a sound output: values on
        s.outputs[4].type = SoundEffect;
        s.inputs[0].type = Latching;
        connectOutput(s, 0, 4);
        QVERIFY(s.inputs[0].hasCustomValues());
        QCOMPARE(s.inputs[0].route[1].output, 4);
    }

    void simulatorCustomValues()
    {
        Rig r; makeRig(r);
        OutputConfig servo;
        servo.type = ServoType; servo.pinA = 3;
        servo.data = { { 80, 100, 1, 0 } };
        r.sim->setOutput(3, servo);
        InputConfig toggle;
        toggle.type = Momentary; toggle.address = 30; toggle.outputId = 3;
        r.sim->setInput(3, toggle);
        InputConfig values = toggle;                             // sent with SET_ROUTE, like the GUI does
        values.setCustomValues(true, ServoType);
        values.route[0].raw = true; values.route[0].value = 45;
        values.route[1].raw = true; values.route[1].value = 135;
        r.link->writeRoute(3, values.route);
        waitFinished(r);
        QVERIFY(r.sim->input(3).hasCustomValues());
        r.link->readRoute(3);
        waitFinished(r);
        QCOMPARE(r.routes[3].size(), 2);
        QVERIFY(r.routes[3][0].values && r.routes[3][1].values);

        QSignalSpy events(r.link, &DeviceLink::outputEvent);
        auto seen = [&events](int o, int state, int value) {
            for (const QList<QVariant> &e : events)
                if (e.at(0).toInt() == o && e.at(1).toInt() == state && e.at(2).toInt() == value)
                    return true;
            return false;
        };
        auto last = [&events]() { return events.isEmpty() ? -1 : events.last().at(2).toInt(); };
        r.link->triggerInput(3, 1);              // toggle 0 -> 1: on = 135 degrees (the servo sweeps there)
        QTRY_COMPARE_WITH_TIMEOUT(last(), 135, 5000);
        QTest::qWait(550);                       // DCC lockout of a toggle input: 500 ms
        r.link->triggerInput(3, 1);              // 1 -> 0: off = 45 degrees
        QTRY_COMPARE_WITH_TIMEOUT(last(), 45, 5000);
        QTest::qWait(50);

        OutputConfig pwm;                        // no fading: the new value is reported as one PWM step
        pwm.type = Pwm; pwm.pinA = 5;
        r.sim->setOutput(6, pwm);
        events.clear();
        InputConfig absolute;                    // absolute + DCC command
        absolute.type = Latching; absolute.address = 40; absolute.outputId = 6;
        absolute.setCustomValues(true, Pwm);
        absolute.route[0].raw = true; absolute.route[0].value = 60;
        absolute.route[1].raw = true; absolute.route[1].value = 120;
        r.sim->setInput(4, absolute);
        r.link->triggerInput(4, 1);
        QTRY_VERIFY_WITH_TIMEOUT(seen(6, 2, 120), 3000);
        r.link->triggerInput(4, 0);
        QTRY_VERIFY_WITH_TIMEOUT(seen(6, 2, 60), 3000);

        // a button press (analog condition) uses them too
        InputConfig analog = absolute;
        analog.address = AddrNone; analog.analog = AnalogBelow; analog.pin = 14; analog.threshold = 512;
        analog.route[0].value = 70;              // pressed = LOW = state 0
        r.sim->setInput(5, analog);
        events.clear();
        QTest::qWait(150);                       // released first: the input's first edge only arms it
        r.sim->setAnalogValue(0, 100);
        QTRY_VERIFY_WITH_TIMEOUT(seen(6, 2, 70), 3000);

        // leftover steps without the marker are ignored: the plain state goes to the output
        OutputConfig light = r.sim->output(0);
        light.state = 1;
        r.sim->setOutput(0, light);
        InputConfig stale;
        stale.type = Latching; stale.address = 90; stale.outputId = 0;
        stale.route = { RouteStep{ 0, 1, false, 0 }, RouteStep{ 0, 1, false, 0 } };
        r.sim->setInput(9, stale);
        r.link->triggerInput(9, 0);
        waitFinished(r);
        QCOMPARE(int(r.sim->output(0).state), 0);
        QCOMPARE(r.errors, 0);
    }

    void pwmFadeTime()
    {
        OutputConfig c;
        c.type = Pwm; c.pinA = 5;
        c.data = { { 200, 30, 0, 0 } };                          // brightness 200, 30 s for 0 -> 255
        QCOMPARE(int(c.pwmFade()), 30);
        int idx = -1;
        OutputConfig d;
        QVERIFY(decodeOutput(encodeOutput(4, c), idx, d));
        QCOMPARE(int(d.data[1]), 30);
        QVERIFY(d == c);
        QVERIFY(OutputConfig::fromJson(c.toJson()) == c);

        // the board keeps the fade time
        Rig r; makeRig(r);
        r.link->writeOutput(4, c);
        r.link->readOutput(4);
        waitFinished(r);
        QCOMPARE(int(r.outputs[4].data[1]), 30);

        // a new brightness fades there step by step; every step is reported as OUTPUT_EVENT state 2
        // (skipped when the serial buffer is full, never the last one), not the target at once
        c.data[1] = 2;                                           // 2 s for 0 -> 255: about 8 ms per step
        r.link->writeOutput(4, c);
        waitFinished(r);
        QTRY_COMPARE_WITH_TIMEOUT(r.sim->servoPosition(4), 200, 5000);   // faded up to 200 first
        InputConfig route;
        route.type = Route; route.address = 60;
        route.route = { RouteStep{ 4, 90, true, 0 } };
        r.sim->setInput(8, route);
        QSignalSpy events(r.link, &DeviceLink::outputEvent);
        r.link->triggerInput(8, 1);
        QTRY_VERIFY_WITH_TIMEOUT(!events.isEmpty() && events.last().at(0).toInt() == 4
                                 && events.last().at(1).toInt() == 2 && events.last().at(2).toInt() == 90, 5000);
        QVector<int> steps;
        for (const QList<QVariant> &e : events)
            if (e.at(0).toInt() == 4 && e.at(1).toInt() == 2) steps << e.at(2).toInt();
        QVERIFY2(steps.size() > 10, qPrintable(QString::number(steps.size())));   // 110 steps, a few may be skipped
        QVERIFY(steps.first() < 200);
        for (int k = 1; k < steps.size(); ++k)
            QVERIFY(steps[k] < steps[k - 1] && steps[k] >= 90);  // downwards, ending at the target
        QCOMPARE(steps.last(), 90);
        QCOMPARE(int(r.sim->output(4).data[1]), 2);
        QCOMPARE(r.errors, 0);
    }

    // ── The simulated board is the real firmware ───────────────────────────
    void firmwareFactoryDefaults()
    {
        // the GUI's offline defaults are what a blank board gets from Output / Input::defaults()
        Rig r; makeRig(r);
        r.link->readAll();
        waitFinished(r);
        QCOMPARE(r.errors, 0);
        const BoardConfig d = SimulatedDevice::factoryDefaults();
        for (int i = 0; i < NumOutputs; ++i)
        {
            QVERIFY2(r.outputs[i] == d.outputs[i], qPrintable(QStringLiteral("output %1").arg(i)));
            QVERIFY(r.sim->output(i) == d.outputs[i]);
        }
        for (int i = 0; i < NumInputs; ++i)
        {
            QVERIFY2(r.inputs[i] == d.inputs[i], qPrintable(QStringLiteral("input %1").arg(i)));
            QVERIFY(r.sim->input(i) == d.inputs[i]);
        }
        QCOMPARE(r.sim->board(), BoardSettings());
        QCOMPARE(int(r.sim->pin(13).mode), 1);               // D13 = config LED: an output, on
        QCOMPARE(int(r.sim->pin(13).level), 1);
        QVERIFY(!r.sim->serialIsIO());                       // HELLO came within the boot window
    }

    void firmwareDccLockout()
    {
        // input.cpp: a toggle input takes the first of the repeated DCC packets, the same address
        // is ignored for DCC_LOCKOUT_MS (500 ms)
        Rig r; makeRig(r);
        InputConfig toggle = r.sim->input(4);                // DCC address 5 -> output 4
        toggle.type = Momentary;
        r.sim->setInput(4, toggle);
        QCOMPARE(int(r.sim->output(4).state), 0);
        for (int k = 0; k < 4; ++k)
            r.sim->dccAccessory(5, 1);                       // a command station repeats the packet
        QTRY_COMPARE(int(r.sim->output(4).state), 1);
        QTest::qWait(200);
        QCOMPARE(int(r.sim->output(4).state), 1);            // toggled once, not four times
        QTest::qWait(400);
        r.sim->dccAccessory(5, 1);                           // a new command after the lockout
        QTRY_COMPARE(int(r.sim->output(4).state), 0);
        QTRY_COMPARE(int(r.sim->pin(r.sim->output(4).pinA).level), 0);
    }

    void firmwareDrivesPins()
    {
        Rig r; makeRig(r);
        const int pin = r.sim->output(1).pinA;               // single steady on D4
        QCOMPARE(pin, 4);
        r.link->triggerInput(1, 1);                          // absolute input 1 -> output 1 on
        waitFinished(r);
        QTRY_COMPARE(int(r.sim->pin(pin).level), 1);
        QCOMPARE(int(r.sim->pin(pin).mode), 1);              // OUTPUT
        r.link->triggerInput(1, 0);
        QTRY_COMPARE(int(r.sim->pin(pin).level), 0);
    }

    void firmwareBlockSignals()
    {
        // GPIOcore.ino blockSignals(): Dutch 3-aspect signals, three blocks in a row; sensor k on block k,
        // signal k protects block k and looks at block k + 1. Block 1's detector is on D0 (simulator: D0 works).
        Rig r; makeRig(r);
        OutputConfig none;
        for (int o = 3; o < NumOutputs; ++o)
            r.sim->setOutput(o, none);                   // frees the default pins
        const quint8 pins[3][3] = { { 3, 4, 5 }, { 6, 7, 8 }, { 9, 10, 11 } };
        const quint8 sensorPin[3] = { 0, 17, 18 };       // D0, A3, A4
        for (int k = 0; k < 3; ++k)
        {
            r.sim->setOutput(k, makeBlockSignal(k, k < 2 ? k + 1 : -1, pins[k][0], pins[k][1], pins[k][2]));
            InputConfig in;
            in.type = Sensor; in.pin = sensorPin[k];
            r.sim->setInput(k, in);
        }
        const int green = 1, yellow = 2, red = 4;
        auto aspects = [&r] { return QVector<int>{ r.sim->servoPosition(0), r.sim->servoPosition(1), r.sim->servoPosition(2) }; };
        QTRY_COMPARE(aspects(), (QVector<int>{ green, green, green }));
        QTest::qWait(150);                               // the inputs see 'free' first: armed

        // shown at once after connecting: GET_OUTPUT_STATE answers state 2 + the current aspect
        QSignalSpy events(r.link, &DeviceLink::outputEvent);
        for (int k = 0; k < 3; ++k)
            r.link->readOutputState(k);
        waitFinished(r);
        QTRY_COMPARE(events.size(), 3);
        for (const QList<QVariant> &e : events)
            QVERIFY(e.at(1).toInt() == 2 && e.at(2).toInt() == green);
        events.clear();

        r.sim->setPinLevel(17, false);                   // a train in block 2
        QTRY_COMPARE(aspects(), (QVector<int>{ yellow, red, green }));
        QTRY_VERIFY(!events.isEmpty());                  // reported: OUTPUT_EVENT state 2 + aspect
        bool reported = false;
        for (const QList<QVariant> &e : events)
            reported |= e.at(0).toInt() == 1 && e.at(1).toInt() == 2 && e.at(2).toInt() == red;
        QVERIFY(reported);
        QTRY_COMPARE(int(r.sim->pin(8).level), 1);       // signal 2: red lamp C (D8) on, green A (D6) off
        QCOMPARE(int(r.sim->pin(6).level), 0);
        QCOMPARE(int(r.sim->pin(4).level), 1);           // signal 1: yellow lamp B (D4) on
        QCOMPARE(int(r.sim->pin(9).level), 1);           // signal 3: green A (D9) on, red C (D11) off
        QCOMPARE(int(r.sim->pin(11).level), 0);

        r.sim->setPinLevel(18, false);                   // the train is in blocks 2 and 3
        QTRY_COMPARE(aspects(), (QVector<int>{ yellow, red, red }));
        r.sim->setPinLevel(17, true);                    // block 2 free again, block 3 still occupied
        QTRY_COMPARE(aspects(), (QVector<int>{ green, yellow, red }));
        r.sim->setPinLevel(18, true);
        QTRY_COMPARE(aspects(), (QVector<int>{ green, green, green }));
        r.sim->setPinLevel(0, false);                    // block 1 on D0
        QTRY_COMPARE(aspects(), (QVector<int>{ red, green, green }));
        r.sim->setPinLevel(0, true);
        QTRY_COMPARE(aspects(), (QVector<int>{ green, green, green }));

        // manual commands are ignored: a DCC extended value for an input on signal 2
        InputConfig dcc;
        dcc.type = Latching; dcc.address = 7; dcc.outputId = 1;
        r.sim->setInput(5, dcc);
        r.sim->dccExtended(7, 0x08);
        r.sim->dccAccessory(7, 0);
        QTest::qWait(200);
        QCOMPARE(aspects(), (QVector<int>{ green, green, green }));
        QCOMPARE(int(r.sim->pin(6).level), 1);
        QCOMPARE(r.errors, 0);
    }

    void firmwareMainAndDistantSignal()
    {
        // One detector drives two signals: an unlinked main signal (no next block: red / green) and its distant
        // signal further back, whose 'own block' is the block behind the main signal (occupied = yellow).
        Rig r; makeRig(r);
        OutputConfig none;
        for (int o = 2; o < NumOutputs; ++o)
            r.sim->setOutput(o, none);
        OutputConfig main = makeBlockSignal(4, -1, 3, PinNA, 5);
        main.data[1] = 0x02;                             // never shown without a next block
        r.sim->setOutput(0, main);
        OutputConfig distant = makeBlockSignal(4, -1, 6, 7, PinNA);
        distant.data = { { 0x02, 0x02, 0x01, distant.data[3] } };   // occupied: yellow (B), free: green (A)
        r.sim->setOutput(1, distant);
        InputConfig in;
        in.type = Sensor; in.analog = AnalogBelow; in.pin = 16; in.threshold = 512;   // analog detector on A2
        r.sim->setInput(4, in);
        auto aspects = [&r] { return QVector<int>{ r.sim->servoPosition(0), r.sim->servoPosition(1) }; };
        QTRY_COMPARE(aspects(), (QVector<int>{ 0x01, 0x01 }));
        QTest::qWait(150);
        r.sim->setAnalogValue(2, 100);                   // occupied
        QTRY_COMPARE(aspects(), (QVector<int>{ 0x04, 0x02 }));
        QTRY_COMPARE(int(r.sim->pin(5).level), 1);       // main: red lamp C
        QCOMPARE(int(r.sim->pin(3).level), 0);
        QCOMPARE(int(r.sim->pin(7).level), 1);           // distant: yellow lamp B
        r.sim->setAnalogValue(2, 1023);
        QTRY_COMPARE(aspects(), (QVector<int>{ 0x01, 0x01 }));
        QCOMPARE(r.errors, 0);
    }

    // Board-to-board link without extra firmware: a second block signal with only lamp A on TX (D1) watches the
    // same detector. Occupied = D1 LOW (all lamps off), free = D1 HIGH (lamp A on); D1 sinks the LED of an
    // optocoupler on the next board, whose RX (D0) is a Sensor input there. One extra output, no extra input.
    void blockSignalRelayOnTx()
    {
        SimulatedDevice sim;
        QVERIFY(sim.open(QIODevice::ReadWrite));
        OutputConfig none;
        for (int o = 0; o < NumOutputs; ++o)
            sim.setOutput(o, none);
        InputConfig det;
        det.type = Sensor; det.pin = 5;
        sim.setInput(4, det);
        sim.setOutput(0, makeBlockSignal(4, -1, 3, PinNA, 6));   // the signal itself: green D3, red D6
        OutputConfig relay = makeBlockSignal(4, -1, 1, PinNA, PinNA);
        relay.data[0] = 0x00;                                    // own block occupied: off = LOW
        relay.data[1] = 0x01;                                    // (no next block)
        relay.data[2] = 0x01;                                    // free: lamp A on = HIGH
        sim.setOutput(1, relay);
        sim.advanceClock(5100);                                  // no GUI: after the window D0/D1 are I/O
        QTRY_VERIFY(sim.serialIsIO());
        QTRY_COMPARE(int(sim.pin(1).level), 1);                  // free
        QCOMPARE(int(sim.pin(3).level), 1);                      // green
        QTest::qWait(150);
        sim.setPinLevel(5, false);                               // a train in the block
        QTRY_COMPARE(int(sim.pin(1).level), 0);
        QCOMPARE(int(sim.pin(6).level), 1);                      // red at the same time
        QCOMPARE(int(sim.pin(3).level), 0);
        sim.setPinLevel(5, true);
        QTRY_COMPARE(int(sim.pin(1).level), 1);
        QCOMPARE(int(sim.pin(3).level), 1);
    }

    // One light effect type: the effect in data[3], swing times 12 bits each; old files (one type per effect,
    // swing times in 16 bits) load as the new type
    void lightEffectType()
    {
        OutputConfig o;
        o.type = LightEffect;
        o.setLightEffect(LightSwing);
        o.setSwing(4095, 300);
        QCOMPARE(int(o.swingPulse()), 4095);
        QCOMPARE(int(o.swingPause()), 300);
        QCOMPARE(int(o.lightEffect()), int(LightSwing));
        o.setSwing(9000, 9000);                                  // clamped to 12 bits
        QCOMPARE(int(o.swingPulse()), MaxSwingTime);
        QCOMPARE(QString::fromUtf8(outputTypeInfo(o).key), QStringLiteral("swing"));
        QCOMPARE(outputTypeInfo(o).pinCount, 2);
        o.setLightEffect(LightPhoto);
        QCOMPARE(outputTypeInfo(o).pinCount, 1);
        o.setLightEffect(LightTube);
        QVERIFY(outputTypeInfo(o).needsPwm);
        QVERIFY(isUnusedOutputType(7) && isUnusedOutputType(9) && !isUnusedOutputType(LightEffect));

        const QJsonObject j = o.toJson();
        QCOMPARE(j["type"].toString(), QStringLiteral("lightEffect"));
        QCOMPARE(j["effect"].toString(), QStringLiteral("tube"));
        QCOMPARE(OutputConfig::fromJson(j), o);

        QJsonObject old;                                         // a file from before: swing 0.4 s / 1.2 s
        old["type"] = QStringLiteral("swingLight");
        old["pinA"] = 3; old["pinB"] = 4;
        old["data"] = QJsonArray{ 40, 0, 120, 0 };
        const OutputConfig s = OutputConfig::fromJson(old);
        QCOMPARE(int(s.type), int(LightEffect));
        QCOMPARE(int(s.lightEffect()), int(LightSwing));
        QCOMPARE(int(s.swingPulse()), 40);
        QCOMPARE(int(s.swingPause()), 120);
        old["type"] = QStringLiteral("welderLight");
        QCOMPARE(int(OutputConfig::fromJson(old).lightEffect()), int(LightWelder));
        QCOMPARE(int(OutputConfig::fromJson(old).swingPulse()), 0);
    }

    // The firmware runs the effect from data[3]: swing alternates A and B, the photographer flashes A now and then
    void firmwareLightEffects()
    {
        SimulatedDevice sim;
        QVERIFY(sim.open(QIODevice::ReadWrite));
        OutputConfig none;
        for (int o = 0; o < NumOutputs; ++o)
            sim.setOutput(o, none);
        OutputConfig swing;
        swing.type = LightEffect; swing.pinA = 3; swing.pinB = 4; swing.state = 1;
        swing.setLightEffect(LightSwing);
        swing.setSwing(15, 10);                                  // 150 ms each lamp, 100 ms pause
        sim.setOutput(0, swing);
        QTRY_COMPARE_WITH_TIMEOUT(int(sim.pin(3).level), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(int(sim.pin(4).level), 1, 2000);
        QCOMPARE(int(sim.pin(3).level), 0);                      // never both

        OutputConfig photo;
        photo.type = LightEffect; photo.pinA = 5; photo.state = 1;
        photo.setLightEffect(LightPhoto);
        sim.setOutput(1, photo);
        bool flashed = false;
        QElapsedTimer t;
        t.start();
        while (!flashed && t.elapsed() < 8000)                   // the first flash within 0.5-3 s (+ pre-flash)
        {
            QTest::qWait(5);
            flashed = sim.pin(5).level;
        }
        QVERIFY(flashed);
        QTRY_COMPARE_WITH_TIMEOUT(int(sim.pin(5).level), 0, 1000);   // a flash is short
    }

    // Block signals blink at the board's rate (board flags bits 4-7), 1 Hz by default
    void firmwareBlockSignalBlinkRate()
    {
        Rig r; makeRig(r);
        OutputConfig none;
        for (int o = 1; o < NumOutputs; ++o)
            r.sim->setOutput(o, none);
        OutputConfig sig = makeBlockSignal(4, -1, 3, PinNA, PinNA);
        sig.data[2] = 0x11;                              // free: lamp A blinks
        r.sim->setOutput(0, sig);
        auto hz = [&r] {                                 // lamp A's blink rate, measured over 2 s
            int n = 0, last = r.sim->pin(3).level;
            QElapsedTimer t;
            t.start();
            while (t.elapsed() < 2000)
            {
                QTest::qWait(10);
                if (int(r.sim->pin(3).level) != last) { ++n; last = r.sim->pin(3).level; }
            }
            return n * 500.0 / t.elapsed();             // two level changes per period
        };
        QTRY_COMPARE(r.sim->servoPosition(0), 0x11);
        const double slow = hz();
        BoardSettings b = r.sim->board();
        b.blockBlink = BoardSettings::blockBlinkFromHz(3.0);   // 180 a minute
        r.link->writeBoard(b);
        waitFinished(r);
        QCOMPARE(int(r.sim->board().blockBlink), 12);
        const double fast = hz();
        QVERIFY2(slow > 0.7 && slow < 1.3, qPrintable(QString::number(slow)));
        QVERIFY2(fast > 2.4 && fast < 3.6, qPrintable(QString::number(fast)));
        QCOMPARE(r.errors, 0);
    }

    void firmwareServoSemaphore()
    {
        // GPIOcore.ino blockSignals(): a servo with data[3] bit7 follows a detector: occupied = curved (stop),
        // free = straight (clear), at its own speed; bit6 swaps them
        Rig r; makeRig(r);
        OutputConfig servo;
        servo.type = ServoType; servo.pinA = 3; servo.state = 0;
        servo.data = { { 30, 150, 2, quint8(0x80 | 6) } };
        r.sim->setOutput(0, servo);
        OutputConfig unused = r.sim->output(13);         // output 13 had A3
        unused.type = OutputNotInUse; unused.pinA = PinNA;
        r.sim->setOutput(13, unused);
        InputConfig in;
        in.type = Sensor; in.pin = 17;                   // A3
        r.sim->setInput(6, in);
        QTRY_COMPARE(int(r.sim->output(0).state), 1);    // free: straight
        QTRY_COMPARE(r.sim->servoPosition(0), 150);
        QTest::qWait(150);
        r.sim->setPinLevel(17, false);                   // occupied: curved = stop
        QTRY_COMPARE(int(r.sim->output(0).state), 0);
        QTRY_COMPARE(r.sim->servoPosition(0), 30);
        servo.data[3] = quint8(0x80 | 0x40 | 6);         // swapped: occupied = straight
        r.sim->setOutput(0, servo);
        QTRY_COMPARE(int(r.sim->output(0).state), 1);
        r.sim->setPinLevel(17, true);
        QTRY_COMPARE(int(r.sim->output(0).state), 0);
        QCOMPARE(r.errors, 0);
    }

    void firmwareInvertFrog()
    {
        // output.cpp relayWrite(): with invertFrog the relay pins are written inverted; the logical relay
        // state (frogRelays(), OUTPUT_EVENT state 3) does not change. GET_OUTPUT_DATA: state byte bit1.
        Rig r; makeRig(r);
        OutputConfig unused = r.sim->output(1);          // output 1 had D4
        unused.type = OutputNotInUse; unused.pinA = PinNA;
        r.sim->setOutput(1, unused);
        OutputConfig servo;
        servo.type = ServoType; servo.pinA = 3; servo.relayC = 4;   // unifrog relay on D4
        servo.data = { { 30, 150, 2, 0 } };
        for (bool invert : { false, true })
        {
            servo.invertFrog = invert;
            r.sim->setOutput(0, servo);
            for (int state : { 1, 0 })
            {
                r.link->triggerInput(0, state);          // absolute input 0 -> output 0
                waitFinished(r);
                QTRY_COMPARE(r.sim->servoPosition(0), state ? 150 : 30);
                QTRY_COMPARE((r.sim->frogRelays(0) >> 2) & 1, state);       // logical: straight = on
                QCOMPARE(int(r.sim->pin(4).level), state ^ int(invert));   // the pin itself
            }
            r.link->readOutput(0);
            waitFinished(r);
            QCOMPARE(r.outputs[0].invertFrog, invert);
            QCOMPARE(r.sim->output(0).invertFrog, invert);
        }
        QCOMPARE(r.errors, 0);
    }

    void firmwareAnalogNeedsTwoSamples()
    {
        // input.cpp readAnalog(): the condition must hold in two samples 50 ms apart, so a resistor
        // ladder passing other values while a button moves does not count
        Rig r; makeRig(r);
        OutputConfig lamp = r.sim->output(6);
        lamp.state = 1;
        r.sim->setOutput(6, lamp);
        InputConfig a;
        a.type = Latching; a.analog = AnalogBelow; a.pin = 16; a.threshold = 512; a.outputId = 6;   // A2
        r.sim->setInput(6, a);
        OutputConfig unused = r.sim->output(12);             // output 12 had A2
        unused.type = OutputNotInUse; unused.pinA = PinNA;
        r.sim->setOutput(12, unused);
        QTest::qWait(150);                                   // 'released' first: arms the input
        for (int k = 0; k < 3; ++k)                          // short dips, shorter than a sample period
        {
            r.sim->setAnalogValue(2, 100);
            QTest::qWait(5);
            r.sim->setAnalogValue(2, 1023);
            QTest::qWait(120);
        }
        QCOMPARE(int(r.sim->output(6).state), 1);
        r.sim->setAnalogValue(2, 100);                       // held: pressed = LOW = off
        QTRY_COMPARE(int(r.sim->output(6).state), 0);
    }

    void firmwareAbsoluteInputAtStart()
    {
        // input.cpp update(): the first sample is the starting level. An absolute input applies it once when its
        // output differs (a switch moved while the power was off); a toggle input does nothing. SET_INPUT_DATA
        // starts an input again, so it is the same for a new configuration.
        SimulatedDevice sim;
        for (int o : { 0, 2 })
        {
            OutputConfig lamp = sim.output(o);               // single steady on D3 / D5, on
            lamp.state = 1;
            sim.setOutput(o, lamp);
        }
        InputConfig absolute;
        absolute.type = Latching; absolute.pin = 0; absolute.outputId = 0;
        sim.setInput(0, absolute);
        absolute.pin = 20; absolute.outputId = 2;            // A6, not pressed: equal to the output, nothing to do
        sim.setInput(2, absolute);
        InputConfig toggle;
        toggle.type = Momentary; toggle.pin = 1; toggle.outputId = 1;
        sim.setInput(1, toggle);
        sim.setPinLevel(0, false);                           // D0 and D1 LOW from the start
        sim.setPinLevel(1, false);
        QVERIFY(sim.open(QIODevice::ReadWrite));
        QTest::qWait(300);
        QCOMPARE(int(sim.output(0).state), 0);               // no edge: it followed its switch
        QCOMPARE(int(sim.pin(3).level), 0);
        QCOMPARE(int(sim.output(1).state), 0);               // the toggle input did nothing
        QCOMPARE(int(sim.output(2).state), 1);

        sim.setPinLevel(0, true);                            // edges as before
        QTRY_COMPARE(int(sim.output(0).state), 1);
        sim.setPinLevel(1, true);
        QTest::qWait(150);
        sim.setPinLevel(1, false);                           // a press toggles
        QTRY_COMPARE(int(sim.output(1).state), 1);

        OutputConfig lamp = sim.output(3);                   // a new absolute input on A6 (released = 1), output off
        lamp.state = 0;
        sim.setOutput(3, lamp);
        absolute.outputId = 3;
        sim.setInput(3, absolute);
        QTRY_COMPARE(int(sim.output(3).state), 1);
    }
};

QTEST_GUILESS_MAIN(TestGpioCore)
#include "tst_gpiocore.moc"
