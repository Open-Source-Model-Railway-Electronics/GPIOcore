#include "mainwindow.h"

#include "connections.h"
#include "connectionview.h"
#include "devicelink.h"
#include "flasher.h"
#include "qrcode.h"
#include "webremote.h"
#include "infotexts.h"
#include "inputeditor.h"
#include "outputeditor.h"
#include "simulateddevice.h"
#include "uihelpers.h"

#include <QAction>
#include <QListWidget>
#include <QRadioButton>
#include <QDesktopServices>
#include <QDialog>
#include <QUrl>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QColorDialog>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QActionGroup>
#include <QApplication>
#include <QLocale>
#include <QBoxLayout>
#include <QCheckBox>
#include <QCollator>
#include <QPropertyAnimation>
#include <QSettings>
#include <QShortcut>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QImage>
#include <QPainter>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>
#include <QProgressBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QSerialPortInfo>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include <algorithm>
#include <memory>

using namespace Gpio;

// ── Presets: board files shipped in presets/ next to the program (read only) ─
static const char *const kEmptyPreset = "empty-1-input-1-output.json";   // also the start screen

struct PresetFile
{
    QString     name;              // "presetName" in the file, otherwise the file name
    QString     path;
    BoardConfig board;
    bool        hasType = false;   // the file names a board type; otherwise the current type stays
};

static bool readPreset(const QString &path, PresetFile &p)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    if (!BoardConfig::fromJson(root, p.board, nullptr))
        return false;
    p.path    = path;
    p.name    = root["presetName"].toString(QFileInfo(path).completeBaseName());
    p.hasType = root["board"].toObject().contains(QLatin1String("type"));
    return true;
}

// All presets, sorted by name ("4 ..." before "16 ...")
static QVector<PresetFile> presetFiles()
{
    QVector<PresetFile> list;
    const QDir dir(MainWindow::presetsDir());
    for (const QString &f : dir.entryList({ QStringLiteral("*.json") }, QDir::Files))
    {
        PresetFile p;
        if (readPreset(dir.filePath(f), p))
            list << p;
    }
    QCollator byName;
    byName.setNumericMode(true);
    byName.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(list.begin(), list.end(),
              [&byName](const PresetFile &a, const PresetFile &b) {
                  const bool ea = a.name.startsWith(QLatin1String("Empty")), eb = b.name.startsWith(QLatin1String("Empty"));
                  if (ea != eb) return ea;                       // "Empty" first, the rest by name
                  return byName.compare(a.name, b.name) < 0;
              });
    return list;
}


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(tr("GPIOcore Configurator"));
    resize(1600, 900);

    m_link = new DeviceLink(this);

    // Start screen, offline in Configure: an empty board with 1 input and 1 output (the "Empty" preset).
    PresetFile empty;
    if (readPreset(QDir(presetsDir()).filePath(QLatin1String(kEmptyPreset)), empty))
    {
        m_edit.outputs = empty.board.outputs;
        m_edit.inputs  = empty.board.inputs;
    }
    m_edit.board.type = quint8(QSettings().value(QStringLiteral("board/type"), int(BoardNano)).toInt());
    m_shadow = m_edit;
    Gpio::setBoardType(m_edit.board.type);
    m_dev = m_edit;

    m_liveTimer = new QTimer(this);
    m_liveTimer->setSingleShot(true);
    m_liveTimer->setInterval(350);       // edits are sent after a short pause (slider drags, typing)
    connect(m_liveTimer, &QTimer::timeout, this, &MainWindow::sendLive);
    m_analogTimer = new QTimer(this);
    m_analogTimer->setInterval(250);     // asks only while an analog input is shown (or in a live test)
    connect(m_analogTimer, &QTimer::timeout, this, &MainWindow::pollAnalog);
    m_analogTimer->start();

    buildUi();
    m_inEd->setTimingEnabled(m_edit.board.timing);   // wait time per route step: a board setting
    buildToolbar();
    setupWebRemote();
    refreshPorts();

    connect(m_link, &DeviceLink::opened,  this, &MainWindow::onConnectionChanged);
    connect(m_link, &DeviceLink::closed,  this, &MainWindow::onConnectionChanged);
    connect(m_link, &DeviceLink::ready,   this, &MainWindow::onReady);
    connect(m_link, &DeviceLink::outputReceived, this, &MainWindow::onOutputReceived);
    connect(m_link, &DeviceLink::inputReceived,  this, &MainWindow::onInputReceived);
    connect(m_link, &DeviceLink::routeReceived,  this, &MainWindow::onRouteReceived);
    connect(m_link, &DeviceLink::boardReceived,  this, &MainWindow::onBoardReceived);
    connect(m_link, &DeviceLink::outputEvent, this, [this](int o, int state, int value) {
        m_view->setOutputLive(o, state, value);
        if (m_view->liveMode() && o >= 0 && o < NumOutputs)
        {
            const QString name = Ui::outputDisplayName(m_edit.outputs[o], o);
            const QString what = state == 3 ? tr("frog relays A %1, B %2, unifrog %3")
                                                  .arg(value & 1 ? tr("on") : tr("off"), value & 2 ? tr("on") : tr("off"),
                                                       value & 4 ? tr("on") : tr("off"))
                               : state == 2 && m_edit.outputs[o].type == SoundEffect
                                            ? (value ? tr("track %1").arg(value) : tr("sound stopped"))
                               : state == 2 ? tr("value %1").arg(value)
                                            : (state ? tr("straight / on") : tr("curved / off"));
            statusBar()->showMessage(tr("%1 → %2").arg(name, what), 4000);
        }
    });
    connect(m_link, &DeviceLink::capabilitiesChanged, this, [this] { applyCapabilities(); });
    connect(m_link, &DeviceLink::analogValue, this, [this](int channel, int value) {
        m_inEd->setAnalogReading(channel, value);
        m_view->setAnalogValue(channel, value);
    });
    connect(m_link, &DeviceLink::errorOccurred,  this, [this](const QString &m) {
        statusBar()->showMessage(m, 6000);
    });
    connect(m_link, &DeviceLink::noBoard, this, [this](const QString &port) {
        // a status-bar line is easy to miss: say it clearly (the link is already closed)
        QMessageBox::warning(this, windowTitle(),
                             tr("No GPIOcore board answered on %1.\n\nCheck that the right port is chosen and that the "
                                "board has the GPIOcore firmware (Board > Flash firmware). A board that has been running "
                                "for more than 5 seconds without the program may need a reset (unplug and plug in "
                                "the USB cable).").arg(port));
    });
    connect(m_link, &DeviceLink::progress, this, [this](int done, int total) {
        m_progress->setVisible(total > 0);
        m_progress->setMaximum(qMax(1, total));
        m_progress->setValue(done);
        updateActions();
    });
    connect(m_link, &DeviceLink::finished, this, [this](int failures) {
        m_keepEdits = false;
        if (m_readingBoard)
        {
            m_readingBoard = false;
            markFileDirty();             // the screen now shows the board, not the file
            m_shadow = m_edit;
            statusBar()->showMessage(tr("Board read"), 4000);
            QTimer::singleShot(0, this, &MainWindow::raiseLevelFor);
        }
        updateActions();
        if (failures)
            statusBar()->showMessage(tr("Done with %1 error(s)").arg(failures), 6000);
        if (dirtyCount() > 0 && m_liveTimer->isActive() == false && failures == 0)
            m_liveTimer->start();        // edits made while the board was busy (sendLive skips refused items)
        if (!m_nameAsked && m_link->caps().boardSettings && m_dev.board.name.isEmpty())
            QTimer::singleShot(0, this, &MainWindow::askBoardName);
    });

    applyCapabilities();
    setLevel(Ui::level());
    updateConnState();
    updateEditing();
    refreshAll();
    showOutput(m_outEd->index());           // the panels show the start screen's input and output
    showInput(m_inEd->index());
    onConnectionChanged();
}

// ── UI construction ─────────────────────────────────────────────────────────
static QScrollArea *scrollWrap(QWidget *w, int minWidth)
{
    auto *sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setWidget(w);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setMinimumWidth(minWidth);
    return sa;
}

void MainWindow::buildUi()
{
    // Configuration: input settings slide in on the left, output settings on the
    // right, the canvas (inputs | wires | outputs) in between. Only one is open.
    m_view  = new ConnectionView;
    m_inEd  = new InputEditor;
    m_outEd = new OutputEditor;

    auto makePanel = [this](QWidget *editor, const QString &title, QWidget *&panel, QLineEdit *&name) {
        panel = new QWidget;
        panel->setObjectName(QStringLiteral("sidePanel"));
        panel->setAttribute(Qt::WA_StyledBackground);
        auto *v = new QVBoxLayout(panel);
        v->setContentsMargins(8, 6, 4, 6);
        auto *head = new QHBoxLayout;
        auto *t = new QLabel(title);
        t->setObjectName(QStringLiteral("help"));
        auto *close = new QToolButton;
        close->setObjectName(QStringLiteral("panelClose"));
        close->setText(QStringLiteral("✕"));
        close->setToolTip(tr("Close (Esc)"));
        connect(close, &QToolButton::clicked, this, &MainWindow::closePanels);
        auto *remove = new QToolButton;
        m_editWidgets << remove;
        remove->setText(tr("Remove"));
        remove->setToolTip(tr("Set this to 'not in use' and take it off the canvas"));
        const bool isInput = (editor == m_inEd);
        connect(remove, &QToolButton::clicked, this, [this, isInput] {
            m_open = isInput ? OpenInput : OpenOutput;
            removeSelected();
        });
        head->addWidget(t, 1);
        head->addWidget(remove);
        close->hide();                      // panels stay; Esc clears the selection
        v->addLayout(head);
        name = new QLineEdit;
        m_editWidgets << name;
        name->setPlaceholderText(tr("Name, e.g. \"Turnout station left\" (kept on this computer)"));
        name->setMaxLength(40);
        v->addWidget(name);
        auto *sa = new QScrollArea;
        sa->setWidgetResizable(true);
        sa->setWidget(editor);
        sa->setFrameShape(QFrame::NoFrame);
        sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);   // never cut anything off
        v->addWidget(sa, 1);
        panel->setMinimumWidth(320);       // always visible; the splitter sets the width
    };
    makePanel(m_inEd,  tr("INPUT SETTINGS"),  m_leftPanel,  m_inName);
    makePanel(m_outEd, tr("OUTPUT SETTINGS"), m_rightPanel, m_outName);
    connect(m_inName, &QLineEdit::textEdited, this, [this](const QString &t) {
        m_edit.inputs[m_inEd->index()].name = t.trimmed();
        m_inEd->setName(t.trimmed());
        refreshAll();                    // the name shows wherever this input is meant
        markFileDirty();
    });
    // lamp colours of a 4-aspect / block signal: only for the GUI, kept in the board file
    m_lampRow = new QWidget;
    auto *lampLay = new QHBoxLayout(m_lampRow);
    lampLay->setContentsMargins(0, 0, 0, 0);
    lampLay->addWidget(new QLabel(tr("Lamp colours (only shown in the GUI):")));
    for (int k = 0; k < 4; ++k)
    {
        auto *b = new QToolButton;
        b->setToolTip(tr("Colour of lamp %1").arg(k + 1));
        b->setFixedSize(26, 22);
        m_lampButtons << b;
        lampLay->addWidget(b);
        connect(b, &QToolButton::clicked, this, [this, k] {
            OutputConfig &o = m_edit.outputs[m_outEd->index()];
            const QColor c = QColorDialog::getColor(Ui::lampColor(o, k), this, tr("Colour of lamp %1").arg(k + 1));
            if (!c.isValid())
                return;
            while (o.lampColors.size() < 4)
                o.lampColors << Ui::lampColor(o, o.lampColors.size()).name();
            o.lampColors[k] = c.name();
            refreshLampRow();
            refreshRows();
            markFileDirty();
        });
    }
    lampLay->addStretch(1);
    if (auto *v = qobject_cast<QVBoxLayout *>(m_rightPanel->layout()))
        v->insertWidget(2, m_lampRow);           // below the name field
    m_lampRow->hide();

    connect(m_outName, &QLineEdit::textEdited, this, [this](const QString &t) {
        m_edit.outputs[m_outEd->index()].name = t.trimmed();
        m_outEd->setName(t.trimmed());
        refreshAll();                    // the name shows wherever this output is meant
        markFileDirty();
    });

    auto *viewScroll = new QScrollArea;
    viewScroll->setWidgetResizable(true);
    viewScroll->setWidget(m_view);
    viewScroll->setFrameShape(QFrame::NoFrame);
    viewScroll->setMinimumWidth(560);

    auto *centre = new QWidget;
    auto *cv = new QVBoxLayout(centre);
    cv->setContentsMargins(0, 0, 0, 0);
    auto *addRow = new QHBoxLayout;
    auto *addIn  = new QPushButton(tr("+ Add input"));
    auto *addOut = new QPushButton(tr("+ Add output"));
    addIn->setToolTip(tr("Take the next free input and open its settings"));
    addOut->setToolTip(tr("Take the next free output and open its settings"));
    m_editWidgets << addIn << addOut;
    connect(addIn,  &QPushButton::clicked, this, [this] { addBlock(false); });
    connect(addOut, &QPushButton::clicked, this, [this] { addBlock(true); });
    addRow->addWidget(addIn);
    addRow->addStretch(1);
    addRow->addWidget(addOut);
    cv->addLayout(addRow);
    cv->addWidget(viewScroll, 1);

    auto *config = new QWidget;
    config->setObjectName(QStringLiteral("configArea"));
    auto *h = new QHBoxLayout(config);
    h->setContentsMargins(6, 6, 6, 6);
    // input settings | canvas | output settings, all three always visible; drag the handles to resize
    auto *split = new QSplitter(Qt::Horizontal);
    split->setObjectName(QStringLiteral("configSplitter"));
    split->setChildrenCollapsible(false);
    split->setHandleWidth(6);
    split->addWidget(m_leftPanel);
    split->addWidget(centre);
    split->addWidget(m_rightPanel);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setStretchFactor(2, 0);
    const QByteArray state = QSettings().value(QStringLiteral("ui/configSplitter")).toByteArray();
    if (state.isEmpty() || !split->restoreState(state))
        split->setSizes({ 460, 640, 460 });
    connect(split, &QSplitter::splitterMoved, this, [split] {
        QSettings().setValue(QStringLiteral("ui/configSplitter"), split->saveState());
    });
    h->addWidget(split);

    auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), config);
    connect(esc, &QShortcut::activated, this, &MainWindow::closePanels);

    setCentralWidget(config);              // straight into the workspace

    // Status bar
    m_connChip   = new QLabel;
    m_dirtyChip = new QLabel;
    m_dirtyChip->setObjectName(QStringLiteral("chipWarn"));
    m_progress = new QProgressBar;
    m_progress->setMaximumWidth(180);
    m_progress->setTextVisible(false);
    m_progress->hide();
    statusBar()->addPermanentWidget(m_dirtyChip);
    statusBar()->addPermanentWidget(m_progress);
    statusBar()->addPermanentWidget(m_connChip);

    // Wiring
    connect(m_view, &ConnectionView::inputSelected,  this, &MainWindow::selectInput);
    connect(m_view, &ConnectionView::liveTriggerRequested, this, [this](int in, int dir) {
        m_link->triggerInput(in, dir);
    });
    connect(m_view, &ConnectionView::liveValueRequested, this, &MainWindow::openLiveValue);
    connect(m_view, &ConnectionView::analogValueRequested, this, &MainWindow::openAnalogValue);
    connect(m_view, &ConnectionView::sensorRequested, this, [this](int in, bool occupied) {
        // simulation: the detector pulls its pin low while the block is occupied
        if (auto *sim = m_link->findChild<SimulatedDevice *>())
            sim->setPinLevel(m_edit.inputs[in].pin, !occupied);
    });
    connect(m_view, &ConnectionView::removeRequested, this, [this](bool output, int index) {
        if (output) { selectOutput(index); m_open = OpenOutput; }
        else        { selectInput(index);  m_open = OpenInput; }
        removeSelected();
    });
    connect(m_view, &ConnectionView::outputSelected, this, &MainWindow::selectOutput);
    connect(m_view, &ConnectionView::connectRequested, this, [this](int in, int out) {
        switch (connectOutput(m_edit, in, out, routeLimits()))
        {
        case ConnectResult::Connected:
        case ConnectResult::Replaced:
            statusBar()->showMessage(tr("%1 → %2").arg(Ui::inputDisplayName(m_edit.inputs[in], in),
                                                       Ui::outputDisplayName(m_edit.outputs[out], out)), 3000);
            break;
        case ConnectResult::AlreadyConnected:
            statusBar()->showMessage(tr("%1 already drives %2").arg(Ui::inputDisplayName(m_edit.inputs[in], in),
                                                                     Ui::outputDisplayName(m_edit.outputs[out], out)), 3000);
            break;
        case ConnectResult::RouteFull:
            statusBar()->showMessage(tr("The route of %1 already has %2 steps (maximum)")
                                         .arg(Ui::inputDisplayName(m_edit.inputs[in], in),
                                              QString::number(routeLimits().maxSteps)), 4000);
            break;
        case ConnectResult::PoolFull:
            statusBar()->showMessage(tr("The board has no room for more route steps (%1 in total)")
                                         .arg(routeLimits().pool), 4000);
            break;
        case ConnectResult::NotConnectable:
            if (out >= 0 && out < NumOutputs && m_edit.outputs[out].type == BlockSignal)
                statusBar()->showMessage(tr("%1 is a block signal: it sets its own aspect from its detectors, "
                                            "an input cannot switch it.").arg(Ui::outputDisplayName(m_edit.outputs[out], out)), 5000);
            break;
        }
        userEdited();
        selectInput(in);
    });
    connect(m_view, &ConnectionView::disconnectRequested, this, [this](int in, int step) {
        Gpio::disconnect(m_edit, in, step);
        userEdited();
    });
    connect(m_view, &ConnectionView::toggleStepRequested, this, [this](int in, int step) {
        toggleStepState(m_edit, in, step);
        userEdited();
    });
    connect(m_view, &ConnectionView::moveStepRequested, this, [this](int in, int step, int delta) {
        moveStep(m_edit, in, step, delta);
        userEdited();
    });
    connect(m_outEd, &OutputEditor::changed, this, [this](int i, const OutputConfig &c) {
        QVector<quint8> fresh;                                      // pins this edit newly gives to output i
        const OutputTypeInfo &info = outputTypeInfo(c.type);
        for (int k = 0; k < 7; ++k)
        {
            const bool active = (k < 4) ? (k < info.pinCount) : info.relays;
            const quint8 p = c.pin(k);
            bool had = false;
            for (int j = 0; j < 7; ++j) had |= (m_edit.outputs[i].pin(j) == p);
            if (active && p < NumPins && !had) fresh << p;
        }
        const QString taken = takeOverPins(fresh, i, -1, Ui::pinShare(c));
        if (!taken.isEmpty()) statusBar()->showMessage(taken, 8000);
        const QString name = m_edit.outputs[i].name;
        const QStringList lamps = m_edit.outputs[i].lampColors;
        const quint8 oldType = m_edit.outputs[i].type;
        for (InputConfig &in : m_edit.inputs)           // toggle / absolute inputs on this output
        {
            if (!isDirectType(in.type) || in.outputId != i || oldType == c.type) continue;
            if (c.type == SoundEffect && !in.hasCustomValues())
                in.setCustomValues(true, c.type);        // a sound output plays the input's tracks
            else if (in.hasCustomValues() && stepKind(c.type) != stepKind(oldType))
                in.setCustomValues(true, c.type);        // another kind of value: start from its defaults
        }
        m_edit.outputs[i] = c;
        m_edit.outputs[i].name = name;
        m_edit.outputs[i].lampColors = lamps;
        refreshLampRow();
        m_inEd->setOutputs(outputNames(), outputTypes());
        userEdited();
    });
    connect(m_outEd, &OutputEditor::liveServo, this, [this](int i, int angle) {
        m_link->servoLive(i, angle);    // queued sends are merged, only the newest angle goes out
    });
    connect(m_inEd, &InputEditor::changed, this, [this](int i, const InputConfig &c) {
        if (c.type != InputNotInUse && c.pin < NumPins
            && (c.pin != m_edit.inputs[i].pin || c.analog != m_edit.inputs[i].analog))
        {
            const QString taken = takeOverPins({ c.pin }, -1, i, Ui::pinShare(c));
            if (!taken.isEmpty()) statusBar()->showMessage(taken, 8000);
        }
        const QString name = m_edit.inputs[i].name;
        m_edit.inputs[i] = c;
        m_edit.inputs[i].name = name;
        userEdited();
    });
    connect(m_outEd, &OutputEditor::testRequested, this, &MainWindow::testOutput);
}

void MainWindow::userEdited()
{
    for (int i = 0; i < NumOutputs; ++i) if (m_edit.outputs[i] != m_shadow.outputs[i]) m_outTouched[i] = true;
    for (int i = 0; i < NumInputs; ++i)  if (m_edit.inputs[i]  != m_shadow.inputs[i])  m_inTouched[i]  = true;
    m_shadow = m_edit;
    m_liveTried.clear();                 // a new edit: everything may be sent again
    m_liveWarned = false;
    boardEdited();
    if (m_link->isReady())
        m_liveTimer->start();           // (re)start: send after a short pause
    markFileDirty();
}

void MainWindow::sendLive()
{
    if (!m_link->isReady() || dirtyCount() == 0)
        return;
    if (m_link->isBusy())
    {
        m_liveTimer->start();            // try again when the board is free
        return;
    }
    writeDirty(true);
}

void MainWindow::boardEdited()
{
    refreshRows();
    showInput(m_inEd->index());   // the panel is always visible
    updateInputEditorLimits();
    refreshWarnings();
    updateActions();
}

// ── Panels ───────────────────────────────────────────────────────────────────
// Refresh an editor only when what it shows differs from the configuration, and keep the
// scroll position: a read-back after every live write must not make the panel jump.
static QScrollArea *scrollOf(QWidget *w)
{
    for (QWidget *p = w ? w->parentWidget() : nullptr; p; p = p->parentWidget())
        if (auto *sa = qobject_cast<QScrollArea *>(p))
            return sa;
    return nullptr;
}

void MainWindow::showInput(int i)
{
    if (m_inEd->index() == i && m_inEd->config().normalized() == m_edit.inputs[i].normalized())
        return;
    const bool same = m_inEd->index() == i;
    QScrollArea *sa = scrollOf(m_inEd);
    const int y = sa ? sa->verticalScrollBar()->value() : 0;
    m_inEd->setInput(i, m_edit.inputs[i]);
    if (same && sa)
        QTimer::singleShot(0, sa, [sa, y] { sa->verticalScrollBar()->setValue(y); });
}

void MainWindow::showOutput(int i)
{
    if (m_outEd->index() == i && m_outEd->config().normalized() == m_edit.outputs[i].normalized())
        return;
    const bool same = m_outEd->index() == i;
    QScrollArea *sa = scrollOf(m_outEd);
    const int y = sa ? sa->verticalScrollBar()->value() : 0;
    m_outEd->setOutput(i, m_edit.outputs[i]);
    if (same && sa)
        QTimer::singleShot(0, sa, [sa, y] { sa->verticalScrollBar()->setValue(y); });
}

void MainWindow::openPanel(OpenPanel which)
{
    m_open = which;                      // both panels are always visible; this is the one in focus
}

void MainWindow::closePanels()
{
    openPanel(OpenNone);
    m_view->selectInput(-1);
}

// ── Board capabilities / profile ────────────────────────────────────────────
RouteLimits MainWindow::routeLimits() const
{
    RouteLimits l;
    const DeviceCaps &c = m_link->caps();
    if (m_link->isOpen() && c.known)
    {
        l.maxSteps = qBound(1, c.maxRouteSteps, MaxRouteSteps);
        l.pool     = qMax(1, c.stepPool);
    }
    return l;                            // offline: what a decoder can do
}

void MainWindow::applyCapabilities()
{
    const DeviceCaps &c = m_link->caps();
    const bool known = m_link->isOpen() && c.known;
    boardOptions().serialPinsUsable = known ? c.serialPins : true;
    m_outEd->setLiveAvailable(known && c.servoLive);
    m_inEd->setLiveReadingAvailable(m_link->isReady() && c.analogSound);
    m_outEd->refreshPinCombos();
    m_inEd->refreshPinCombos();
    updateInputEditorLimits();
    refreshAll();
}

void MainWindow::updateInputEditorLimits()
{
    const int i = m_inEd->index();
    const int mine = isRouteType(m_edit.inputs[i].type) || m_edit.inputs[i].hasCustomValues()
                         ? m_edit.inputs[i].route.size() : 0;
    m_inEd->setRouteLimits(routeLimits(), totalRouteSteps(m_edit) - mine);
}

void MainWindow::setBoardType(int type)
{
    m_edit.board.type = quint8(type);
    Gpio::setBoardType(m_edit.board.type);
    QSettings().setValue(QStringLiteral("board/type"), type);
    m_inEd->setTimingEnabled(m_edit.board.timing);
    if (m_boardType && m_boardType->currentIndex() != type)
        m_boardType->setCurrentIndex(type);
    m_outEd->refreshPinCombos();
    m_inEd->refreshPinCombos();
    refreshAll();
}

void MainWindow::buildToolbar()
{
    auto *tb = addToolBar(tr("Main"));
    tb->setObjectName(QStringLiteral("mainToolbar"));
    tb->setMovable(false);
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    tb->setIconSize(QSize(16, 16));

    // Open / Save at the far left (Ctrl+O / Ctrl+S); the actions are created with the File menu below
    QAction *tbOpen = tb->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Open"));
    QAction *tbSave = tb->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save"));
    tbOpen->setToolTip(tr("Open a board file (Ctrl+O)"));
    tbSave->setToolTip(tr("Save to this board's file (Ctrl+S)"));
    connect(tbOpen, &QAction::triggered, this, [this] { openFile(); });
    connect(tbSave, &QAction::triggered, this, [this] { quickSave(); });
    tb->addSeparator();

    m_port = new QComboBox;
    m_port->setMinimumWidth(200);
    m_port->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    tb->addWidget(new QLabel(tr("  Port ")));
    tb->addWidget(m_port);
    m_actRefresh = tb->addAction(style()->standardIcon(QStyle::SP_BrowserReload), QString(), this, &MainWindow::refreshPorts);
    m_actRefresh->setToolTip(tr("Refresh port list"));
    m_actConnect = tb->addAction(tr("Connect"), this, &MainWindow::toggleConnection);   // also in the Board menu
    m_actConnectTb = m_actConnect;
    m_connState = new QLabel;
    m_connState->setObjectName(QStringLiteral("chip"));
    tb->addWidget(m_connState);
    tb->addSeparator();

    // Always one of three modes, in the colours of the canvas frame:
    // Configure (red when connected) = change things, offline or live on the board;
    // Simulate (yellow) = your configuration on a simulated board, look and press buttons only;
    // Live test (green when connected) = press buttons on the real board, look only. Connects by itself.
    auto *modes = new QActionGroup(this);
    modes->setExclusive(true);
    m_actConfigure = tb->addAction(tr("● Configure"));
    m_actConfigure->setToolTip(tr("Change the configuration: offline, or connected (red frame) so every change goes "
                                  "to the board at once."));
    m_actDemo = tb->addAction(tr("● Simulate"));
    m_actDemo->setToolTip(tr("Try your configuration without a board: press the buttons on the inputs and watch "
                             "the outputs. Nothing can be changed here."));
    m_actLive = tb->addAction(tr("● Live test"));
    m_actLive->setToolTip(tr("Press the buttons on the inputs and watch what the real board does. Connects by itself. "
                             "Blue = straight / on, red = curved / off; servos show their angle."));
    for (QAction *a : { m_actConfigure, m_actDemo, m_actLive }) { a->setCheckable(true); modes->addAction(a); }
    m_actConfigure->setChecked(true);
    connect(m_actConfigure, &QAction::triggered, this, [this] { setMode(ModeConfigure); });
    connect(m_actDemo,      &QAction::triggered, this, [this] { setMode(ModeSimulate); });
    connect(m_actLive,      &QAction::triggered, this, [this] { setMode(ModeLive); });
    if (QWidget *w = tb->widgetForAction(m_actConfigure)) w->setObjectName(QStringLiteral("modeConfigure"));
    if (QWidget *w = tb->widgetForAction(m_actDemo))      w->setObjectName(QStringLiteral("modeSimulate"));
    if (QWidget *w = tb->widgetForAction(m_actLive))      w->setObjectName(QStringLiteral("modeLive"));
    tb->addWidget(Info::button(QStringLiteral("modes")));

    // second row, always visible: board contents, presets and board type
    addToolBarBreak();
    tb = addToolBar(tr("Board"));
    tb->setObjectName(QStringLiteral("boardToolbar"));
    tb->setMovable(false);
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    tb->setIconSize(QSize(16, 16));

    m_actRead   = tb->addAction(style()->standardIcon(QStyle::SP_ArrowDown), tr("Read board"), this, &MainWindow::readFromBoard);
    m_actRead->setToolTip(tr("Load the configuration from the board (replaces what is on screen)"));
    m_actWriteAll = tb->addAction(style()->standardIcon(QStyle::SP_ArrowUp), tr("Write all"), this, &MainWindow::writeAllToBoard);
    m_actWriteAll->setToolTip(tr("Put the whole configuration on screen on the board"));
    m_actWrite  = new QAction(style()->standardIcon(QStyle::SP_ArrowUp), tr("Write changes"), this);
    connect(m_actWrite, &QAction::triggered, this, &MainWindow::writeChanges);
    m_actRevert = new QAction(tr("Revert"), this);
    connect(m_actRevert, &QAction::triggered, this, &MainWindow::revertChanges);
    tb->addSeparator();

    m_preset = new QComboBox;
    m_preset->setObjectName(QStringLiteral("presetCombo"));
    for (const PresetFile &p : presetFiles())
        m_preset->addItem(p.name, p.path);
    m_preset->setToolTip(tr("Ready-made configurations from %1").arg(QDir::toNativeSeparators(presetsDir())));
    tb->addWidget(new QLabel(tr(" Preset ")));
    tb->addWidget(m_preset);
    m_actPreset = tb->addAction(tr("Apply"), this, &MainWindow::applyPreset);
    tb->addSeparator();

    m_boardType = new QComboBox;
    for (int t = 0; t < BoardTypeCount; ++t)
        m_boardType->addItem(boardTypeName(quint8(t)));
    m_boardType->setCurrentIndex(m_edit.board.type);
    m_boardType->setToolTip(tr("Which board this is: decides which pins exist"));
    connect(m_boardType, QOverload<int>::of(&QComboBox::activated), this, [this](int t) {
        setBoardType(t);
        if (m_link->isReady() && m_link->caps().boardSettings)
            m_link->writeBoard(m_edit.board);
        markFileDirty();
    });
    tb->addWidget(new QLabel(tr(" Board ")));
    tb->addWidget(m_boardType);
    tb->addWidget(Info::button(QStringLiteral("board.dccnext")));
    m_actBoardSettings = tb->addAction(tr("Board settings…"), this, &MainWindow::editBoardSettings);

    // Menus
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&New board (offline)…"), this, &MainWindow::newOfflineBoard);
    m_actOpen = file->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("&Open configuration…"),
                                this, &MainWindow::openFile);
    m_actOpen->setShortcut(QKeySequence::Open);
    m_actSave = file->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("&Save"),
                                this, &MainWindow::quickSave);
    m_actSave->setShortcut(QKeySequence::Save);
    m_actSave->setToolTip(tr("Save to this board's file (Ctrl+S)"));
    m_actSaveAs = file->addAction(tr("Save &as…"), this, &MainWindow::saveFile);
    m_actSaveAs->setShortcut(QKeySequence::SaveAs);
    file->addAction(tr("Open &example…"), this, &MainWindow::openExampleDialog);
    file->addSeparator();
    file->addAction(tr("&Export…"), this, &MainWindow::exportDialog)
        ->setToolTip(tr("Save some outputs, inputs or the board settings in a file of their own, e.g. as your own example"));
    file->addAction(tr("&Import…"), this, &MainWindow::importDialog)
        ->setToolTip(tr("Add the outputs, inputs or board settings of another board file to this one"));
    file->addSeparator();
    file->addAction(tr("Open the boards folder"), this, [] {
        QDir().mkpath(boardsDir());
        QDesktopServices::openUrl(QUrl::fromLocalFile(boardsDir()));
    });
    file->addSeparator();
    file->addAction(tr("&Quit"), this, &QWidget::close)->setShortcut(QKeySequence::Quit);

    QMenu *board = menuBar()->addMenu(tr("&Board"));
    board->addAction(m_actConnect);
    board->addSeparator();
    board->addAction(m_actRead);
    board->addAction(m_actWrite);
    board->addAction(m_actRevert);
    board->addSeparator();
    board->addAction(m_actBoardSettings);
    board->addAction(tr("Apply a preset…"), this, [this] { m_preset->setFocus(); });
    board->addSeparator();
    board->addAction(tr("&Flash firmware…"), this, &MainWindow::flashFirmware);
    board->addAction(tr("&Phone remote…"), this, &MainWindow::openPhoneRemote);
    m_actWrite->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    m_actRead->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));

    QMenu *view = menuBar()->addMenu(tr("&View"));
    // user level: decides what is offered, never changes the configuration
    auto *levels = new QActionGroup(view);
    levels->setExclusive(true);
    const QString levelTips[3] = { tr("Servos, lights and turnout coils; buttons, DCC addresses and simple routes"),
                                   tr("Also light effects, signals, PWM, sound, frog relays, analog inputs and custom values"),
                                   tr("Also automatic block signals, sensor inputs and semaphore arms") };
    for (int l = Ui::Beginner; l <= Ui::Expert; ++l)
    {
        QAction *a = view->addAction(Ui::levelName(l));
        a->setCheckable(true);
        a->setToolTip(levelTips[l]);
        a->setStatusTip(levelTips[l]);
        levels->addAction(a);
        m_actLevel << a;
        connect(a, &QAction::triggered, this, [this, l] { setLevel(l); });
    }
    view->addAction(tr("What do the levels offer?"), this, [this] {
        Info::showDialog(QStringLiteral("levels"), tr("User levels"), this);
    });
    view->addSeparator();
    m_actShowUnused = view->addAction(tr("Show unused inputs and outputs"));
    m_actShowUnused->setCheckable(true);
    m_actShowUnused->setChecked(QSettings().value(QStringLiteral("ui/showUnused2"), false).toBool());
    m_view->setHideUnused(!m_actShowUnused->isChecked());
    connect(m_actShowUnused, &QAction::toggled, this, [this](bool on) {
        QSettings().setValue(QStringLiteral("ui/showUnused2"), on);
        m_view->setHideUnused(!on);
    });

    // Theme: automatic (Windows), light, dark; applied at once
    view->addSeparator();
    QMenu *theme = view->addMenu(tr("Theme"));
    auto *themeGroup = new QActionGroup(theme);
    const QList<QPair<QString, QString>> themes = { { QStringLiteral("auto"), tr("Automatic (Windows)") },
                                                    { QStringLiteral("light"), tr("Light") },
                                                    { QStringLiteral("dark"), tr("Dark") } };
    for (const auto &t : themes)
    {
        QAction *a = theme->addAction(t.second);
        a->setCheckable(true);
        a->setChecked(Ui::themeSetting() == t.first);
        themeGroup->addAction(a);
        const QString key = t.first;
        connect(a, &QAction::triggered, this, [this, key] {
            Ui::setThemeSetting(key);
            m_view->update();
        });
    }

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    QAction *start = help->addAction(tr("&Getting started"), this, [this] {
        Info::showDialog(QStringLiteral("app.start"), tr("Getting started"), this);
    });
    start->setShortcut(QKeySequence::HelpContents);
    help->addAction(tr("&Configure, simulate and live test"), this, [this] {
        Info::showDialog(QStringLiteral("modes"), tr("Configure, simulate and live test"), this);
    });
    help->addAction(tr("&User levels"), this, [this] {
        Info::showDialog(QStringLiteral("levels"), tr("User levels"), this);
    });
    help->addAction(tr("&Board settings"), this, [this] {
        Info::showDialog(QStringLiteral("board.settings"), tr("Board settings"), this);
    });
    help->addAction(tr("Config &button and teaching servos (9999)"), this, [this] {
        Info::showDialog(QStringLiteral("board.config"), tr("Config button and teaching servos"), this);
    });
    help->addSeparator();
    help->addAction(tr("&About"), this, [this] {
        QMessageBox::about(this, windowTitle(),
                           tr("<b>GPIOcore Configurator</b> %1<br><br>Configures GPIOcore boards (Arduino Nano / Uno, "
                              "DCCnext) for model railways.")
                               .arg(QApplication::applicationVersion()));
    });
}

// ── Connection ──────────────────────────────────────────────────────────────
void MainWindow::refreshPorts()
{
    const QString previous = m_port->currentData().toString();
    m_port->clear();
    int preferred = -1;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &p : ports)
    {
        QString text = p.portName();
        if (!p.description().isEmpty())
            text += QStringLiteral("  —  ") + p.description();
        m_port->addItem(text, p.portName());
        const quint16 vid = p.vendorIdentifier();
        if (preferred < 0 && (vid == 0x2341 || vid == 0x2A03 || vid == 0x1A86 || vid == 0x0403 || vid == 0x10C4))
            preferred = m_port->count() - 1;    // Arduino, CH340, FTDI, CP210x
    }
    if (ports.isEmpty())
        m_port->addItem(tr("no serial ports found"), QString());

    const int prev = m_port->findData(previous);
    m_port->setCurrentIndex(prev >= 0 ? prev : qMax(0, preferred));
}

void MainWindow::toggleConnection()
{
    if (m_link->isOpen() && !m_simulating)
    {
        m_link->close();                 // disconnect: back to configuring offline
        setMode(ModeConfigure);
        return;
    }
    if (m_mode == ModeSimulate)
        setMode(ModeConfigure);          // stops the simulation
    connectPort(m_port->currentData().toString());
}

void MainWindow::connectPort(const QString &port)
{
    if (port.isEmpty())
    {
        QMessageBox::information(this, windowTitle(), tr("Select a serial port first, or use Demo."));
        return;
    }
    m_outKnown.fill(false);
    m_inKnown.fill(false);
    m_outTouched.fill(false);
    m_inTouched.fill(false);
    m_shadow = m_edit;
    m_nameAsked = false;
    m_simulating = false;
    QString err;
    if (!m_link->openSerial(port, &err))
        QMessageBox::warning(this, windowTitle(), tr("Could not open %1:\n%2").arg(port, err));
}

void MainWindow::connectDemo()
{
    setMode(ModeSimulate);
}

void MainWindow::connectSimulatedBoard()
{
    // like plugging in a board: Configure mode, nothing read automatically
    setMode(ModeConfigure);
    if (m_link->isOpen())
        m_link->close();
    m_outKnown.fill(false);
    m_inKnown.fill(false);
    m_outTouched.fill(false);
    m_inTouched.fill(false);
    m_shadow = m_edit;
    m_nameAsked = true;
    m_simulating = false;
    QString err;
    if (!m_link->openDevice(new SimulatedDevice, false, &err))     // the firmware in it waits for a request
        QMessageBox::warning(this, windowTitle(), err);
    updateFrame();
}

void MainWindow::setMode(Mode m)
{
    m_mode = m;
    m_actConfigure->setChecked(m == ModeConfigure);
    m_actDemo->setChecked(m == ModeSimulate);
    m_actLive->setChecked(m == ModeLive);
    switch (m)
    {
    case ModeConfigure:
        if (m_view->liveMode()) setLiveTest(false);
        if (m_simulating) m_link->close();          // back to the real world (offline or a board)
        break;
    case ModeSimulate:
        if (m_view->liveMode()) setLiveTest(false);
        startSimulation();                          // the input buttons come on when it is ready
        break;
    case ModeLive:
        if (m_simulating) m_link->close();
        if (!m_link->isOpen())
            connectPort(m_port->currentData().toString());   // live test connects by itself
        else if (m_link->isReady())
            setLiveTest(true);
        break;
    }
    updateFrame();
}

void MainWindow::startSimulation()
{
    if (m_link->isOpen())
        m_link->close();
    m_outKnown.fill(false);
    m_inKnown.fill(false);
    m_nameAsked = true;                  // a simulation does not ask for a name
    // the simulated board starts with exactly what is on screen: your own configuration
    auto *sim = new SimulatedDevice;
    for (int i = 0; i < NumOutputs; ++i) sim->setOutput(i, m_edit.outputs[i]);
    for (int i = 0; i < NumInputs; ++i)  sim->setInput(i, m_edit.inputs[i]);
    sim->setBoard(m_edit.board);
    m_dev = m_edit;
    m_outKnown.fill(true);
    m_inKnown.fill(true);
    m_shadow = m_edit;
    m_simulating = true;
    QString err;
    if (!m_link->openDevice(sim, false, &err))
    {
        m_simulating = false;
        QMessageBox::warning(this, windowTitle(), err);
    }
    updateFrame();
    // a simulation is for trying things: the input buttons are on right away
    auto *once = new QObject(this);
    connect(m_link, &DeviceLink::finished, once, [this, once] {
        once->deleteLater();
        if (m_simulating && m_link->isReady() && !m_view->liveMode())
            setLiveTest(true);
    });
}

void MainWindow::updateConnState()
{
    if (!m_connState) return;
    QString text, name = QStringLiteral("chip");
    if (m_simulating)
        text = tr("● Simulation"), name = QStringLiteral("chipWarn");
    else if (m_link->isReady())
        text = tr("● Connected: %1 (%2)").arg(m_dev.board.name.isEmpty() ? tr("board") : m_dev.board.name, m_link->portName()),
        name = QStringLiteral("chipOk");
    else if (m_link->isOpen())
        text = tr("◌ Connecting to %1…").arg(m_link->portName());
    else
        text = tr("○ Offline");
    m_connState->setText(text);
    m_connState->setObjectName(name);
    m_connState->style()->unpolish(m_connState);
    m_connState->style()->polish(m_connState);
    m_actConnect->setText(m_link->isOpen() && !m_simulating ? tr("Disconnect") : tr("Connect"));
}

void MainWindow::updateFrame()
{
    using F = ConnectionView::Frame;
    const bool open = m_link->isOpen();
    m_view->setFrame(!open ? F::None : m_simulating ? F::Simulate : F::Configure);
    updateEditing();
    updateConnState();
}

void MainWindow::onConnectionChanged()
{
    const bool open = m_link->isOpen();
    m_actConnect->setText(open ? tr("Disconnect") : tr("Connect"));
    if (open)
    {
        m_connChip->setObjectName(QStringLiteral("chip"));
        m_connChip->setText(tr("● %1 — waiting for board…").arg(m_link->portName()));
    }
    else
    {
        m_connChip->setObjectName(QStringLiteral("chip"));
        m_connChip->setText(tr("○ Not connected"));
    }
    m_connChip->style()->unpolish(m_connChip);
    m_connChip->style()->polish(m_connChip);
    m_outEd->setConnected(open);
    if (!open)
    {
        if (m_view->liveMode()) setLiveTest(false);
        m_simulating = false;
        if (m_mode == ModeSimulate) { m_mode = ModeConfigure; m_actConfigure->setChecked(true); }
    }
    updateFrame();
    updateActions();
}

void MainWindow::onReady(const QString &version)
{
    m_connChip->setObjectName(QStringLiteral("chipOk"));
    m_connChip->setText(tr("● %1 — GPIOcore %2").arg(m_link->portName(), version));
    m_connChip->style()->unpolish(m_connChip);
    m_connChip->style()->polish(m_connChip);
    updateConnState();
    if (m_mode == ModeLive && !m_simulating)
    {
        auto *once = new QObject(this);            // after HELLO / board name: live buttons on
        connect(m_link, &DeviceLink::finished, once, [this, once] {
            once->deleteLater();
            if (m_mode == ModeLive && m_link->isReady() && !m_view->liveMode())
                setLiveTest(true);
        });
    }
    // Connecting changes nothing on screen: 'Read board' loads the board, 'Write all' sends the screen.
    if (!m_simulating)
        statusBar()->showMessage(tr("Connected. Read board loads its configuration; Write all puts the one on screen "
                                    "on the board. Changes you make now go to the board."), 12000);
}

// ── Data from the board ─────────────────────────────────────────────────────
void MainWindow::onOutputReceived(int i, const OutputConfig &c)
{
    const bool keep = m_keepEdits || (m_outKnown[i] && outputDirty(i));
    m_dev.outputs[i] = c;
    m_dev.outputs[i].name = m_edit.outputs[i].name;
    m_dev.outputs[i].lampColors = m_edit.outputs[i].lampColors;
    m_outKnown[i] = true;
    if (!keep)
        m_edit.outputs[i] = m_dev.outputs[i];
    m_shadow.outputs[i] = m_edit.outputs[i];
    refreshRows();
    if (m_outEd->index() == i && !keep)
        showOutput(i);
    m_inEd->setOutputs(outputNames(), outputTypes());
    refreshWarnings();
    updateActions();
}

void MainWindow::onInputReceived(int i, const InputConfig &received)
{
    InputConfig c = received;
    c.route = m_dev.inputs[i].route;     // the real route follows with GET_ROUTE
    const bool keep = m_keepEdits || (m_inKnown[i] && inputDirty(i));
    c.name = m_edit.inputs[i].name;
    m_dev.inputs[i] = c;
    m_inKnown[i] = true;
    if (!keep)
        m_edit.inputs[i] = c;
    m_shadow.inputs[i] = m_edit.inputs[i];
    refreshRows();
    if (m_inEd->index() == i && !keep)
        showInput(i);
    refreshWarnings();
    updateActions();
}

void MainWindow::onRouteReceived(int i, const QVector<RouteStep> &steps)
{
    const bool keep = m_keepEdits
                   || (m_inKnown[i] && asStored(m_edit.inputs[i]).route != asStored(m_dev.inputs[i]).route);
    setRouteSteps(m_dev.inputs[i], steps);
    m_inKnown[i] = true;
    if (!keep)
        setRouteSteps(m_edit.inputs[i], steps);
    if (m_inEd->index() == i && !keep)
        showInput(i);
    boardEdited();
}

void MainWindow::readFromBoard()
{
    if (QMessageBox::question(this, windowTitle(),
                              tr("Read everything from the board?\n\nWhat is on screen now is replaced by the board's "
                                 "configuration. Save it first (Ctrl+S) if you want to keep it."))
        != QMessageBox::Yes)
        return;
    m_outKnown.fill(false);
    m_inKnown.fill(false);
    m_outTouched.fill(false);
    m_inTouched.fill(false);
    m_keepEdits = false;
    m_readingBoard = true;
    if (m_link->caps().boardSettings)
        m_link->readBoard();             // name first: the names come from that board's file
    m_link->readAll();
}

void MainWindow::writeAllToBoard()
{
    if (!m_link->isReady())
        return;
    const QString name = m_dev.board.name.isEmpty() ? tr("the board") : QStringLiteral("\"%1\"").arg(m_dev.board.name);
    if (QMessageBox::question(this, windowTitle(),
                              tr("Write the whole configuration on screen to %1?\n\nEverything on the board is "
                                 "replaced.").arg(name))
        != QMessageBox::Yes)
        return;
    m_outTouched.fill(true);
    m_inTouched.fill(true);
    if (m_link->caps().boardSettings)
        m_link->writeBoard(m_edit.board);
    m_liveTried.clear();
    writeDirty(false);
}

void MainWindow::writeChanges()
{
    m_liveTried.clear();
    writeDirty(false);
}

void MainWindow::writeDirty(bool live)
{
    // Live writing sends every edited item once. If the board still reports something else
    // afterwards (it cannot store it), the item is not sent again until the user edits again:
    // otherwise the GUI would write the same thing forever.
    QStringList refused;
    for (int i = 0; i < NumInputs; ++i)
        if (inputDirty(i))
        {
            if (live && m_liveTried.contains(i)) { refused << Ui::inputDisplayName(m_edit.inputs[i], i); continue; }
            m_liveTried.insert(i);
            m_link->writeInput(i, m_edit.inputs[i]);
            m_link->writeRoute(i, m_edit.inputs[i].route);
            m_link->readInput(i);                             // also reads the route back
        }
    for (int i = 0; i < NumOutputs; ++i)
        if (outputDirty(i))
        {
            if (live && m_liveTried.contains(100 + i)) { refused << Ui::outputDisplayName(m_edit.outputs[i], i); continue; }
            m_liveTried.insert(100 + i);
            m_link->writeOutput(i, m_edit.outputs[i]);
            m_link->readOutput(i);
        }
    if (!refused.isEmpty() && !m_liveWarned)
    {
        m_liveWarned = true;
        statusBar()->showMessage(tr("The board keeps a different value for %1; not sending it again.")
                                     .arg(refused.join(QStringLiteral(", "))), 8000);
    }
}

void MainWindow::revertChanges()
{
    for (int i = 0; i < NumOutputs; ++i) if (m_outKnown[i]) m_edit.outputs[i] = m_dev.outputs[i];
    for (int i = 0; i < NumInputs;  ++i) if (m_inKnown[i])  m_edit.inputs[i]  = m_dev.inputs[i];
    refreshAll();
    showOutput(m_outEd->index());
    showInput(m_inEd->index());
}

void MainWindow::applyPreset()
{
    const QString name = m_preset->currentText();
    const QString path = m_preset->currentData().toString();
    if (path.isEmpty())
        return;
    QMessageBox box(QMessageBox::Question, windowTitle(),
                    tr("Apply preset \"%1\"?\n\nThis replaces all outputs and inputs (and their routes).").arg(name),
                    QMessageBox::Yes | QMessageBox::No, this);
    auto *clear = new QCheckBox(tr("Also clear the names of inputs and outputs (recommended)"));
    clear->setChecked(true);
    box.setCheckBox(clear);
    if (box.exec() != QMessageBox::Yes)
        return;
    applyPresetNow(path, clear->isChecked());
}

void MainWindow::applyPresetNow(const QString &path, bool clearNames)
{
    // A preset is a board file; only its outputs, inputs and board type are used. It reaches the board
    // as normal output / input data (live write), so it also works offline and in a simulation.
    PresetFile file;
    if (!readPreset(path, file))
    {
        QMessageBox::warning(this, windowTitle(), tr("Cannot read the preset %1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    if (file.hasType && file.board.board.type != m_edit.board.type)
    {
        setBoardType(file.board.board.type);
        if (m_link->isReady() && m_link->caps().boardSettings)
            m_link->writeBoard(m_edit.board);
    }
    const BoardConfig &p = file.board;
    for (int i = 0; i < NumOutputs; ++i)
    {
        OutputConfig c = p.outputs[i];
        if (!clearNames)
        {
            c.name = m_edit.outputs[i].name;
            c.lampColors = m_edit.outputs[i].lampColors;
        }
        m_edit.outputs[i] = c;
    }
    for (int i = 0; i < NumInputs; ++i)
    {
        InputConfig c = p.inputs[i];
        if (!clearNames)
            c.name = m_edit.inputs[i].name;
        m_edit.inputs[i] = c;
    }
    showOutput(m_outEd->index());
    showInput(m_inEd->index());
    m_outName->setText(m_edit.outputs[m_outEd->index()].name);
    m_inName->setText(m_edit.inputs[m_inEd->index()].name);
    m_inEd->setOutputs(outputNames(), outputTypes());
    refreshRows();
    userEdited();
    markFileDirty();
    statusBar()->showMessage(tr("Preset applied"), 4000);
    raiseLevelFor();
}

void MainWindow::testOutput(int index, int state)
{
    m_edit.outputs[index].state = quint8(state);
    showOutput(index);
    m_link->writeOutput(index, m_edit.outputs[index]);
    m_link->readOutput(index);
    refreshRows();
}

// ── Files ───────────────────────────────────────────────────────────────────
void MainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open configuration"), boardsDir(),
                                                      tr("GPIOcore configuration (*.json)"));
    if (!path.isEmpty())
        loadFile(path);
}

static bool readConfig(const QString &path, BoardConfig &b, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        *error = f.errorString();
        return false;
    }
    return BoardConfig::fromJson(QJsonDocument::fromJson(f.readAll()).object(), b, error);
}

bool MainWindow::loadFile(const QString &path)
{
    BoardConfig b;
    QString err;
    if (!readConfig(path, b, &err))
    {
        QMessageBox::warning(this, windowTitle(), err);
        return false;
    }
    if (m_link->isReady())
    {
        const BoardSettings onBoard = m_edit.board;
        m_edit = b;
        m_edit.board.name = onBoard.name;    // the name belongs to the connected board
        if (QMessageBox::question(this, windowTitle(),
                                  tr("Send this configuration to the connected board now?"))
            == QMessageBox::Yes)
        {
            if (m_link->caps().boardSettings)
                m_link->writeBoard(m_edit.board);
            m_liveTimer->start();
        }
    }
    else
    {
        m_edit = b;
        m_dev  = b;                          // offline: nothing to compare with
    }
    setBoardType(m_edit.board.type);
    if (!m_link->isReady())
        m_shadow = m_edit;
    refreshAll();
    showOutput(m_outEd->index());
    showInput(m_inEd->index());
    m_filePath = path;
    m_fileDirty = false;
    statusBar()->showMessage(tr("Loaded %1").arg(path), 6000);
    raiseLevelFor();
    return true;
}

bool MainWindow::openExample(const QString &path)
{
    if (!loadFile(path))
        return false;
    m_filePath.clear();                      // an example is a template: Save always asks for a new name
    statusBar()->showMessage(tr("Example opened. Save it under a name of your own (Ctrl+S); the example itself "
                                "is never changed."), 8000);
    return true;
}

QString MainWindow::examplesDir()
{
    const QString local = QCoreApplication::applicationDirPath() + QStringLiteral("/examples");
#ifdef GPIOCORE_SOURCE_DIR
    if (!QFileInfo(local).isDir())
        return QStringLiteral(GPIOCORE_SOURCE_DIR) + QStringLiteral("/examples");   // dev build: the source tree
#endif
    return local;
}

QString MainWindow::presetsDir()
{
    const QString local = QCoreApplication::applicationDirPath() + QStringLiteral("/presets");
#ifdef GPIOCORE_SOURCE_DIR
    if (!QFileInfo(local).isDir())
        return QStringLiteral(GPIOCORE_SOURCE_DIR) + QStringLiteral("/presets");   // dev build: the source tree
#endif
    return local;
}

// "Ahob level crossing" from ahob-level-crossing.json
static QString exampleTitle(const QString &path)
{
    QString t = QFileInfo(path).completeBaseName().replace(QLatin1Char('-'), QLatin1Char(' '));
    if (!t.isEmpty())
        t[0] = t[0].toUpper();
    return t;
}

// What is in it: the board name and the first names, then the counts
static QString exampleDescription(const BoardConfig &b)
{
    QStringList names;
    int outs = 0, ins = 0;
    for (const OutputConfig &o : b.outputs)
    {
        if (o.type == OutputNotInUse) continue;
        ++outs;
        if (!o.name.isEmpty() && names.size() < 3) names << o.name;
    }
    for (const InputConfig &i : b.inputs)
    {
        if (i.type == InputNotInUse) continue;
        ++ins;
        if (!i.name.isEmpty() && names.size() < 4) names << i.name;
    }
    if (!b.board.name.isEmpty())
        names.prepend(b.board.name);
    QString d = QCoreApplication::translate("MainWindow", "%1 output(s), %2 input(s), %3")
                    .arg(outs).arg(ins).arg(boardTypeName(b.board.type));
    if (!names.isEmpty())
        d = names.join(QStringLiteral(", ")) + QStringLiteral(" · ") + d;
    return d;
}

void MainWindow::openExampleDialog()
{
    QDialog d(this);
    d.setObjectName(QStringLiteral("exampleDialog"));
    d.setWindowTitle(tr("Open example"));
    auto *v = new QVBoxLayout(&d);
    auto *intro = new QLabel(tr("Start from an example. It opens like a board file, but without a file name: when you "
                                "save, you choose a new name, so the example stays as it is."));
    intro->setWordWrap(true);
    v->addWidget(intro);
    auto *list = new QListWidget;
    const QDir dir(examplesDir());
    for (const QString &f : dir.entryList({ QStringLiteral("*.json") }, QDir::Files, QDir::Name))
    {
        BoardConfig b;
        QString err;
        if (!readConfig(dir.filePath(f), b, &err))
            continue;
        auto *item = new QListWidgetItem(exampleTitle(f) + QLatin1Char('\n') + exampleDescription(b), list);
        item->setData(Qt::UserRole, dir.filePath(f));
        item->setToolTip(QDir::toNativeSeparators(dir.filePath(f)));
    }
    if (list->count() == 0)
        new QListWidgetItem(tr("No examples found in %1").arg(QDir::toNativeSeparators(dir.path())), list);
    list->setCurrentRow(0);
    list->setSpacing(3);
    v->addWidget(list, 1);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, &d, &QDialog::accept);
    v->addWidget(bb);
    d.resize(620, 480);
    if (d.exec() != QDialog::Accepted || !list->currentItem())
        return;
    const QString path = list->currentItem()->data(Qt::UserRole).toString();
    if (!path.isEmpty())
        openExample(path);
}

// ── Export / import ─────────────────────────────────────────────────────────
// Check boxes Outputs / Inputs / Board settings, with the number of items in use
static QVector<QCheckBox *> sectionBoxes(QFormLayout *form, int outputs, int inputs)
{
    const QVector<QCheckBox *> boxes {
        new QCheckBox(QCoreApplication::translate("MainWindow", "Outputs (%1 in use)").arg(outputs)),
        new QCheckBox(QCoreApplication::translate("MainWindow", "Inputs (%1 in use)").arg(inputs)),
        new QCheckBox(QCoreApplication::translate("MainWindow", "Board settings (type, route timing, interlocking)")),
    };
    const char *const names[3] = { "partOutputs", "partInputs", "partBoard" };
    for (int k = 0; k < 3; ++k)
    {
        boxes[k]->setObjectName(QLatin1String(names[k]));
        const bool empty = ( k == 0 && outputs == 0 ) || ( k == 1 && inputs == 0 );   // nothing in use: nothing to take
        boxes[k]->setChecked(k < 2 && !empty);
        boxes[k]->setEnabled(!empty);
        form->addRow(boxes[k]);
    }
    return boxes;
}

static int sectionsOf(const QVector<QCheckBox *> &boxes)
{
    return (boxes[0]->isChecked() ? PartOutputs : 0) | (boxes[1]->isChecked() ? PartInputs : 0)
         | (boxes[2]->isChecked() ? PartBoard : 0);
}

static QDialogButtonBox *sectionButtons(QDialog &d, QFormLayout *form, const QVector<QCheckBox *> &boxes, const QString &ok)
{
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText(ok);
    QObject::connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    for (QCheckBox *b : boxes)               // nothing ticked: nothing to do
        QObject::connect(b, &QCheckBox::toggled, &d, [bb, boxes] {
            bb->button(QDialogButtonBox::Ok)->setEnabled(sectionsOf(boxes) != 0);
        });
    form->addRow(bb);
    return bb;
}

bool MainWindow::exportParts(const QString &path, int sections)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        QMessageBox::warning(this, windowTitle(), f.errorString());
        return false;
    }
    f.write(QJsonDocument(exportPart(m_edit, sections).toJson()).toJson(QJsonDocument::Indented));
    statusBar()->showMessage(tr("Exported to %1").arg(QDir::toNativeSeparators(path)), 6000);
    return true;
}

void MainWindow::exportDialog()
{
    int outs = 0, ins = 0;
    for (const OutputConfig &o : m_edit.outputs) outs += o.type != OutputNotInUse;
    for (const InputConfig &i : m_edit.inputs)   ins  += i.type != InputNotInUse;
    QDialog d(this);
    d.setObjectName(QStringLiteral("exportDialog"));
    d.setWindowTitle(tr("Export"));
    auto *form = new QFormLayout(&d);
    auto *intro = new QLabel(tr("Save the items in use of the parts you tick in a board file of their own, with their "
                                "names, settings and routes, e.g. to make your own example. Import adds them to "
                                "another board."));
    intro->setWordWrap(true);
    form->addRow(intro);
    const QVector<QCheckBox *> boxes = sectionBoxes(form, outs, ins);
    sectionButtons(d, form, boxes, tr("Export…"));
    if (d.exec() != QDialog::Accepted)
        return;
    QDir().mkpath(boardsDir());
    const QString path = QFileDialog::getSaveFileName(this, tr("Export"), QDir(boardsDir()).filePath(QStringLiteral("export.json")),
                                                      tr("GPIOcore configuration (*.json)"));
    if (!path.isEmpty())
        exportParts(path, sectionsOf(boxes));
}

void MainWindow::importDialog()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Import from a board file"), boardsDir(),
                                                      tr("GPIOcore configuration (*.json)"));
    if (path.isEmpty())
        return;
    BoardConfig file;
    QString err;
    if (!readConfig(path, file, &err))
    {
        QMessageBox::warning(this, windowTitle(), err);
        return;
    }
    int outs = 0, ins = 0;
    for (const OutputConfig &o : file.outputs) outs += o.type != OutputNotInUse;
    for (const InputConfig &i : file.inputs)   ins  += i.type != InputNotInUse;
    QDialog d(this);
    d.setObjectName(QStringLiteral("importDialog"));
    d.setWindowTitle(tr("Import from %1").arg(QFileInfo(path).fileName()));
    auto *form = new QFormLayout(&d);
    auto *intro = new QLabel(tr("Add the items in use of the parts you tick to this board."));
    intro->setWordWrap(true);
    form->addRow(intro);
    const QVector<QCheckBox *> boxes = sectionBoxes(form, outs, ins);
    auto *where = new QLabel(tr("Where to:"));
    where->setObjectName(QStringLiteral("help"));
    auto *same = new QRadioButton(tr("Same numbers (replace): every item goes to its own number"));
    auto *freeSlots = new QRadioButton(tr("Into free slots: the first free outputs and inputs, links between the imported "
                                     "items are renumbered"));
    same->setObjectName(QStringLiteral("importSame"));
    freeSlots->setObjectName(QStringLiteral("importFree"));
    freeSlots->setChecked(true);
    form->addRow(where);
    form->addRow(same);
    form->addRow(freeSlots);
    auto *note = new QLabel(tr("A pin that is already in use goes to the imported item: your own item keeps its "
                               "function, but loses that pin. The items concerned are listed afterwards."));
    note->setWordWrap(true);
    note->setObjectName(QStringLiteral("help"));
    form->addRow(note);
    sectionButtons(d, form, boxes, tr("Import"));
    if (d.exec() != QDialog::Accepted)
        return;
    importParts(path, sectionsOf(boxes), same->isChecked());
}

bool MainWindow::importParts(const QString &path, int sections, bool sameNumbers)
{
    BoardConfig file;
    QString err;
    if (!readConfig(path, file, &err))
    {
        QMessageBox::warning(this, windowTitle(), err);
        return false;
    }
    const ImportResult plan = planImport(m_edit, file, sections, sameNumbers);
    if (!plan.error.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(), plan.error);
        return false;
    }
    QStringList replaced;
    int outs = 0, ins = 0;
    for (int o = 0; o < NumOutputs; ++o)
    {
        const int t = plan.outputs[o];
        if (t < 0) continue;
        ++outs;
        const OutputConfig &mine = m_edit.outputs[t];
        if (mine.type != OutputNotInUse)
            replaced << tr("%1 · %2").arg(Ui::outputDisplayName(mine, t), outputTypeName(mine.type));
    }
    for (int i = 0; i < NumInputs; ++i)
    {
        const int t = plan.inputs[i];
        if (t < 0) continue;
        ++ins;
        const InputConfig &mine = m_edit.inputs[t];
        if (mine.type != InputNotInUse)
            replaced << tr("%1 · %2").arg(Ui::inputDisplayName(mine, t), inputTypeName(mine.type));
    }
    if (!replaced.isEmpty()
        && QMessageBox::question(this, windowTitle(),
                                 tr("These items of yours are replaced by the ones from the file:\n\n%1\n\nImport?")
                                     .arg(replaced.join(QLatin1Char('\n'))))
               != QMessageBox::Yes)
        return false;

    // The target slots are emptied first, so taking over pins only touches the items that stay.
    for (int o = 0; o < NumOutputs; ++o) if (plan.outputs[o] >= 0) m_edit.outputs[plan.outputs[o]] = OutputConfig();
    for (int i = 0; i < NumInputs; ++i)  if (plan.inputs[i] >= 0)  m_edit.inputs[plan.inputs[i]] = InputConfig();
    QStringList taken;
    QVector<OutputConfig> newOut(NumOutputs);
    QVector<InputConfig>  newIn(NumInputs);
    for (int o = 0; o < NumOutputs; ++o)
    {
        const int t = plan.outputs[o];
        if (t < 0) continue;
        newOut[o] = importedOutput(file, o, plan);
        const OutputTypeInfo &info = outputTypeInfo(newOut[o].type);
        QVector<quint8> pins;
        for (int k = 0; k < 7; ++k)
            if (((k < 4) ? (k < info.pinCount) : info.relays) && newOut[o].pin(k) < NumPins)
                pins << newOut[o].pin(k);
        const QString msg = takeOverPins(pins, t, -1, Ui::pinShare(newOut[o]));
        if (!msg.isEmpty()) taken << tr("%1: %2").arg(Ui::outputDisplayName(newOut[o], t), msg);
    }
    for (int i = 0; i < NumInputs; ++i)
    {
        const int t = plan.inputs[i];
        if (t < 0) continue;
        newIn[i] = importedInput(file, i, plan);
        if (newIn[i].pin >= NumPins) continue;
        const QString msg = takeOverPins({ newIn[i].pin }, -1, t, Ui::pinShare(newIn[i]));
        if (!msg.isEmpty()) taken << tr("%1: %2").arg(Ui::inputDisplayName(newIn[i], t), msg);
    }
    for (int o = 0; o < NumOutputs; ++o) if (plan.outputs[o] >= 0) m_edit.outputs[plan.outputs[o]] = newOut[o];
    for (int i = 0; i < NumInputs; ++i)  if (plan.inputs[i] >= 0)  m_edit.inputs[plan.inputs[i]] = newIn[i];
    if (sections & PartBoard)
    {
        const QString name = m_edit.board.name;          // the name belongs to this board
        m_edit.board = file.board;
        m_edit.board.name = name;
        setBoardType(m_edit.board.type);
        if (m_link->isReady() && m_link->caps().boardSettings)
            m_link->writeBoard(m_edit.board);
    }
    // one edit: refreshed, sent to a connected board, the file is dirty
    showOutput(m_outEd->index());
    showInput(m_inEd->index());
    m_outName->setText(m_edit.outputs[m_outEd->index()].name);
    m_inName->setText(m_edit.inputs[m_inEd->index()].name);
    m_inEd->setOutputs(outputNames(), outputTypes());
    userEdited();
    statusBar()->showMessage(tr("Imported %1 output(s) and %2 input(s) from %3").arg(outs).arg(ins)
                                 .arg(QFileInfo(path).fileName()), 8000);
    if (!taken.isEmpty())
        QMessageBox::information(this, windowTitle(), tr("Some pins were already in use:\n\n%1").arg(taken.join(QLatin1Char('\n'))));
    raiseLevelFor();
    return true;
}

// ── User level raised by a configuration ────────────────────────────────────
void MainWindow::raiseLevelFor()
{
    // only what the GUI shows: never lowered, the configuration never changed
    const int need = Ui::configLevel(m_edit);
    if (need <= Ui::level())
        return;
    const QStringList features = Ui::levelFeatures(m_edit, Ui::level());
    setLevel(need);
    QMessageBox::information(this, windowTitle(), tr("This project uses %1 features (%2). The level is set to %1.")
                                                      .arg(Ui::levelName(need), features.join(QStringLiteral(", "))));
}

void MainWindow::saveFile()
{
    QDir().mkpath(boardsDir());
    const QString suggested = QDir(boardsDir()).filePath(
        (m_edit.board.name.isEmpty() ? QStringLiteral("gpiocore") : m_edit.board.name) + QStringLiteral(".json"));
    const QString path = QFileDialog::getSaveFileName(this, tr("Save configuration"),
                                                      m_filePath.isEmpty() ? suggested : m_filePath,
                                                      tr("GPIOcore configuration (*.json)"));
    if (path.isEmpty())
        return;
    backupBoardFile(path);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        QMessageBox::warning(this, windowTitle(), f.errorString());
        return;
    }
    f.write(QJsonDocument(m_edit.toJson()).toJson(QJsonDocument::Indented));
    m_filePath = path;
    m_fileDirty = false;
    setWindowTitle(m_edit.board.name.isEmpty() ? tr("GPIOcore Configurator")
                                               : tr("%1 — GPIOcore Configurator").arg(m_edit.board.name));
    statusBar()->showMessage(tr("Saved %1").arg(QDir::toNativeSeparators(path)), 4000);
}

// ── Dirty tracking ──────────────────────────────────────────────────────────
bool MainWindow::outputDirty(int i) const
{
    if (!m_outKnown[i])
        return m_link->isReady() && m_outTouched[i];     // board state unknown: send what the user changed
    return m_edit.outputs[i] != m_dev.outputs[i];
}

InputConfig MainWindow::asStored(const InputConfig &c) const
{
    InputConfig s = c;
    if (!m_dev.board.timing)             // timing off: the board keeps no wait time per step
        for (RouteStep &step : s.route)
            step.delay = 0;
    return s;
}

bool MainWindow::inputDirty(int i) const
{
    if (!m_inKnown[i])
        return m_link->isReady() && m_inTouched[i];
    return asStored(m_edit.inputs[i]) != asStored(m_dev.inputs[i]);
}

int MainWindow::dirtyCount() const
{
    int n = 0;
    for (int i = 0; i < NumOutputs; ++i) n += outputDirty(i);
    for (int i = 0; i < NumInputs;  ++i) n += inputDirty(i);
    return n;
}

void MainWindow::updateActions()
{
    const bool ready = m_link->isReady();
    const bool busy  = m_link->isBusy();
    const int  dirty = dirtyCount();
    m_actRead->setEnabled(ready && !busy && !m_simulating);
    if (m_actWriteAll) m_actWriteAll->setEnabled(ready && !busy && !m_simulating);
    m_actWrite->setEnabled(ready && !busy && dirty > 0);
    m_actWrite->setText(dirty ? tr("Write %1 change(s)").arg(dirty) : tr("Write changes"));
    m_actRevert->setEnabled(dirty > 0);
    m_actPreset->setEnabled(m_mode == ModeConfigure && !m_simulating);   // presets are files read by the GUI: also offline
    m_port->setEnabled(!m_link->isOpen());
    m_actRefresh->setEnabled(!m_link->isOpen());
    if (m_link->isReady())
        m_dirtyChip->setText(busy ? tr("Saving on the board…") : tr("%1 change(s) waiting").arg(dirty));
    else
        m_dirtyChip->setText(tr("%1 unsent change(s)").arg(dirty));
    m_dirtyChip->setVisible(dirty > 0 && (!m_link->isReady() || busy));
}

// ── Canvas refresh ──────────────────────────────────────────────────────────
bool MainWindow::outputHasConflict(int i) const
{
    const OutputConfig &c = m_edit.outputs[i];
    const OutputTypeInfo &info = outputTypeInfo(c.type);
    const auto usage = Ui::pinUsage(m_edit);
    for (int s = 0; s < 7; ++s)
    {
        const bool active = (s < 4) ? (s < info.pinCount) : info.relays;
        const quint8 p = c.pin(s);
        if (active && p < NumPins && (Ui::pinClash(usage[p]) || !pinReservedReason(p).isEmpty()))
            return true;
    }
    return false;
}

bool MainWindow::inputHasConflict(int i) const
{
    const InputConfig &c = m_edit.inputs[i];
    if (c.type == InputNotInUse || c.pin >= NumPins)
        return false;
    return Ui::pinClash(Ui::pinUsage(m_edit)[c.pin]) || !pinReservedReason(c.pin).isEmpty();
}

void MainWindow::refreshRows()
{
    Ui::setPinBoard(m_edit);                 // pin lists: used pins in red
    m_outEd->refreshPinMarks();
    m_inEd->refreshPinMarks();
    m_outEd->setInputs(inputNames(), inputTypes());   // block detectors of a signal
    // all nodes: pin conflicts and colours depend on the other inputs and outputs
    QVector<bool> inDirty(NumInputs), outDirty(NumOutputs), inConflict(NumInputs), outConflict(NumOutputs);
    for (int i = 0; i < NumInputs; ++i)  { inDirty[i]  = inputDirty(i);  inConflict[i]  = inputHasConflict(i);  }
    for (int i = 0; i < NumOutputs; ++i) { outDirty[i] = outputDirty(i); outConflict[i] = outputHasConflict(i); }
    m_view->setBoard(m_edit);
    m_view->setMarks(inDirty, outDirty, inConflict, outConflict);
}

void MainWindow::refreshAll()
{
    refreshRows();
    m_inEd->setOutputs(outputNames(), outputTypes());
    refreshWarnings();
    updateActions();
}

void MainWindow::selectOutput(int i)
{
    m_view->selectOutput(i);
    showOutput(i);
    m_outEd->refreshPinMarks();
    m_outEd->setWarnings(outputWarnings(i), outputNote(i));
    m_outName->setText(m_edit.outputs[i].name);
    refreshLampRow();
    openPanel(OpenOutput);
}

void MainWindow::selectInput(int i)
{
    m_view->selectInput(i);
    showInput(i);
    m_inEd->refreshPinMarks();
    m_inEd->setWarnings(inputWarnings(i), inputNote(i));
    m_inName->setText(m_edit.inputs[i].name);
    updateInputEditorLimits();
    openPanel(OpenInput);
}

void MainWindow::refreshWarnings()
{
    m_outEd->setWarnings(outputWarnings(m_outEd->index()), outputNote(m_outEd->index()));
    m_inEd->setWarnings(inputWarnings(m_inEd->index()), inputNote(m_inEd->index()));
}

QStringList MainWindow::outputNames() const
{
    QStringList names;
    for (int i = 0; i < NumOutputs; ++i)
        names << Ui::outputDisplayName(m_edit.outputs[i], i);
    return names;
}

QVector<quint8> MainWindow::outputTypes() const
{
    QVector<quint8> t;
    for (const OutputConfig &o : m_edit.outputs)
        t << o.type;
    return t;
}

QStringList MainWindow::inputNames() const
{
    QStringList names;
    for (int i = 0; i < NumInputs; ++i)
        names << Ui::inputDisplayName(m_edit.inputs[i], i);
    return names;
}

QVector<quint8> MainWindow::inputTypes() const
{
    QVector<quint8> t;
    for (const InputConfig &c : m_edit.inputs)
        t << c.type;
    return t;
}

QStringList MainWindow::outputWarnings(int i) const
{
    QStringList w;
    const OutputConfig &c = m_edit.outputs[i];
    const OutputTypeInfo &info = outputTypeInfo(c.type);
    const auto usage = Ui::pinUsage(m_edit);
    static const char *roles[7] = { QT_TRANSLATE_NOOP("Gpio", "Pin A"), QT_TRANSLATE_NOOP("Gpio", "Pin B"),
                                    QT_TRANSLATE_NOOP("Gpio", "Pin C"), QT_TRANSLATE_NOOP("Gpio", "Pin D"),
                                    QT_TRANSLATE_NOOP("Gpio", "Electrofrog A"), QT_TRANSLATE_NOOP("Gpio", "Electrofrog B"),
                                    QT_TRANSLATE_NOOP("Gpio", "Unifrog") };

    for (int s = 0; s < 7; ++s)
    {
        const bool active = (s < 4) ? (s < info.pinCount) : info.relays;
        const quint8 p = c.pin(s);
        if (!active) continue;
        const bool optional = s > 0 && (c.type == FourToneSignal || c.type == BlockSignal);   // fewer lamps
        if (s < info.pinCount && p == PinNA && !optional)
            w << tr("%1 is not set.").arg(trGpio(roles[s]));
        if (p >= NumPins) continue;
        const QString reserved = pinReservedReason(p);
        if (!reserved.isEmpty())
            w << tr("%1 uses %2, which cannot be used here: %3.").arg(trGpio(roles[s]), pinName(p), reserved);
        for (const auto &u : usage[p])
        {
            if (u.output && u.index == i) continue;
            if (Ui::pinShare(c) != Ui::ShareNone && u.share == Ui::pinShare(c)) continue;   // sound outputs share D1
            w << (u.output ? tr("%1 (%2) is also used by %3 (%4).").arg(trGpio(roles[s]), pinName(p),
                                                                         Ui::outputDisplayName(m_edit.outputs[u.index], u.index), u.role)
                           : tr("%1 (%2) is also used by %3.").arg(trGpio(roles[s]), pinName(p),
                                                                   Ui::inputDisplayName(m_edit.inputs[u.index], u.index)));
        }
        if (info.needsPwm && s < info.pinCount && !isPwmPin(p))
            w << tr("%1 (%2) is not PWM capable; %3 needs PWM.")
                     .arg(trGpio(roles[s]), pinName(p), outputTypeName(c.type));
    }
    for (int b : c.watchedInputs())             // block signal / semaphore arm: Input::occupied() needs a pin
        if (m_edit.inputs[b].type == InputNotInUse || m_edit.inputs[b].pin == PinNA)
            w << tr("Detector %1 is not in use or has no pin: the board never sees that block occupied.")
                     .arg(Ui::inputDisplayName(m_edit.inputs[b], b));
    if (c.type == SoundEffect && m_link->isOpen() && !m_simulating && m_link->caps().known && !m_link->caps().analogSound)
        w << tr("The firmware on this board has no sound effect output yet: update the firmware (Board > Flash firmware).");
    if (info.needsPwm || c.type == Pwm)
    {
        bool servo = false;
        for (const OutputConfig &o : m_edit.outputs)
            servo |= (o.type == ServoType);
        for (int s = 0; s < info.pinCount; ++s)
            if (servo && (c.pin(s) == 9 || c.pin(s) == 10))
                w << tr("%1 (%2): conflict with the servo(s). The Servo library uses timer 1, so PWM on %3 and %4 "
                        "stops working as soon as at least one servo output exists. Use %5 for PWM, "
                        "or remove the servo.").arg(trGpio(roles[s]), pinName(c.pin(s)), pinName(9), pinName(10),
                                                    pwmPinNames(false).join(QStringLiteral(", ")));
    }
    w.removeDuplicates();
    return w;
}

QStringList MainWindow::inputWarnings(int i) const
{
    QStringList w;
    const InputConfig &c = m_edit.inputs[i];
    const InputTypeInfo &info = inputTypeInfo(c.type);
    if (c.type == InputNotInUse)
        return w;

    if (c.pin < NumPins)
    {
        const QString reserved = pinReservedReason(c.pin);
        if (!reserved.isEmpty())
            w << tr("Pin %1 cannot be used here: %2.").arg(pinName(c.pin), reserved);
        const auto usage = Ui::pinUsage(m_edit);
        for (const auto &u : usage[c.pin])
            if ((u.output || u.index != i) && !(c.isAnalog() && u.share == Ui::ShareAnalog))   // analog inputs share a channel
                w << tr("Pin %1 is also used by %2.").arg(pinName(c.pin),
                                                          u.output ? Ui::outputDisplayName(m_edit.outputs[u.index], u.index)
                                                                   : Ui::inputDisplayName(m_edit.inputs[u.index], u.index));
    }
    if (c.isAnalog() && m_link->isOpen() && !m_simulating && m_link->caps().known && !m_link->caps().analogSound)
        w << tr("The firmware on this board cannot read analog inputs yet: update the firmware (Board > Flash firmware).");
    if (info.usesAddress && c.address == AddrNone && c.pin == PinNA)
        w << tr("Neither a DCC address nor a pin is set: this input never triggers.");
    if (c.type == Sensor && c.pin == PinNA)
        w << tr("No pin is set: this sensor never reports its block occupied.");
    if (info.usesRoute)
    {
        for (int s = 0; s < c.route.size(); ++s)
        {
            const int o = c.route[s].output;
            if (m_edit.outputs[o].type == OutputNotInUse)
                w << tr("Route step %1 sets %2, which is not in use.").arg(QString::number(s + 1),
                                                                          Ui::outputDisplayName(m_edit.outputs[o], o));
            if (m_edit.outputs[o].type == BlockSignal)
                w << tr("Route step %1 sets %2, a block signal: it ignores the step.").arg(QString::number(s + 1),
                                                                                          Ui::outputDisplayName(m_edit.outputs[o], o));
        }
        const RouteLimits l = routeLimits();
        if (totalRouteSteps(m_edit) > l.pool)
            w << tr("All routes together use %1 steps; the board has room for %2.").arg(totalRouteSteps(m_edit)).arg(l.pool);
    }
    if (info.usesOutput && m_edit.outputs[c.outputId].type == OutputNotInUse)
        w << tr("%1 is not in use.").arg(Ui::outputDisplayName(m_edit.outputs[c.outputId], c.outputId));
    if (info.usesOutput && m_edit.outputs[c.outputId].type == BlockSignal)
        w << tr("%1 is a block signal: it sets its own aspect and ignores this input.")
                 .arg(Ui::outputDisplayName(m_edit.outputs[c.outputId], c.outputId));
    return w;
}

// D0/D1: usable, with a caveat on a real board (shown as a note, not as an error)
static QString serialPinNote()
{
    return QCoreApplication::translate("MainWindow", "D0/D1 are the USB serial pins: on the board they only work when no PC "
                                                     "is connected (5 s after power-up without the GUI). In the simulator "
                                                     "they always work.");
}

QString MainWindow::outputNote(int i) const
{
    const OutputConfig &c = m_edit.outputs[i];
    if (c.type == SoundEffect)               // D1 by design, see the type help
        return QString();
    for (int s = 0; s < outputTypeInfo(c.type).pinCount; ++s)
        if (c.pin(s) < 2 && !boardOptions().dccNext)          // a DCCnext has no D0/D1: a warning instead
            return serialPinNote();
    return QString();
}

QString MainWindow::inputNote(int i) const
{
    const InputConfig &c = m_edit.inputs[i];
    return c.type != InputNotInUse && c.pin < 2 && !boardOptions().dccNext ? serialPinNote() : QString();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (m_fileDirty)
    {
        const auto answer = QMessageBox::question(this, windowTitle(),
                                                  tr("The configuration has changes that are not saved in a file. Save them?"),
                                                  QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (answer == QMessageBox::Cancel) { e->ignore(); return; }
        if (answer == QMessageBox::Save)
        {
            saveFile();
            if (m_fileDirty) { e->ignore(); return; }   // save dialog cancelled
        }
    }
    if (m_link->isReady() && dirtyCount() > 0
        && QMessageBox::question(this, windowTitle(),
                                 tr("%1 change(s) have not been written to the board. Quit anyway?").arg(dirtyCount()))
               != QMessageBox::Yes)
    {
        e->ignore();
        return;
    }
    e->accept();
}

// ── Pins taken over ─────────────────────────────────────────────────────────
QString MainWindow::takeOverPins(const QVector<quint8> &pins, int output, int input, int share)
{
    // The other input / output keeps its type, settings and links: only that pin becomes 'not used',
    // so it stays on the canvas with a 'not set' warning.
    QStringList freed;
    for (quint8 p : pins)
    {
        for (int o = 0; o < NumOutputs; ++o)
        {
            if (o == output) continue;
            OutputConfig &c = m_edit.outputs[o];
            const OutputTypeInfo &info = outputTypeInfo(c.type);
            if (c.type == OutputNotInUse) continue;
            if (share != Ui::ShareNone && Ui::pinShare(c) == share) continue;   // several sound outputs on D1
            for (int k = 0; k < 7; ++k)
                if (((k < 4) ? (k < info.pinCount) : info.relays) && c.pin(k) == p)
                {
                    c.setPin(k, PinNA);
                    freed << Ui::outputDisplayName(c, o);
                }
        }
        for (int j = 0; j < NumInputs; ++j)
        {
            if (j == input) continue;
            InputConfig &c = m_edit.inputs[j];
            if (c.type == InputNotInUse || c.pin != p) continue;
            if (share != Ui::ShareNone && Ui::pinShare(c) == share) continue;   // several analog inputs on one channel
            c.pin    = PinNA;
            c.analog = AnalogNone;
            freed << Ui::inputDisplayName(c, j);
        }
    }
    freed.removeDuplicates();
    return freed.isEmpty() ? QString()
                           : tr("The pin was in use: %1 keeps its settings, but has no pin there now.")
                                 .arg(freed.join(QStringLiteral(", ")));
}

// ── Phone remote ────────────────────────────────────────────────────────────
void MainWindow::setupWebRemote()
{
    m_web = new WebRemote(this);
    m_web->stateProvider = [this] {
        QJsonArray servos;
        for (int i = 0; i < NumOutputs; ++i)
        {
            const OutputConfig &c = m_edit.outputs[i];
            if (c.type != ServoType) continue;
            QJsonObject o;
            o["index"] = i;
            o["name"]  = Ui::outputDisplayName(c, i);   // as everywhere in the GUI
            o["min"]   = c.servoMin();
            o["max"]   = c.servoMax();
            o["speed"] = c.servoSpeed();
            o["state"] = c.state;
            servos.append(o);
        }
        QJsonObject root;
        root["board"]     = m_edit.board.name;
        root["connected"] = m_link->isReady();
        root["servos"]    = servos;
        return root;
    };
    connect(m_web, &WebRemote::liveRequested, this, [this](int o, int angle) {
        if (o >= 0 && o < NumOutputs && m_link->isReady()) m_link->servoLive(o, angle);
    });
    connect(m_web, &WebRemote::storeRequested, this, [this](int o, int which, int angle) {
        if (o < 0 || o >= NumOutputs || m_edit.outputs[o].type != ServoType) return;
        OutputConfig &c = m_edit.outputs[o];
        c.data[which ? 1 : 0] = quint8(angle);   // data[0] = curved, data[1] = straight
        c.state = quint8(which);                 // the board then holds the servo at this angle
        if (m_outEd->index() == o) showOutput(o);
        userEdited();
        statusBar()->showMessage(tr("Phone: %1 %2 = %3°").arg(Ui::outputDisplayName(c, o),
                                                              which ? tr("straight") : tr("curved"), QString::number(angle)), 5000);
    });
    connect(m_web, &WebRemote::adjustRequested, this, [this](int o, int which, int angle) {
        // like dragging a position slider with Live on: the servo follows, the angle is stored,
        // and the GUI shows this servo with its slider moving along
        if (o < 0 || o >= NumOutputs || m_edit.outputs[o].type != ServoType) return;
        if (m_mode != ModeConfigure && m_mode != ModeLive) return;
        if (m_link->isReady()) m_link->servoLive(o, angle);
        OutputConfig &c = m_edit.outputs[o];
        c.data[which ? 1 : 0] = quint8(angle);
        c.state = quint8(which);
        if (m_outEd->index() != o) selectOutput(o);
        else showOutput(o);
        userEdited();
    });
    connect(m_web, &WebRemote::moveRequested, this, [this](int o, int state) {
        if (o < 0 || o >= NumOutputs) return;
        if (m_link->isReady()) testOutput(o, state);
        else { m_edit.outputs[o].state = quint8(state); userEdited(); }
    });
}

void MainWindow::openPhoneRemote()
{
    QDialog d(this);
    d.setWindowTitle(tr("Phone remote"));
    auto *v = new QVBoxLayout(&d);
    auto *intro = new QLabel(tr("Adjust servos from your phone: it must be on the same Wi-Fi as this computer. "
                                "Scan the code with the phone's camera, or type the address in its browser. "
                                "Keep this program open; the code changes every time the remote is switched on."));
    intro->setWordWrap(true);
    v->addWidget(intro);
    auto *on = new QCheckBox(tr("Allow phones on my network"));
    v->addWidget(on);
    auto *qr = new QLabel;
    qr->setAlignment(Qt::AlignCenter);
    qr->setMinimumSize(260, 260);
    v->addWidget(qr, 1);
    auto *addr = new QLabel;
    addr->setAlignment(Qt::AlignCenter);
    addr->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont big = addr->font();
    big.setPointSizeF(big.pointSizeF() + 3);
    big.setBold(true);
    addr->setFont(big);
    v->addWidget(addr);
    auto *note = new QLabel(tr("The first time, Windows may ask whether the program may use the network: allow it "
                               "for private networks."));
    note->setWordWrap(true);
    note->setObjectName(QStringLiteral("help"));
    v->addWidget(note);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    v->addWidget(bb);

    auto refresh = [this, qr, addr, on] {
        on->setChecked(m_web->isRunning());
        if (!m_web->isRunning())
        {
            qr->setPixmap(QPixmap());
            qr->setText(tr("Off"));
            addr->clear();
            return;
        }
        const QString url = m_web->url();
        const auto m = Qr::encode(url.toUtf8());
        const int scale = 6, quiet = 4, n = m.size();
        QImage img((n + 2 * quiet) * scale, (n + 2 * quiet) * scale, QImage::Format_RGB32);
        img.fill(Qt::white);
        QPainter p(&img);
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
                if (m[y][x]) p.fillRect((x + quiet) * scale, (y + quiet) * scale, scale, scale, Qt::black);
        p.end();
        qr->setPixmap(QPixmap::fromImage(img));
        addr->setText(url);
    };
    connect(on, &QCheckBox::toggled, &d, [this, refresh](bool enable) {
        if (enable && !m_web->isRunning() && !m_web->start(8080))
            QMessageBox::warning(this, windowTitle(), tr("Could not open a network port (8080-8089)."));
        if (!enable) m_web->stop();
        refresh();
    });
    refresh();
    d.resize(420, 560);
    d.exec();
}

// ── Live test ───────────────────────────────────────────────────────────────
void MainWindow::setLiveTest(bool on)
{
    if (on && !m_link->isReady())
    {
        setMode(ModeConfigure);
        return;
    }
    closePanels();
    m_view->setLiveMode(on);
    updateEditing();
    if (on)
    {
        m_view->clearLive();
        for (int o = 0; o < NumOutputs; ++o)
            if (m_edit.outputs[o].type != OutputNotInUse)
                m_link->readOutputState(o);
        statusBar()->showMessage(tr("Live test: click ◀ / ▶ or ▶ start on an input. The board switches its pins and "
                                    "reports back; nothing needs to be connected."), 8000);
    }
}

void MainWindow::openLiveValue(int input)
{
    const InputConfig &in = m_edit.inputs[input];
    if (in.address == AddrNone)
    {
        QMessageBox::information(this, windowTitle(),
                                 tr("%1 has no DCC address, so it cannot receive a DCC extended command.")
                                     .arg(Ui::inputDisplayName(in, input)));
        return;
    }
    const int address = in.address;
    const OutputConfig &out = m_edit.outputs[in.outputId];
    auto *d = new QDialog(this);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setWindowTitle(tr("DCC extended — address %1").arg(address));
    auto *form = new QFormLayout(d);
    auto *value = new QLabel;
    auto send = [this, address, value](int v) {
        value->setText(tr("Value sent: %1 (0x%2)").arg(v).arg(v, 2, 16, QLatin1Char('0')));
        m_link->dccExt(address, v);
    };
    if (out.type == FourToneSignal)
    {
        // aspect byte: bits 0-3 lamp on, bits 4-7 lamp flashing; flashing without 'on' = opposite phase
        // modes: 0 off, 1 on, 2 flashing, 3 flashing in opposite phase
        auto *lamps = new QVector<QComboBox *>;
        auto collect = [lamps, send] {
            int v = 0;
            for (int k = 0; k < 4; ++k)
            {
                const int m = (*lamps)[k]->currentIndex();
                if (m == 1 || m == 2) v |= 1 << k;
                if (m == 2 || m == 3) v |= 1 << (k + 4);
            }
            send(v);
        };
        for (int k = 0; k < 4; ++k)
        {
            auto *c = new QComboBox;
            c->addItems({ tr("off"), tr("on"), tr("flashing"), tr("flashing, opposite phase") });
            const quint8 a = out.data[0];
            const bool lit = (a >> k) & 1, blink = (a >> (k + 4)) & 1;
            c->setCurrentIndex(blink ? (lit ? 2 : 3) : (lit ? 1 : 0));
            lamps->append(c);
            form->addRow(tr("Lamp %1").arg(k + 1), c);
            connect(c, QOverload<int>::of(&QComboBox::activated), d, collect);
        }
        connect(d, &QObject::destroyed, [lamps] { delete lamps; });
    }
    else
    {
        auto *slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 255);
        slider->setValue(out.pwmValue());
        form->addRow(tr("PWM value"), slider);
        connect(slider, &QSlider::valueChanged, d, send);
    }
    form->addRow(value);
    auto *note = new QLabel(tr("Sent as if your command station sent a DCC extended accessory command to "
                               "address %1. Every change is sent straight away.").arg(address));
    note->setWordWrap(true);
    note->setObjectName(QStringLiteral("help"));
    form->addRow(note);
    auto *close = new QPushButton(tr("Close"));
    connect(close, &QPushButton::clicked, d, &QDialog::close);
    form->addRow(close);
    d->show();
}

// Simulation: the voltage on the pin of an analog input. Every analog input on that pin reacts,
// as on a real board with a resistor ladder.
void MainWindow::openAnalogValue(int input)
{
    QPointer<SimulatedDevice> sim = m_link->findChild<SimulatedDevice *>();
    const InputConfig &in = m_edit.inputs[input];
    if (!sim || !in.isAnalog())
        return;
    const int ch = in.analogChannel();
    auto *d = new QDialog(this);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setObjectName(QStringLiteral("analogValueDialog"));
    d->setWindowTitle(tr("Value on A%1 (0–1023)").arg(ch));
    auto *form = new QFormLayout(d);
    auto *spin = new QSpinBox;
    spin->setRange(0, AnalogMaxValue);
    spin->setValue(sim->analogValue(ch));
    spin->setKeyboardTracking(false);            // typed values count after Enter / leaving the field
    auto *slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, AnalogMaxValue);
    slider->setValue(spin->value());
    connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
    connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), slider, &QSlider::setValue);
    // the value goes to the board when the slider is released (not while dragging), or on a step / Enter
    auto send = [sim, ch, slider] { if (sim) sim->setAnalogValue(ch, slider->value()); };
    connect(slider, &QSlider::valueChanged, d, [slider, send] { if (!slider->isSliderDown()) send(); });
    connect(slider, &QSlider::sliderReleased, d, send);
    auto *row = new QHBoxLayout;
    row->addWidget(slider, 1);
    row->addWidget(spin);
    form->addRow(tr("A%1").arg(ch), row);
    QStringList users;
    for (int i = 0; i < NumInputs; ++i)
    {
        const InputConfig &c = m_edit.inputs[i];
        if (c.isAnalog() && c.analogChannel() == ch && c.type != InputNotInUse)
            users << tr("%1: %2 %3").arg(Ui::inputDisplayName(c, i),
                                         c.analog == AnalogBelow ? QStringLiteral("<") : c.analog == AnalogAbove ? QStringLiteral(">")
                                                                                                                 : QStringLiteral("≈"),
                                         QString::number(c.threshold));
    }
    auto *note = new QLabel(tr("1023 = 5 V, nothing pressed. Every input on A%1 reacts: %2").arg(ch).arg(users.join(QStringLiteral(", "))));
    note->setWordWrap(true);
    note->setObjectName(QStringLiteral("help"));
    form->addRow(note);
    auto *close = new QPushButton(tr("Close"));
    connect(close, &QPushButton::clicked, d, &QDialog::close);
    form->addRow(close);
    d->show();
}

void MainWindow::pollAnalog()
{
    const bool available = m_link->isReady() && m_link->caps().analogSound;
    m_inEd->setLiveReadingAvailable(available);
    if (!available)
        return;
    QSet<int> channels;
    const InputConfig shown = m_inEd->config();
    if (shown.isAnalog())
        channels << shown.analogChannel();
    if (m_view->liveMode())                  // live test / simulation: the canvas shows every reading
        for (const InputConfig &c : m_edit.inputs)
            if (c.isAnalog() && c.type != InputNotInUse)
                channels << c.analogChannel();
    for (int ch : channels)
        m_link->readAnalog(ch);
}

void MainWindow::refreshLampRow()
{
    const OutputConfig &o = m_edit.outputs[m_outEd->index()];
    m_lampRow->setVisible(o.type == FourToneSignal || o.type == BlockSignal);
    for (int k = 0; k < m_lampButtons.size(); ++k)
        m_lampButtons[k]->setStyleSheet(QStringLiteral("QToolButton { background: %1; border: 1px solid #64748B; border-radius: 4px; }")
                                            .arg(Ui::lampColor(o, k).name()));
}

// ── User level ──────────────────────────────────────────────────────────────
void MainWindow::setLevel(int level)
{
    // only what the GUI offers changes: the configuration (types, settings) stays exactly as it is
    Ui::setLevel(level);
    for (int l = 0; l < m_actLevel.size(); ++l)
        m_actLevel[l]->setChecked(l == Ui::level());
    m_outEd->refreshPinCombos();             // D0 / D1, offered types, boxes, read-only above the level
    m_inEd->refreshPinCombos();
    refreshWarnings();
}

// ── Board settings, name and board files ────────────────────────────────────
QString MainWindow::boardsDir()
{
    const QString def = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                            .filePath(QStringLiteral("GPIOcore/boards"));
    return QSettings().value(QStringLiteral("files/boardsDir"), def).toString();
}

void MainWindow::onBoardReceived(const BoardSettings &b)
{
    m_dev.board = b;
    m_connChip->setText(tr("● %1 — %2").arg(m_link->portName(), b.name.isEmpty() ? tr("board without a name") : b.name));
    if (!m_readingBoard)
    {
        // just connected: show which board it is, change nothing on screen
        updateConnState();
        if (!m_simulating && !b.name.isEmpty() && b.name != m_edit.board.name)
        {
            if (m_mode == ModeLive)
            {
                if (QMessageBox::question(this, windowTitle(),
                                          tr("The board is \"%1\", the screen shows \"%2\". The live test shows the "
                                             "screen's wiring, so read the board first?").arg(b.name, m_edit.board.name))
                    == QMessageBox::Yes)
                    QTimer::singleShot(0, this, [this] { readFromBoard(); });
            }
            else
                statusBar()->showMessage(tr("Connected to \"%1\", the screen shows \"%2\". Read board or Write all.")
                                             .arg(b.name, m_edit.board.name), 12000);
        }
        return;
    }
    // 'Read board': names (and lamp colours) come from <name>.json, the board wins for all settings,
    // which arrive right after this.
    const QString path = QDir(boardsDir()).filePath(b.name + QStringLiteral(".json"));
    if (!b.name.isEmpty() && QFileInfo::exists(path))
    {
        QFile f(path);
        BoardConfig file;
        if (f.open(QIODevice::ReadOnly)
            && BoardConfig::fromJson(QJsonDocument::fromJson(f.readAll()).object(), file, nullptr))
        {
            for (int i = 0; i < NumOutputs; ++i) { m_edit.outputs[i].name = file.outputs[i].name; m_edit.outputs[i].lampColors = file.outputs[i].lampColors; }
            for (int i = 0; i < NumInputs; ++i)  m_edit.inputs[i].name  = file.inputs[i].name;
        }
    }
    m_edit.board = b;
    setBoardType(b.type);
    setWindowTitle(b.name.isEmpty() ? tr("GPIOcore Configurator") : tr("%1 — GPIOcore Configurator").arg(b.name));
    refreshAll();
}

void MainWindow::saveBoardFile()
{
    const QString name = m_edit.board.name;
    if (name.isEmpty() || name == QLatin1String("Demo"))
        return;
    QDir().mkpath(boardsDir());
    const QString path = QDir(boardsDir()).filePath(name + QStringLiteral(".json"));
    const QByteArray data = QJsonDocument(m_edit.toJson()).toJson(QJsonDocument::Indented);
    QFile old(path);
    if (old.open(QIODevice::ReadOnly) && old.readAll() == data)
        return;                          // unchanged
    old.close();
    backupBoardFile(path);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(data);
}

// A copy in boards/backup/<name>-<date-time>.json before a board file changes; at most one per
// 5 minutes per board, the newest 30 are kept.
void MainWindow::backupBoardFile(const QString &path)
{
    const QFileInfo fi(path);
    if (!fi.exists())
        return;
    QDir dir(fi.absolutePath());
    dir.mkpath(QStringLiteral("backup"));
    dir.cd(QStringLiteral("backup"));
    const QString base = fi.completeBaseName();
    const QStringList existing = dir.entryList({ base + QStringLiteral("-*.json") }, QDir::Files, QDir::Time);
    if (!existing.isEmpty()
        && QFileInfo(dir.filePath(existing.first())).lastModified().secsTo(QDateTime::currentDateTime()) < 300)
        return;
    QFile::copy(path, dir.filePath(base + QStringLiteral("-")
                                   + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))
                                   + QStringLiteral(".json")));
    const QStringList all = dir.entryList({ base + QStringLiteral("-*.json") }, QDir::Files, QDir::Time);
    for (int k = 30; k < all.size(); ++k)
        dir.remove(all[k]);
}

void MainWindow::quickSave()
{
    saveFile();                          // always 'save as': nothing is overwritten without seeing it
}

void MainWindow::markFileDirty()
{
    m_fileDirty = true;
    const QString name = m_edit.board.name.isEmpty() ? tr("GPIOcore Configurator")
                                                     : tr("%1 — GPIOcore Configurator").arg(m_edit.board.name);
    setWindowTitle(name + QStringLiteral(" *"));
}

void MainWindow::updateEditing()
{
    // configuring (red) or offline: everything can be changed; simulation / live test: look only
    const bool edit = m_mode == ModeConfigure && !m_simulating;
    m_view->setEditable(edit);
    m_inEd->setEnabled(edit);
    m_outEd->setEnabled(edit);
    for (QWidget *w : m_editWidgets) if (w) w->setEnabled(edit);
    if (m_actPreset) m_actPreset->setEnabled(edit);
    if (m_actBoardSettings) m_actBoardSettings->setEnabled(edit);
    if (m_boardType) m_boardType->setEnabled(edit);
}

void MainWindow::askBoardName()
{
    m_nameAsked = true;
    bool ok = false;
    const QString name = cleanBoardName(QInputDialog::getText(
        this, tr("Name this board"),
        tr("This board has no name yet. Give it a short, unique name (max. 16 letters, e.g. \"Station-left\").\n"
           "The name is stored on the board; your names for inputs and outputs are kept in a file with this name."),
        QLineEdit::Normal, QString(), &ok));
    if (!ok || name.isEmpty())
        return;
    m_edit.board.name = name;
    m_dev.board.name  = name;
    m_link->writeBoard(m_edit.board);
    onBoardReceived(m_edit.board);
    markFileDirty();
}

void MainWindow::editBoardSettings()
{
    QDialog d(this);
    d.setWindowTitle(tr("Board settings"));
    auto *form = new QFormLayout(&d);
    form->addRow(Info::button(QStringLiteral("board.settings")));
    auto *name = new QLineEdit(m_edit.board.name);
    name->setMaxLength(MaxBoardName);
    auto *type = new QComboBox;
    for (int t = 0; t < BoardTypeCount; ++t) type->addItem(boardTypeName(quint8(t)));
    type->setCurrentIndex(m_edit.board.type);
    auto *timing = new QCheckBox(tr("Own wait time per route step"));
    timing->setChecked(m_edit.board.timing);
    timing->setToolTip(tr("Off: every step waits the same time and routes need half the memory"));
    auto *delay = new QDoubleSpinBox;
    delay->setRange(0.0, 25.5);
    delay->setSingleStep(0.1);
    delay->setDecimals(1);
    delay->setSuffix(tr(" s"));
    delay->setValue(m_edit.board.delay / 10.0);
    auto *interlock = new QComboBox;
    interlock->addItem(tr("Off — routes may run at the same time"));
    interlock->addItem(tr("On — one route at a time"));
    interlock->addItem(tr("Automatic — turnout routes one at a time, lights in parallel"));
    interlock->setCurrentIndex(m_edit.board.interlock);
    form->addRow(tr("Name"), name);
    form->addRow(tr("Board type"), type);
    form->addRow(QString(), timing);
    form->addRow(tr("Wait time per step"), delay);
    form->addRow(tr("Route interlocking"), interlock);
    if (Ui::level() < Ui::Advanced)          // Beginner: name and board type only (the rest is kept as it is)
        for (QWidget *w : { static_cast<QWidget *>(timing), static_cast<QWidget *>(delay), static_cast<QWidget *>(interlock) })
        {
            w->hide();
            if (QWidget *label = form->labelForField(w)) label->hide();
        }
    auto *note = new QLabel(m_link->isReady() && !m_link->caps().boardSettings
                                ? tr("This board's firmware cannot store these settings yet; they are only kept in the file.")
                                : QString());
    note->setWordWrap(true);
    note->setObjectName(QStringLiteral("help"));
    form->addRow(note);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    auto sync = [&] { delay->setEnabled(!timing->isChecked()); };
    connect(timing, &QCheckBox::toggled, &d, sync);
    sync();
    if (d.exec() != QDialog::Accepted)
        return;

    BoardSettings b = m_edit.board;
    b.name      = cleanBoardName(name->text());
    b.type      = quint8(type->currentIndex());
    b.timing    = timing->isChecked();
    b.delay     = quint8(qRound(delay->value() * 10));
    b.interlock = quint8(interlock->currentIndex());
    m_edit.board = b;
    setBoardType(b.type);
    m_inEd->setTimingEnabled(b.timing);
    if (m_link->isReady() && m_link->caps().boardSettings)
    {
        const bool timingChanged = b.timing != m_dev.board.timing;
        m_link->writeBoard(b);
        m_link->readBoard();
        if (timingChanged)
        {
            m_inKnown.fill(false);       // the board converted all routes (wait times added / dropped)
            m_link->readAll();
        }
    }
    markFileDirty();
    setWindowTitle(b.name.isEmpty() ? tr("GPIOcore Configurator") : tr("%1 — GPIOcore Configurator").arg(b.name));
}

void MainWindow::newOfflineBoard()
{
    if (m_link->isOpen())
        m_link->close();
    QDialog d(this);
    d.setWindowTitle(tr("New board"));
    auto *form = new QFormLayout(&d);
    auto *nameEdit = new QLineEdit;
    nameEdit->setMaxLength(MaxBoardName);
    nameEdit->setPlaceholderText(tr("max. 16 letters, e.g. Station-left"));
    auto *start = new QComboBox;
    for (const PresetFile &p : presetFiles())
        start->addItem(p.name, p.path);
    start->setCurrentIndex(qMax(0, start->findData(QDir(presetsDir()).filePath(QLatin1String(kEmptyPreset)))));
    form->addRow(tr("Name"), nameEdit);
    form->addRow(tr("Start with"), start);
    auto *hint = new QLabel(tr("When you later connect a board and give it this name, these settings belong to it. "
                               "Use Write all to put them on the board."));
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("help"));
    form->addRow(hint);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    form->addRow(bb);
    if (d.exec() != QDialog::Accepted)
        return;
    const QString name = cleanBoardName(nameEdit->text());
    PresetFile p;                            // no preset found: nothing in use
    readPreset(start->currentData().toString(), p);
    BoardConfig b = p.board;
    b.board.name = name;
    if (!p.hasType)
        b.board.type = m_edit.board.type;
    m_edit = m_dev = b;
    m_shadow = b;
    setBoardType(b.board.type);
    showOutput(m_outEd->index());
    showInput(m_inEd->index());
    refreshAll();
    markFileDirty();
    raiseLevelFor();
}

void MainWindow::addBlock(bool output)
{
    if (output)
    {
        for (int o = 0; o < NumOutputs; ++o)
            if (m_edit.outputs[o].type == OutputNotInUse && outputSources(m_edit, o).isEmpty())
            {
                m_edit.outputs[o].type = SingleSteady;
                userEdited();
                selectOutput(o);
                return;
            }
        statusBar()->showMessage(tr("All 16 outputs are in use."), 4000);
    }
    else
    {
        for (int i = 0; i < NumInputs; ++i)
            if (m_edit.inputs[i].type == InputNotInUse)
            {
                m_edit.inputs[i].type = Momentary;
                userEdited();
                selectInput(i);
                return;
            }
        statusBar()->showMessage(tr("All 16 inputs are in use."), 4000);
    }
}

void MainWindow::removeSelected()
{
    if (m_open == OpenInput)
    {
        const int i = m_inEd->index();
        InputConfig c;                   // not in use, no pin, no address, no route
        c.name.clear();
        m_edit.inputs[i] = c;
    }
    else if (m_open == OpenOutput)
    {
        const int o = m_outEd->index();
        for (int i = 0; i < NumInputs; ++i)          // take it out of every route
            for (int s = m_edit.inputs[i].route.size() - 1; s >= 0; --s)
                if (m_edit.inputs[i].route[s].output == o)
                    Gpio::disconnect(m_edit, i, s);
        QStringList freed;                           // a direct input that only drove this output goes too
        for (int i = 0; i < NumInputs; ++i)
            if (isDirectType(m_edit.inputs[i].type) && m_edit.inputs[i].outputId == o)
            {
                InputConfig off;
                off.name = m_edit.inputs[i].name;
                m_edit.inputs[i] = off;
                freed << Ui::inputDisplayName(m_edit.inputs[i], i);
            }
        const QString name = Ui::outputDisplayName(m_edit.outputs[o], o);
        OutputConfig c;
        m_edit.outputs[o] = c;
        if (!freed.isEmpty())
            statusBar()->showMessage(tr("%1 removed; %2 drove it and is now not in use as well.")
                                         .arg(name, freed.join(QStringLiteral(", "))), 8000);
    }
    closePanels();
    userEdited();
}

void MainWindow::flashFirmware()
{
    const QString port = m_link->isOpen() ? m_link->portName() : m_port->currentData().toString();
    FlashDialog d(port, this);
    bool reconnect = false;
    connect(&d, &FlashDialog::aboutToFlash, this, [this, &reconnect] {
        reconnect = m_link->isOpen() && m_link->portName() != QLatin1String("simulator");
        if (m_link->isOpen())
            m_link->close();             // avrdude needs the port
    });
    connect(&d, &FlashDialog::flashFinished, this, [this, &reconnect, port](bool ok) {
        if (ok && reconnect)
            QTimer::singleShot(1500, this, [this, port] { connectPort(port); });
    });
    d.exec();
}

// ── Screenshot mode (for documentation / automated checks) ──────────────────
void MainWindow::applyPresetOnStart(const QString &preset)
{
    // by its name or file name, e.g. "16 servos" or "16-servos"
    QString path;
    for (const PresetFile &p : presetFiles())
        if (p.name.compare(preset, Qt::CaseInsensitive) == 0
            || QFileInfo(p.path).completeBaseName().compare(preset, Qt::CaseInsensitive) == 0)
            path = p.path;
    if (path.isEmpty())
    {
        statusBar()->showMessage(tr("No preset \"%1\" in %2").arg(preset, QDir::toNativeSeparators(presetsDir())), 8000);
        return;
    }
    m_startPreset = true;
    auto *once = new QObject(this);
    connect(m_link, &DeviceLink::finished, once, [this, path, once] {
        once->deleteLater();
        applyPresetNow(path);
    });
}

void MainWindow::scheduleScreenshot(const QString &path, const QString &tab, int select)
{
    auto *once = new QObject(this);
    auto remaining = std::make_shared<int>(m_startPreset ? 2 : 1);
    connect(m_link, &DeviceLink::finished, once, [this, path, tab, select, once, remaining] {
        if (--*remaining > 0)
            return;
        once->deleteLater();
        if (tab == QLatin1String("inputs")) selectInput(select);
        else                                selectOutput(select);
        QTimer::singleShot(300, this, [this, path] {
            grab().save(path);
            qApp->exit(0);
        });
    });
}
