// Main window against the firmware simulator: edit -> dirty -> write ->
// read back -> clean, presets, and connecting inputs / outputs by dragging.

#include "connections.h"
#include "connectionview.h"
#include "devicelink.h"
#include "inputeditor.h"
#include "mainwindow.h"
#include "outputeditor.h"
#include "simulateddevice.h"
#include "uihelpers.h"
#include "qrcode.h"
#include "webremote.h"

#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QSlider>
#include <QTcpSocket>

#include <QAction>
#include <QComboBox>
#include <QMouseEvent>
#include <QStandardItemModel>
#include <QToolButton>
#include <QSignalSpy>
#include <QtTest>

using namespace Gpio;

class TestGui : public QObject
{
    Q_OBJECT

    QAction *writeAction(MainWindow &w)
    {
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text().startsWith(QLatin1String("Write")) && !a->text().startsWith(QLatin1String("Write all")))
                return a;
        return nullptr;
    }

    void waitIdle(DeviceLink *link)
    {
        QSignalSpy done(link, &DeviceLink::finished);
        QVERIFY(done.wait(5000));
    }

    // Shown main window connected to the demo simulator, board read.
    ConnectionView *startDemo(MainWindow &w)
    {
        auto *link = w.findChild<DeviceLink *>();
        link->drainMs = 3;
        w.resize(1320, 900);
        w.show();
        w.connectSimulatedBoard();      // Configure mode on a (simulated) board: everything can be edited
        waitIdle(link);
        QTest::qWaitFor([link] { return !link->isBusy(); }, 5000);   // the simulation reads the live states too
        QTest::qWait(50);
        QTest::qWaitFor([link] { return !link->isBusy(); }, 5000);
        QCoreApplication::processEvents();
        return w.findChild<ConnectionView *>();
    }

    static void mouse(QWidget *target, QEvent::Type type, const QPointF &pos, Qt::MouseButtons buttons)
    {
        QMouseEvent e(type, pos, target->mapToGlobal(pos.toPoint()),
                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons, Qt::NoModifier);
        QApplication::sendEvent(target, &e);
    }

    static void drag(QWidget *view, const QPointF &from, const QPointF &to)
    {
        mouse(view, QEvent::MouseButtonPress, from, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, (from + to) / 2, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, to, Qt::LeftButton);
        mouse(view, QEvent::MouseButtonRelease, to, Qt::NoButton);
    }

    static void click(QWidget *view, const QPointF &at)
    {
        mouse(view, QEvent::MouseButtonPress, at, Qt::LeftButton);
        mouse(view, QEvent::MouseButtonRelease, at, Qt::NoButton);
    }

    // First combo box below 'root' whose first item text starts with 'firstItem'
    static QComboBox *comboWith(QWidget *root, const QString &itemText)
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);   // rebuilt rows
        for (QComboBox *c : root->findChildren<QComboBox *>())
            for (int i = 0; i < c->count(); ++i)
                if (c->itemText(i).startsWith(itemText))
                    return c;
        return nullptr;
    }

    void maybeScreenshot(MainWindow &w, const char *name)
    {
        const QByteArray dir = qgetenv("GPIOCORE_SHOT_DIR");
        if (dir.isEmpty())
            return;
        QTest::qWait(400);                       // let the side panel slide in
        if (!dir.isEmpty())
            w.grab().save(QString::fromLocal8Bit(dir) + QLatin1Char('/') + QLatin1String(name));
    }

    void waitIdleOrNot(DeviceLink *link)
    {
        QTRY_VERIFY_WITH_TIMEOUT(!link->isBusy(), 5000);
    }

private slots:
    void editWriteReadBack()
    {
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *link = w.findChild<DeviceLink *>();
        auto *ed   = w.findChild<OutputEditor *>();
        QVERIFY(view && ed);
        QAction *write = writeAction(w);
        QVERIFY(write);
        QVERIFY(!write->isEnabled());             // nothing changed yet

        OutputConfig c = ed->config();
        QCOMPARE(ed->index(), 0);
        c.type = ServoType;                       // same pin (D3): nothing is taken over
        c.data = { { 70, 110, 15, 0 } };
        emit ed->changed(0, c);
        QVERIFY(write->isEnabled());
        QCOMPARE(write->text(), QStringLiteral("Write 1 change(s)"));
        QVERIFY(view->outputDirtyShown(0));

        write->trigger();
        waitIdle(link);
        QVERIFY(!write->isEnabled());             // read back equals what we wrote
        QCOMPARE(int(view->board().outputs[0].type), int(ServoType));
        QVERIFY(!view->outputDirtyShown(0));
    }

    void presetClearsStaleConflicts()
    {
        MainWindow w;
        auto *link = w.findChild<DeviceLink *>();
        link->drainMs = 3;
        w.applyPresetOnStart(2);                  // 5 servos + electrofrog relays
        ConnectionView *view = startDemo(w);      // read all
        QTest::qWaitFor([link] { return !link->isBusy(); }, 8000);
        QCOMPARE(int(view->board().outputs[2].type), int(ServoType));
        QCOMPARE(view->board().outputs[2].pinA, quint8(9));
        // Before the preset output 6 used D9; now only output 2 does
        QVERIFY(!view->outputConflictShown(2));
        QVERIFY(!view->outputConflictShown(0));

        // Giving D9 to output 3 takes it from output 2: no conflict, output 2 is not in use
        auto *ed = w.findChild<OutputEditor *>();
        OutputConfig c = ed->config();
        c.type = SingleSteady;
        c.pinA = 9;
        emit ed->changed(3, c);
        QVERIFY(!view->outputConflictShown(3));
        QCOMPARE(int(view->board().outputs[2].type), int(OutputNotInUse));
    }

    void dragOutputOntoInput()
    {
        MainWindow w;
        ConnectionView *view = startDemo(w);
        QAction *write = writeAction(w);
        QCOMPARE(view->board().inputs[2].outputId, quint8(2));   // factory default: in n -> out n

        drag(view, view->outputRect(5).center(), view->inputRect(2).center());
        QCOMPARE(view->board().inputs[2].outputId, quint8(5));   // direct control: replaced
        QCOMPARE(outputSources(view->board(), 5), QVector<int>({ 2, 5 }));
        QCOMPARE(view->outputColor(5), Ui::inputColor(2));        // first (direct) input gives the colour
        QVERIFY(outputSources(view->board(), 2).isEmpty());
        QVERIFY(view->outputColor(2) != Ui::inputColor(2));       // output 2 lost its input: grey
        QVERIFY(write->isEnabled());

        // dragging the other way (input port -> output) works too
        drag(view, view->inputPort(2), view->outputRect(9).center());
        QCOMPARE(view->board().inputs[2].outputId, quint8(9));
    }

    void liveTest()
    {
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *link = w.findChild<DeviceLink *>();
        QAction *live = nullptr;
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text().contains(QLatin1String("Live test"))) live = a;
        QVERIFY(live);

        // a route on input 4: output 1 on, output 2 off (sent to the simulator by live write)
        InputConfig c = view->board().inputs[4];
        c.type = Route;
        c.route = { RouteStep{ 1, 1, false, 3 }, RouteStep{ 2, 0, false, 3 } };
        emit w.findChild<InputEditor *>()->changed(4, c);
        QTRY_VERIFY_WITH_TIMEOUT(!link->isBusy() && view->board().inputs[4].route.size() == 2, 3000);
        QTest::qWait(600);
        waitIdleOrNot(link);

        live->trigger();
        QTRY_VERIFY(view->liveMode());
        QTRY_VERIFY_WITH_TIMEOUT(view->liveState(0) >= 0, 3000);        // states read from the board

        click(view, view->liveButtonRect(0, 1).center());                 // input 0 (absolute) -> straight
        QTRY_COMPARE_WITH_TIMEOUT(view->liveState(0), 1, 2000);
        click(view, view->liveButtonRect(0, 0).center());                 // -> curved
        QTRY_COMPARE_WITH_TIMEOUT(view->liveState(0), 0, 2000);

        click(view, view->liveButtonRect(4, 1).center());                 // route
        QTRY_COMPARE_WITH_TIMEOUT(view->liveState(1), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(view->liveState(2), 0, 2000);
        maybeScreenshot(w, "live.png");

        w.setMode(MainWindow::ModeConfigure);
        QVERIFY(!view->liveMode());
    }

    void liveWriteStopsWhenSettled()
    {
        // Timing off (default): the board keeps no wait times. A route edit must be sent once,
        // not again and again because the read-back route has wait time 0.
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *link = w.findChild<DeviceLink *>();
        int writes = 0;
        connect(link, &DeviceLink::inputWritten, this, [&writes](int) { ++writes; });

        InputConfig c = view->board().inputs[4];
        c.type = Route;
        c.route = { RouteStep{ 1, 1, false, 3 }, RouteStep{ 2, 0, false, 7 } };
        emit w.findChild<InputEditor *>()->changed(4, c);
        QTest::qWait(2500);
        QCOMPARE(writes, 1);
        QCOMPARE(view->board().inputs[4].route.size(), 2);
    }

    void liveCustomValuesSignalAndPwm()
    {
        // a direct input with custom values on a signal / PWM output: the canvas shows the normal
        // press buttons (no DCC extended value button) and they send the two values
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *link = w.findChild<DeviceLink *>();
        auto *oed = w.findChild<OutputEditor *>();
        auto *ied = w.findChild<InputEditor *>();

        OutputConfig sig = view->board().outputs[5];
        sig.type = FourToneSignal; sig.data = { { 0, 0, 0, 0 } };
        emit oed->changed(5, sig);
        OutputConfig pwm = view->board().outputs[6];
        pwm.type = Pwm; pwm.data = { { 128, 0, 0, 0 } };
        emit oed->changed(6, pwm);
        QTRY_VERIFY_WITH_TIMEOUT(view->board().outputs[6].type == Pwm, 3000);

        InputConfig in5 = view->board().inputs[5];
        in5.type = Latching; in5.outputId = 5;
        in5.setCustomValues(true, FourToneSignal);
        in5.route[0].value = 3; in5.route[1].value = 9;
        emit ied->changed(5, in5);
        InputConfig in6 = view->board().inputs[6];
        in6.type = Latching; in6.outputId = 6;
        in6.setCustomValues(true, Pwm);
        in6.route[0].value = 50; in6.route[1].value = 200;
        emit ied->changed(6, in6);
        QTest::qWait(800);
        QTRY_VERIFY_WITH_TIMEOUT(!link->isBusy(), 5000);

        int aspect = -1, level = -1;
        connect(link, &DeviceLink::outputEvent, this, [&](int o, int st, int v) {
            if (o == 5 && st == 2) aspect = v;
            if (o == 6 && st == 2) level = v;
        });
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text().contains(QLatin1String("Live test"))) a->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(view->liveState(5) >= 0, 3000);

        QVERIFY(!view->liveValueInput(5));
        QVERIFY(!view->liveValueInput(6));
        click(view, view->liveButtonRect(5, 1).center());
        QTRY_COMPARE_WITH_TIMEOUT(aspect, 9, 2000);
        click(view, view->liveButtonRect(5, 0).center());
        QTRY_COMPARE_WITH_TIMEOUT(aspect, 3, 2000);
        click(view, view->liveButtonRect(6, 1).center());
        QTRY_COMPARE_WITH_TIMEOUT(level, 200, 2000);
        click(view, view->liveButtonRect(6, 0).center());
        QTRY_COMPARE_WITH_TIMEOUT(level, 50, 2000);
    }

    void liveServoAndSignal()
    {
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *link = w.findChild<DeviceLink *>();
        auto *oed = w.findChild<OutputEditor *>();

        OutputConfig servo = view->board().outputs[3];
        servo.type = ServoType; servo.data = { { 80, 100, 2, 0 } };
        emit oed->changed(3, servo);
        OutputConfig sig = view->board().outputs[5];
        sig.type = FourToneSignal; sig.data = { { 0, 0, 0, 0 } };
        emit oed->changed(5, sig);
        QTRY_VERIFY_WITH_TIMEOUT(view->board().outputs[5].type == FourToneSignal && !link->isBusy(), 3000);
        QTest::qWait(800);
        QTRY_VERIFY_WITH_TIMEOUT(!link->isBusy(), 5000);

        QVector<int> angles;
        int aspect = -1;
        connect(link, &DeviceLink::outputEvent, this, [&](int o, int st, int v) {
            if (o == 3 && st == 2) angles << v;
            if (o == 5 && st == 2) aspect = v;
        });
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text().contains(QLatin1String("Live test"))) a->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(view->liveState(3) >= 0, 3000);

        // input 3 (absolute) -> other end: every degree is reported
        const int dir = view->liveState(3) ? 0 : 1;
        click(view, view->liveButtonRect(3, 1).center());
        if (dir == 0) click(view, view->liveButtonRect(3, 0).center());
        QTRY_VERIFY_WITH_TIMEOUT(angles.size() >= 20, 3000);
        for (int k = 1; k < angles.size(); ++k)
            QCOMPARE(qAbs(angles[k] - angles[k - 1]), 1);

        // input 5 drives the signal: DCC extended with aspect 0x21
        QVERIFY(view->liveValueInput(5));
        link->dccExt(view->board().inputs[5].address, 0x21);
        QTRY_COMPARE_WITH_TIMEOUT(aspect, 0x21, 2000);
        // DCC extended to the servo's input: the value is the angle, it sweeps there degree by degree
        angles.clear();
        link->dccExt(view->board().inputs[3].address, 90);
        QTRY_VERIFY_WITH_TIMEOUT(!angles.isEmpty() && angles.last() == 90, 3000);
        for (int k = 1; k < angles.size(); ++k)
            QCOMPARE(qAbs(angles[k] - angles[k - 1]), 1);
        angles.clear();
        link->dccExt(view->board().inputs[3].address, 200);  // above 180: ignored
        QTest::qWait(300);
        QVERIFY(angles.isEmpty());
        maybeScreenshot(w, "live_servo_signal.png");
    }

    void liveFrogRelays()
    {
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *link = w.findChild<DeviceLink *>();
        auto *oed = w.findChild<OutputEditor *>();
        OutputConfig t = view->board().outputs[2];             // double pulse turnout with electrofrog on D8 / D9
        t.type = DoublePulse; t.pinA = 5; t.pinB = 6; t.relayA = 8; t.relayB = 9; t.relayC = PinNA;
        t.setSwitchTime(10);
        emit oed->changed(2, t);
        QTRY_VERIFY_WITH_TIMEOUT(view->board().outputs[2].type == DoublePulse && !link->isBusy(), 3000);
        QTest::qWait(800);
        QTRY_VERIFY_WITH_TIMEOUT(!link->isBusy(), 5000);
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text().contains(QLatin1String("Live test"))) a->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(view->liveState(2) >= 0, 3000);
        const int to = view->liveState(2) ? 0 : 1;
        click(view, view->liveButtonRect(2, to).center());
        QTRY_COMPARE_WITH_TIMEOUT(view->liveFrog(2), to ? 1 : 2, 2000);   // A for straight, B for curved
        maybeScreenshot(w, "live_frog.png");
    }

    void routeOnDirection()
    {
        // level crossing: address 30 curved closes (output 1 on), address 30 straight opens (output 1 off)
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *link = w.findChild<DeviceLink *>();
        auto *ied = w.findChild<InputEditor *>();
        InputConfig close = view->board().inputs[4], open = view->board().inputs[5];
        close.type = RouteCurved;   close.address = 30; close.route = { RouteStep{ 1, 1, false, 0 } };
        open.type  = RouteStraight; open.address  = 30; open.route  = { RouteStep{ 1, 0, false, 0 } };
        emit ied->changed(4, close);
        emit ied->changed(5, open);
        QTest::qWait(900);
        QTRY_VERIFY_WITH_TIMEOUT(!link->isBusy(), 5000);
        InputConfig back;
        BoardConfig b; QVERIFY(BoardConfig::fromJson(view->board().toJson(), b, nullptr));
        QCOMPARE(int(b.inputs[4].type), int(RouteCurved));     // board file round trip

        int last = -1;
        connect(link, &DeviceLink::outputEvent, this, [&last](int o, int st, int) { if (o == 1 && st < 2) last = st; });
        link->triggerInput(4, 0);                              // curved -> close
        QTRY_COMPARE_WITH_TIMEOUT(last, 1, 2000);
        last = -1;
        link->triggerInput(4, 1);                              // straight is not for this route
        QTest::qWait(400);
        QCOMPARE(last, -1);
        link->triggerInput(5, 1);                              // straight -> open
        QTRY_COMPARE_WITH_TIMEOUT(last, 0, 2000);
    }

    void pinTakeOver()
    {
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *oed = w.findChild<OutputEditor *>();
        const quint8 pinOf1 = view->board().outputs[1].pinA;
        OutputConfig c = view->board().outputs[0];
        c.pinA = pinOf1;                                     // output 0 takes the pin of output 1
        emit oed->changed(0, c);
        QCOMPARE(int(view->board().outputs[0].pinA), int(pinOf1));
        QCOMPARE(int(view->board().outputs[1].type), int(OutputNotInUse));
        QCOMPARE(int(view->board().outputs[2].type), int(SingleSteady));   // others untouched
        for (int i = 0; i < NumOutputs; ++i)                 // D13 / A7 are never handed out by default
            QVERIFY(view->board().outputs[i].pinA != 13 && view->board().outputs[i].pinA != 21);
    }

    void analogInputEditor()
    {
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *link = w.findChild<DeviceLink *>();
        auto *ied  = w.findChild<InputEditor *>();
        auto *sim  = w.findChild<SimulatedDevice *>();
        QVERIFY(view && ied && sim);
        QCOMPARE(ied->index(), 0);
        InputConfig used = view->board().inputs[0];
        used.type = Latching;
        emit ied->changed(0, used);
        QComboBox *readAs = comboWith(ied, QStringLiteral("Digital"));
        QVERIFY(readAs);
        readAs->setCurrentIndex(readAs->findData(int(AnalogAt)));
        QVERIFY(view->board().inputs[0].isAnalog());
        QCOMPARE(int(view->board().inputs[0].analog), int(AnalogAt));
        QCOMPARE(int(view->board().inputs[0].pin), 14);          // no pin yet: starts on A0
        QCOMPARE(int(view->board().outputs[10].type), int(OutputNotInUse));   // output 10 had A0

        sim->setAnalogValue(0, 437);                             // the live reading comes from the board
        QTRY_COMPARE_WITH_TIMEOUT(ied->analogReading(), 437, 3000);
        QPushButton *use = nullptr;
        for (QPushButton *b : ied->findChildren<QPushButton *>())
            if (b->text() == QLatin1String("Use this value")) use = b;
        QVERIFY(use);
        QTRY_VERIFY(use->isEnabled());
        use->click();
        QCOMPARE(int(view->board().inputs[0].threshold), 440);  // rounded to a step of 8
        QTRY_COMPARE_WITH_TIMEOUT(int(sim->input(0).threshold), 440, 3000);   // live write
        QVERIFY(sim->input(0).isAnalog());
        maybeScreenshot(w, "analog-input.png");

        QSpinBox *threshold = nullptr;
        for (QSpinBox *s : ied->findChildren<QSpinBox *>())
            if (s->maximum() == AnalogMaxThreshold) threshold = s;
        QVERIFY(threshold);
        threshold->setValue(515);
        QCOMPARE(int(view->board().inputs[0].threshold), 512);
        waitIdleOrNot(link);
    }

    void simulateAnalogValue()
    {
        MainWindow w;
        auto *link = w.findChild<DeviceLink *>();
        link->drainMs = 3;
        w.resize(1320, 900);
        w.show();
        ConnectionView *view = w.findChild<ConnectionView *>();
        InputConfig c = view->board().inputs[0];
        c.type = Latching; c.analog = AnalogBelow; c.pin = 15; c.threshold = 512; c.outputId = 0;
        emit w.findChild<InputEditor *>()->changed(0, c);
        w.setMode(MainWindow::ModeSimulate);
        QTRY_VERIFY_WITH_TIMEOUT(view->liveMode(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(view->liveState(0) >= 0, 3000);
        QVERIFY(view->analogValueInput(0));                      // a value instead of a press
        click(view, view->liveButtonRect(0, 1).center());
        auto *d = w.findChild<QDialog *>(QStringLiteral("analogValueDialog"));
        QVERIFY(d);
        QSpinBox *spin = d->findChild<QSpinBox *>();
        QVERIFY(spin);
        spin->setValue(100);                                     // below 512: pressed = curved / off
        QTRY_COMPARE_WITH_TIMEOUT(view->liveState(0), 0, 2000);
        maybeScreenshot(w, "analog-simulate.png");
        spin->setValue(1000);                                    // released
        QTRY_COMPARE_WITH_TIMEOUT(view->liveState(0), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(w.findChild<InputEditor *>()->analogReading(), 1000, 3000);   // read back (GET_ANALOG)
        d->close();
        w.setMode(MainWindow::ModeConfigure);
    }

    void analogInputsShareChannel()
    {
        MainWindow w;
        ConnectionView *view = w.findChild<ConnectionView *>();
        auto *ied = w.findChild<InputEditor *>();
        auto analog = [&](int i, int threshold) {
            InputConfig c = view->board().inputs[i];
            c.type = Latching; c.analog = AnalogAt; c.pin = 14; c.threshold = quint16(threshold);
            emit ied->changed(i, c);
        };
        analog(0, 296);
        analog(1, 600);                                          // the same channel: a resistor ladder
        QVERIFY(view->board().inputs[0].isAnalog());
        QCOMPARE(int(view->board().inputs[0].type), int(Latching));
        QVERIFY(!view->inputConflictShown(0));
        QVERIFY(!view->inputConflictShown(1));
        QVERIFY(!Ui::pinClash(Ui::pinUsage(view->board())[14]));

        InputConfig digital = view->board().inputs[2];           // a button on A0 takes the pin
        digital.type = Latching; digital.pin = 14;
        emit ied->changed(2, digital);
        QCOMPARE(int(view->board().inputs[0].type), int(InputNotInUse));
        QCOMPARE(int(view->board().inputs[1].type), int(InputNotInUse));
        QCOMPARE(int(view->board().inputs[2].pin), 14);
    }

    void customValuesWriteRoute()
    {
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *link = w.findChild<DeviceLink *>();
        auto *ied  = w.findChild<InputEditor *>();
        auto *sim  = w.findChild<SimulatedDevice *>();
        InputConfig c = view->board().inputs[0];
        c.type = Latching; c.outputId = 0;
        emit ied->changed(0, c);
        QCheckBox *custom = nullptr;
        for (QCheckBox *b : ied->findChildren<QCheckBox *>())
            if (b->text() == QLatin1String("Custom values")) custom = b;
        QVERIFY(custom);
        QVERIFY(!custom->isChecked());
        custom->setChecked(true);
        QVERIFY(view->board().inputs[0].hasCustomValues());
        QTRY_VERIFY_WITH_TIMEOUT(sim->input(0).hasCustomValues(), 3000);      // 2 marked steps on the board
        QCOMPARE(sim->input(0).route.size(), 2);
        maybeScreenshot(w, "custom-values.png");
        waitIdleOrNot(link);
        custom->setChecked(false);
        QVERIFY(view->board().inputs[0].route.isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(sim->input(0).route.isEmpty(), 3000);        // the slot is cleared
        waitIdleOrNot(link);
    }

    void soundOutputOnD1()
    {
        MainWindow w;
        ConnectionView *view = w.findChild<ConnectionView *>();
        auto *oed = w.findChild<OutputEditor *>();
        InputConfig direct = view->board().inputs[0];           // an absolute input on output 0
        direct.type = Latching; direct.outputId = 0;
        emit w.findChild<InputEditor *>()->changed(0, direct);
        QVERIFY(!view->board().inputs[0].hasCustomValues());
        QCOMPARE(oed->index(), 0);
        QComboBox *type = comboWith(oed, outputTypeName(SingleSteady));
        QVERIFY(type);
        type->setCurrentIndex(type->findData(int(SoundEffect)));
        QCOMPARE(int(view->board().outputs[0].type), int(SoundEffect));
        QCOMPARE(int(view->board().outputs[0].pinA), 1);        // always TX (D1)
        QVERIFY(view->board().inputs[0].hasCustomValues());     // the input chooses the track now
        QVERIFY(view->board().inputs[0].route[0].raw && view->board().inputs[0].route[0].value == 0);
        QVERIFY(view->board().inputs[0].route[1].raw && view->board().inputs[0].route[1].value == 1);
        QComboBox *pinA = comboWith(oed, trGpio("—  not used"));
        QVERIFY(pinA);
        QCOMPARE(Ui::currentData(pinA), 1);
        QVERIFY(!pinA->isEnabled());
        for (QSpinBox *s : oed->findChildren<QSpinBox *>())          // no tracks on the output itself
            QVERIFY(s->specialValueText() != QLatin1String("stop"));
        if (!qgetenv("GPIOCORE_SHOT_DIR").isEmpty())
        {
            w.resize(1320, 900);
            w.show();
            w.showWorkspace();
            maybeScreenshot(w, "sound-output.png");
        }

        OutputConfig second = view->board().outputs[1];
        second.type = SoundEffect; second.pinA = SoundPin; second.data = { { 5, 6, 0, 0 } };
        emit oed->changed(1, second);                            // two sound outputs share D1
        QCOMPARE(int(view->board().outputs[0].type), int(SoundEffect));
        QVERIFY(!view->outputConflictShown(0));
        QVERIFY(!view->outputConflictShown(1));

        InputConfig in = view->board().inputs[3];                // an input on D1 takes the pin from both
        in.type = Latching; in.pin = 1;
        emit w.findChild<InputEditor *>()->changed(3, in);
        QCOMPARE(int(view->board().outputs[0].type), int(OutputNotInUse));
        QCOMPARE(int(view->board().outputs[1].type), int(OutputNotInUse));
    }

    void pwmFadeTimeEditor()
    {
        MainWindow w;
        ConnectionView *view = w.findChild<ConnectionView *>();
        auto *oed = w.findChild<OutputEditor *>();
        QComboBox *type = comboWith(oed, outputTypeName(SingleSteady));
        QVERIFY(type);
        type->setCurrentIndex(type->findData(int(Pwm)));
        QCOMPARE(int(view->board().outputs[0].type), int(Pwm));
        QCOMPARE(int(view->board().outputs[0].data[1]), 0);      // default: no fading
        QSpinBox *fade = nullptr;
        for (QSpinBox *s : oed->findChildren<QSpinBox *>())
            if (s->specialValueText() == QLatin1String("at once")) fade = s;
        QVERIFY(fade);
        fade->setValue(60);
        QCOMPARE(int(view->board().outputs[0].data[1]), 60);
        QCOMPARE(int(view->board().outputs[0].data[0]), 128);    // the brightness stays
        bool note = false;
        for (QLabel *l : oed->findChildren<QLabel *>())
            note |= l->text().contains(QLatin1String("takes 60 s, a half change 30 s"));
        QVERIFY(note);
    }

    void qrStructure()
    {
        const auto m = Qr::encode("http://192.168.178.23:8080/?t=ab12cd");
        QCOMPARE(m.size(), 29);                              // 37 bytes -> version 3 (29 x 29)
        for (int i = 0; i < 7; ++i)                          // finder pattern: dark outer ring
            QVERIFY(m[0][i] && m[i][0] && m[6][i] && m[0][28 - i]);
        QVERIFY(!m[1][1] && m[2][2] && !m[7][7]);            // light ring, dark core, separator
        for (int i = 8; i < 21; ++i)                         // timing pattern
            QCOMPARE(bool(m[6][i]), i % 2 == 0);
        QVERIFY(m[29 - 8][8]);                               // the dark module
        QVERIFY(Qr::encode(QByteArray(200, 'x')).isEmpty()); // too long
    }

    void phoneRemoteServer()
    {
        WebRemote web;
        web.stateProvider = [] { QJsonObject o; o["board"] = QStringLiteral("T"); return o; };
        int live = -1;
        connect(&web, &WebRemote::liveRequested, this, [&live](int, int a) { live = a; });
        QVERIFY(web.start(18080));
        auto get = [&web](const QByteArray &line) {
            QTcpSocket s;                                    // the server runs in this thread: never block
            s.connectToHost(QHostAddress::LocalHost, web.port());
            QTest::qWaitFor([&s] { return s.state() == QAbstractSocket::ConnectedState; }, 2000);
            s.write(line + " HTTP/1.1\r\nHost: x\r\n\r\n");
            QByteArray all;
            QTest::qWaitFor([&s, &all] { all += s.readAll(); return s.state() == QAbstractSocket::UnconnectedState; }, 3000);
            return all + s.readAll();
        };
        QVERIFY(get("GET /?t=wrong").startsWith("HTTP/1.1 403"));
        QVERIFY(get("GET /?t=" + web.token().toLatin1()).contains("GPIOcore remote"));
        QVERIFY(get("GET /api/state?t=" + web.token().toLatin1()).contains("\"board\":\"T\""));
        QVERIFY(get("POST /api/live?t=" + web.token().toLatin1() + "&o=2&a=123").startsWith("HTTP/1.1 200"));
        QTRY_COMPARE(live, 123);
        web.stop();
    }

    void removeOutputHidesIt()
    {
        MainWindow w;
        ConnectionView *view = startDemo(w);
        QVERIFY(view->outputShown(3));
        emit view->removeRequested(true, 3);                 // trash on output 3 (input 3 drives it)
        QCOMPARE(int(view->board().outputs[3].type), int(OutputNotInUse));
        QCOMPARE(int(view->board().inputs[3].type), int(InputNotInUse));
        QVERIFY(!view->outputShown(3));
        QVERIFY(!view->inputShown(3));
    }

    void presetButtonApplies()
    {
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *link = w.findChild<DeviceLink *>();
        QAction *apply = nullptr;
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text() == QLatin1String("Apply")) apply = a;
        QVERIFY(apply);
        QVERIFY(apply->isEnabled());
        QComboBox *combo = nullptr;
        for (QComboBox *c : w.findChildren<QComboBox *>())
            if (c->count() == 9 && c->itemText(3).contains(QLatin1String("single"))) combo = c;
        QVERIFY(combo);
        combo->setCurrentIndex(3);                          // 16 single steady
        QTimer::singleShot(200, [] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Yes)->click();
        });
        apply->trigger();
        for (int o = 0; o < NumOutputs; ++o)
            QCOMPARE(int(view->board().outputs[o].type), int(presetBoard(3).outputs[o].type));
        QTest::qWait(800);
        QTRY_VERIFY_WITH_TIMEOUT(!link->isBusy(), 10000);
        bool clean = false;                                  // live written: nothing left to write
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text() == QLatin1String("Write changes")) clean = true;
        QVERIFY(clean);
    }

    void phoneLiveReachesTheServo()
    {
        MainWindow w;
        ConnectionView *view = startDemo(w);                  // a (simulated) board in Configure mode
        auto *link = w.findChild<DeviceLink *>();
        auto *oed = w.findChild<OutputEditor *>();
        OutputConfig servo = view->board().outputs[3];
        servo.type = ServoType; servo.data = { { 60, 120, 20, 0 } };
        emit oed->changed(3, servo);
        QTest::qWait(800);
        QTRY_VERIFY_WITH_TIMEOUT(!link->isBusy(), 5000);
        auto *web = w.findChild<WebRemote *>();
        QVERIFY(web);
        QVERIFY(web->start(18090));
        int sent = -1;
        connect(link, &DeviceLink::logMessage, this, [&sent](const QString &k, const QString &t) {
            if (k == QLatin1String("TX") && t.startsWith(QLatin1String("SERVO_LIVE"))) sent = 1;
        });
        QTcpSocket s;
        s.connectToHost(QHostAddress::LocalHost, web->port());
        QTest::qWaitFor([&s] { return s.state() == QAbstractSocket::ConnectedState; }, 2000);
        s.write("POST /api/live?t=" + web->token().toLatin1() + "&o=3&a=77 HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\n\r\n");
        QTest::qWaitFor([&s] { return s.state() == QAbstractSocket::UnconnectedState; }, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(sent, 1, 2000);

        // sliding the straight position on the phone: servo follows, value stored, GUI shows that servo
        click(view, view->outputRect(0).center());           // the GUI is looking at another output
        sent = -1;
        QTcpSocket s2;
        s2.connectToHost(QHostAddress::LocalHost, web->port());
        QTest::qWaitFor([&s2] { return s2.state() == QAbstractSocket::ConnectedState; }, 2000);
        s2.write("POST /api/adjust?t=" + web->token().toLatin1() + "&o=3&w=1&a=140 HTTP/1.1\r\nHost: x\r\n\r\n");
        QTest::qWaitFor([&s2] { return s2.state() == QAbstractSocket::UnconnectedState; }, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(sent, 1, 2000);
        QCOMPARE(int(view->board().outputs[3].data[1]), 140);
        QCOMPARE(int(view->board().outputs[3].state), 1);
        QCOMPARE(oed->index(), 3);
        bool shown = false;
        for (QSlider *sl : oed->findChildren<QSlider *>())
            if (sl->isVisibleTo(oed) && sl->value() == 140) shown = true;
        QVERIFY(shown);
        web->stop();
    }

    void routeByDragging()
    {
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *ied = w.findChild<InputEditor *>();

        // make input 4 a route without steps
        click(view, view->inputRect(4).center());
        QCOMPARE(ied->index(), 4);
        InputConfig c = view->board().inputs[4];
        c.type = Route;
        c.route.clear();
        c.linked.fill(LinkNone);
        emit ied->changed(4, c);
        QVERIFY(!inputConnected(view->board().inputs[4]));
        QVERIFY(view->inputColor(4) != Ui::inputColor(4));        // grey while the route is empty

        drag(view, view->outputRect(10).center(), view->inputRect(4).center());
        drag(view, view->outputRect(11).center(), view->inputRect(4).center());
        drag(view, view->outputRect(12).center(), view->inputRect(4).center());
        QVector<RouteStep> steps = routeSteps(view->board().inputs[4]);
        QCOMPARE(steps.size(), 3);
        QCOMPARE(steps[0].output, 10);
        QCOMPARE(steps[2].output, 12);
        QCOMPARE(view->inputColor(4), Ui::inputColor(4));
        maybeScreenshot(w, "route.png");

        // click the label of step 1 and press Delete
        const QPointF label = view->routeLabelPos(4, 0);
        QVERIFY(label.x() >= 0);
        click(view, label);
        QTest::keyClick(view, Qt::Key_Delete);
        steps = routeSteps(view->board().inputs[4]);
        QCOMPARE(steps.size(), 2);
        QCOMPARE(steps[0].output, 11);
        QCOMPARE(int(view->board().inputs[4].type), int(Route));
    }

    void frogInterlock()
    {
        MainWindow w;
        auto *link = w.findChild<DeviceLink *>();
        link->drainMs = 3;
        w.applyPresetOnStart(2);                  // servos with electrofrog relays
        ConnectionView *view = startDemo(w);
        QTest::qWaitFor([link] { return !link->isBusy(); }, 8000);
        QVERIFY(view->board().outputs[1].relayA != PinNA);

        click(view, view->outputRect(1).center());
        auto *oed = w.findChild<OutputEditor *>();
        QComboBox *frog = comboWith(oed, QStringLiteral("Unifrog"));
        QVERIFY(frog);
        frog->setCurrentIndex(frog->findData(int(OutputEditor::FrogUni)));
        const OutputConfig &o = view->board().outputs[1];
        QCOMPARE(o.relayA, PinNA);                // electrofrog relays removed: one frog mode only
        QCOMPARE(o.relayB, PinNA);
    }

    void onlyOneRecorder()
    {
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *ied = w.findChild<InputEditor *>();

        click(view, view->inputRect(3).center());
        InputConfig c = view->board().inputs[3];
        c.type = Recorder;
        c.pin = 8;
        emit ied->changed(3, c);

        click(view, view->inputRect(5).center());
        QCOMPARE(ied->index(), 5);
        QComboBox *fn = comboWith(ied, QStringLiteral("Recorder"));
        QVERIFY(fn);
        auto *model = qobject_cast<QStandardItemModel *>(fn->model());
        QVERIFY(model);
        const int row = fn->findText(QStringLiteral("Recorder"));
        QVERIFY(!model->item(row)->isEnabled());  // input 3 already is the recorder

        click(view, view->inputRect(3).center());
        QVERIFY(model->item(row)->isEnabled());   // the recorder itself may stay one
    }

    void servoAngleStep()
    {
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *ied = w.findChild<InputEditor *>();
        auto *oed = w.findChild<OutputEditor *>();

        click(view, view->outputRect(7).center());
        OutputConfig servo = view->board().outputs[7];
        servo.type = ServoType;
        servo.data = { { 40, 140, 10, 0 } };
        emit oed->changed(7, servo);

        click(view, view->inputRect(2).center());
        InputConfig c = view->board().inputs[2];
        c.type = Route;
        c.route.clear();
        emit ied->changed(2, c);
        drag(view, view->outputRect(7).center(), view->inputRect(2).center());
        drag(view, view->outputRect(7).center(), view->inputRect(2).center());   // servo twice
        QCOMPARE(view->board().inputs[2].route.size(), 2);

        QComboBox *mode = comboWith(ied, QStringLiteral("Angle"));
        QVERIFY(mode);
        mode->setCurrentIndex(2);                 // curved, straight, angle
        const RouteStep s = view->board().inputs[2].route[0];
        QVERIFY(s.raw);
        QVERIFY(s.value >= ServoAngleMin && s.value <= ServoAngleMax);
        QCOMPARE(int(s.delay), int(DefaultStepDelay));
        maybeScreenshot(w, "servo_route.png");
    }

    void firstStepAlwaysOpenAndDeletable()
    {
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *ied = w.findChild<InputEditor *>();

        click(view, view->inputRect(6).center());
        InputConfig c = view->board().inputs[6];
        c.type = Route;
        c.route.clear();
        emit ied->changed(6, c);

        // empty route: one row is open, choosing an output there creates step 1
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        auto *first = ied->findChild<QComboBox *>(QStringLiteral("firstStep"));
        QVERIFY(first);
        first->setCurrentIndex(first->findData(9));
        QCoreApplication::processEvents();
        QCOMPARE(view->board().inputs[6].route.size(), 1);
        QCOMPARE(view->board().inputs[6].route[0].output, 9);

        // delete it with the bin button: back to the open empty row
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QToolButton *bin = nullptr;
        for (QToolButton *b : ied->findChildren<QToolButton *>(QStringLiteral("stepDelete")))
            if (b->isVisible() || !bin) bin = b;
        QVERIFY(bin);
        bin->click();
        QCoreApplication::processEvents();
        QVERIFY(view->board().inputs[6].route.isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(ied->findChild<QComboBox *>(QStringLiteral("firstStep")));
    }
};

QTEST_MAIN(TestGui)
#include "tst_gui.moc"
