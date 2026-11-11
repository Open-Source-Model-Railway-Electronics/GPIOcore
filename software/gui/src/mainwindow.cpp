#include "mainwindow.h"

#include "connections.h"
#include "connectionview.h"
#include "devicelink.h"
#include "i18n.h"
#include "infotexts.h"
#include "inputeditor.h"
#include "outputeditor.h"
#include "pinmapview.h"
#include "simulateddevice.h"
#include "uihelpers.h"

#include <QAction>
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
#include <QJsonDocument>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QScrollArea>
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

static constexpr int PanelWidth = 580;

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
    resize(1320, 860);

    m_link = new DeviceLink(this);

    // Start with what a factory-fresh board contains, so the GUI is useful offline.
    SimulatedDevice defaults;
    for (int i = 0; i < NumOutputs; ++i) m_edit.outputs[i] = defaults.output(i);
    for (int i = 0; i < NumInputs;  ++i) m_edit.inputs[i]  = defaults.input(i);
    m_dev = m_edit;

    buildUi();
    buildToolbar();
    refreshPorts();

    connect(m_link, &DeviceLink::opened,  this, &MainWindow::onConnectionChanged);
    connect(m_link, &DeviceLink::closed,  this, &MainWindow::onConnectionChanged);
    connect(m_link, &DeviceLink::ready,   this, &MainWindow::onReady);
    connect(m_link, &DeviceLink::outputReceived, this, &MainWindow::onOutputReceived);
    connect(m_link, &DeviceLink::inputReceived,  this, &MainWindow::onInputReceived);
    connect(m_link, &DeviceLink::routeReceived,  this, &MainWindow::onRouteReceived);
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
        updateActions();
        statusBar()->showMessage(failures ? tr("Done with %1 error(s)").arg(failures) : tr("Done"), 4000);
    });
    connect(m_link, &DeviceLink::presetApplied, this, [this](int preset) {
        m_outKnown.fill(false);         // board content replaced: take everything from the board
        m_inKnown.fill(false);
        appendLog(QStringLiteral("INFO"), tr("Preset %1 applied, reading back").arg(preset));
    });

    applyCapabilities();
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

    auto makePanel = [this](QWidget *editor, const QString &title, QWidget *&panel) {
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
        head->addWidget(t, 1);
        head->addWidget(close);
        v->addLayout(head);
        auto *sa = new QScrollArea;
        sa->setWidgetResizable(true);
        sa->setWidget(editor);
        sa->setFrameShape(QFrame::NoFrame);
        sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);   // never cut anything off
        v->addWidget(sa, 1);
        panel->setMinimumWidth(0);
        panel->setMaximumWidth(0);
        panel->hide();
    };
    makePanel(m_inEd,  tr("INPUT SETTINGS"),  m_leftPanel);
    makePanel(m_outEd, tr("OUTPUT SETTINGS"), m_rightPanel);

    auto *viewScroll = new QScrollArea;
    viewScroll->setWidgetResizable(true);
    viewScroll->setWidget(m_view);
    viewScroll->setFrameShape(QFrame::NoFrame);
    viewScroll->setMinimumWidth(560);

    auto *config = new QWidget;
    config->setObjectName(QStringLiteral("configArea"));
    auto *h = new QHBoxLayout(config);
    h->setContentsMargins(6, 6, 6, 6);
    h->setSpacing(6);
    h->addWidget(m_leftPanel);
    h->addWidget(viewScroll, 1);
    h->addWidget(m_rightPanel);
    m_tabs->addTab(config, tr("Configuration"));

    auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), config);
    connect(esc, &QShortcut::activated, this, &MainWindow::closePanels);

    // Pin map
    m_pinMap = new PinMapView;
    m_tabs->addTab(m_pinMap, tr("Pin map"));

    setCentralWidget(m_tabs);

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
        boardEdited();
        selectInput(in);
    });
    connect(m_view, &ConnectionView::disconnectRequested, this, [this](int in, int step) {
        Gpio::disconnect(m_edit, in, step);
        boardEdited();
    });
    connect(m_view, &ConnectionView::toggleStepRequested, this, [this](int in, int step) {
        toggleStepState(m_edit, in, step);
        boardEdited();
    });
    connect(m_view, &ConnectionView::moveStepRequested, this, [this](int in, int step, int delta) {
        moveStep(m_edit, in, step, delta);
        boardEdited();
    });
    connect(m_outEd, &OutputEditor::changed, this, [this](int i, const OutputConfig &c) {
        m_edit.outputs[i] = c;
        m_inEd->setOutputs(outputNames(), outputTypes());
        boardEdited();
    });
    connect(m_outEd, &OutputEditor::liveServo, this, [this](int i, int angle) {
        m_link->servoLive(i, angle);    // queued sends are merged, only the newest angle goes out
    });
    connect(m_inEd, &InputEditor::changed, this, [this](int i, const InputConfig &c) {
        m_edit.inputs[i] = c;
        boardEdited();
    });
    connect(m_outEd, &OutputEditor::testRequested, this, &MainWindow::testOutput);
    connect(m_pinMap, &PinMapView::outputActivated, this, [this](int i) { m_tabs->setCurrentIndex(0); selectOutput(i); });
    connect(m_pinMap, &PinMapView::inputActivated,  this, [this](int i) { m_tabs->setCurrentIndex(0); selectInput(i); });
}

void MainWindow::boardEdited()
{
    refreshRows();
    if (m_open == OpenInput)
        m_inEd->setInput(m_inEd->index(), m_edit.inputs[m_inEd->index()]);
    updateInputEditorLimits();
    refreshWarnings();
    m_pinMap->setBoard(m_edit);
    updateActions();
}

// ── Sliding panels ──────────────────────────────────────────────────────────
static void slide(QWidget *panel, bool open)
{
    auto *anim = new QPropertyAnimation(panel, "maximumWidth", panel);
    anim->setDuration(180);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setStartValue(panel->isVisible() ? panel->width() : 0);
    anim->setEndValue(open ? PanelWidth : 0);
    // Minimum follows maximum: the panel gets exactly this width, so the layout
    // can never squeeze it (on wide windows the canvas would take the space).
    QObject::connect(anim, &QVariantAnimation::valueChanged, panel,
                     [panel](const QVariant &v) { panel->setMinimumWidth(v.toInt()); });
    if (open)
        panel->show();
    else
        QObject::connect(anim, &QPropertyAnimation::finished, panel, &QWidget::hide);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::openPanel(OpenPanel which)
{
    if (which == m_open)
        return;
    if (m_open == OpenInput)  slide(m_leftPanel,  false);
    if (m_open == OpenOutput) slide(m_rightPanel, false);
    if (which == OpenInput)   slide(m_leftPanel,  true);
    if (which == OpenOutput)  slide(m_rightPanel, true);
    m_open = which;
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
    m_protoChip->setVisible(known);
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

void MainWindow::setDccNext(bool on)
{
    boardOptions().dccNext = on;
    QSettings().setValue(QStringLiteral("board/dccNext"), on);
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

    m_port = new QComboBox;
    m_port->setMinimumWidth(200);
    m_port->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    tb->addWidget(new QLabel(tr("  Port ")));
    tb->addWidget(m_port);
    m_actRefresh = tb->addAction(style()->standardIcon(QStyle::SP_BrowserReload), QString(), this, &MainWindow::refreshPorts);
    m_actRefresh->setToolTip(tr("Refresh port list"));
    m_actConnect = tb->addAction(tr("Connect"), this, &MainWindow::toggleConnection);
    m_actDemo    = tb->addAction(tr("Demo"), this, &MainWindow::connectDemo);
    m_actDemo->setToolTip(tr("Connect to a built-in simulation of the GPIOcore firmware"));
    tb->addSeparator();

    m_actRead   = tb->addAction(style()->standardIcon(QStyle::SP_ArrowDown), tr("Read board"), this, &MainWindow::readFromBoard);
    m_actWrite  = tb->addAction(style()->standardIcon(QStyle::SP_ArrowUp),   tr("Write changes"), this, &MainWindow::writeChanges);
    m_actRevert = tb->addAction(tr("Revert"), this, &MainWindow::revertChanges);
    if (QWidget *w = tb->widgetForAction(m_actWrite))
        w->setObjectName(QStringLiteral("primary"));
    tb->addSeparator();

    m_preset = new QComboBox;
    m_preset->addItems(presetNames());
    tb->addWidget(new QLabel(tr(" Preset ")));
    tb->addWidget(m_preset);
    m_actPreset = tb->addAction(tr("Apply"), this, &MainWindow::applyPreset);
    tb->addSeparator();

    m_dccNext = new QCheckBox(tr("DCCnext board"));
    m_dccNext->setChecked(QSettings().value(QStringLiteral("board/dccNext"), false).toBool());
    boardOptions().dccNext = m_dccNext->isChecked();
    connect(m_dccNext, &QCheckBox::toggled, this, &MainWindow::setDccNext);
    tb->addWidget(m_dccNext);
    tb->addWidget(Info::button(QStringLiteral("board.dccnext")));

    // Menus
    QMenu *file = menuBar()->addMenu(tr("&File"));
    m_actOpen = file->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("&Open configuration…"),
                                this, &MainWindow::openFile);
    m_actOpen->setShortcut(QKeySequence::Open);
    m_actSave = file->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("&Save configuration…"),
                                this, &MainWindow::saveFile);
    m_actSave->setShortcut(QKeySequence::Save);
    file->addSeparator();
    file->addAction(tr("&Quit"), this, &QWidget::close)->setShortcut(QKeySequence::Quit);

    QMenu *board = menuBar()->addMenu(tr("&Board"));
    board->addAction(m_actConnect);
    board->addAction(m_actDemo);
    board->addSeparator();
    board->addAction(m_actRead);
    board->addAction(m_actWrite);
    board->addAction(m_actRevert);
    m_actWrite->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    m_actRead->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));

    QMenu *view = menuBar()->addMenu(tr("&View"));
    m_actRaw = view->addAction(tr("Show TX/RX bytes in log"));
    m_actRaw->setCheckable(true);
    m_actRaw->setChecked(true);
    view->addAction(tr("Clear log"), m_log, &QPlainTextEdit::clear);
    if (auto *dock = findChild<QDockWidget *>(QStringLiteral("logDock")))
        view->addAction(dock->toggleViewAction());

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
    if (m_link->isOpen())
    {
        m_link->close();
        return;
    }
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
    QString err;
    if (!m_link->openSerial(port, &err))
        QMessageBox::warning(this, windowTitle(), tr("Could not open %1:\n%2").arg(port, err));
}

void MainWindow::connectDemo()
{
    if (m_link->isOpen())
        m_link->close();
    m_outKnown.fill(false);
    m_inKnown.fill(false);
    QString err;
    m_link->openDevice(new SimulatedDevice, true, &err);
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
    updateActions();
}

void MainWindow::onReady(const QString &version)
{
    m_connChip->setObjectName(QStringLiteral("chipOk"));
    m_connChip->setText(tr("● %1 — GPIOcore %2").arg(m_link->portName(), version));
    m_connChip->style()->unpolish(m_connChip);
    m_connChip->style()->polish(m_connChip);
    m_legacyChip->setVisible(m_link->legacyOutputFormat());
    m_link->readAll();
}

// ── Data from the board ─────────────────────────────────────────────────────
void MainWindow::onOutputReceived(int i, const OutputConfig &c)
{
    const bool keep = m_keepEdits || (m_outKnown[i] && outputDirty(i));
    m_dev.outputs[i] = c;
    m_outKnown[i] = true;
    if (!keep)
        m_edit.outputs[i] = c;
    refreshRows();
    if (m_outEd->index() == i && !keep)
        m_outEd->setOutput(i, m_edit.outputs[i]);
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
    m_dev.inputs[i] = c;
    m_inKnown[i] = true;
    if (!keep)
        m_edit.inputs[i] = c;
    refreshRows();
    if (m_inEd->index() == i && !keep)
        m_inEd->setInput(i, m_edit.inputs[i]);
    m_pinMap->setBoard(m_edit);
    refreshWarnings();
    updateActions();
}

void MainWindow::onRouteReceived(int i, const QVector<RouteStep> &steps)
{
    const bool keep = m_keepEdits || (m_inKnown[i] && m_edit.inputs[i].route != m_dev.inputs[i].route);
    setRouteSteps(m_dev.inputs[i], steps);
    m_inKnown[i] = true;
    if (!keep)
        setRouteSteps(m_edit.inputs[i], steps);
    if (m_inEd->index() == i && !keep)
        m_inEd->setInput(i, m_edit.inputs[i]);
    boardEdited();
}

void MainWindow::readFromBoard()
{
    if (dirtyCount() > 0
        && QMessageBox::question(this, windowTitle(),
                                 tr("Discard %1 unsaved change(s) and reload everything from the board?").arg(dirtyCount()))
               != QMessageBox::Yes)
        return;
    m_outKnown.fill(false);
    m_inKnown.fill(false);
    m_keepEdits = false;
    m_link->readAll();
}

void MainWindow::writeChanges()
{
    int n = 0;
    for (int i = 0; i < NumInputs; ++i)
        if (inputDirty(i))
        {
            m_link->writeInput(i, m_edit.inputs[i]);
            m_link->writeRoute(i, m_edit.inputs[i].route);   // skipped on v1 boards (linked bytes)
            m_link->readInput(i);                             // also reads the route back
            ++n;
        }
    for (int i = 0; i < NumOutputs; ++i)
        if (outputDirty(i)) { m_link->writeOutput(i, m_edit.outputs[i]); m_link->readOutput(i); ++n; }
    appendLog(QStringLiteral("INFO"), tr("Writing %1 item(s), each is read back for verification").arg(n));
}

void MainWindow::revertChanges()
{
    for (int i = 0; i < NumOutputs; ++i) if (m_outKnown[i]) m_edit.outputs[i] = m_dev.outputs[i];
    for (int i = 0; i < NumInputs;  ++i) if (m_inKnown[i])  m_edit.inputs[i]  = m_dev.inputs[i];
    refreshAll();
    m_outEd->setOutput(m_outEd->index(), m_edit.outputs[m_outEd->index()]);
    m_inEd->setInput(m_inEd->index(), m_edit.inputs[m_inEd->index()]);
}

void MainWindow::applyPreset()
{
    const QString name = m_preset->currentText();
    if (QMessageBox::question(this, windowTitle(),
                              tr("Apply preset \"%1\"?\n\nThis overwrites the outputs (and for most presets the "
                                 "inputs) on the board. It takes a few seconds.").arg(name))
        != QMessageBox::Yes)
        return;
    m_link->applyPreset(m_preset->currentIndex());
}

void MainWindow::testOutput(int index, int state)
{
    m_edit.outputs[index].state = quint8(state);
    m_outEd->setOutput(index, m_edit.outputs[index]);
    m_link->writeOutput(index, m_edit.outputs[index]);
    m_link->readOutput(index);
    refreshRows();
}

// ── Files ───────────────────────────────────────────────────────────────────
void MainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open configuration"), QString(),
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
    m_edit = b;
    m_keepEdits = !m_link->isReady();     // will be compared with the board after connecting
    refreshAll();
    m_outEd->setOutput(m_outEd->index(), m_edit.outputs[m_outEd->index()]);
    m_inEd->setInput(m_inEd->index(), m_edit.inputs[m_inEd->index()]);
    statusBar()->showMessage(tr("Loaded %1 — use 'Write changes' to send it to the board").arg(path), 6000);
    return true;
}

void MainWindow::saveFile()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save configuration"), QStringLiteral("gpiocore.json"),
                                                      tr("GPIOcore configuration (*.json)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        QMessageBox::warning(this, windowTitle(), f.errorString());
        return;
    }
    f.write(QJsonDocument(m_edit.toJson()).toJson(QJsonDocument::Indented));
    statusBar()->showMessage(tr("Saved %1").arg(path), 4000);
}

// ── Dirty tracking ──────────────────────────────────────────────────────────
bool MainWindow::outputDirty(int i) const
{
    return m_outKnown[i] && !sameOutput(m_edit.outputs[i], m_dev.outputs[i], m_link->legacyOutputFormat());
}

bool MainWindow::inputDirty(int i) const
{
    if (!m_inKnown[i])
        return false;
    if (m_link->isOpen() && m_link->caps().known && m_link->caps().protocol < 2)
        return m_edit.inputs[i].asV1() != m_dev.inputs[i];   // v1 stores only what fits in linked[]
    return m_edit.inputs[i] != m_dev.inputs[i];
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
    m_actRead->setEnabled(ready && !busy);
    m_actWrite->setEnabled(ready && !busy && dirty > 0);
    m_actWrite->setText(dirty ? tr("Write %1 change(s)").arg(dirty) : tr("Write changes"));
    m_actRevert->setEnabled(dirty > 0);
    m_actPreset->setEnabled(ready && !busy);
    m_port->setEnabled(!m_link->isOpen());
    m_actRefresh->setEnabled(!m_link->isOpen());
    m_dirtyChip->setText(tr("%1 unsent change(s)").arg(dirty));
    m_dirtyChip->setVisible(dirty > 0);
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
    m_outEd->setOutput(i, m_edit.outputs[i]);
    m_outEd->setWarnings(outputWarnings(i));
    openPanel(OpenOutput);
}

void MainWindow::selectInput(int i)
{
    m_view->selectInput(i);
    m_inEd->setInput(i, m_edit.inputs[i]);
    m_inEd->setWarnings(inputWarnings(i));
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
        names << tr("Output %1 · %2").arg(i).arg(outputTypeName(m_edit.outputs[i].type));
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
        if (!pinNote(c.pin(s)).isEmpty())
            w << tr("%1 (%2) is a serial pin: it only works when no GUI is connected.")
                     .arg(trGpio(roles[s]), pinName(c.pin(s)));
    if (info.needsPwm || c.type == Pwm)
    {
        bool servo = false;
        for (const OutputConfig &o : m_edit.outputs)
            servo |= (o.type == ServoType);
        for (int s = 0; s < info.pinCount; ++s)
            if (servo && (c.pin(s) == 9 || c.pin(s) == 10))
                w << tr("%1 (%2): the Servo library uses timer 1, so PWM on D9 / D10 does not work while a servo "
                        "output exists.").arg(trGpio(roles[s]), pinName(c.pin(s)));
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
    if (dirtyCount() > 0
        && QMessageBox::question(this, windowTitle(),
                                 tr("%1 change(s) have not been written to the board. Quit anyway?").arg(dirtyCount()))
               != QMessageBox::Yes)
    {
        e->ignore();
        return;
    }
    e->accept();
}

// ── Screenshot mode (for documentation / automated checks) ──────────────────
void MainWindow::applyPresetOnStart(int preset)
{
    m_startPreset = preset;
    auto *once = new QObject(this);
    connect(m_link, &DeviceLink::finished, once, [this, preset, once] {
        once->deleteLater();
        m_link->applyPreset(preset);
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
