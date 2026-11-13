// Unit tests: protocol framing, config model and DeviceLink against the
// firmware simulator.

#include "connections.h"
#include "devicelink.h"
#include "gpioconfig.h"
#include "protocol.h"
#include "simulateddevice.h"

#include <QJsonDocument>
#include <QSignalSpy>
#include <QtTest>

using namespace Gpio;

Q_DECLARE_METATYPE(Gpio::InputConfig)
Q_DECLARE_METATYPE(Gpio::OutputConfig)

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

    // Link + simulator with short timings, connected and version received.
    void makeRig(Rig &r, SimulatedDevice::Options opt)
    {
        opt.bootDelayMs = 5;
        r.link = new DeviceLink(this);
        r.link->bootWaitMs = 200;
        r.link->timeoutMs  = 100;
        r.link->drainMs    = 3;
        r.sim = new SimulatedDevice(opt);
        connect(r.link, &DeviceLink::outputReceived, this, [&r](int i, const OutputConfig &c) { r.outputs[i] = c; ++r.outputsSeen; });
        connect(r.link, &DeviceLink::inputReceived,  this, [&r](int i, const InputConfig &c)  { r.inputs[i]  = c; ++r.inputsSeen;  });
        connect(r.link, &DeviceLink::errorOccurred,  this, [&r](const QString &) { ++r.errors; });
        connect(r.link, &DeviceLink::routeReceived,  this, [&r](int i, const QVector<RouteStep> &s) { r.routes[i] = s; ++r.routesSeen; });

        QSignalSpy ready(r.link, &DeviceLink::ready);
        QString err;
        QVERIFY(r.link->openDevice(r.sim, true, &err));
        QVERIFY(ready.wait(1000));
        QCOMPARE(ready.first().first().toString(),
                 opt.oldDefaults ? QStringLiteral("12345678") : QStringLiteral("v0.0.0"));
        waitFinished(r);                 // HELLO (or its timeout on v1 boards)
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
        const QByteArray p = encodeOutput(3, c, false);
        QCOMPARE(p.size(), 14);
        int idx = -1; OutputConfig d;
        QVERIFY(decodeOutput(p, idx, d, false));
        QCOMPARE(idx, 3);
        QVERIFY(d == c);

        const QByteArray legacy = encodeOutput(3, c, true);
        QCOMPARE(legacy.size(), 13);
        QVERIFY(decodeOutput(legacy, idx, d, true));
        QCOMPARE(d.data[3], quint8(0));            // not transmitted
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
        b.outputs[2].type = SwingLight; b.outputs[2].pinA = 5; b.outputs[2].pinB = 6;
        b.outputs[2].setSwing(40, 120);
        b.inputs[1].type = Route; b.inputs[1].address = 12;
        RouteStep a; a.output = 2; a.value = 1;
        RouteStep c2; c2.output = 7; c2.value = 120; c2.raw = true; c2.delay = 16383;
        b.inputs[1].route = { a, c2 };
        const QByteArray json = QJsonDocument(b.toJson()).toJson();
        BoardConfig c; QString err;
        QVERIFY(BoardConfig::fromJson(QJsonDocument::fromJson(json).object(), c, &err));
        QVERIFY(c.outputs[2] == b.outputs[2]);
        QVERIFY(c.inputs[1] == b.inputs[1]);
    }

    // ── Device link vs simulator ────────────────────────────────────────────
    void readAllFromFixedFirmware()
    {
        Rig r; makeRig(r, SimulatedDevice::Options());
        r.link->readAll();
        waitFinished(r);
        QCOMPARE(r.errors, 0);
        QCOMPARE(r.inputsSeen, 16);
        QCOMPARE(r.outputsSeen, 16);
        QVERIFY(!r.link->legacyOutputFormat());
        QCOMPARE(r.outputs[0].pinA, quint8(3));     // F7: D0..D2 are skipped
        QCOMPARE(r.outputs[15].pinA, quint8(19));          // D3-D12, A0-A5: D13 is the config LED
        QCOMPARE(r.outputs[7].switchTime(), quint32(50));
        QCOMPARE(r.inputs[4].linked[0], LinkNone);  // F16: end of route, not NA
    }

    void presetBroadcastMatchesGetLayout()
    {
        // F5: preset broadcasts use the same 14-byte layout as GET_OUTPUT_DATA
        SimulatedDevice::Options opt; opt.bootDelayMs = 100000; opt.replyDelayMs = 0;
        SimulatedDevice sim(opt);
        QVERIFY(sim.open(QIODevice::ReadWrite));
        sim.write(Proto::encode(Proto::SET_PRESET, QByteArray(1, 4)));   // 8 double pulse
        QTRY_VERIFY(sim.bytesAvailable() > 0);
        Proto::FrameParser parser;
        Proto::FrameParser::Result res;
        parser.feed(sim.readAll(), res);
        QCOMPARE(res.errors.size(), 0);
        int outputs = 0, inputs = 0;
        for (const Proto::Frame &f : res.frames)
        {
            int idx = -1;
            if (f.command == Proto::SET_OUTPUT_DATA)
            {
                OutputConfig c;
                QVERIFY(decodeOutput(f.payload, idx, c, false));
                QVERIFY(c == sim.output(idx));
                ++outputs;
            }
            if (f.command == Proto::SET_INPUT_DATA) ++inputs;
        }
        QCOMPARE(outputs, 16);
        QCOMPARE(inputs, 16);                       // F4: preset 4 now sets up the inputs
        QCOMPARE(sim.output(0).switchTime(), quint32(50));
        QCOMPARE(sim.output(0).pinC, PinNA);
        QCOMPARE(sim.input(7).outputId, quint8(7));
        QVERIFY(res.frames.last().isAck());
    }

    void readAllFromOldFirmware()
    {
        Rig r; makeRig(r, SimulatedDevice::Options::oldFirmware());
        r.link->readAll();
        waitFinished(r);
        QCOMPARE(r.errors, 0);
        QCOMPARE(r.inputsSeen, 16);
        QCOMPARE(r.outputsSeen, 16);
        QVERIFY(r.link->legacyOutputFormat());
        QCOMPARE(r.outputs[7].pinA, quint8(7));
        QCOMPARE(int(r.outputs[7].type), int(SingleSteady));
        QCOMPARE(r.inputs[4].address, quint16(5));
        QCOMPARE(r.inputs[4].linked[0], PinNA);    // firmware default NA (0x3F), see review
    }

    void writeAndReadBackFixedFirmware()
    {
        Rig r; makeRig(r, SimulatedDevice::Options());
        r.link->readOutput(0);
        waitFinished(r);
        QVERIFY(!r.link->legacyOutputFormat());

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

    void legacyFirmwareLosesFourthDataByte()
    {
        Rig r; makeRig(r, SimulatedDevice::Options::oldFirmware());
        r.link->readOutput(0);
        waitFinished(r);
        OutputConfig c;
        c.type = ServoType; c.pinA = 9;
        c.data = { { 60, 120, 25, 3 } };
        r.link->writeOutput(1, c);
        waitFinished(r);
        QCOMPARE(r.sim->output(1).data[2], quint8(25));
        QVERIFY(r.sim->output(1).data[3] != 3);    // stale buffer byte instead of 3
    }

    void doubleAckDoesNotDesync()
    {
        Rig r; makeRig(r, SimulatedDevice::Options::oldFirmware());
        InputConfig in;
        in.type = Momentary; in.address = 100; in.pin = 4; in.outputId = 3;
        for (int i = 0; i < 4; ++i)
        {
            r.link->writeInput(i, in);
            r.link->readInput(i);
        }
        waitFinished(r);
        QCOMPARE(r.errors, 0);
        QCOMPARE(r.inputsSeen, 4);
        for (int i = 0; i < 4; ++i)
            QVERIFY(r.inputs[i] == in);
    }

    void nackIsRetried()
    {
        Rig r; makeRig(r, SimulatedDevice::Options());
        r.sim->corruptNextFrames(1);
        r.link->readInput(2);
        waitFinished(r);
        QCOMPARE(r.errors, 0);
        QCOMPARE(r.inputsSeen, 1);
    }

    void presetThenReadBack()
    {
        Rig r; makeRig(r, SimulatedDevice::Options());
        QSignalSpy applied(r.link, &DeviceLink::presetApplied);
        r.link->applyPreset(2);           // 5 servos + electrofrog relays
        QVERIFY(applied.wait(2000));
        waitFinished(r);
        QCOMPARE(r.errors, 0);
        QCOMPARE(r.outputsSeen, 16);
        QCOMPARE(int(r.outputs[4].type), int(ServoType));
        QCOMPARE(r.outputs[0].pinA,   quint8(3));   // F7: presets start at D3
        QCOMPARE(r.outputs[4].pinA,   quint8(16));          // D13 (config LED) is skipped
        QCOMPARE(r.outputs[4].relayA, quint8(17));
        QCOMPARE(r.outputs[4].relayB, quint8(18));
        QCOMPARE(int(r.outputs[5].type), int(OutputNotInUse));
        QCOMPARE(r.inputs[4].outputId, quint8(4));
        QCOMPARE(r.inputs[5].address, AddrNone);
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

    void routeLimitsV1AndPool()
    {
        BoardConfig b;
        b.inputs[1].type = Route;
        RouteLimits v1; v1.v1 = true; v1.maxSteps = MaxRouteStepsV1;
        for (int o = 0; o < 8; ++o)
            QCOMPARE(connectOutput(b, 1, o, v1), ConnectResult::Connected);
        QCOMPARE(connectOutput(b, 1, 9, v1), ConnectResult::RouteFull);
        QCOMPARE(int(b.inputs[1].route[0].delay), int(V1StepDelay));

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

    void recorderOnlyOnce()
    {
        BoardConfig b;
        b.inputs[3].type = Recorder;
        QCOMPARE(recorderInput(b), 3);
        QCOMPARE(recorderInput(b, 3), -1);                  // ignoring itself: no other recorder
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

    // ── Protocol v2 ─────────────────────────────────────────────────────────
    void helloV2Capabilities()
    {
        Rig r; makeRig(r, SimulatedDevice::Options());
        QCOMPARE(r.link->caps().protocol, 2);
        QCOMPARE(r.link->caps().maxRouteSteps, 16);
        QCOMPARE(r.link->caps().stepPool, 256);
        QVERIFY(r.link->caps().servoLive);
        QVERIFY(r.sim->configSession());
        QCOMPARE(r.errors, 0);
    }

    void boardSettingsRoundTrip()
    {
        BoardSettings b;
        b.name = QStringLiteral("Station/left:1234567890");   // path characters dropped, max 16
        b.type = BoardDccNext; b.recorder = false; b.timing = false; b.interlock = InterlockOn; b.delay = 7;
        BoardSettings d;
        QVERIFY(decodeBoard(encodeBoard(b), d));
        QCOMPARE(d.name, QStringLiteral("Stationleft12345"));
        QCOMPARE(int(d.type), int(BoardDccNext));
        QVERIFY(!d.recorder && !d.timing);
        QCOMPARE(int(d.interlock), int(InterlockOn));
        QCOMPARE(int(d.delay), 7);
        QCOMPARE(BoardSettings::fromJson(d.toJson()), d);

        Rig r; makeRig(r, SimulatedDevice::Options());
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

    void presetsInTheGui()
    {
        const BoardConfig s16 = presetBoard(0);              // 16 servos: D3-D12, A0-A5, never D13
        QCOMPARE(int(s16.outputs[0].pinA), 3);
        QCOMPARE(int(s16.outputs[9].pinA), 12);
        QCOMPARE(int(s16.outputs[10].pinA), 14);
        QCOMPARE(int(s16.outputs[15].pinA), 19);
        QCOMPARE(int(s16.outputs[15].type), int(ServoType));
        QCOMPARE(int(s16.inputs[15].type), int(Latching));
        QCOMPARE(int(s16.inputs[15].address), 16);
        const BoardConfig e = presetBoard(6);                // 4 double pulse + electrofrog
        QCOMPARE(int(e.outputs[3].relayB), 19);
        QCOMPARE(int(e.outputs[4].type), int(OutputNotInUse));
        QCOMPARE(int(e.inputs[4].type), int(InputNotInUse));
        QCOMPARE(int(e.outputs[0].switchTime()), 50);
    }

    void helloTimeoutIsV1()
    {
        Rig r; makeRig(r, SimulatedDevice::Options::currentFirmware());
        QCOMPARE(r.link->caps().protocol, 1);
        QCOMPARE(r.errors, 0);                              // no HELLO reply is not an error
    }

    void routeRoundTripV2()
    {
        Rig r; makeRig(r, SimulatedDevice::Options());
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

    void routeOnV1UsesLinkedBytes()
    {
        Rig r; makeRig(r, SimulatedDevice::Options::currentFirmware());
        InputConfig in;
        in.type = Route;
        RouteStep a; a.output = 5; a.value = 1; a.delay = V1StepDelay;
        RouteStep b; b.output = 6; b.value = 0; b.delay = V1StepDelay;
        in.route = { a, b };
        r.link->writeInput(2, in);
        r.link->writeRoute(2, in.route);                    // skipped: v1 board
        r.link->readInput(2);
        waitFinished(r);
        QCOMPARE(r.errors, 0);
        QCOMPARE(r.routesSeen, 0);
        QCOMPARE(r.inputs[2].route, in.route);              // from the 8 linked bytes
        QCOMPARE(r.sim->input(2).linked[0], InputConfig::makeLink(5, 1));
    }

    void servoLiveV2()
    {
        Rig r; makeRig(r, SimulatedDevice::Options());
        for (int a = 60; a <= 120; a += 5)
            r.link->servoLive(2, a);                         // a burst, as from a slider
        waitFinished(r);
        QCOMPARE(r.sim->liveAngle(2), 120);
        QCOMPARE(r.errors, 0);
    }

    void routeEndsAtInvalidStep()
    {
        InputConfig c;
        c.type = Route;
        c.linked[0] = InputConfig::makeLink(4, 1);
        c.linked[1] = PinNA;                                   // 0x3F: old firmware default
        c.linked[2] = InputConfig::makeLink(6, 1);
        c.routeFromLinked();
        QCOMPARE(routeSteps(c).size(), 1);                     // like Input::triggerOutputs()
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
        b.inputs[5].type = Recorder;
        QCOMPARE(connectOutput(b, 5, 3), ConnectResult::NotConnectable);
    }
};

QTEST_GUILESS_MAIN(TestGpioCore)
#include "tst_gpiocore.moc"
