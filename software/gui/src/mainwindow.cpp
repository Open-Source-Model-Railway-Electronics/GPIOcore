#include "mainwindow.h"

#include "connections.h"
#include "connectionview.h"
#include "devicelink.h"
#include "flasher.h"
#include "qrcode.h"
#include "webremote.h"
#include "i18n.h"
#include "infotexts.h"
#include "inputeditor.h"
#include "outputeditor.h"
#include "pinmapview.h"
#include "simulateddevice.h"
#include "uihelpers.h"

#include <QAction>
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
#include <QPropertyAnimation>
#include <QSettings>
#include <QShortcut>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDockWidget>
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
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QSerialPortInfo>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include <memory>

using namespace Gpio;


static bool sameOutput(const OutputConfig &a, const OutputConfig &b, bool ignoreLastByte)
{
    if (!ignoreLastByte)
        return a == b;
    OutputConfig x = a, y = b;
    x.data[3] = y.data[3] = 0;
    return x == y;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(tr("GPIOcore Configurator"));
    resize(1600, 900);

    m_link = new DeviceLink(this);

    // Start with what a factory-fresh board contains, so the GUI is useful offline.
    SimulatedDevice defaults;
    for (int i = 0; i < NumOutputs; ++i) m_edit.outputs[i] = defaults.output(i);
    for (int i = 0; i < NumInputs;  ++i) m_edit.inputs[i]  = defaults.input(i);
    m_edit.board.type = quint8(QSettings().value(QStringLiteral("board/type"), int(BoardNano)).toInt());
    m_shadow = m_edit;
    Gpio::setBoardType(m_edit.board.type);
    m_dev = m_edit;

    m_liveTimer = new QTimer(this);
    m_liveTimer->setSingleShot(true);
    m_liveTimer->setInterval(350);       // edits are sent after a short pause (slider drags, typing)
    connect(m_liveTimer, &QTimer::timeout, this, &MainWindow::sendLive);

    buildUi();
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
            const QString name = m_edit.outputs[o].name.isEmpty() ? tr("Output %1").arg(o) : m_edit.outputs[o].name;
            const QString what = state == 3 ? tr("frog relays A %1, B %2, unifrog %3")
                                                  .arg(value & 1 ? tr("on") : tr("off"), value & 2 ? tr("on") : tr("off"),
                                                       value & 4 ? tr("on") : tr("off"))
                               : state == 2 ? tr("value %1").arg(value)
                                            : (state ? tr("straight / on") : tr("curved / off"));
            statusBar()->showMessage(tr("%1 → %2").arg(name, what), 4000);
        }
    });
    connect(m_link, &DeviceLink::capabilitiesChanged, this, [this] { applyCapabilities(); });
    connect(m_link, &DeviceLink::logMessage,     this, &MainWindow::appendLog);
    connect(m_link, &DeviceLink::errorOccurred,  this, [this](const QString &m) {
        appendLog(QStringLiteral("ERR"), m);
        statusBar()->showMessage(m, 6000);
    });
    connect(m_link, &DeviceLink::legacyOutputFormatChanged, this, [this](bool legacy) {
        m_outEd->setLegacyFormat(legacy);
        m_legacyChip->setVisible(legacy && m_link->isOpen());
        refreshAll();
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
        }
        updateActions();
        if (failures)
            statusBar()->showMessage(tr("Done with %1 error(s)").arg(failures), 6000);
        if (dirtyCount() > 0 && m_liveTimer->isActive() == false && failures == 0)
            m_liveTimer->start();        // edits made while the board was busy (sendLive skips refused items)
        if (!m_nameAsked && m_link->caps().boardSettings && m_dev.board.name.isEmpty())
            QTimer::singleShot(0, this, &MainWindow::askBoardName);
    });
    connect(m_link, &DeviceLink::presetApplied, this, [this](int preset) {
        m_outKnown.fill(false);         // board content replaced: take everything from the board
        m_inKnown.fill(false);
        appendLog(QStringLiteral("INFO"), tr("Preset %1 applied, reading back").arg(preset));
    });

    applyCapabilities();
    setExpertMode(QSettings().value(QStringLiteral("ui/expert"), false).toBool());
    updateConnState();
    updateEditing();
    refreshAll();
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
    m_tabs = new QTabWidget;
    m_tabs->setDocumentMode(true);

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
        refreshRows();
        markFileDirty();
    });
    // lamp colours of a 4-aspect signal: only for the GUI, kept in the board file
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
        refreshRows();
        m_inEd->setOutputs(outputNames(), outputTypes());
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
    m_tabs->addTab(config, tr("Configuration"));

    auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), config);
    connect(esc, &QShortcut::activated, this, &MainWindow::closePanels);

    // Pin map
    m_pinMap = new PinMapView;
    m_tabs->addTab(m_pinMap, tr("Pin map"));

    m_stack = new QStackedWidget;
    m_stack->addWidget(buildStartPage());
    m_stack->addWidget(m_tabs);
    setCentralWidget(m_stack);

    // Log dock
    m_log = new QPlainTextEdit;
    m_log->setObjectName(QStringLiteral("log"));
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(3000);
    auto *dock = new QDockWidget(tr("Serial log"), this);
    dock->setObjectName(QStringLiteral("logDock"));
    dock->setWidget(m_log);
    dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::BottomDockWidgetArea, dock);
    connect(dock->toggleViewAction(), &QAction::triggered, this, [](bool on) {
        QSettings().setValue(QStringLiteral("ui/showLog"), on);
    });
    resizeDocks({ dock }, { 170 }, Qt::Vertical);

    // Status bar
    m_connChip   = new QLabel;
    m_legacyChip = new QLabel(tr("Legacy output format 0x2D"));
    m_legacyChip->setObjectName(QStringLiteral("chipWarn"));
    m_legacyChip->setToolTip(tr("The firmware sends/receives output data with 13 bytes; the 4th data byte "
                                "(servo pattern, high bytes of times) is lost. See review: OPCODE_SET_OUTPUT_DATA."));
    m_dirtyChip = new QLabel;
    m_dirtyChip->setObjectName(QStringLiteral("chipWarn"));
    m_progress = new QProgressBar;
    m_progress->setMaximumWidth(180);
    m_progress->setTextVisible(false);
    m_progress->hide();
    m_protoChip = new QLabel;
    m_protoChip->setObjectName(QStringLiteral("chip"));
    m_protoChip->hide();
    statusBar()->addPermanentWidget(m_dirtyChip);
    statusBar()->addPermanentWidget(m_protoChip);
    statusBar()->addPermanentWidget(m_legacyChip);
    statusBar()->addPermanentWidget(m_progress);
    statusBar()->addPermanentWidget(m_connChip);

    // Wiring
    connect(m_view, &ConnectionView::inputSelected,  this, &MainWindow::selectInput);
    connect(m_view, &ConnectionView::liveTriggerRequested, this, [this](int in, int dir) {
        m_link->triggerInput(in, dir);
    });
    connect(m_view, &ConnectionView::liveValueRequested, this, &MainWindow::openLiveValue);
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
            statusBar()->showMessage(tr("Input %1 → output %2").arg(in).arg(out), 3000);
            break;
        case ConnectResult::AlreadyConnected:
            statusBar()->showMessage(tr("Input %1 already drives output %2").arg(in).arg(out), 3000);
            break;
        case ConnectResult::RouteFull:
            statusBar()->showMessage(tr("The route of input %1 already has %2 steps (maximum)")
                                         .arg(in).arg(routeLimits().maxSteps), 4000);
            break;
        case ConnectResult::PoolFull:
            statusBar()->showMessage(tr("The board has no room for more route steps (%1 in total)")
                                         .arg(routeLimits().pool), 4000);
            break;
        case ConnectResult::NotConnectable:
            statusBar()->showMessage(tr("Input %1 is a recorder / feedback input and cannot drive outputs").arg(in), 4000);
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
        const QString taken = takeOverPins(fresh, i, -1);
        if (!taken.isEmpty()) statusBar()->showMessage(taken, 8000);
        const QString name = m_edit.outputs[i].name;
        const QStringList lamps = m_edit.outputs[i].lampColors;
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
        if (c.type != InputNotInUse && c.pin < NumPins && c.pin != m_edit.inputs[i].pin)
        {
            const QString taken = takeOverPins({ c.pin }, -1, i);
            if (!taken.isEmpty()) statusBar()->showMessage(taken, 8000);
        }
        const QString name = m_edit.inputs[i].name;
        m_edit.inputs[i] = c;
        m_edit.inputs[i].name = name;
        userEdited();
    });
    connect(m_outEd, &OutputEditor::testRequested, this, &MainWindow::testOutput);
    connect(m_pinMap, &PinMapView::outputActivated, this, [this](int i) { m_tabs->setCurrentIndex(0); selectOutput(i); });
    connect(m_pinMap, &PinMapView::inputActivated,  this, [this](int i) { m_tabs->setCurrentIndex(0); selectInput(i); });
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
    m_pinMap->setBoard(m_edit);
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
        l.v1       = c.protocol < 2;
        l.maxSteps = l.v1 ? MaxRouteStepsV1 : qBound(1, c.maxRouteSteps, MaxRouteSteps);
        l.pool     = l.v1 ? NumInputs * MaxRouteStepsV1 : qMax(1, c.stepPool);
    }
    return l;                            // offline: what a v2 decoder with external EEPROM can do
}

void MainWindow::applyCapabilities()
{
    const DeviceCaps &c = m_link->caps();
    const bool known = m_link->isOpen() && c.known;
    boardOptions().serialPinsUsable = known ? (c.protocol >= 2 && c.serialPins) : true;
    m_outEd->setLiveAvailable(known && c.protocol >= 2 && c.servoLive);
    m_outEd->refreshPinCombos();
    m_inEd->refreshPinCombos();
    if (known)
    {
        m_protoChip->setText(c.protocol >= 2
                                 ? tr("protocol v%1 · %2 steps/route · %3 in total").arg(c.protocol).arg(c.maxRouteSteps).arg(c.stepPool)
                                 : tr("protocol v1 · routes: 8 steps, no times"));
        m_protoChip->setObjectName(c.protocol >= 2 ? QStringLiteral("chip") : QStringLiteral("chipWarn"));
        m_protoChip->style()->unpolish(m_protoChip);
        m_protoChip->style()->polish(m_protoChip);
    }
    m_protoChip->setVisible(known && m_expert);
    updateInputEditorLimits();
    refreshAll();
}

void MainWindow::updateInputEditorLimits()
{
    const int i = m_inEd->index();
    const int mine = isRouteType(m_edit.inputs[i].type) ? m_edit.inputs[i].route.size() : 0;
    m_inEd->setRouteLimits(routeLimits(), totalRouteSteps(m_edit) - mine);
    m_inEd->setRecorderTaken(recorderInput(m_edit, i) >= 0);
}

void MainWindow::setBoardType(int type)
{
    m_edit.board.type = quint8(type);
    Gpio::setBoardType(m_edit.board.type);
    boardOptions().recorder = m_edit.board.recorder;
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
    m_preset->addItems(presetNames());
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
    board->addAction(tr("Apply a preset…"), this, [this] { setExpertMode(true); m_preset->setFocus(); });
    board->addSeparator();
    board->addAction(tr("&Flash firmware…"), this, &MainWindow::flashFirmware);
    board->addAction(tr("&Phone remote…"), this, &MainWindow::openPhoneRemote);
    m_actWrite->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    m_actRead->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));

    QMenu *view = menuBar()->addMenu(tr("&View"));
    m_actExpert = view->addAction(tr("&Expert mode"));
    m_actExpert->setCheckable(true);
    m_actExpert->setToolTip(tr("Show bytes, the serial log and protocol details"));
    connect(m_actExpert, &QAction::toggled, this, &MainWindow::setExpertMode);
    m_actShowUnused = view->addAction(tr("Show unused inputs and outputs"));
    m_actShowUnused->setCheckable(true);
    m_actShowUnused->setChecked(QSettings().value(QStringLiteral("ui/showUnused2"), false).toBool());
    m_view->setHideUnused(!m_actShowUnused->isChecked());
    connect(m_actShowUnused, &QAction::toggled, this, [this](bool on) {
        QSettings().setValue(QStringLiteral("ui/showUnused2"), on);
        m_view->setHideUnused(!on);
    });
    view->addSeparator();
    m_actRaw = view->addAction(tr("Show TX/RX bytes in log"));
    m_actRaw->setCheckable(true);
    m_actRaw->setChecked(true);
    view->addAction(tr("Clear log"), m_log, &QPlainTextEdit::clear);
    if (auto *dock = findChild<QDockWidget *>(QStringLiteral("logDock")))
        view->addAction(dock->toggleViewAction());

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
            m_pinMap->update();
        });
    }

    // Language: every language with a translation or help texts is listed
    view->addSeparator();
    QMenu *language = view->addMenu(tr("Language"));
    auto *langGroup = new QActionGroup(language);
    auto addLang = [this, language, langGroup](const QString &code, const QString &name) {
        QAction *a = language->addAction(name);
        a->setCheckable(true);
        a->setChecked(Lang::configured() == code);
        langGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, code] {
            Lang::setConfigured(code);
            QMessageBox::information(this, windowTitle(),
                                     tr("The language changes the next time you start the program."));
        });
    };
    addLang(QString(), tr("Automatic (system language)"));
    for (const QString &code : Lang::available())
    {
        const QLocale loc(code);
        QString name = loc.nativeLanguageName();
        if (!name.isEmpty()) name[0] = name[0].toUpper();
        addLang(code, QStringLiteral("%1 (%2)").arg(name, code));
    }

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    QAction *start = help->addAction(tr("&Getting started"), this, [this] {
        Info::showDialog(QStringLiteral("app.start"), tr("Getting started"), this);
    });
    start->setShortcut(QKeySequence::HelpContents);
    help->addAction(tr("&Configure, simulate and live test"), this, [this] {
        Info::showDialog(QStringLiteral("modes"), tr("Configure, simulate and live test"), this);
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
                              "DCCnext) for model railways.<br>Language: %2")
                               .arg(QApplication::applicationVersion(), Lang::current()));
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
    showWorkspace();
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
    showWorkspace();
    QString err;
    m_link->openDevice(new SimulatedDevice, true, &err);
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
    showWorkspace();
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
    m_link->openDevice(sim, true, &err);
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
        m_legacyChip->hide();
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
    m_legacyChip->setVisible(m_link->legacyOutputFormat());
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
    m_pinMap->setBoard(m_edit);
    refreshWarnings();
    updateActions();
}

void MainWindow::onInputReceived(int i, const InputConfig &received)
{
    InputConfig c = received;
    if (m_link->caps().protocol >= 2)
        c.route = m_dev.inputs[i].route;   // the real route follows with GET_ROUTE
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
    m_pinMap->setBoard(m_edit);
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
    int n = 0;
    QStringList refused;
    for (int i = 0; i < NumInputs; ++i)
        if (inputDirty(i))
        {
            if (live && m_liveTried.contains(i)) { refused << tr("input %1").arg(i); continue; }
            m_liveTried.insert(i);
            m_link->writeInput(i, m_edit.inputs[i]);
            m_link->writeRoute(i, m_edit.inputs[i].route);   // skipped on v1 boards (linked bytes)
            m_link->readInput(i);                             // also reads the route back
            ++n;
        }
    for (int i = 0; i < NumOutputs; ++i)
        if (outputDirty(i))
        {
            if (live && m_liveTried.contains(100 + i)) { refused << tr("output %1").arg(i); continue; }
            m_liveTried.insert(100 + i);
            m_link->writeOutput(i, m_edit.outputs[i]);
            m_link->readOutput(i);
            ++n;
        }
    if (n)
        appendLog(QStringLiteral("INFO"), tr("Writing %1 item(s), each is read back for verification").arg(n));
    if (!refused.isEmpty() && !m_liveWarned)
    {
        m_liveWarned = true;
        const QString msg = tr("The board keeps a different value for %1; not sending it again.").arg(refused.join(QStringLiteral(", ")));
        appendLog(QStringLiteral("WARN"), msg);
        statusBar()->showMessage(msg, 8000);
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
    QMessageBox box(QMessageBox::Question, windowTitle(),
                    tr("Apply preset \"%1\"?\n\nThis replaces all outputs and inputs (and their routes).").arg(name),
                    QMessageBox::Yes | QMessageBox::No, this);
    auto *clear = new QCheckBox(tr("Also clear the names of inputs and outputs (recommended)"));
    clear->setChecked(true);
    box.setCheckBox(clear);
    if (box.exec() != QMessageBox::Yes)
        return;
    applyPresetNow(m_preset->currentIndex(), clear->isChecked());
}

void MainWindow::applyPresetNow(int preset, bool clearNames)
{
    // The GUI computes the preset; it reaches the board as normal output / input data (live write),
    // so it also works offline and in a simulation.
    const BoardConfig p = presetBoard(preset);
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

bool MainWindow::loadFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, windowTitle(), f.errorString());
        return false;
    }
    BoardConfig b;
    QString err;
    if (!BoardConfig::fromJson(QJsonDocument::fromJson(f.readAll()).object(), b, &err))
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
    showWorkspace();
    refreshAll();
    showOutput(m_outEd->index());
    showInput(m_inEd->index());
    m_filePath = path;
    m_fileDirty = false;
    statusBar()->showMessage(tr("Loaded %1").arg(path), 6000);
    return true;
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
    return !sameOutput(m_edit.outputs[i], m_dev.outputs[i], m_link->legacyOutputFormat());
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
    if (m_link->isOpen() && m_link->caps().known && m_link->caps().protocol < 2)
        return m_edit.inputs[i].asV1() != m_dev.inputs[i];   // v1 stores only what fits in linked[]
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
    m_actPreset->setEnabled(m_mode == ModeConfigure && !m_simulating);   // presets are computed by the GUI: also offline
    m_port->setEnabled(!m_link->isOpen());
    m_actRefresh->setEnabled(!m_link->isOpen());
    if (m_link->isReady())
        m_dirtyChip->setText(busy ? tr("Saving on the board…") : tr("%1 change(s) waiting").arg(dirty));
    else
        m_dirtyChip->setText(tr("%1 unsent change(s)").arg(dirty));
    m_dirtyChip->setVisible(dirty > 0 && (m_expert || !m_link->isReady() || busy));
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
        if (active && p < NumPins && (usage[p].size() > 1 || !pinReservedReason(p).isEmpty()))
            return true;
    }
    return false;
}

bool MainWindow::inputHasConflict(int i) const
{
    const InputConfig &c = m_edit.inputs[i];
    if (c.type == InputNotInUse || c.pin >= NumPins)
        return false;
    return Ui::pinUsage(m_edit)[c.pin].size() > 1 || !pinReservedReason(c.pin).isEmpty();
}

void MainWindow::refreshRows()
{
    Ui::setPinBoard(m_edit);                 // pin lists: used pins in red
    m_outEd->refreshPinMarks();
    m_inEd->refreshPinMarks();
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
    m_pinMap->setBoard(m_edit);
    refreshWarnings();
    updateActions();
}

void MainWindow::selectOutput(int i)
{
    m_view->selectOutput(i);
    showOutput(i);
    m_outEd->refreshPinMarks();
    m_outEd->setWarnings(outputWarnings(i));
    m_outName->setText(m_edit.outputs[i].name);
    refreshLampRow();
    openPanel(OpenOutput);
}

void MainWindow::selectInput(int i)
{
    m_view->selectInput(i);
    showInput(i);
    m_inEd->refreshPinMarks();
    m_inEd->setWarnings(inputWarnings(i));
    m_inName->setText(m_edit.inputs[i].name);
    updateInputEditorLimits();
    openPanel(OpenInput);
}

void MainWindow::refreshWarnings()
{
    m_outEd->setWarnings(outputWarnings(m_outEd->index()));
    m_inEd->setWarnings(inputWarnings(m_inEd->index()));
}

QStringList MainWindow::outputNames() const
{
    QStringList names;
    for (int i = 0; i < NumOutputs; ++i)
        names << (m_edit.outputs[i].name.isEmpty()
                      ? tr("Output %1 · %2").arg(i).arg(outputTypeName(m_edit.outputs[i].type))
                      : tr("Output %1 · %2").arg(i).arg(m_edit.outputs[i].name));
    return names;
}

QVector<quint8> MainWindow::outputTypes() const
{
    QVector<quint8> t;
    for (const OutputConfig &o : m_edit.outputs)
        t << o.type;
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
        if (s < info.pinCount && p == PinNA)
            w << tr("%1 is not set.").arg(trGpio(roles[s]));
        if (p >= NumPins) continue;
        const QString reserved = pinReservedReason(p);
        if (!reserved.isEmpty())
            w << tr("%1 uses %2, which is the %3.").arg(trGpio(roles[s]), pinName(p), reserved);
        for (const auto &u : usage[p])
        {
            if (u.output && u.index == i) continue;
            w << (u.output ? tr("%1 (%2) is also used by output %3 (%4).").arg(trGpio(roles[s]), pinName(p)).arg(u.index).arg(u.role)
                           : tr("%1 (%2) is also used by input %3.").arg(trGpio(roles[s]), pinName(p)).arg(u.index));
        }
        if (info.needsPwm && s < info.pinCount && !isPwmPin(p))
            w << tr("%1 (%2) is not PWM capable; %3 needs PWM.")
                     .arg(trGpio(roles[s]), pinName(p), outputTypeName(c.type));
    }
    if ((c.type == SinglePulse || c.type == DoublePulse) && m_link->legacyOutputFormat())
        w << tr("This board runs old firmware, which never ends the pulse — "
                "a coil can stay energised. Update the firmware (review F9).");
    if (c.type == ServoType && c.servoMin() > c.servoMax())
        w << tr("Min is larger than max: positions are swapped.");
    for (int s = 0; s < info.pinCount; ++s)
        if (c.pin(s) < 2 && !pinNote(c.pin(s)).isEmpty())
            w << tr("%1 (%2) is a serial pin: it only works when no GUI is connected.")
                     .arg(trGpio(roles[s]), pinName(c.pin(s)));
    if (info.needsPwm || c.type == Pwm)
    {
        bool servo = false;
        for (const OutputConfig &o : m_edit.outputs)
            servo |= (o.type == ServoType);
        for (int s = 0; s < info.pinCount; ++s)
            if (servo && (c.pin(s) == 9 || c.pin(s) == 10))
                w << tr("%1 (%2): conflict with the servo(s). The Servo library uses timer 1, so PWM on D9 and D10 "
                        "stops working as soon as at least one servo output exists. Use D3, D5, D6 or D11 for PWM, "
                        "or remove the servo.").arg(trGpio(roles[s]), pinName(c.pin(s)));
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
            w << tr("Pin %1 is the %2.").arg(pinName(c.pin), reserved);
        const auto usage = Ui::pinUsage(m_edit);
        for (const auto &u : usage[c.pin])
            if (u.output || u.index != i)
                w << (u.output ? tr("Pin %1 is also used by output %2.").arg(pinName(c.pin)).arg(u.index)
                               : tr("Pin %1 is also used by input %2.").arg(pinName(c.pin)).arg(u.index));
    }
    if ((c.type == Recorder || c.type == FeedbackInput) && c.pin == PinNA)
        w << tr("This type needs an input pin.");
    if (c.type == Recorder && recorderInput(m_edit, i) >= 0)
        w << tr("Input %1 is a recorder too. Only one recorder per board is allowed.").arg(recorderInput(m_edit, i));
    if (info.usesAddress && c.address == AddrNone && c.pin == PinNA)
        w << tr("Neither a DCC address nor a pin is set: this input never triggers.");
    if (info.usesRoute)
    {
        for (int s = 0; s < c.route.size(); ++s)
            if (m_edit.outputs[c.route[s].output].type == OutputNotInUse)
                w << tr("Route step %1 sets output %2, which is not in use.").arg(s + 1).arg(c.route[s].output);
        const RouteLimits l = routeLimits();
        if (l.v1 && c.route.size() > MaxRouteStepsV1)
            w << tr("This board stores only %1 route steps (protocol v1); steps %2 and later are lost.")
                     .arg(MaxRouteStepsV1).arg(MaxRouteStepsV1 + 1);
        if (totalRouteSteps(m_edit) > l.pool)
            w << tr("All routes together use %1 steps; the board has room for %2.").arg(totalRouteSteps(m_edit)).arg(l.pool);
    }
    if (info.usesOutput && c.type != FeedbackInput && m_edit.outputs[c.outputId].type == OutputNotInUse)
        w << tr("Output %1 is not in use.").arg(c.outputId);
    return w;
}

// ── Log ─────────────────────────────────────────────────────────────────────
void MainWindow::appendLog(const QString &kind, const QString &text)
{
    if ((kind == QLatin1String("TX") || kind == QLatin1String("RX")) && m_actRaw && !m_actRaw->isChecked())
        return;
    static const QHash<QString, QString> colors = {
        { QStringLiteral("TX"),   QStringLiteral("#93C5FD") },
        { QStringLiteral("RX"),   QStringLiteral("#86EFAC") },
        { QStringLiteral("INFO"), QStringLiteral("#CBD5E1") },
        { QStringLiteral("WARN"), QStringLiteral("#FCD34D") },
        { QStringLiteral("ERR"),  QStringLiteral("#FCA5A5") },
    };
    const QString time = QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    m_log->appendHtml(QStringLiteral("<span style=\"color:#64748B\">%1</span> "
                                     "<span style=\"color:%2; white-space:pre\">%3  %4</span>")
                          .arg(time, colors.value(kind, QStringLiteral("#E2E8F0")),
                               kind.leftJustified(4), text.toHtmlEscaped()));
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
QString MainWindow::takeOverPins(const QVector<quint8> &pins, int output, int input)
{
    QStringList freed;
    for (quint8 p : pins)
    {
        for (int o = 0; o < NumOutputs; ++o)
        {
            if (o == output) continue;
            const OutputConfig &c = m_edit.outputs[o];
            const OutputTypeInfo &info = outputTypeInfo(c.type);
            bool uses = false;
            for (int k = 0; k < 7; ++k)
                uses |= ((k < 4) ? (k < info.pinCount) : info.relays) && c.pin(k) == p;
            if (!uses || c.type == OutputNotInUse) continue;
            OutputConfig off;
            off.name = c.name;
            off.lampColors = c.lampColors;
            m_edit.outputs[o] = off;
            freed << tr("output %1").arg(o);
        }
        for (int j = 0; j < NumInputs; ++j)
        {
            if (j == input) continue;
            const InputConfig &c = m_edit.inputs[j];
            if (c.type == InputNotInUse || c.pin != p) continue;
            InputConfig off;
            off.name = c.name;
            m_edit.inputs[j] = off;
            freed << tr("input %1").arg(j);
        }
    }
    freed.removeDuplicates();
    return freed.isEmpty() ? QString()
                           : tr("The pin was in use: %1 is now not in use.").arg(freed.join(QStringLiteral(", ")));
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
            o["name"]  = c.name;
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
        statusBar()->showMessage(tr("Phone: output %1 %2 = %3°").arg(o)
                                     .arg(which ? tr("straight") : tr("curved")).arg(angle), 5000);
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
    if (on && !(m_link->isReady() && m_link->caps().protocol >= 2))
    {
        QMessageBox::information(this, windowTitle(), tr("Live test needs a connected board with firmware v2."));
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
                                 tr("Input %1 has no DCC address, so it cannot receive a DCC extended command.").arg(input));
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

void MainWindow::refreshLampRow()
{
    const OutputConfig &o = m_edit.outputs[m_outEd->index()];
    m_lampRow->setVisible(o.type == FourToneSignal);
    for (int k = 0; k < m_lampButtons.size(); ++k)
        m_lampButtons[k]->setStyleSheet(QStringLiteral("QToolButton { background: %1; border: 1px solid #64748B; border-radius: 4px; }")
                                            .arg(Ui::lampColor(o, k).name()));
}

// ── Simple / expert mode ────────────────────────────────────────────────────
void MainWindow::setExpertMode(bool on)
{
    m_expert = on;
    QSettings().setValue(QStringLiteral("ui/expert"), on);
    if (m_actExpert && m_actExpert->isChecked() != on)
        m_actExpert->setChecked(on);
    for (QAction *a : m_expertOnly)
        a->setVisible(on);
    if (auto *dock = findChild<QDockWidget *>(QStringLiteral("logDock")))
    {
        // the serial log is off by default, also in expert mode (View > Serial log)
        dock->setVisible(on && QSettings().value(QStringLiteral("ui/showLog"), false).toBool());
        dock->toggleViewAction()->setVisible(on);
    }
    if (m_actRaw) m_actRaw->setVisible(on);
    // payload bytes under the editors
    for (QWidget *ed : { static_cast<QWidget *>(m_inEd), static_cast<QWidget *>(m_outEd) })
        for (QLabel *l : ed->findChildren<QLabel *>(QStringLiteral("mono")))
            l->setVisible(on);
    m_protoChip->setVisible(on && m_link->isOpen() && m_link->caps().known);
    updateActions();
}

void MainWindow::showWorkspace()
{
    if (m_stack)
        m_stack->setCurrentIndex(1);
}

QWidget *MainWindow::buildStartPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("startPage"));
    auto *outer = new QVBoxLayout(page);
    outer->addStretch(1);
    auto *box = new QWidget;
    box->setMaximumWidth(520);
    auto *v = new QVBoxLayout(box);
    auto *title = new QLabel(tr("<h2>GPIOcore Configurator</h2>"
                                "<p>Plug a GPIOcore board into a USB port, choose its port at the top and press "
                                "<b>Connect</b>. Everything you change is sent to the board straight away.</p>"));
    title->setWordWrap(true);
    v->addWidget(title);
    auto add = [this, v](const QString &text, const QString &help, auto slot) {
        auto *b = new QPushButton(text);
        b->setMinimumHeight(40);
        b->setToolTip(help);
        connect(b, &QPushButton::clicked, this, slot);
        v->addWidget(b);
        auto *l = new QLabel(help);
        l->setObjectName(QStringLiteral("help"));
        l->setWordWrap(true);
        v->addWidget(l);
    };
    add(tr("Connect to the board"), tr("Uses the port selected at the top."), [this] { toggleConnection(); });
    add(tr("Simulate"), tr("Try a configuration without a board: open a board file or start a new one, then "
                           "simulate it. Nothing can break."), [this] { connectDemo(); });
    add(tr("Open a board file…"), tr("Look at or prepare the settings of a board without connecting it."), [this] { openFile(); });
    add(tr("New board (offline)…"), tr("Prepare a new board on this computer and send it later."), [this] { newOfflineBoard(); });
    add(tr("Flash firmware…"), tr("Put the GPIOcore firmware on an Arduino."), [this] { flashFirmware(); });
    v->addWidget(Info::button(QStringLiteral("app.start")), 0, Qt::AlignLeft);
    auto *h = new QHBoxLayout;
    h->addStretch(1);
    h->addWidget(box);
    h->addStretch(1);
    outer->addLayout(h);
    outer->addStretch(2);
    return page;
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
            appendLog(QStringLiteral("INFO"), tr("Names loaded from %1").arg(QDir::toNativeSeparators(path)));
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
    auto *recorder = new QCheckBox(tr("Use the recorder (needs an I2C EEPROM on A4/A5)"));
    recorder->setChecked(m_edit.board.recorder);
    recorder->setToolTip(tr("The recorder stores its recording in an external 24LC256 EEPROM. "
                            "While it is on, A4 and A5 cannot be used as inputs or outputs."));
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
    form->addRow(QString(), recorder);
    form->addRow(QString(), timing);
    form->addRow(tr("Wait time per step"), delay);
    form->addRow(tr("Route interlocking"), interlock);
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

    if (recorder->isChecked() && !m_edit.board.recorder)
    {
        const auto usage = Ui::pinUsage(m_edit);
        if (!usage[18].isEmpty() || !usage[19].isEmpty())
        {
            QMessageBox::warning(this, tr("Board settings"),
                                 tr("The recorder needs an I2C EEPROM on A4 (SDA) and A5 (SCL), but A4 or A5 is in use "
                                    "by an input or output. Move that first, then switch the recorder on."));
            return;
        }
    }
    BoardSettings b = m_edit.board;
    b.name      = cleanBoardName(name->text());
    b.type      = quint8(type->currentIndex());
    b.recorder  = recorder->isChecked();
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
    const QStringList presets = presetNames();
    start->addItem(tr("Empty (1 input, 1 output)"), 8);
    for (int k = 0; k < 8; ++k)
        start->addItem(tr("Preset: %1").arg(presets.value(k)), k);
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
    BoardConfig b = presetBoard(start->currentData().toInt());
    b.board.name = name;
    b.board.type = m_edit.board.type;
    m_edit = m_dev = b;
    m_shadow = b;
    setBoardType(b.board.type);
    showWorkspace();
    showOutput(m_outEd->index());
    showInput(m_inEd->index());
    refreshAll();
    markFileDirty();
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
                freed << tr("input %1").arg(i);
            }
        OutputConfig c;
        m_edit.outputs[o] = c;
        if (!freed.isEmpty())
            statusBar()->showMessage(tr("Output %1 removed; %2 drove it and is now not in use as well.")
                                         .arg(o).arg(freed.join(QStringLiteral(", "))), 8000);
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
void MainWindow::applyPresetOnStart(int preset)
{
    m_startPreset = preset;
    auto *once = new QObject(this);
    connect(m_link, &DeviceLink::finished, once, [this, preset, once] {
        once->deleteLater();
        applyPresetNow(preset);
    });
}

void MainWindow::scheduleScreenshot(const QString &path, const QString &tab, int select)
{
    auto *once = new QObject(this);
    auto remaining = std::make_shared<int>(m_startPreset >= 0 ? 2 : 1);
    connect(m_link, &DeviceLink::finished, once, [this, path, tab, select, once, remaining] {
        if (--*remaining > 0)
            return;
        once->deleteLater();
        if      (tab == QLatin1String("inputs")) { m_tabs->setCurrentIndex(0); selectInput(select); }
        else if (tab == QLatin1String("pins"))   { m_tabs->setCurrentIndex(1); }
        else                                     { m_tabs->setCurrentIndex(0); selectOutput(select); }
        QTimer::singleShot(300, this, [this, path] {
            grab().save(path);
            qApp->exit(0);
        });
    });
}
