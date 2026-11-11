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
    m_edit.board.type = quint8(QSettings().value(QStringLiteral("board/type"), int(BoardNano)).toInt());
    Gpio::setBoardType(m_edit.board.type);
    m_dev = m_edit;

    m_liveTimer = new QTimer(this);
    m_liveTimer->setSingleShot(true);
    m_liveTimer->setInterval(350);       // edits are sent after a short pause (slider drags, typing)
    connect(m_liveTimer, &QTimer::timeout, this, &MainWindow::sendLive);

    buildUi();
    buildToolbar();
    refreshPorts();

    connect(m_link, &DeviceLink::opened,  this, &MainWindow::onConnectionChanged);
    connect(m_link, &DeviceLink::closed,  this, &MainWindow::onConnectionChanged);
    connect(m_link, &DeviceLink::ready,   this, &MainWindow::onReady);
    connect(m_link, &DeviceLink::outputReceived, this, &MainWindow::onOutputReceived);
    connect(m_link, &DeviceLink::inputReceived,  this, &MainWindow::onInputReceived);
    connect(m_link, &DeviceLink::routeReceived,  this, &MainWindow::onRouteReceived);
    connect(m_link, &DeviceLink::boardReceived,  this, &MainWindow::onBoardReceived);
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
        if (failures)
            statusBar()->showMessage(tr("Done with %1 error(s)").arg(failures), 6000);
        if (dirtyCount() > 0 && m_liveTimer->isActive() == false && failures == 0)
            m_liveTimer->start();        // edits made while the board was busy
        saveBoardFile();
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
        remove->setText(tr("Remove"));
        remove->setToolTip(tr("Set this to 'not in use' and take it off the canvas"));
        connect(remove, &QToolButton::clicked, this, &MainWindow::removeSelected);
        head->addWidget(t, 1);
        head->addWidget(remove);
        head->addWidget(close);
        v->addLayout(head);
        name = new QLineEdit;
        name->setPlaceholderText(tr("Name, e.g. \"Turnout station left\" (kept on this computer)"));
        name->setMaxLength(40);
        v->addWidget(name);
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
    makePanel(m_inEd,  tr("INPUT SETTINGS"),  m_leftPanel,  m_inName);
    makePanel(m_outEd, tr("OUTPUT SETTINGS"), m_rightPanel, m_outName);
    connect(m_inName, &QLineEdit::textEdited, this, [this](const QString &t) {
        m_edit.inputs[m_inEd->index()].name = t.trimmed();
        refreshRows();
        saveBoardFile();
    });
    connect(m_outName, &QLineEdit::textEdited, this, [this](const QString &t) {
        m_edit.outputs[m_outEd->index()].name = t.trimmed();
        refreshRows();
        m_inEd->setOutputs(outputNames(), outputTypes());
        saveBoardFile();
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
    h->setSpacing(6);
    h->addWidget(m_leftPanel);
    h->addWidget(centre, 1);
    h->addWidget(m_rightPanel);
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
        const QString name = m_edit.outputs[i].name;
        m_edit.outputs[i] = c;
        m_edit.outputs[i].name = name;
        m_inEd->setOutputs(outputNames(), outputTypes());
        userEdited();
    });
    connect(m_outEd, &OutputEditor::liveServo, this, [this](int i, int angle) {
        m_link->servoLive(i, angle);    // queued sends are merged, only the newest angle goes out
    });
    connect(m_inEd, &InputEditor::changed, this, [this](int i, const InputConfig &c) {
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
    boardEdited();
    if (m_link->isReady())
        m_liveTimer->start();           // (re)start: send after a short pause
    else
        saveBoardFile();
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
    writeChanges();
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
    m_actRead->setToolTip(tr("Read everything from the board again"));
    m_actWrite  = new QAction(style()->standardIcon(QStyle::SP_ArrowUp), tr("Write changes"), this);
    connect(m_actWrite, &QAction::triggered, this, &MainWindow::writeChanges);
    m_actRevert = new QAction(tr("Revert"), this);
    connect(m_actRevert, &QAction::triggered, this, &MainWindow::revertChanges);
    m_expertOnly << tb->addSeparator();

    m_preset = new QComboBox;
    m_preset->addItems(presetNames());
    m_expertOnly << tb->addWidget(new QLabel(tr(" Preset ")));
    m_expertOnly << tb->addWidget(m_preset);
    m_actPreset = tb->addAction(tr("Apply"), this, &MainWindow::applyPreset);
    m_expertOnly << m_actPreset;
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
        saveBoardFile();
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
    m_actSave = file->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("&Save configuration…"),
                                this, &MainWindow::saveFile);
    m_actSave->setShortcut(QKeySequence::Save);
    file->addAction(tr("Open the boards folder"), this, [] {
        QDir().mkpath(boardsDir());
        QDesktopServices::openUrl(QUrl::fromLocalFile(boardsDir()));
    });
    file->addSeparator();
    file->addAction(tr("&Quit"), this, &QWidget::close)->setShortcut(QKeySequence::Quit);

    QMenu *board = menuBar()->addMenu(tr("&Board"));
    board->addAction(m_actConnect);
    board->addAction(m_actDemo);
    board->addSeparator();
    board->addAction(m_actRead);
    board->addAction(m_actWrite);
    board->addAction(m_actRevert);
    board->addSeparator();
    board->addAction(m_actBoardSettings);
    board->addAction(tr("Apply a preset…"), this, [this] { setExpertMode(true); m_preset->setFocus(); });
    board->addSeparator();
    board->addAction(tr("&Flash firmware…"), this, &MainWindow::flashFirmware);
    m_actWrite->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    m_actRead->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));

    QMenu *view = menuBar()->addMenu(tr("&View"));
    m_actExpert = view->addAction(tr("&Expert mode"));
    m_actExpert->setCheckable(true);
    m_actExpert->setToolTip(tr("Show bytes, the serial log, protocol details and presets"));
    connect(m_actExpert, &QAction::toggled, this, &MainWindow::setExpertMode);
    m_actShowUnused = view->addAction(tr("Show unused inputs and outputs"));
    m_actShowUnused->setCheckable(true);
    m_actShowUnused->setChecked(QSettings().value(QStringLiteral("ui/showUnused"), false).toBool());
    m_view->setHideUnused(!m_actShowUnused->isChecked());
    connect(m_actShowUnused, &QAction::toggled, this, [this](bool on) {
        QSettings().setValue(QStringLiteral("ui/showUnused"), on);
        m_view->setHideUnused(!on);
    });
    view->addSeparator();
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
    m_nameAsked = false;
    showWorkspace();
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
    m_nameAsked = true;                  // the demo board does not need a name
    showWorkspace();
    QString err;
    auto *sim = new SimulatedDevice;
    BoardSettings demo;
    demo.name = QStringLiteral("Demo");
    demo.type = m_edit.board.type;
    sim->setBoard(demo);
    m_link->openDevice(sim, true, &err);
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
    m_dev.outputs[i].name = m_edit.outputs[i].name;
    m_outKnown[i] = true;
    if (!keep)
        m_edit.outputs[i] = m_dev.outputs[i];
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
    c.name = m_edit.inputs[i].name;
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
    showWorkspace();
    refreshAll();
    m_outEd->setOutput(m_outEd->index(), m_edit.outputs[m_outEd->index()]);
    m_inEd->setInput(m_inEd->index(), m_edit.inputs[m_inEd->index()]);
    statusBar()->showMessage(tr("Loaded %1").arg(path), 6000);
    return true;
}

void MainWindow::saveFile()
{
    QDir().mkpath(boardsDir());
    const QString suggested = QDir(boardsDir()).filePath(
        (m_edit.board.name.isEmpty() ? QStringLiteral("gpiocore") : m_edit.board.name) + QStringLiteral(".json"));
    const QString path = QFileDialog::getSaveFileName(this, tr("Save configuration"), suggested,
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
    m_outName->setText(m_edit.outputs[i].name);
    openPanel(OpenOutput);
}

void MainWindow::selectInput(int i)
{
    m_view->selectInput(i);
    m_inEd->setInput(i, m_edit.inputs[i]);
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
    saveBoardFile();
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
        dock->setVisible(on);
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
    add(tr("Try the demo"), tr("A simulated board, nothing can break."), [this] { connectDemo(); });
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
    // Names (and offline edits) live in <name>.json; the board itself wins for all settings,
    // which arrive right after this.
    const QString path = QDir(boardsDir()).filePath(b.name + QStringLiteral(".json"));
    if (!b.name.isEmpty() && QFileInfo::exists(path))
    {
        QFile f(path);
        BoardConfig file;
        if (f.open(QIODevice::ReadOnly)
            && BoardConfig::fromJson(QJsonDocument::fromJson(f.readAll()).object(), file, nullptr))
        {
            for (int i = 0; i < NumOutputs; ++i) m_edit.outputs[i].name = file.outputs[i].name;
            for (int i = 0; i < NumInputs; ++i)  m_edit.inputs[i].name  = file.inputs[i].name;
            appendLog(QStringLiteral("INFO"), tr("Names loaded from %1").arg(QDir::toNativeSeparators(path)));
        }
    }
    m_edit.board = b;
    setBoardType(b.type);
    setWindowTitle(b.name.isEmpty() ? tr("GPIOcore Configurator") : tr("%1 — GPIOcore Configurator").arg(b.name));
    m_connChip->setText(tr("● %1 — %2").arg(m_link->portName(), b.name.isEmpty() ? tr("board without a name") : b.name));
    refreshAll();
}

void MainWindow::saveBoardFile()
{
    const QString name = m_edit.board.name;
    if (name.isEmpty() || name == QLatin1String("Demo"))
        return;
    QDir().mkpath(boardsDir());
    QFile f(QDir(boardsDir()).filePath(name + QStringLiteral(".json")));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(m_edit.toJson()).toJson(QJsonDocument::Indented));
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
    saveBoardFile();
}

void MainWindow::editBoardSettings()
{
    QDialog d(this);
    d.setWindowTitle(tr("Board settings"));
    auto *form = new QFormLayout(&d);
    auto *name = new QLineEdit(m_edit.board.name);
    name->setMaxLength(MaxBoardName);
    auto *type = new QComboBox;
    for (int t = 0; t < BoardTypeCount; ++t) type->addItem(boardTypeName(quint8(t)));
    type->setCurrentIndex(m_edit.board.type);
    auto *recorder = new QCheckBox(tr("Use the recorder"));
    recorder->setChecked(m_edit.board.recorder);
    recorder->setToolTip(tr("Off: the recorder's memory is used for routes instead"));
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
        m_link->writeBoard(b);
        m_link->readBoard();
    }
    saveBoardFile();
    setWindowTitle(b.name.isEmpty() ? tr("GPIOcore Configurator") : tr("%1 — GPIOcore Configurator").arg(b.name));
}

void MainWindow::newOfflineBoard()
{
    if (m_link->isOpen())
        m_link->close();
    bool ok = false;
    const QString name = cleanBoardName(QInputDialog::getText(this, tr("New board"),
        tr("Name of the new board (max. 16 letters). When you later connect a board and give it this name, "
           "these settings belong to it."), QLineEdit::Normal, QString(), &ok));
    if (!ok)
        return;
    SimulatedDevice defaults;
    BoardConfig b;
    for (int i = 0; i < NumOutputs; ++i) b.outputs[i] = defaults.output(i);
    for (int i = 0; i < NumInputs;  ++i) b.inputs[i]  = defaults.input(i);
    b.board.name = name;
    b.board.type = m_edit.board.type;
    m_edit = m_dev = b;
    setBoardType(b.board.type);
    showWorkspace();
    refreshAll();
    saveBoardFile();
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
        OutputConfig c;
        m_edit.outputs[o] = c;
    }
    closePanels();
    userEdited();
}

void MainWindow::flashFirmware()
{
    QMessageBox::information(this, windowTitle(), tr("Flashing is not available in this build."));
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
