// Main window against the firmware simulator: edit -> dirty -> write ->
// read back -> clean, presets, and connecting inputs / outputs by dragging.

#include "boardtools.h"
#include "connections.h"
#include "connectionview.h"
#include "devicelink.h"
#include "infotexts.h"
#include "inputeditor.h"
#include "mainwindow.h"
#include "outputeditor.h"
#include "simulateddevice.h"
#include "uihelpers.h"
#include "qrcode.h"
#include "webremote.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QListWidget>
#include <QRadioButton>
#include <QTemporaryDir>
#include <QCheckBox>
#include <QDialog>
#include <QDockWidget>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QSettings>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QSlider>
#include <QStatusBar>
#include <QStandardItemModel>
#include <QMenuBar>
#include <QPainter>
#include <QTabWidget>
#include <QTcpSocket>

#include <QAction>
#include <QComboBox>
#include <QMouseEvent>
#include <QToolButton>
#include <QSignalSpy>
#include <QtTest>

using namespace Gpio;

// Answers the popups of the window under test: message boxes (the text is kept), file dialogs (with
// 'file') and other dialogs (onDialog returns true when it handled one).
class Popups : public QObject
{
public:
    QStringList texts;
    QString file;
    QMessageBox::StandardButton button = QMessageBox::Yes;
    std::function<bool(QDialog *)> onDialog;

    Popups()
    {
        m_timer.setInterval(30);
        connect(&m_timer, &QTimer::timeout, this, &Popups::check);
        m_timer.start();
    }

private:
    void check()
    {
        for (QWidget *top : QApplication::topLevelWidgets())
        {
            if (!top->isVisible())
                continue;
            if (auto *m = qobject_cast<QMessageBox *>(top))
            {
                texts << m->text();
                QAbstractButton *b = m->button(button) ? m->button(button) : m->button(QMessageBox::Ok);
                if (b) b->click(); else m->reject();
            }
            else if (auto *f = qobject_cast<QFileDialog *>(top))
            {
                if (auto *name = f->findChild<QLineEdit *>(QStringLiteral("fileNameEdit")))
                    name->setText(QDir::toNativeSeparators(file));   // typed: the full path
                static_cast<QDialog *>(f)->accept();
            }
            else if (auto *d = qobject_cast<QDialog *>(top); d && onDialog)
                onDialog(d);
        }
    }
    QTimer m_timer;
};

static QString writeBoard(QTemporaryDir &dir, const char *name, const BoardConfig &b)
{
    const QString path = dir.filePath(QLatin1String(name));
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(b.toJson()).toJson());
    return path;
}

static bool readBoard(const QString &path, BoardConfig &b)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) && BoardConfig::fromJson(QJsonDocument::fromJson(f.readAll()).object(), b, nullptr);
}

// A preset as shipped: presets/<file>.json
static BoardConfig shippedPreset(const char *file)
{
    BoardConfig b;
    readBoard(QDir(MainWindow::presetsDir()).filePath(QLatin1String(file) + QStringLiteral(".json")), b);
    return b;
}

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

    // A factory-fresh board's configuration on screen, offline: what the simulated board in the tests holds
    // (the start screen is an empty board with 1 input and 1 output)
    static void factoryScreen(MainWindow &w)
    {
        QTemporaryDir dir;
        QVERIFY(w.openExample(writeBoard(dir, "factory.json", SimulatedDevice::factoryDefaults())));
    }

    // Shown main window connected to the demo simulator, with the board's (factory) configuration on screen.
    ConnectionView *startDemo(MainWindow &w)
    {
        factoryScreen(w);
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

    // The item data a combo offers at the current user level (rows not hidden)
    static QList<int> offered(QComboBox *combo)
    {
        QList<int> data;
        auto *view = qobject_cast<QListView *>(combo->view());
        for (int i = 0; i < combo->count(); ++i)
            if (!view || !view->isRowHidden(i))
                data << combo->itemData(i).toInt();
        return data;
    }

    static QAction *levelAction(MainWindow &w, const QString &name)
    {
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text() == name && a->isCheckable()) return a;
        return nullptr;
    }

    static bool visibleLabel(QWidget *root, const QString &part, const QString &objectName = QString())
    {
        for (QLabel *l : root->findChildren<QLabel *>())
            if (l->isVisibleTo(root) && (objectName.isEmpty() || l->objectName() == objectName) && l->text().contains(part))
                return true;
        return false;
    }

    // The toolbar's board type combo; choosing an item there is what the user does
    static void chooseBoardType(MainWindow &w, int type)
    {
        for (QComboBox *c : w.findChildren<QComboBox *>())
            if (c->count() == BoardTypeCount && c->itemText(BoardDccNext) == boardTypeName(BoardDccNext))
            {
                c->setCurrentIndex(type);
                emit c->activated(type);
                return;
            }
        QFAIL("no board type combo");
    }

private slots:
    void initTestCase()
    {
        Ui::setLevel(Ui::Expert);               // most tests use every feature; the level tests switch themselves
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);   // file dialogs Popups can answer
    }

    void cleanup()
    {
        Ui::setLevel(Ui::Expert);
    }

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
        w.applyPresetOnStart(QStringLiteral("5-servos-electrofrog"));
        ConnectionView *view = startDemo(w);      // read all
        QTest::qWaitFor([link] { return !link->isBusy(); }, 8000);
        QCOMPARE(int(view->board().outputs[2].type), int(ServoType));
        QCOMPARE(view->board().outputs[2].pinA, quint8(9));
        // Before the preset output 6 used D9; now only output 2 does
        QVERIFY(!view->outputConflictShown(2));
        QVERIFY(!view->outputConflictShown(0));

        // Giving D9 to output 3 takes it from output 2: no conflict, output 2 keeps its type without the pin
        auto *ed = w.findChild<OutputEditor *>();
        OutputConfig c = ed->config();
        c.type = SingleSteady;
        c.pinA = 9;
        emit ed->changed(3, c);
        QVERIFY(!view->outputConflictShown(3));
        QCOMPARE(int(view->board().outputs[2].type), int(ServoType));
        QCOMPARE(int(view->board().outputs[2].pinA), int(PinNA));
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
        // the other output keeps its type, settings and links; only that pin becomes 'not used'
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *oed = w.findChild<OutputEditor *>();
        const quint8 pinOf1 = view->board().outputs[1].pinA;
        const OutputConfig before = view->board().outputs[1];
        OutputConfig c = view->board().outputs[0];
        c.pinA = pinOf1;                                     // output 0 takes the pin of output 1
        emit oed->changed(0, c);
        QCOMPARE(int(view->board().outputs[0].pinA), int(pinOf1));
        QCOMPARE(int(view->board().outputs[1].type), int(before.type));   // kept
        QCOMPARE(int(view->board().outputs[1].pinA), int(PinNA));
        QCOMPARE(view->board().outputs[1].data, before.data);
        QCOMPARE(outputSources(view->board(), 1), QVector<int>{ 1 });      // its input still drives it
        QVERIFY(view->outputShown(1));
        QVERIFY(w.statusBar()->currentMessage().contains(QLatin1String("Out 2 keeps its settings")));
        click(view, view->outputRect(1).center());            // its panel: the missing pin is a warning
        bool warned = false;
        for (QLabel *l : oed->findChildren<QLabel *>(QStringLiteral("error")))
            warned |= l->isVisibleTo(oed) && l->text().contains(QLatin1String("Pin A is not set."));
        QVERIFY(warned);
        QCOMPARE(int(view->board().outputs[2].type), int(SingleSteady));   // others untouched
        for (int i = 0; i < NumOutputs; ++i)                 // D13 / A7 are never handed out by default
            QVERIFY(view->board().outputs[i].pinA != 13 && view->board().outputs[i].pinA != 21);

        auto *ied = w.findChild<InputEditor *>();            // an input taking a pin from another input
        InputConfig a = view->board().inputs[3];
        a.type = Sensor; a.pin = 0;
        emit ied->changed(3, a);
        InputConfig b = view->board().inputs[4];
        b.type = Latching; b.pin = 0;
        emit ied->changed(4, b);
        QCOMPARE(int(view->board().inputs[3].type), int(Sensor));
        QCOMPARE(int(view->board().inputs[3].pin), int(PinNA));
        QVERIFY(view->inputShown(3));
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
        QCOMPARE(int(view->board().outputs[10].pinA), int(PinNA));            // output 10 had A0: kept without it
        QVERIFY(view->board().outputs[10].type != OutputNotInUse);

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
        QTest::qWait(300);                                       // the board samples it twice (50 ms apart), then switches
        QTRY_COMPARE_WITH_TIMEOUT(view->liveState(0), 0, 2000);
        maybeScreenshot(w, "analog-simulate.png");
        spin->setValue(1000);                                    // released
        QTRY_COMPARE_WITH_TIMEOUT(view->liveState(0), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(w.findChild<InputEditor *>()->analogReading(), 1000, 3000);   // read back (GET_ANALOG)
        auto *sim = w.findChild<SimulatedDevice *>();
        QSlider *slider = d->findChild<QSlider *>();
        QVERIFY(sim && slider);
        slider->setSliderDown(true);                             // dragging: nothing is sent yet
        for (int v : { 900, 600, 200 })
            slider->setValue(v);
        QCOMPARE(spin->value(), 200);
        QTest::qWait(150);
        QCOMPARE(sim->analogValue(1), 1000);
        QCOMPARE(view->liveState(0), 1);
        slider->setSliderDown(false);                            // released: the value goes out
        QCOMPARE(sim->analogValue(1), 200);
        QTRY_COMPARE_WITH_TIMEOUT(view->liveState(0), 0, 2000);
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
        QCOMPARE(int(view->board().inputs[0].type), int(Latching));   // kept, without the pin
        QCOMPARE(int(view->board().inputs[0].pin), int(PinNA));
        QVERIFY(!view->board().inputs[0].isAnalog());
        QCOMPARE(int(view->board().inputs[1].pin), int(PinNA));
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
        QCOMPARE(int(view->board().outputs[0].type), int(SoundEffect));   // kept, without the pin
        QCOMPARE(int(view->board().outputs[0].pinA), int(PinNA));
        QCOMPARE(int(view->board().outputs[1].pinA), int(PinNA));
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
        auto *combo = w.findChild<QComboBox *>(QStringLiteral("presetCombo"));
        QVERIFY(combo);
        combo->setCurrentIndex(combo->findText(QStringLiteral("16 single steady")));
        QVERIFY(combo->currentIndex() >= 0);
        QTimer::singleShot(200, [] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Yes)->click();
        });
        apply->trigger();
        for (int o = 0; o < NumOutputs; ++o)
            QCOMPARE(int(view->board().outputs[o].type), int(shippedPreset("16-single-steady").outputs[o].type));
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
        w.applyPresetOnStart(QStringLiteral("5 servos + electrofrog relays"));   // by its name
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

        // relays that switch on LOW: only with a relay pin
        QCheckBox *invert = nullptr;
        for (QCheckBox *b : oed->findChildren<QCheckBox *>())
            if (b->text() == QLatin1String("Relays switch on LOW (inverted)")) invert = b;
        QVERIFY(invert && invert->isVisibleTo(oed));
        QVERIFY(!invert->isEnabled());
        QComboBox *relayC = nullptr;
        for (QLabel *l : oed->findChildren<QLabel *>())
            if (l->text() == QLatin1String("Unifrog relay")) relayC = qobject_cast<QComboBox *>(l->buddy());
        QVERIFY(relayC);
        relayC->setCurrentIndex(relayC->findData(12));
        QCOMPARE(int(o.relayC), 12);
        QVERIFY(invert->isEnabled());
        invert->setChecked(true);
        QVERIFY(view->board().outputs[1].invertFrog);
        QCOMPARE(int(view->board().outputs[1].relayC), 12);

        // min > max is allowed (the firmware sweeps both ways): no warning
        OutputConfig servo = view->board().outputs[1];
        servo.data[0] = 140; servo.data[1] = 40;
        emit oed->changed(1, servo);
        for (QLabel *l : oed->findChildren<QLabel *>(QStringLiteral("error")))
            QVERIFY2(!l->isVisibleTo(oed) || !l->text().contains(QLatin1String("larger than max")), qPrintable(l->text()));
        QCOMPARE(int(view->board().outputs[1].servoMin()), 140);
    }

    void noRecorderOrFeedback()
    {
        MainWindow w;
        startDemo(w);
        auto *ied = w.findChild<InputEditor *>();
        QComboBox *fn = comboWith(ied, QStringLiteral("Direct control"));
        QVERIFY(fn);
        QCOMPARE(fn->count(), 4);                 // direct control, route, sensor, not in use
        QVERIFY(fn->findText(QStringLiteral("Sensor (occupancy)")) >= 0);
        QCOMPARE(fn->findText(QStringLiteral("Recorder")), -1);
        QCOMPARE(fn->findText(QStringLiteral("Feedback")), -1);
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

    void namesReplaceNumbers()
    {
        // the user's name replaces "In n" / "Out n" everywhere; without a name the number is shown
        QTemporaryDir dir;
        BoardConfig b;
        b.outputs[0].type = SingleSteady; b.outputs[0].pinA = 3;
        b.outputs[1].type = ServoType;    b.outputs[1].pinA = 9; b.outputs[1].name = QStringLiteral("Turnout station left side");
        b.outputs[2].type = SingleSteady; b.outputs[2].pinA = 4;
        b.inputs[0].type = Latching;  b.inputs[0].address = 1; b.inputs[0].outputId = 0;
        b.inputs[1].type = Momentary; b.inputs[1].address = 2; b.inputs[1].outputId = 1; b.inputs[1].name = QStringLiteral("Button");
        b.inputs[2].type = Latching;  b.inputs[2].address = 3; b.inputs[2].outputId = 2;
        MainWindow w;
        w.resize(1320, 900);
        w.show();
        ConnectionView *view = w.findChild<ConnectionView *>();
        auto *oed = w.findChild<OutputEditor *>();
        auto *ied = w.findChild<InputEditor *>();
        QVERIFY(w.loadFile(writeBoard(dir, "names.json", b)));
        auto nameField = [&w](QWidget *editor) -> QLineEdit * {     // the name field above an editor
            for (QLineEdit *e : w.findChildren<QLineEdit *>())
                if (e->placeholderText().startsWith(QLatin1String("Name, e.g.")) && e->parentWidget()->isAncestorOf(editor))
                    return e;
            return nullptr;
        };
        auto rename = [](QLineEdit *field, const QString &name) {
            field->setFocus();
            QTest::keyClick(field, Qt::Key_A, Qt::ControlModifier);
            QTest::keyClick(field, Qt::Key_Backspace);
            QTest::keyClicks(field, name);
        };

        click(view, view->outputRect(0).center());
        QCOMPARE(oed->index(), 0);
        QLineEdit *outName = nameField(oed);
        QVERIFY(outName);
        rename(outName, QStringLiteral("yolo1"));
        click(view, view->inputRect(0).center());
        QCOMPARE(ied->index(), 0);
        QLineEdit *inName = nameField(ied);
        QVERIFY(inName && inName != outName);
        rename(inName, QStringLiteral("Port 1"));

        // canvas: the top line of the block
        QCOMPARE(view->outputTitle(0), QStringLiteral("yolo1"));
        QCOMPARE(view->outputTitle(2), QStringLiteral("Out 3"));
        QCOMPARE(view->inputTitle(0), QStringLiteral("Port 1"));
        QCOMPARE(view->inputTitle(2), QStringLiteral("In 3"));
        // editor title: the name with the number after it
        QVERIFY(visibleLabel(ied, QStringLiteral("Port 1"), QStringLiteral("title")));
        QVERIFY(visibleLabel(ied, QStringLiteral("Input 1"), QStringLiteral("title")));
        // the input editor's output list: name (or number) and type
        QComboBox *outs = comboWith(ied, QStringLiteral("yolo1 · "));
        QVERIFY(outs);
        QCOMPARE(outs->itemText(0), QStringLiteral("yolo1 · %1").arg(outputTypeName(SingleSteady)));
        QCOMPARE(outs->itemText(1), QStringLiteral("Turnout station left side · %1").arg(outputTypeName(ServoType)));
        QCOMPARE(outs->itemText(2), QStringLiteral("Out 3 · %1").arg(outputTypeName(SingleSteady)));
        // pin lists: who uses a pin
        bool marked = false;
        for (QComboBox *c : ied->findChildren<QComboBox *>())
            for (int k = 0; k < c->count(); ++k)
                marked |= c->itemData(k).toInt() == 3 && c->itemText(k).contains(QLatin1String("in use: yolo1"));
        QVERIFY(marked);
        // the output editor's detector lists
        click(view, view->outputRect(0).center());
        QVERIFY(visibleLabel(oed, QStringLiteral("yolo1"), QStringLiteral("title")));
        QVERIFY(comboWith(oed, QStringLiteral("Port 1 · %1").arg(inputTypeName(Latching))));
        maybeScreenshot(w, "names.png");
        if (!qgetenv("GPIOCORE_SHOT_DIR").isEmpty())         // the canvas alone, the trash bin of 'yolo1' shown
        {
            mouse(view, QEvent::MouseMove, view->outputRect(0).center(), Qt::NoButton);
            view->grab().save(QString::fromLocal8Bit(qgetenv("GPIOCORE_SHOT_DIR")) + QStringLiteral("/names-canvas.png"));
        }

        // a warning: input 1 drives an output that became a block signal
        QComboBox *type = comboWith(oed, outputTypeName(SingleSteady));
        QVERIFY(type);
        type->setCurrentIndex(type->findData(int(BlockSignal)));
        click(view, view->inputRect(0).center());
        QVERIFY(visibleLabel(ied, QStringLiteral("yolo1 is a block signal"), QStringLiteral("error")));

        // no name: the number again
        click(view, view->outputRect(0).center());
        rename(outName, QString());
        QCOMPARE(view->outputTitle(0), QStringLiteral("Out 1"));
        QVERIFY(visibleLabel(oed, QStringLiteral("Output 1"), QStringLiteral("title")));
        QCOMPARE(outs->itemText(0), QStringLiteral("Out 1 · %1").arg(outputTypeName(BlockSignal)));
        click(view, view->inputRect(0).center());
        QVERIFY(visibleLabel(ied, QStringLiteral("Out 1 is a block signal"), QStringLiteral("error")));
    }

    void blockSignalEditor()
    {
        MainWindow w;
        w.resize(1320, 900);
        w.show();
        factoryScreen(w);
        ConnectionView *view = w.findChild<ConnectionView *>();
        auto *oed = w.findChild<OutputEditor *>();
        auto *ied = w.findChild<InputEditor *>();
        InputConfig sensor = view->board().inputs[4];
        sensor.type = Sensor; sensor.pin = 19; sensor.address = AddrNone;
        emit ied->changed(4, sensor);
        // default lamp colours: A green, B yellow, C red, D white
        QCOMPARE(Ui::lampColor(OutputConfig(), 0), QColor(0x22, 0xC5, 0x5E));
        QCOMPARE(Ui::lampColor(OutputConfig(), 1), QColor(0xFA, 0xCC, 0x15));
        QCOMPARE(Ui::lampColor(OutputConfig(), 2), QColor(0xEF, 0x44, 0x44));
        QCOMPARE(Ui::lampColor(OutputConfig(), 3), QColor(0xF8, 0xFA, 0xFC));

        QComboBox *type = comboWith(oed, outputTypeName(SingleSteady));
        QVERIFY(type);
        type->setCurrentIndex(type->findData(int(BlockSignal)));
        QCOMPARE(type->currentText(), QStringLiteral("Block signal"));
        // defaults: own block = the first sensor input, no next block; occupied C, next occupied B, free A
        const OutputConfig sig = view->board().outputs[0];
        QCOMPARE(int(sig.type), int(BlockSignal));
        QCOMPARE(sig.data, (std::array<quint8, 4>{ { 0x04, 0x02, 0x01, 0x44 } }));
        QCOMPARE(sig.blockOwn(), 4);
        QCOMPARE(sig.blockNext(), -1);
        auto visibleCombo = [oed](const QString &first) -> QComboBox * {
            for (QComboBox *c : oed->findChildren<QComboBox *>())
                if (c->isVisibleTo(oed) && c->count() && c->itemText(0) == first) return c;
            return nullptr;
        };
        QComboBox *own = visibleCombo(QStringLiteral("In 1 · Absolute"));
        QComboBox *next = visibleCombo(QStringLiteral("— none"));
        QVERIFY(own && next);
        QCOMPARE(own->currentText(), QStringLiteral("In 5 · Sensor (occupancy)"));
        QCOMPARE(Ui::currentData(next), -1);
        QVector<QSpinBox *> aspects;                         // own occupied, next occupied, both free
        for (QSpinBox *b : oed->findChildren<QSpinBox *>())
            if (b->isVisibleTo(oed) && b->prefix() == QLatin1String("aspect ")) aspects << b;
        QCOMPARE(aspects.size(), 3);
        QCOMPARE(aspects[0]->value(), 4);
        QCOMPARE(aspects[1]->value(), 2);
        QCOMPARE(aspects[2]->value(), 1);
        QVERIFY(aspects[0]->isEnabled() && !aspects[1]->isEnabled() && aspects[2]->isEnabled());   // no next block
        const QStringList aspectNames = { QStringLiteral("Unsafe"), QStringLiteral("Caution"), QStringLiteral("Safe") };
        for (int k = 0; k < 3; ++k)
        {
            QLabel *label = nullptr;
            for (QLabel *l : oed->findChildren<QLabel *>())
                if (l->isVisibleTo(oed) && l->text() == aspectNames[k]) label = l;
            QVERIFY2(label, qPrintable(aspectNames[k]));
            QVERIFY(!label->toolTip().isEmpty());
        }
        for (QLabel *l : oed->findChildren<QLabel *>())
            QVERIFY2(!l->text().contains(QLatin1String("Own block occupied")) && l->text() != QLatin1String("Both free"),
                     qPrintable(l->text()));
        auto visibleText = [](QWidget *root, const QString &part, const QString &objectName = QString()) {
            for (QLabel *l : root->findChildren<QLabel *>())
                if (l->isVisibleTo(root) && (objectName.isEmpty() || l->objectName() == objectName) && l->text().contains(part))
                    return true;
            return false;
        };
        QVERIFY(visibleText(oed, QStringLiteral("Flashing lamps flash at 1 Hz")));
        QComboBox *pinA = comboWith(oed, trGpio("—  not used"));
        QVERIFY(pinA);
        pinA->setCurrentIndex(pinA->findData(3));            // lamp A on D3, B-D not used
        QCOMPARE(int(view->board().outputs[0].pinA), 3);
        for (QLabel *l : oed->findChildren<QLabel *>(QStringLiteral("error")))   // pins B-D are optional
            QVERIFY2(!l->isVisibleTo(oed) || !l->text().contains(QLatin1String("is not set")), qPrintable(l->text()));
        bool stateShown = false;                             // no stored state, no test buttons
        for (QGroupBox *g : oed->findChildren<QGroupBox *>())
            stateShown |= g->title() == QLatin1String("State") && g->isVisibleTo(oed);
        QVERIFY(!stateShown);
        click(view, view->outputRect(0).center());            // its panel
        QCOMPARE(oed->index(), 0);
        maybeScreenshot(w, "block-signal.png");

        InputConfig unused = view->board().inputs[6];
        unused.type = InputNotInUse;
        emit ied->changed(6, unused);
        next->setCurrentIndex(next->findData(6));
        QCOMPARE(int(view->board().outputs[0].data[3]), 0x64);
        QVERIFY(aspects[1]->isEnabled());
        QVERIFY(visibleText(oed, QStringLiteral("Detector In 7 is not in use"), QStringLiteral("error")));
        aspects[2]->setValue(0x11);                          // free: lamp A flashing
        QCOMPARE(int(view->board().outputs[0].data[2]), 0x11);
        QVERIFY(visibleText(oed, QStringLiteral("lamp 1 flashing")));

        // not a target: disabled in the input editor's output list, a drag onto it is refused
        QComboBox *out = nullptr;
        for (QComboBox *c : ied->findChildren<QComboBox *>())
            if (c->isVisibleTo(ied) && c->count() == NumOutputs && c->itemText(0).startsWith(QStringLiteral("Out 1 · "))) out = c;
        QVERIFY(out);
        QVERIFY(!qobject_cast<QStandardItemModel *>(out->model())->item(0)->isEnabled());
        QVERIFY(qobject_cast<QStandardItemModel *>(out->model())->item(1)->isEnabled());
        QVERIFY(visibleText(ied, QStringLiteral("Out 1 is a block signal"), QStringLiteral("error")));   // input 0 (shown as 1) drove it

        // the input editor offers 'Sensor': no output, behaviour, custom values or route
        QComboBox *fn = comboWith(ied, QStringLiteral("Direct control"));
        QVERIFY(fn);
        fn->setCurrentIndex(fn->findText(QStringLiteral("Sensor (occupancy)")));
        QCOMPARE(int(view->board().inputs[0].type), int(Sensor));
        QCOMPARE(view->board().inputs[0].address, AddrNone);
        for (QGroupBox *g : ied->findChildren<QGroupBox *>())
            QVERIFY2(!g->isVisibleTo(ied) || g->title() == QLatin1String("Triggered by"), qPrintable(g->title()));
        QVERIFY(comboWith(ied, QStringLiteral("Digital"))->isVisibleTo(ied));   // 'Read as' stays

        const InputConfig in1 = view->board().inputs[1];
        drag(view, view->inputPort(1), view->outputRect(0).center());
        QVERIFY(view->board().inputs[1] == in1);
        QVERIFY(w.statusBar()->currentMessage().contains(QLatin1String("Out 1 is a block signal")));

        // a 4-aspect signal with lamp A only: no 'pin not set' either
        type->setCurrentIndex(type->findData(int(FourToneSignal)));
        QVERIFY(!visibleText(oed, QStringLiteral("is not set"), QStringLiteral("error")));
        // 'Aspect numbers…': a read-only lookup of the DCC extended number, the configuration stays
        QPushButton *numbers = nullptr;
        for (QPushButton *b : oed->findChildren<QPushButton *>())
            if (b->text() == QStringLiteral("Aspect numbers…") && b->isVisibleTo(oed)) numbers = b;
        QVERIFY(numbers);
        maybeScreenshot(w, "signal-aspect-numbers.png");
        const OutputConfig signal = view->board().outputs[0];
        QString shown;
        QTimer::singleShot(200, this, [&shown] {
            for (QWidget *top : QApplication::topLevelWidgets())
                if (auto *d = qobject_cast<QDialog *>(top); d && d->isVisible())
                {
                    QList<QComboBox *> lamps = d->findChildren<QComboBox *>();
                    if (lamps.size() == 4) lamps[1]->setCurrentIndex(2);   // lamp 2 flashing
                    for (QLabel *l : d->findChildren<QLabel *>())
                        if (l->text().startsWith(QLatin1String("DCC extended number"))) shown = l->text();
                    if (const QByteArray dir = qgetenv("GPIOCORE_SHOT_DIR"); !dir.isEmpty())
                        d->grab().save(QString::fromLocal8Bit(dir) + QLatin1String("/aspect-numbers.png"));
                    d->reject();
                }
        });
        numbers->click();
        const int v = 0x22 | (signal.data[0] & 0xDD);                  // the dialog starts at the current aspect
        QCOMPARE(shown, QStringLiteral("DCC extended number: %1  (0x%2)").arg(v).arg(v, 2, 16, QLatin1Char('0')));
        QVERIFY(view->board().outputs[0] == signal);

        // servo: follows a detector (semaphore arm)
        type->setCurrentIndex(type->findData(int(ServoType)));
        QCheckBox *follow = nullptr, *swap = nullptr;
        for (QCheckBox *b : oed->findChildren<QCheckBox *>())
        {
            if (b->text() == QLatin1String("Follows a detector (semaphore arm)")) follow = b;
            if (b->text() == QLatin1String("Swap: occupied = straight")) swap = b;
        }
        QVERIFY(follow && swap);
        QVERIFY(follow->isVisibleTo(oed) && !follow->isChecked() && !swap->isVisibleTo(oed));
        follow->setChecked(true);                            // the first sensor input: 0 now
        QCOMPARE(int(view->board().outputs[0].data[3]), 0x80);
        QCOMPARE(view->board().outputs[0].watchedInputs(), QVector<int>{ 0 });
        QVERIFY(swap->isVisibleTo(oed));
        QComboBox *detector = visibleCombo(QStringLiteral("In 1 · Sensor (occupancy)"));
        QVERIFY(detector);
        detector->setCurrentIndex(detector->findData(4));
        swap->setChecked(true);
        QCOMPARE(int(view->board().outputs[0].data[3]), 0xC4);
        follow->setChecked(false);
        QCOMPARE(int(view->board().outputs[0].data[3]), 0);
    }

    void simulateSensorAndBlockSignal()
    {
        // Simulate: a sensor input gets a button that holds the block occupied until clicked again; block
        // signals show their aspect right after connecting (GET_OUTPUT_STATE state 2), without any change
        MainWindow w;
        auto *link = w.findChild<DeviceLink *>();
        link->drainMs = 3;
        w.resize(1320, 900);
        w.show();
        ConnectionView *view = w.findChild<ConnectionView *>();
        auto *oed = w.findChild<OutputEditor *>();
        auto *ied = w.findChild<InputEditor *>();
        OutputConfig unused = view->board().outputs[15];
        unused.type = OutputNotInUse; unused.pinA = PinNA;       // frees A5
        emit oed->changed(15, unused);
        OutputConfig sig = view->board().outputs[0];             // main signal: red / green, block 5
        sig.type = BlockSignal; sig.data = { { 0x04, 0x02, 0x01, 0 } };
        sig.setBlockInputs(5, -1);
        emit oed->changed(0, sig);
        OutputConfig dist = view->board().outputs[1];            // its distant signal: yellow / green, block 5
        dist.type = BlockSignal; dist.data = { { 0x02, 0x02, 0x01, 0 } };
        dist.setBlockInputs(5, -1);
        emit oed->changed(1, dist);
        OutputConfig d0 = view->board().outputs[2];              // a signal whose detector is on D0
        d0.type = BlockSignal; d0.data = { { 0x04, 0x02, 0x01, 0 } };
        d0.setBlockInputs(6, -1);
        emit oed->changed(2, d0);
        InputConfig sensor = view->board().inputs[5];
        sensor.type = Sensor; sensor.pin = 19; sensor.address = AddrNone;
        emit ied->changed(5, sensor);
        InputConfig onD0 = view->board().inputs[6];
        onD0.type = Sensor; onD0.pin = 0; onD0.address = AddrNone;
        emit ied->changed(6, onD0);
        QVERIFY(!view->sensorInput(5));                          // only in the simulation
        click(view, view->inputRect(6).center());                // D0: a note, not an error
        bool note = false, error = false;
        for (QLabel *l : ied->findChildren<QLabel *>())
        {
            note  |= l->isVisibleTo(ied) && l->objectName() == QLatin1String("note")
                  && l->text().startsWith(QLatin1String("D0/D1 are the USB serial pins")) && l->text().contains(QLatin1String("simulator"));
            error |= l->isVisibleTo(ied) && l->objectName() == QLatin1String("error") && l->text().contains(QLatin1String("D0"));
        }
        QVERIFY(note && !error);

        w.setMode(MainWindow::ModeSimulate);
        QTRY_VERIFY_WITH_TIMEOUT(view->liveMode(), 5000);
        auto *sim = w.findChild<SimulatedDevice *>();
        QVERIFY(sim);
        QTRY_COMPARE_WITH_TIMEOUT(view->liveValue(0), 0x01, 3000);   // free: green, shown at once
        QTRY_COMPARE_WITH_TIMEOUT(view->liveValue(1), 0x01, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(view->liveValue(2), 0x01, 3000);
        QVERIFY(view->liveState(0) >= 0);
        QVERIFY(view->sensorInput(5) && view->sensorInput(6));
        QTest::qWait(150);
        click(view, view->liveButtonRect(5, 1).center());
        QVERIFY(view->sensorOccupied(5));
        QTRY_COMPARE_WITH_TIMEOUT(view->liveValue(0), 0x04, 2000);   // occupied: main red (C), distant yellow (B)
        QTRY_COMPARE_WITH_TIMEOUT(view->liveValue(1), 0x02, 2000);
        QCOMPARE(sim->servoPosition(0), 0x04);
        maybeScreenshot(w, "sensor-simulate.png");
        click(view, view->liveButtonRect(5, 1).center());
        QVERIFY(!view->sensorOccupied(5));
        QTRY_COMPARE_WITH_TIMEOUT(view->liveValue(0), 0x01, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(view->liveValue(1), 0x01, 2000);
        click(view, view->liveButtonRect(6, 1).center());        // the detector on D0 works in the simulator
        QTRY_COMPARE_WITH_TIMEOUT(view->liveValue(2), 0x04, 2000);
        w.setMode(MainWindow::ModeConfigure);
    }

    // ── Board types: DCCnext port names, pin rules ─────────────────────────
    void dccNextOverlay()
    {
        MainWindow w;
        w.resize(1320, 900);
        w.show();
        auto *oed = w.findChild<OutputEditor *>();
        auto *ied = w.findChild<InputEditor *>();
        ConnectionView *view = w.findChild<ConnectionView *>();
        chooseBoardType(w, BoardDccNext);
        QCOMPARE(int(view->board().board.type), int(BoardDccNext));

        // pin lists: only P1-P16, the port first, the Arduino pin in brackets
        auto items = [](QComboBox *c) { QList<int> d; for (int i = 0; i < c->count(); ++i) d << c->itemData(i).toInt(); return d; };
        QList<int> ports { int(PinNA) };
        for (int p = 3; p <= 12; ++p) ports << p;
        for (int p = 14; p <= 19; ++p) ports << p;
        QComboBox *pinA = comboWith(oed, trGpio("—  not used"));
        QComboBox *inPin = comboWith(ied, trGpio("—  not used"));
        QVERIFY(pinA && inPin);
        QCOMPARE(items(pinA), ports);                        // no D0, D1, D2, D13, A6, A7
        QCOMPARE(items(inPin), ports);
        QVERIFY(pinA->itemText(pinA->findData(3)).startsWith(QLatin1String("P1 (D3)")));
        QVERIFY(inPin->itemText(inPin->findData(19)).startsWith(QLatin1String("P16 (A5)")));

        // canvas subtitle and warnings use the port names
        QCOMPARE(Ui::outputSummaryPins(view->board().outputs[0]), QStringLiteral("P1"));
        OutputConfig c = view->board().outputs[0];
        c.pinA = 13;                                         // e.g. from a Nano file: kept, with a warning
        emit oed->changed(0, c);
        QCOMPARE(int(view->board().outputs[0].pinA), 13);
        QVERIFY(visibleLabel(oed, QStringLiteral("Pin A uses D13, which cannot be used here: not a DCCnext port")));
        click(view, view->outputRect(0).center());
        QCOMPARE(pinA->currentText(), QStringLiteral("⚠ D13"));
        c.pinA = 4;
        emit oed->changed(0, c);
        maybeScreenshot(w, "dccnext-overlay.png");

        // Uno: no A6 / A7; Nano: A6 back, D-names again
        chooseBoardType(w, BoardUno);
        QVERIFY(!items(inPin).contains(20) && !items(inPin).contains(21) && items(inPin).contains(13));
        QVERIFY(pinA->itemText(pinA->findData(3)).startsWith(QLatin1String("D3")));
        chooseBoardType(w, BoardNano);
        QVERIFY(items(inPin).contains(20) && !items(inPin).contains(21));
        QCOMPARE(Ui::outputSummaryPins(view->board().outputs[0]), QStringLiteral("D4"));
    }

    void simulateButtonsWorkOnThePin()
    {
        // Simulate: an input with a digital pin is pressed on its pin, so whatever reads the pin reacts too:
        // servo A follows the switch (direct control), servo B follows the same input as a detector
        MainWindow w;
        auto *link = w.findChild<DeviceLink *>();
        link->drainMs = 3;
        w.resize(1320, 900);
        w.show();
        ConnectionView *view = w.findChild<ConnectionView *>();
        auto *oed = w.findChild<OutputEditor *>();
        auto *ied = w.findChild<InputEditor *>();
        OutputConfig unused = view->board().outputs[15];
        unused.type = OutputNotInUse; unused.pinA = PinNA;       // frees A5
        emit oed->changed(15, unused);
        OutputConfig a = view->board().outputs[0];
        a.type = ServoType; a.data = { { 60, 120, 1, 0 } };
        emit oed->changed(0, a);
        OutputConfig b = view->board().outputs[1];
        b.type = ServoType; b.data = { { 60, 120, 1, 0x80 | 0 } };   // follows input 1: occupied = 60°
        emit oed->changed(1, b);
        InputConfig in = view->board().inputs[0];
        in.type = Latching; in.pin = 19; in.outputId = 0;
        emit ied->changed(0, in);

        w.setMode(MainWindow::ModeSimulate);
        QTRY_VERIFY_WITH_TIMEOUT(view->liveMode(), 5000);
        auto *sim = link->findChild<SimulatedDevice *>();
        QVERIFY(sim && view->pinInput(0));
        QTest::qWait(150);
        click(view, view->liveButtonRect(0, 0).center());        // ◀: the switch holds the pin low
        QVERIFY(view->sensorOccupied(0));
        QTRY_COMPARE_WITH_TIMEOUT(sim->servoPosition(1), 60, 3000);  // B: occupied
        const int aLow = sim->servoPosition(0);
        click(view, view->liveButtonRect(0, 1).center());        // ▶: high
        QVERIFY(!view->sensorOccupied(0));
        QTRY_COMPARE_WITH_TIMEOUT(sim->servoPosition(1), 120, 3000); // B: free
        QTRY_VERIFY_WITH_TIMEOUT(sim->servoPosition(0) != aLow && (sim->servoPosition(0) == 60 || sim->servoPosition(0) == 120), 3000);
        QVERIFY(aLow == 60 || aLow == 120);                      // A followed the pin both times

        // a push button: pressed and released again, A switches over, B is back at 'free'
        w.setMode(MainWindow::ModeConfigure);
        in.type = Momentary;
        emit ied->changed(0, in);
        w.setMode(MainWindow::ModeSimulate);
        QTRY_VERIFY_WITH_TIMEOUT(view->liveMode(), 5000);
        sim = link->findChild<SimulatedDevice *>();
        QTest::qWait(300);
        const int before = sim->servoPosition(0);
        click(view, view->liveButtonRect(0, 1).center());
        QTRY_VERIFY_WITH_TIMEOUT(sim->servoPosition(0) != before && (sim->servoPosition(0) == 60 || sim->servoPosition(0) == 120), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(sim->servoPosition(1), 120, 3000);
        w.setMode(MainWindow::ModeConfigure);
    }

    // ── Workspace, user levels ──────────────────────────────────────────────
    void opensStraightIntoTheWorkspace()
    {
        // no welcome page, no serial log, no payload bytes, no tabs: the configuration at every level
        for (int level : { Ui::Beginner, Ui::Advanced, Ui::Expert })
        {
            Ui::setLevel(level);
            MainWindow w;
            w.resize(1320, 900);
            w.show();
            QCOMPARE(w.centralWidget()->objectName(), QStringLiteral("configArea"));
            QVERIFY(w.findChildren<QTabWidget *>().isEmpty());
            QVERIFY(w.findChild<ConnectionView *>()->isVisible());
            QVERIFY(!w.findChild<QWidget *>(QStringLiteral("startPage")));
            QVERIFY(w.findChildren<QDockWidget *>().isEmpty());
            QVERIFY(w.findChildren<QLabel *>(QStringLiteral("mono")).isEmpty());
            for (QAction *a : w.findChildren<QAction *>())
                QVERIFY2(!a->text().contains(QLatin1String("Expert mode")) && !a->text().contains(QLatin1String("log")),
                         qPrintable(a->text()));
            QCOMPARE(w.mode(), MainWindow::ModeConfigure);
        }
    }

    void levelsOfferWhatTheyShould()
    {
        Ui::setLevel(Ui::Beginner);
        MainWindow w;
        w.resize(1320, 900);
        w.show();
        auto *oed = w.findChild<OutputEditor *>();
        auto *ied = w.findChild<InputEditor *>();
        ConnectionView *view = w.findChild<ConnectionView *>();
        QAction *beginner = levelAction(w, QStringLiteral("Beginner"));
        QAction *advanced = levelAction(w, QStringLiteral("Advanced"));
        QAction *expert   = levelAction(w, QStringLiteral("Expert"));
        QVERIFY(beginner && advanced && expert);
        QVERIFY(beginner->isChecked() && !advanced->isChecked() && !expert->isChecked());

        QComboBox *type = comboWith(oed, outputTypeName(SingleSteady));
        QComboBox *fn = comboWith(ied, QStringLiteral("Direct control"));
        QComboBox *pinA = comboWith(oed, trGpio("—  not used"));
        QVERIFY(type && fn && pinA);
        auto box = [](QWidget *root, const QString &title) -> QGroupBox * {
            for (QGroupBox *g : root->findChildren<QGroupBox *>())
                if (g->title() == title) return g;
            return nullptr;
        };
        auto checkBox = [](QWidget *root, const QString &text) -> QCheckBox * {
            for (QCheckBox *b : root->findChildren<QCheckBox *>())
                if (b->text() == text) return b;
            return nullptr;
        };
        QGroupBox *frogBox = box(oed, QStringLiteral("Frog polarity"));
        QCheckBox *custom = checkBox(ied, QStringLiteral("Custom values"));
        QComboBox *readAs = comboWith(ied, QStringLiteral("Digital"));
        QComboBox *startsOn = comboWith(ied, QStringLiteral("any command"));
        QVERIFY(frogBox && custom && readAs && startsOn);

        // Beginner: servo, single / double steady and pulse; direct control and route; no frogs, analog,
        // custom values, 'Route starts on', D0 / D1
        const QList<int> beginnerTypes = { SingleSteady, SinglePulse, DoubleSteady, DoublePulse, ServoType, OutputNotInUse };
        QCOMPARE(offered(type), beginnerTypes);
        QCOMPARE(offered(fn).size(), 3);                     // direct control, route, not in use
        QVERIFY(!offered(fn).contains(3));                   // no Sensor
        QVERIFY(!offered(pinA).contains(0) && !offered(pinA).contains(1) && offered(pinA).contains(3));
        QVERIFY(!frogBox->isVisibleTo(oed));                 // output 1 is a single steady output (frog relays possible)
        QVERIFY(!readAs->isVisibleTo(ied));
        InputConfig direct = view->board().inputs[0];
        direct.type = Latching;
        emit ied->changed(0, direct);
        QVERIFY(!custom->isVisibleTo(ied));
        type->setCurrentIndex(type->findData(int(ServoType)));
        QCOMPARE(int(view->board().outputs[0].type), int(ServoType));
        QVERIFY(!frogBox->isVisibleTo(oed));
        QVERIFY(!checkBox(oed, QStringLiteral("Follows a detector (semaphore arm)"))->isVisibleTo(oed));
        fn->setCurrentIndex(fn->findData(1));                // route
        QVERIFY(isRouteType(view->board().inputs[0].type));
        QVERIFY(!startsOn->isVisibleTo(ied));
        QVERIFY(!visibleLabel(ied, QStringLiteral("Route starts on")));
        QVERIFY(!visibleLabel(ied, QStringLiteral("Uses ")));      // all Beginner: editable
        QVERIFY(fn->isEnabled() && type->isEnabled());
        auto *preset = w.findChild<QComboBox *>(QStringLiteral("presetCombo"));
        QVERIFY(preset);
        QCOMPARE(offered(preset).size(), 9);                 // every preset: one above the level raises it
        maybeScreenshot(w, "level-beginner.png");

        // Advanced: + effects, signal, PWM, sound, frog relays, analog, custom values, 'Route starts on', D0 / D1
        advanced->trigger();
        QCOMPARE(Ui::level(), int(Ui::Advanced));
        QVERIFY(advanced->isChecked() && !beginner->isChecked());
        QCOMPARE(offered(type).size(), int(OutputTypeCount) - 1);   // all but the block signal
        QVERIFY(!offered(type).contains(BlockSignal));
        QVERIFY(offered(type).contains(FourToneSignal) && offered(type).contains(Pwm) && offered(type).contains(SoundEffect)
                && offered(type).contains(WelderLight));
        QVERIFY(!offered(fn).contains(3));
        QVERIFY(offered(pinA).contains(0) && offered(pinA).contains(1));
        QVERIFY(frogBox->isVisibleTo(oed));
        QVERIFY(readAs->isVisibleTo(ied));
        QVERIFY(startsOn->isVisibleTo(ied));
        QVERIFY(!checkBox(oed, QStringLiteral("Follows a detector (semaphore arm)"))->isVisibleTo(oed));
        QCOMPARE(offered(preset).size(), 9);

        // Expert: + block signal, sensor input, semaphore arm
        expert->trigger();
        QCOMPARE(offered(type).size(), int(OutputTypeCount));
        QVERIFY(offered(fn).contains(3));
        QVERIFY(checkBox(oed, QStringLiteral("Follows a detector (semaphore arm)"))->isVisibleTo(oed));
        maybeScreenshot(w, "level-expert.png");

        // the level is remembered and changes nothing in the configuration
        const BoardConfig before = view->board();
        beginner->trigger();
        QCOMPARE(QSettings().value(QStringLiteral("ui/level")).toInt(), int(Ui::Beginner));
        QVERIFY(view->board().outputs == before.outputs);
        QVERIFY(view->board().inputs == before.inputs);

        // board settings: name and board type only at Beginner
        auto boardSettingsRows = [&w] {
            int shown = -1;
            QTimer::singleShot(200, &w, [&shown] {
                for (QWidget *top : QApplication::topLevelWidgets())
                    if (auto *d = qobject_cast<QDialog *>(top); d && d->isVisible())
                    {
                        shown = 0;
                        for (QWidget *f : d->findChildren<QWidget *>())
                            if ((qobject_cast<QComboBox *>(f) || qobject_cast<QCheckBox *>(f) || qobject_cast<QAbstractSpinBox *>(f)
                                 || (qobject_cast<QLineEdit *>(f) && !qobject_cast<QAbstractSpinBox *>(f->parentWidget())))
                                && f->isVisibleTo(d))
                                ++shown;
                        d->reject();
                    }
            });
            for (QAction *a : w.findChildren<QAction *>())
                if (a->text() == QStringLiteral("Board settings…")) { a->trigger(); break; }
            return shown;
        };
        QCOMPARE(boardSettingsRows(), 2);                    // name, board type
        advanced->trigger();
        QCOMPARE(boardSettingsRows(), 5);                    // + own wait time, wait time, interlocking
    }

    void higherLevelFeaturesHidden()
    {
        // a block signal, a sensor input and frog relays while on Beginner (the user lowered the level):
        // not shown, not read-only, working, never changed by the level
        Ui::setLevel(Ui::Beginner);
        MainWindow w;
        auto *link = w.findChild<DeviceLink *>();
        link->drainMs = 3;
        w.resize(1320, 900);
        w.show();
        ConnectionView *view = w.findChild<ConnectionView *>();
        auto *oed = w.findChild<OutputEditor *>();
        auto *ied = w.findChild<InputEditor *>();
        InputConfig sensor = view->board().inputs[5];
        sensor.type = Sensor; sensor.pin = 19; sensor.address = AddrNone;
        emit ied->changed(5, sensor);
        OutputConfig sig = view->board().outputs[0];
        sig.type = BlockSignal; sig.data = { { 0x04, 0x02, 0x01, 0 } };
        sig.setBlockInputs(5, -1);
        emit oed->changed(0, sig);
        const OutputConfig sigNow = view->board().outputs[0];
        const InputConfig sensorNow = view->board().inputs[5];

        click(view, view->outputRect(0).center());
        QCOMPARE(oed->index(), 0);
        QVERIFY(!visibleLabel(oed, QStringLiteral("Uses ")));             // no yellow line any more
        QVERIFY(!visibleLabel(ied, QStringLiteral("Uses ")));
        QComboBox *type = comboWith(oed, outputTypeName(SingleSteady));
        QVERIFY(type && type->isEnabled());                                // not read-only
        QVERIFY(!offered(type).contains(BlockSignal));                    // not in the combo

        click(view, view->inputRect(5).center());
        QComboBox *fn = comboWith(ied, QStringLiteral("Direct control"));
        QVERIFY(fn->isEnabled());
        QVERIFY(!offered(fn).contains(3));                                // no Sensor

        // frog relays on a servo: the frog box is hidden, the relay stays
        OutputConfig servo = view->board().outputs[1];
        servo.type = ServoType; servo.data = { { 80, 100, 20, 0 } }; servo.relayC = 12;
        emit oed->changed(1, servo);
        click(view, view->outputRect(1).center());
        for (QGroupBox *g : oed->findChildren<QGroupBox *>())
            if (g->title() == QLatin1String("Frog polarity")) QVERIFY(!g->isVisibleTo(oed));
        for (QCheckBox *b : oed->findChildren<QCheckBox *>())
            if (b->text().startsWith(QLatin1String("Follows a detector"))) QVERIFY(!b->isVisibleTo(oed));
        OutputConfig edited = view->board().outputs[1];                   // an edit of something shown ...
        QSpinBox *speed = nullptr;
        for (QSpinBox *b : oed->findChildren<QSpinBox *>())
            if (b->suffix() == QStringLiteral(" ms/°")) speed = b;
        QVERIFY(speed);
        speed->setValue(30);
        QCOMPARE(view->board().outputs[1].servoSpeed(), quint8(30));
        QCOMPARE(int(view->board().outputs[1].relayC), 12);              // ... keeps the hidden relay
        levelAction(w, QStringLiteral("Advanced"))->trigger();
        for (QGroupBox *g : oed->findChildren<QGroupBox *>())
            if (g->title() == QLatin1String("Frog polarity")) QVERIFY(g->isVisibleTo(oed));

        // switching levels changes nothing
        levelAction(w, QStringLiteral("Expert"))->trigger();
        levelAction(w, QStringLiteral("Beginner"))->trigger();
        QVERIFY(view->board().outputs[0] == sigNow);
        QVERIFY(view->board().inputs[5] == sensorNow);
        maybeScreenshot(w, "beginner-hidden-features.png");

        // it works in the simulation on Beginner
        w.setMode(MainWindow::ModeSimulate);
        QTRY_VERIFY_WITH_TIMEOUT(view->liveMode(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(view->liveValue(0), 0x01, 3000);   // safe: green
        QTest::qWait(150);
        click(view, view->liveButtonRect(5, 1).center());
        QTRY_COMPARE_WITH_TIMEOUT(view->liveValue(0), 0x04, 2000);   // unsafe: red
        w.setMode(MainWindow::ModeConfigure);
        QVERIFY(view->board().outputs[0] == sigNow);
    }

    void levelRaisedForAProject()
    {
        // opening a file that uses Expert features on Beginner: a popup, the level goes up; never down
        QTemporaryDir dir;
        BoardConfig expert;
        expert.outputs[0].type = BlockSignal; expert.outputs[0].pinA = 3; expert.outputs[0].setBlockInputs(1, -1);
        expert.inputs[1].type = Sensor; expert.inputs[1].pin = 5;
        const QString expertFile = writeBoard(dir, "expert.json", expert);
        BoardConfig advanced;
        advanced.outputs[0].type = Pwm; advanced.outputs[0].pinA = 5;
        const QString advancedFile = writeBoard(dir, "advanced.json", advanced);
        BoardConfig beginner;
        beginner.outputs[0].type = ServoType; beginner.outputs[0].pinA = 3;
        const QString beginnerFile = writeBoard(dir, "beginner.json", beginner);

        Ui::setLevel(Ui::Beginner);
        MainWindow w;
        w.resize(1320, 900);
        w.show();
        Popups popups;
        QVERIFY(w.loadFile(beginnerFile));
        QVERIFY(popups.texts.isEmpty());
        QCOMPARE(Ui::level(), int(Ui::Beginner));
        QVERIFY(w.loadFile(expertFile));
        QTRY_COMPARE(popups.texts.size(), 1);
        QCOMPARE(popups.texts[0], QStringLiteral("This project uses Expert features (block signals, sensors). "
                                                 "The level is set to Expert."));
        QCOMPARE(Ui::level(), int(Ui::Expert));
        QVERIFY(levelAction(w, QStringLiteral("Expert"))->isChecked());
        QCOMPARE(int(w.findChild<ConnectionView *>()->board().outputs[0].type), int(BlockSignal));   // unchanged
        QVERIFY(w.loadFile(advancedFile));                   // never lowered
        QCOMPARE(Ui::level(), int(Ui::Expert));
        QCOMPARE(popups.texts.size(), 1);

        Ui::setLevel(Ui::Beginner);
        w.setLevel(Ui::Beginner);
        QVERIFY(w.loadFile(advancedFile));
        QTRY_COMPARE(popups.texts.size(), 2);
        QCOMPARE(popups.texts[1], QStringLiteral("This project uses Advanced features (PWM). The level is set to Advanced."));
        QCOMPARE(Ui::level(), int(Ui::Advanced));

        // importing raises it as well
        QVERIFY(w.loadFile(beginnerFile));
        QVERIFY(w.importParts(expertFile, PartOutputs | PartInputs, false));
        QTRY_VERIFY(popups.texts.last().startsWith(QLatin1String("This project uses Expert features")));
        QCOMPARE(Ui::level(), int(Ui::Expert));
    }

    // ── Export / import, examples ───────────────────────────────────────────
    void exportOnlyInUseItems()
    {
        QTemporaryDir dir;
        MainWindow w;
        w.resize(1320, 900);
        w.show();
        ConnectionView *view = w.findChild<ConnectionView *>();
        BoardConfig b;
        b.board.timing = true;
        b.outputs[1].type = ServoType; b.outputs[1].pinA = 3; b.outputs[1].name = QStringLiteral("Left");
        b.outputs[4].type = FourToneSignal; b.outputs[4].pinA = 4; b.outputs[4].lampColors = QStringList{ "#ff0000" };
        b.inputs[2].type = Route; b.inputs[2].route = { defaultStep(1, ServoType) }; b.inputs[2].name = QStringLiteral("Go");
        QVERIFY(w.loadFile(writeBoard(dir, "mine.json", b)));
        const QString out = dir.filePath(QStringLiteral("export.json"));
        QVERIFY(w.exportParts(out, PartOutputs));
        BoardConfig e;
        QVERIFY(readBoard(out, e));
        QCOMPARE(e.outputs[1].name, QStringLiteral("Left"));
        QVERIFY(e.outputs[1] == view->board().outputs[1]);
        QCOMPARE(e.outputs[4].lampColors, QStringList{ "#ff0000" });
        for (int o : { 0, 2, 3, 5 }) QCOMPARE(int(e.outputs[o].type), int(OutputNotInUse));
        for (const InputConfig &i : e.inputs) QCOMPARE(int(i.type), int(InputNotInUse));
        QVERIFY(!e.board.timing);                            // board settings not ticked
        QVERIFY(w.exportParts(out, PartInputs | PartBoard));
        QVERIFY(readBoard(out, e));
        QCOMPARE(int(e.outputs[1].type), int(OutputNotInUse));
        QVERIFY(e.inputs[2] == view->board().inputs[2] && e.inputs[2].name == QStringLiteral("Go"));
        QVERIFY(e.board.timing);
    }

    void importSameNumbersReplaces()
    {
        // connected: the import is one edit that goes to the board at once and makes the file dirty
        QTemporaryDir dir;
        BoardConfig file;
        file.outputs[0].type = ServoType; file.outputs[0].pinA = 9; file.outputs[0].name = QStringLiteral("Imported");
        file.outputs[0].data = { { 60, 120, 10, 0 } };
        file.inputs[0].type = Momentary; file.inputs[0].address = 33; file.inputs[0].outputId = 0;
        const QString path = writeBoard(dir, "part.json", file);
        MainWindow w;
        ConnectionView *view = startDemo(w);
        auto *link = w.findChild<DeviceLink *>();
        Popups popups;
        QVERIFY(w.importParts(path, PartOutputs | PartInputs, true));
        QTRY_VERIFY(!popups.texts.isEmpty());
        QVERIFY(popups.texts[0].contains(QStringLiteral("Out 1 · Single steady")));   // asked first: what is replaced
        QVERIFY(popups.texts[0].contains(QStringLiteral("In 1 · Absolute")));
        QCOMPARE(view->board().outputs[0].name, QStringLiteral("Imported"));
        QCOMPARE(int(view->board().outputs[0].type), int(ServoType));
        QCOMPARE(int(view->board().inputs[0].address), 33);
        QCOMPARE(int(view->board().outputs[6].pinA), int(PinNA));   // D9 was output 7's: it lost the pin ...
        QCOMPARE(int(view->board().outputs[6].type), int(SingleSteady));   // ... and keeps its function
        QTRY_VERIFY(popups.texts.size() >= 2);
        QVERIFY(popups.texts[1].contains(QLatin1String("Out 7")));   // listed afterwards
        QVERIFY(w.windowTitle().endsWith(QLatin1String(" *")));
        QTRY_VERIFY_WITH_TIMEOUT(!link->isBusy() && !writeAction(w)->isEnabled(), 8000);   // written live
        QCOMPARE(int(view->board().outputs[0].type), int(ServoType));       // read back from the board

        // cancelled: nothing changes
        popups.button = QMessageBox::No;
        file.outputs[0].name = QStringLiteral("Again");
        QVERIFY(!w.importParts(writeBoard(dir, "again.json", file), PartOutputs, true));
        QCOMPARE(view->board().outputs[0].name, QStringLiteral("Imported"));
    }

    void importIntoFreeSlots()
    {
        QTemporaryDir dir;
        BoardConfig mine;                                    // outputs 1-2 and input 1 in use
        mine.outputs[0].type = SingleSteady; mine.outputs[0].pinA = 3;
        mine.outputs[1].type = SingleSteady; mine.outputs[1].pinA = 4; mine.outputs[1].name = QStringLiteral("Mine");
        mine.inputs[0].type = Latching; mine.inputs[0].address = 1; mine.inputs[0].outputId = 1;
        BoardConfig file;                                    // a detector, a block signal and a route to both outputs
        file.outputs[0].type = ServoType; file.outputs[0].pinA = 4;          // takes D4 from 'Mine'
        file.outputs[1].type = BlockSignal; file.outputs[1].pinA = 5; file.outputs[1].setBlockInputs(0, 2);
        file.inputs[0].type = Sensor; file.inputs[0].pin = 6;
        file.inputs[1].type = Route; file.inputs[1].address = 50;
        file.inputs[1].route = { defaultStep(0, ServoType), defaultStep(9, SingleSteady) };   // output 10: not imported
        file.inputs[2].type = Sensor; file.inputs[2].pin = 7;
        MainWindow w;
        w.resize(1320, 900);
        w.show();
        ConnectionView *view = w.findChild<ConnectionView *>();
        QVERIFY(w.loadFile(writeBoard(dir, "mine.json", mine)));
        Popups popups;
        const QString part = writeBoard(dir, "part.json", file);
        QVERIFY(w.importParts(part, PartOutputs | PartInputs, false));
        const BoardConfig &b = view->board();
        QCOMPARE(int(b.outputs[2].type), int(ServoType));    // the first free outputs: 3 and 4
        QCOMPARE(int(b.outputs[3].type), int(BlockSignal));
        QCOMPARE(b.outputs[3].blockOwn(), 1);                // detector inputs 1 / 3 -> 2 / 4
        QCOMPARE(b.outputs[3].blockNext(), 3);
        QCOMPARE(int(b.inputs[1].type), int(Sensor));
        QCOMPARE(int(b.inputs[2].type), int(Route));
        QCOMPARE(b.inputs[2].route[0].output, 2);            // route steps renumbered
        QCOMPARE(b.inputs[2].route[1].output, 9);            // not imported: stays on the user's own output 10
        QCOMPARE(int(b.inputs[3].type), int(Sensor));
        QCOMPARE(b.outputs[1].name, QStringLiteral("Mine")); // the user's own output keeps everything but D4
        QCOMPARE(int(b.outputs[1].pinA), int(PinNA));
        QCOMPARE(int(b.inputs[0].outputId), 1);
        QTRY_VERIFY(!popups.texts.isEmpty());
        QVERIFY(popups.texts.first().contains(QLatin1String("Mine keeps its settings")));   // by its name
        QVERIFY(w.windowTitle().endsWith(QLatin1String(" *")));

        // not enough free slots: said, nothing imported
        BoardConfig big;
        for (int o = 0; o < 15; ++o) { big.outputs[o].type = SingleSteady; }
        const BoardConfig before = view->board();
        QVERIFY(!w.importParts(writeBoard(dir, "big.json", big), PartOutputs, false));
        QTRY_VERIFY(popups.texts.last().startsWith(QLatin1String("Not enough free slots")));
        QVERIFY(view->board().outputs == before.outputs);
    }

    void importDialogUi()
    {
        // File > Import…: choose a file, tick the parts, choose where to
        QTemporaryDir dir;
        BoardConfig file;
        file.outputs[0].type = ServoType; file.outputs[0].pinA = 12;
        file.inputs[0].type = Latching; file.inputs[0].address = 40;
        const QString path = writeBoard(dir, "part.json", file);
        BoardConfig mine;
        mine.outputs[0].type = SingleSteady; mine.outputs[0].pinA = 3;
        MainWindow w;
        w.resize(1320, 900);
        w.show();
        ConnectionView *view = w.findChild<ConnectionView *>();
        QVERIFY(w.loadFile(writeBoard(dir, "mine.json", mine)));
        Popups popups;
        popups.file = path;
        bool seen = false, freeSlots = false;
        popups.onDialog = [&seen, &freeSlots](QDialog *d) {
            if (d->objectName() != QLatin1String("importDialog")) return false;
            seen = true;
            d->findChild<QCheckBox *>(QStringLiteral("partInputs"))->setChecked(false);
            freeSlots = d->findChild<QRadioButton *>(QStringLiteral("importFree"))->isChecked();
            const QByteArray shots = qgetenv("GPIOCORE_SHOT_DIR");
            if (!shots.isEmpty()) d->grab().save(QString::fromLocal8Bit(shots) + QStringLiteral("/import-dialog.png"));
            d->accept();
            return true;
        };
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text() == QStringLiteral("&Import…")) { a->trigger(); break; }
        QVERIFY(seen && freeSlots);                          // "into free slots" is the default
        QCOMPARE(int(view->board().outputs[1].type), int(ServoType));   // into the first free output
        QCOMPARE(int(view->board().outputs[0].type), int(SingleSteady));
        QCOMPARE(int(view->board().inputs[0].type), int(InputNotInUse)); // inputs not ticked
    }

    void openExampleIsATemplate()
    {
        MainWindow w;
        w.resize(1320, 900);
        w.show();
        const QDir examples(MainWindow::examplesDir());
        const QStringList files = examples.entryList({ QStringLiteral("*.json") }, QDir::Files, QDir::Name);
        QVERIFY(files.size() >= 5);
        Popups popups;
        int listed = -1;
        popups.onDialog = [&listed](QDialog *d) {
            if (d->objectName() != QLatin1String("exampleDialog")) return false;
            auto *list = d->findChild<QListWidget *>();
            listed = list->count();
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->text().startsWith(QLatin1String("Servo decoder 16\n"))) list->setCurrentRow(i);
            d->accept();
            return true;
        };
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text() == QStringLiteral("Open &example…")) { a->trigger(); break; }
        QCOMPARE(listed, files.size());
        QVERIFY(w.filePath().isEmpty());                      // Save asks for a new name
        BoardConfig ex;
        QVERIFY(readBoard(examples.filePath(QStringLiteral("servo-decoder-16.json")), ex));
        QVERIFY(w.findChild<ConnectionView *>()->board().outputs == ex.outputs);
        QVERIFY(w.openExample(examples.filePath(QStringLiteral("routes-demo.json"))));
        QVERIFY(w.filePath().isEmpty());
        QVERIFY(w.loadFile(examples.filePath(QStringLiteral("routes-demo.json"))));
        QVERIFY(!w.filePath().isEmpty());                     // an ordinary open keeps its path
    }

    // ── Start screen, presets ───────────────────────────────────────────────
    void startScreenIsAnEmptyBoard()
    {
        MainWindow w;
        w.resize(1320, 900);
        w.show();
        const BoardConfig &b = w.findChild<ConnectionView *>()->board();
        int outs = 0, ins = 0;
        for (const OutputConfig &o : b.outputs) outs += o.type != OutputNotInUse;
        for (const InputConfig &i : b.inputs)   ins  += i.type != InputNotInUse;
        QCOMPARE(outs, 1);
        QCOMPARE(ins, 1);
        QCOMPARE(int(b.outputs[0].type), int(SingleSteady));
        QCOMPARE(int(b.outputs[0].pinA), 3);                 // the first free pin
        QCOMPARE(int(b.inputs[0].type), int(Latching));      // absolute, DCC 1 drives output 1
        QCOMPARE(int(b.inputs[0].address), 1);
        QCOMPARE(int(b.inputs[0].outputId), 0);
        QCOMPARE(w.mode(), MainWindow::ModeConfigure);
        QVERIFY(!w.findChild<DeviceLink *>()->isOpen());     // offline
        // one workspace without a tab bar; no Pin map, no Language menu, no wizard or next steps
        QVERIFY(w.findChildren<QTabWidget *>().isEmpty());
        bool newBoard = false;
        for (QAction *a : w.findChildren<QAction *>())
        {
            const QString t = a->text();
            QVERIFY2(!t.contains(QLatin1String("Pin map")) && !t.contains(QLatin1String("Language"))
                         && !t.contains(QLatin1String("izard")) && !t.contains(QLatin1String("ext steps")),
                     qPrintable(t));
            newBoard |= t == QStringLiteral("&New board (offline)…");
        }
        QVERIFY(newBoard);
        for (QMenu *m : w.findChildren<QMenu *>())
            QVERIFY2(m->title() != QLatin1String("Language"), qPrintable(m->title()));

        // documentation: the start screen with the Preset list open (the list is a window of its own)
        const QByteArray shots = qgetenv("GPIOCORE_SHOT_DIR");
        if (!shots.isEmpty())
        {
            auto *combo = w.findChild<QComboBox *>(QStringLiteral("presetCombo"));
            combo->showPopup();
            QTest::qWait(400);
            QPixmap shot = w.grab();
            QWidget *popup = combo->view()->window();
            QPainter painter(&shot);
            painter.drawPixmap(w.mapFromGlobal(popup->mapToGlobal(QPoint(0, 0))), popup->grab());
            painter.end();
            shot.save(QString::fromLocal8Bit(shots) + QStringLiteral("/start-screen-presets.png"));
            combo->hidePopup();
        }
    }

    void presetsListedFromTheFolder()
    {
        MainWindow w;
        auto *combo = w.findChild<QComboBox *>(QStringLiteral("presetCombo"));
        QVERIFY(combo);
        const QDir dir(MainWindow::presetsDir());
        const QStringList files = dir.entryList({ QStringLiteral("*.json") }, QDir::Files);
        QCOMPARE(files.size(), 9);
        QCOMPARE(combo->count(), files.size());
        QStringList names;
        for (int k = 0; k < combo->count(); ++k)
        {
            names << combo->itemText(k);
            QCOMPARE(QFileInfo(combo->itemData(k).toString()).absolutePath(), dir.absolutePath());
            QVERIFY(files.contains(QFileInfo(combo->itemData(k).toString()).fileName()));
        }
        // "presetName" from the file: "Empty" first, then by name with the numbers in order
        QCOMPARE(names, QStringList({ "Empty: 1 input, 1 output",
                                      "4 double pulse + electrofrog relays", "5 double pulse + unifrog relay",
                                      "5 servos + electrofrog relays", "8 double pulse", "8 double steady",
                                      "8 servos + unifrog relay", "16 servos", "16 single steady" }));

        // what is in them: pins from D3 upwards (D3-D12, A0-A5, never D13), input n -> output n from DCC 1
        const BoardConfig s16 = shippedPreset("16-servos");
        QCOMPARE(int(s16.outputs[0].pinA), 3);
        QCOMPARE(int(s16.outputs[9].pinA), 12);
        QCOMPARE(int(s16.outputs[10].pinA), 14);
        QCOMPARE(int(s16.outputs[15].pinA), 19);
        QCOMPARE(int(s16.outputs[15].type), int(ServoType));
        QCOMPARE(int(s16.inputs[15].type), int(Latching));
        QCOMPARE(int(s16.inputs[15].address), 16);
        const BoardConfig e = shippedPreset("4-double-pulse-electrofrog");
        QCOMPARE(int(e.outputs[3].relayB), 19);
        QCOMPARE(int(e.outputs[4].type), int(OutputNotInUse));
        QCOMPARE(int(e.inputs[4].type), int(InputNotInUse));
        QCOMPARE(int(e.outputs[0].switchTime()), 50);

        // a preset is a normal board file: File > Open reads it too
        QVERIFY(w.loadFile(dir.filePath(QStringLiteral("16-servos.json"))));
        QVERIFY(w.findChild<ConnectionView *>()->board().outputs == s16.outputs);
    }

    void presetAboveTheLevelRaisesIt()
    {
        Ui::setLevel(Ui::Beginner);
        MainWindow w;
        w.resize(1320, 900);
        w.show();
        chooseBoardType(w, BoardDccNext);
        auto *combo = w.findChild<QComboBox *>(QStringLiteral("presetCombo"));
        QCOMPARE(offered(combo).size(), combo->count());       // every preset is offered at every level
        combo->setCurrentIndex(combo->findText(QStringLiteral("5 servos + electrofrog relays")));
        QAction *apply = nullptr;
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text() == QLatin1String("Apply")) apply = a;
        QVERIFY(apply && apply->isEnabled());                   // offline too
        Popups popups;                                          // "Apply preset?" Yes, then the level message
        apply->trigger();
        const BoardConfig &b = w.findChild<ConnectionView *>()->board();
        QVERIFY(b.outputs == shippedPreset("5-servos-electrofrog").outputs);
        QVERIFY(b.inputs == shippedPreset("5-servos-electrofrog").inputs);
        QCOMPARE(int(b.board.type), int(BoardDccNext));         // the file names no board type: it stays
        QCOMPARE(Ui::level(), int(Ui::Advanced));               // frog relays
        QCOMPARE(popups.texts.size(), 2);
        QVERIFY(popups.texts[0].startsWith(QLatin1String("Apply preset \"5 servos + electrofrog relays\"?")));
        QVERIFY(popups.texts[1].contains(QLatin1String("The level is set to Advanced")));
    }

    void newBoardStartsFromAPreset()
    {
        MainWindow w;
        w.resize(1320, 900);
        w.show();
        Popups popups;
        bool listed = false;
        popups.onDialog = [&](QDialog *d) {
            auto *start = d->findChild<QComboBox *>();
            if (!start || d->windowTitle() != QLatin1String("New board")) return false;
            listed = start->count() == 9 && start->currentText() == QLatin1String("Empty: 1 input, 1 output");
            start->setCurrentIndex(start->findText(QStringLiteral("8 double steady")));
            d->findChild<QLineEdit *>()->setText(QStringLiteral("Yard"));
            d->accept();
            return true;
        };
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text() == QStringLiteral("&New board (offline)…")) { a->trigger(); break; }
        QVERIFY(listed);
        const BoardConfig &b = w.findChild<ConnectionView *>()->board();
        QVERIFY(b.outputs == shippedPreset("8-double-steady").outputs);
        QCOMPARE(b.board.name, QStringLiteral("Yard"));
    }

    // Board > Set up Bluetooth (HC-05): the board sets up a (fake) HC-05 in AT mode, the dialog says what happened
    void bluetoothSetupDialog()
    {
        MainWindow w;
        startDemo(w);
        auto *link = w.findChild<DeviceLink *>();
        auto *sim  = w.findChild<SimulatedDevice *>();
        QVERIFY(link && sim);
        QAction *bt = nullptr;
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text() == QStringLiteral("Set up &Bluetooth (HC-05)…")) bt = a;
        QVERIFY(bt);
        QVERIFY(bt->isVisible() && bt->isEnabled());
        w.setLevel(Ui::Beginner);
        QVERIFY(!bt->isVisible());                           // Advanced and up
        w.setLevel(Ui::Advanced);
        QVERIFY(bt->isVisible());

        sim->attachFakeHc05(38400);
        bt->trigger();
        auto *d = w.findChild<QDialog *>(QStringLiteral("bluetoothDialog"));
        QVERIFY(d && d->isVisible());
        QVERIFY(visibleLabel(d, QStringLiteral("Schottky")));  // the wiring, with the diode
        QVERIFY(Info::text(QStringLiteral("board.bluetooth")).contains(QLatin1String("AT mode")));   // its help page
        auto *start  = d->findChild<QPushButton *>(QStringLiteral("btStart"));
        auto *status = d->findChild<QLabel *>(QStringLiteral("btStatus"));
        QVERIFY(start && status);
        start->click();
        QVERIFY(!start->isEnabled());
        QVERIFY(link->isBusy());
        QVERIFY(!bt->isEnabled());                           // one task at a time
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QLatin1String("Done")), 10000);
        QVERIFY(status->text().contains(QLatin1String("115200 baud")));
        QVERIFY(status->text().contains(QLatin1String("'GPIOcore")));
        QVERIFY(start->isEnabled());
        QCOMPARE(sim->hc05Commands().value(1), QStringLiteral("AT+UART=115200,0,0"));
        QVERIFY(sim->hc05Commands().value(2).startsWith(QLatin1String("AT+NAME=GPIOcore")));

        sim->attachFakeHc05(0);                              // no HC-05 (or not in AT mode): the likely causes
        start->click();
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QLatin1String("No HC-05 answered")), 10000);
        QVERIFY(status->text().contains(QLatin1String("AT mode")));
        d->close();
        QVERIFY(link->isReady());
        QTRY_VERIFY(bt->isEnabled());
    }

    // Connect to a board whose serial port is closed (over Bluetooth nothing resets it): the window asks for a
    // short press on the config button and connects when the board announces itself.
    void connectWaitsForTheAnnounce()
    {
        MainWindow w;
        w.resize(1320, 900);
        w.show();
        Popups popups;                                       // the name question: no
        popups.onDialog = [](QDialog *d) { d->reject(); return true; };
        auto *link = w.findChild<DeviceLink *>();
        link->drainMs   = 3;
        link->timeoutMs = 100;
        auto *sim = new SimulatedDevice;
        QVERIFY(sim->open(QIODevice::ReadWrite));
        sim->advanceClock(6000);
        QTRY_VERIFY(sim->serialIsIO());
        QString err;
        QVERIFY(link->openDevice(sim, false, &err));
        QTRY_VERIFY(link->isWaitingForBoard());
        QVERIFY(visibleLabel(&w, QStringLiteral("press its config button briefly")));
        sim->setAnalogValue(7, 0);
        QTest::qWait(150);
        sim->setAnalogValue(7, AnalogMaxValue);
        QTRY_VERIFY(link->isReady() && link->caps().known);
        QVERIFY(!visibleLabel(&w, QStringLiteral("press its config button briefly")));
        QVERIFY(popups.texts.isEmpty());                     // no "no board" message
    }
};

QTEST_MAIN(TestGui)
#include "tst_gui.moc"
