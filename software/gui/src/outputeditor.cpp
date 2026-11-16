#include "outputeditor.h"

#include <QDoubleSpinBox>

#include <QTimer>
#include "infotexts.h"
#include "protocol.h"
#include "uihelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Gpio;

enum Page { PageNone, PageTime, PageSwing, PageServo, PagePwm, PageSignal, PageBlock };

static Page pageFor(ParamKind k)
{
    switch (k)
    {
    case ParamKind::SwitchTime: return PageTime;
    case ParamKind::Swing:      return PageSwing;
    case ParamKind::Servo:      return PageServo;
    case ParamKind::PwmValue:   return PagePwm;
    case ParamKind::FourTone:   return PageSignal;
    case ParamKind::Block:      return PageBlock;
    default:                    break;
    }
    return PageNone;
}

static QSpinBox *makeSpin(int min, int max, const QString &suffix = QString())
{
    auto *s = new QSpinBox;
    s->setRange(min, max);
    s->setSuffix(suffix);
    s->setMinimumWidth(90);
    return s;
}

OutputEditor::OutputEditor(QWidget *parent) : QWidget(parent)
{
    for (int i = 0; i < NumInputs; ++i)
    {
        m_inputNames << tr("Input %1").arg(i);
        m_inputTypes << InputNotInUse;
    }
    buildUi();
    setOutput(0, OutputConfig());
}

OutputEditor::Frog OutputEditor::frogOf(const OutputConfig &c)
{
    if (c.relayA != PinNA || c.relayB != PinNA) return FrogElectro;
    if (c.relayC != PinNA)                      return FrogUni;
    return FrogNone;
}

void OutputEditor::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(8);

    // Header
    auto *head = new QHBoxLayout;
    m_title = new QLabel;
    m_title->setObjectName(QStringLiteral("title"));
    m_type = new QComboBox;
    m_type->setMinimumWidth(140);
    for (quint8 t = 0; t < OutputTypeCount; ++t)
        m_type->addItem(Ui::swatchIcon(Ui::outputTypeColor(t)), outputTypeName(t), int(t));
    head->addWidget(m_title);
    head->addStretch();
    head->addWidget(new QLabel(tr("Type")));
    head->addWidget(m_type);
    head->addWidget(Info::button(QStringLiteral("output.type")));
    root->addLayout(head);

    m_help = new QLabel;
    m_help->setObjectName(QStringLiteral("help"));
    m_help->setWordWrap(true);
    root->addWidget(m_help);

    m_warn = new QLabel;
    m_warn->setObjectName(QStringLiteral("error"));
    m_warn->setWordWrap(true);
    m_warn->hide();
    root->addWidget(m_warn);
    m_note = new QLabel;
    m_note->setObjectName(QStringLiteral("note"));
    m_note->setWordWrap(true);
    m_note->hide();
    root->addWidget(m_note);

    // Pins
    m_pinBox  = new QGroupBox(tr("Pins"));
    m_pinForm = new QFormLayout(m_pinBox);
    const QString pinLabels[4] = { tr("Pin A"), tr("Pin B"), tr("Pin C"), tr("Pin D") };
    for (int i = 0; i < 4; ++i)
    {
        m_pins[i] = new QComboBox;
        if (i == 0)
        {
            auto *row = new QHBoxLayout;
            row->addWidget(m_pins[i], 1);
            row->addWidget(Info::button(QStringLiteral("output.pins")));
            m_soundInfo = Info::button(QStringLiteral("output.sound"));   // shown for a sound effect only
            row->addWidget(m_soundInfo);
            m_pinForm->addRow(pinLabels[i], row);
        }
        else
            m_pinForm->addRow(pinLabels[i], m_pins[i]);
    }
    root->addWidget(m_pinBox);

    // Frog relays: electrofrog OR unifrog (interlocked)
    m_relayBox = new QGroupBox(tr("Frog polarity"));
    auto *relayInner = new QVBoxLayout(m_relayBox);
    auto *relayHost = new QWidget;
    m_relayForm = new QFormLayout(relayHost);
    m_relayForm->setContentsMargins(0, 0, 0, 0);
    m_frogMode = new QComboBox;
    m_frogMode->addItem(tr("None (dead frog)"),         int(FrogNone));
    m_frogMode->addItem(tr("Electrofrog — 2 relays"),    int(FrogElectro));
    m_frogMode->addItem(tr("Unifrog — 1 change-over relay"), int(FrogUni));
    auto *frogRow = new QHBoxLayout;
    frogRow->addWidget(m_frogMode, 1);
    frogRow->addWidget(Info::button(QStringLiteral("output.frog")));
    m_relayForm->addRow(tr("Frog"), frogRow);
    const QString relayLabels[3] = { tr("Relay straight (A)"), tr("Relay curved (B)"), tr("Unifrog relay") };
    for (int i = 0; i < 3; ++i)
    {
        m_relays[i] = new QComboBox;
        m_relayForm->addRow(relayLabels[i], m_relays[i]);
    }
    relayInner->addWidget(relayHost);
    m_frogNote = new QLabel;
    m_frogNote->setObjectName(QStringLiteral("warn"));
    m_frogNote->setWordWrap(true);
    relayInner->addWidget(m_frogNote);
    root->addWidget(m_relayBox);

    // Parameters
    m_paramBox = new QGroupBox(tr("Parameters"));
    auto *paramLay = new QVBoxLayout(m_paramBox);
    m_params = new QStackedWidget;
    paramLay->addWidget(m_params);

    { // none
        auto *w = new QLabel(tr("This type has no parameters."));
        w->setObjectName(QStringLiteral("help"));
        m_params->addWidget(w);
    }
    { // switch time
        auto *w = new QWidget;
        auto *f = new QFormLayout(w);
        m_switchTime   = makeSpin(0, 65535);
        m_switchTimeMs = new QLabel;
        m_switchTimeMs->setObjectName(QStringLiteral("help"));
        auto *row = new QHBoxLayout;
        row->addWidget(m_switchTime);
        row->addWidget(m_switchTimeMs, 1);
        row->addWidget(Info::button(QStringLiteral("output.time")));
        f->addRow(tr("Switch time"), row);
        m_params->addWidget(w);
    }
    { // swing
        auto *w = new QWidget;
        auto *f = new QFormLayout(w);
        m_swingPulse = makeSpin(0, 65535);
        m_swingPause = makeSpin(0, 65535);
        m_swingMs    = new QLabel;
        m_swingMs->setObjectName(QStringLiteral("help"));
        f->addRow(tr("LED on time"), m_swingPulse);
        f->addRow(tr("Pause"), m_swingPause);
        f->addRow(QString(), m_swingMs);
        m_params->addWidget(w);
    }
    { // servo
        auto *w = new QWidget;
        auto *f = new QFormLayout(w);
        auto sliderRow = [](QSpinBox *&spin, QSlider *&slider) {
            spin   = makeSpin(0, 180, QStringLiteral("°"));
            slider = new QSlider(Qt::Horizontal);
            slider->setRange(0, 180);
            QObject::connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
            QObject::connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), slider, &QSlider::setValue);
            auto *row = new QHBoxLayout;
            row->addWidget(slider, 1);
            row->addWidget(spin);
            return row;
        };
        auto *liveRow = new QHBoxLayout;
        m_live = new QCheckBox(tr("Live: move the servo while adjusting"));
        liveRow->addWidget(m_live);
        liveRow->addStretch();
        liveRow->addWidget(Info::button(QStringLiteral("output.servo")));
        f->addRow(liveRow);
        f->addRow(tr("Position 0 (curved)"),   sliderRow(m_servoMin, m_servoMinSlider));
        f->addRow(tr("Position 1 (straight)"), sliderRow(m_servoMax, m_servoMaxSlider));
        m_servoSpeed = makeSpin(1, 255, tr(" ms/°"));
        m_servoTime  = new QLabel;
        m_servoTime->setObjectName(QStringLiteral("help"));
        f->addRow(tr("Speed"), m_servoSpeed);
        f->addRow(QString(), m_servoTime);
        // semaphore arm: the board moves the servo itself when a detector reports its block occupied / free
        m_follow = new QCheckBox(tr("Follows a detector (semaphore arm)"));
        f->addRow(m_follow);
        m_followBox = new QWidget;
        auto *ff = new QFormLayout(m_followBox);
        ff->setContentsMargins(0, 0, 0, 0);
        m_followInput = new QComboBox;
        m_followInput->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_followInput->setMinimumContentsLength(14);
        ff->addRow(tr("Detector"), m_followInput);
        m_followSwap = new QCheckBox(tr("Swap: occupied = straight"));
        ff->addRow(m_followSwap);
        auto *followHint = new QLabel(tr("Occupied = position 0 (curved, arm at stop), free = position 1 (straight, clear), "
                                         "at the speed above."));
        followHint->setWordWrap(true);
        followHint->setObjectName(QStringLiteral("help"));
        ff->addRow(followHint);
        f->addRow(m_followBox);
        m_params->addWidget(w);
        connect(m_follow, &QCheckBox::toggled, this, [this] { if (!m_loading) collect(); });
        connect(m_followSwap, &QCheckBox::toggled, this, [this] { if (!m_loading) collect(); });
        connect(m_followInput, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { if (!m_loading) collect(); });
    }
    { // pwm
        auto *w = new QWidget;
        auto *f = new QFormLayout(w);
        m_pwm = makeSpin(0, 255);
        m_pwmSlider = new QSlider(Qt::Horizontal);
        m_pwmSlider->setRange(0, 255);
        connect(m_pwmSlider, &QSlider::valueChanged, m_pwm, &QSpinBox::setValue);
        connect(m_pwm, QOverload<int>::of(&QSpinBox::valueChanged), m_pwmSlider, &QSlider::setValue);
        auto *row = new QHBoxLayout;
        row->addWidget(m_pwmSlider, 1);
        row->addWidget(m_pwm);
        f->addRow(tr("Duty cycle"), row);
        // data[1]: seconds for a full 0 -> 255 change; every new brightness fades at that rate
        m_fade = makeSpin(0, 255, tr(" s"));
        m_fade->setSpecialValueText(tr("at once"));
        m_fade->setKeyboardTracking(false);
        m_fadeNote = new QLabel;
        m_fadeNote->setObjectName(QStringLiteral("help"));
        m_fadeNote->setWordWrap(true);
        auto *fadeRow = new QHBoxLayout;
        fadeRow->addWidget(m_fade);
        fadeRow->addWidget(m_fadeNote, 1);
        fadeRow->addWidget(Info::button(QStringLiteral("output.pwm")));
        f->addRow(tr("Fade time"), fadeRow);
        m_params->addWidget(w);
    }
    { // signal: blink frequency; the aspect itself comes from DCC extended or a route
        auto *w = new QWidget;
        auto *f = new QFormLayout(w);
        m_blinkHz = new QDoubleSpinBox;
        m_blinkHz->setRange(0.5, 10.0);
        m_blinkHz->setSingleStep(0.1);
        m_blinkHz->setDecimals(1);
        m_blinkHz->setSuffix(tr(" Hz"));
        m_blinkHz->setKeyboardTracking(false);
        f->addRow(tr("Flashing"), m_blinkHz);
        auto *hint = new QLabel(tr("How fast flashing lamps blink (1 Hz = once per second). The aspect itself is "
                                   "chosen with DCC extended or in a route."));
        hint->setWordWrap(true);
        hint->setObjectName(QStringLiteral("help"));
        f->addRow(hint);
        m_params->addWidget(w);
        connect(m_blinkHz, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this] { if (!m_loading) collect(); });
    }
    { // block signal: the board sets the aspect itself from two block detectors (inputs)
        auto *w = new QWidget;
        auto *f = new QFormLayout(w);
        m_blockOwn  = new QComboBox;
        m_blockNext = new QComboBox;
        for (QComboBox *c : { m_blockOwn, m_blockNext })
        {
            c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            c->setMinimumContentsLength(14);
        }
        auto *ownRow = new QHBoxLayout;
        ownRow->addWidget(m_blockOwn, 1);
        ownRow->addWidget(Info::button(QStringLiteral("output.blocksignal")));
        f->addRow(tr("Own block"), ownRow);
        f->addRow(tr("Next block"), m_blockNext);
        const QString aspectLabels[3] = { tr("Own block occupied"), tr("Next block occupied"), tr("Both free") };
        for (int k = 0; k < 3; ++k)
        {
            // the aspect picker of route steps / custom values: a value plus 'choose lamp by lamp'
            m_blockAspect[k] = new QSpinBox;
            m_blockAspect[k]->setRange(0, 255);
            m_blockAspect[k]->setPrefix(tr("aspect "));
            m_blockAspect[k]->setMinimumWidth(90);
            m_blockAspect[k]->setKeyboardTracking(false);
            auto *pick = new QToolButton;
            pick->setText(QStringLiteral("▾"));
            pick->setToolTip(tr("Choose the aspect lamp by lamp"));
            m_blockText[k] = new QLabel;
            m_blockText[k]->setObjectName(QStringLiteral("help"));
            m_blockText[k]->setWordWrap(true);
            auto *lay = new QHBoxLayout;
            lay->setContentsMargins(0, 0, 0, 0);
            lay->setSpacing(4);
            lay->addWidget(m_blockAspect[k]);
            lay->addWidget(pick);
            lay->addWidget(m_blockText[k], 1);
            m_blockRow[k] = new QWidget;
            m_blockRow[k]->setLayout(lay);
            f->addRow(aspectLabels[k], m_blockRow[k]);
            connect(pick, &QToolButton::clicked, this, [this, k] {
                const int v = Ui::pickAspect(this, m_blockAspect[k]->value(), m_index);
                if (v >= 0) m_blockAspect[k]->setValue(v);
            });
            connect(m_blockAspect[k], QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { if (!m_loading) collect(); });
        }
        auto *blockHint = new QLabel(tr("The board chooses the aspect itself every 50 ms: own block occupied first, then the "
                                        "next block, otherwise both free. Inputs, routes and DCC cannot set it. "
                                        "Flashing lamps flash at 1 Hz."));
        blockHint->setWordWrap(true);
        blockHint->setObjectName(QStringLiteral("help"));
        f->addRow(blockHint);
        m_loading = true;                    // the semaphore arm combo is connected already
        fillBlockCombos();
        m_loading = false;
        m_params->addWidget(w);
        for (QComboBox *c : { m_blockOwn, m_blockNext })
            connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { if (!m_loading) collect(); });
    }
    root->addWidget(m_paramBox);

    // State + test
    auto *stateBox = new QGroupBox(tr("State"));
    m_stateBox = stateBox;
    auto *stateInner = new QVBoxLayout(stateBox);
    auto *stateLay = new QHBoxLayout;
    auto *testLay  = new QHBoxLayout;
    stateInner->addLayout(stateLay);
    stateInner->addLayout(testLay);
    m_state = new QComboBox;
    m_state->addItem(tr("0  (off / curved)"), 0);
    m_state->addItem(tr("1  (on / straight)"), 1);
    m_test0 = new QPushButton(tr("▶ Test: curved / off"));
    m_test1 = new QPushButton(tr("▶ Test: straight / on"));
    m_test0->setObjectName(QStringLiteral("test"));
    m_test1->setObjectName(QStringLiteral("test"));
    const QString tip = tr("Writes this output to the board with the chosen state (also stored in EEPROM)");
    m_test0->setToolTip(tip);
    m_test1->setToolTip(tip);
    stateLay->addWidget(new QLabel(tr("Stored state")));
    stateLay->addWidget(m_state);
    stateLay->addStretch();
    stateLay->addWidget(Info::button(QStringLiteral("output.state")));
    testLay->addWidget(m_test0);
    testLay->addWidget(m_test1);
    testLay->addStretch();
    root->addWidget(stateBox);

    m_raw = new QLabel;
    m_raw->setObjectName(QStringLiteral("mono"));
    m_raw->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_raw);
    root->addStretch();

    refreshPinCombos();

    // Wiring
    connect(m_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (m_loading) return;
        // sensible defaults when the type changes
        const quint8 t = quint8(Ui::currentData(m_type));
        m_cfg.type = t;
        switch (t)
        {
        case SinglePulse:    m_cfg.setSwitchTime(5);  break;   // 500 ms
        case DoublePulse:    m_cfg.setSwitchTime(25); break;   // 250 ms
        case SingleFlashing:
        case DoubleFlashing: m_cfg.setSwitchTime(50); break;   // 500 ms
        case SwingLight:     m_cfg.setSwing(50, 50);  break;
        case ServoType:      m_cfg.data = { { 80, 100, 20, 0 } }; break;
        case Pwm:            m_cfg.data = { { 128, 0, 0, 0 } };   break;
        case FourToneSignal: m_cfg.data[2] = m_cfg.data[3] = 0;  // keep the aspect the board has
                             break;
        case BlockSignal:    // own block = the first sensor input, no next block; red C / yellow B / green A
            m_cfg.data = { { 0x04, 0x02, 0x01, 0 } };
            m_cfg.setBlockInputs(qMax(0, int(m_inputTypes.indexOf(quint8(Sensor)))), -1);
            break;
        case SoundEffect:    m_cfg.data = { { 0, 0, 0, 0 } };     // the input chooses the track
                             m_cfg.pinA = SoundPin;   break;
        default:             m_cfg.data = { { 0, 0, 0, 0 } };     break;
        }
        refresh();
        collect();
    });
    connect(m_frogMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (m_loading) return;
        m_frog = Frog(Ui::currentData(m_frogMode));
        collect();
    });
    auto onEdit = [this] { if (!m_loading) collect(); };
    for (auto *c : m_pins)   connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onEdit);
    for (auto *c : m_relays) connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onEdit);
    connect(m_state, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onEdit);
    for (auto *s : { m_switchTime, m_swingPulse, m_swingPause, m_servoMin, m_servoMax, m_servoSpeed, m_pwm, m_fade })
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged), this, onEdit);

    // live servo positions while adjusting: at most 4 per second, the newest angle always goes out
    m_liveTimer = new QTimer(this);
    m_liveTimer->setSingleShot(true);
    m_liveTimer->setInterval(250);
    connect(m_liveTimer, &QTimer::timeout, this, [this] {
        if (m_livePending < 0) return;
        emit liveServo(m_index, m_livePending);
        m_livePending = -1;
        m_liveTimer->start();
    });
    auto live = [this](int angle) {
        if (m_loading || !m_live->isChecked() || !m_live->isEnabled())
            return;
        if (m_liveTimer->isActive()) { m_livePending = angle; return; }
        emit liveServo(m_index, angle);
        m_liveTimer->start();
    };
    // Adjusting a position with Live on makes it the stored state as well: the board applies the
    // stored state after every settings write, so the servo stays at the angle being adjusted.
    auto follow = [this](int state) {
        if (m_loading || !m_live->isChecked() || !m_live->isEnabled() || m_cfg.state == state) return;
        m_state->setCurrentIndex(m_state->findData(state));
    };
    connect(m_servoMin, QOverload<int>::of(&QSpinBox::valueChanged), this, [follow, live](int a) { follow(0); live(a); });
    connect(m_servoMax, QOverload<int>::of(&QSpinBox::valueChanged), this, [follow, live](int a) { follow(1); live(a); });
    connect(m_servoMinSlider, &QSlider::sliderPressed, this, [this, live] { live(m_servoMin->value()); });
    connect(m_servoMaxSlider, &QSlider::sliderPressed, this, [this, live] { live(m_servoMax->value()); });

    connect(m_test0, &QPushButton::clicked, this, [this] { emit testRequested(m_index, 0); });
    connect(m_test1, &QPushButton::clicked, this, [this] { emit testRequested(m_index, 1); });
}

void OutputEditor::refreshPinCombos()
{
    m_loading = true;
    for (auto *c : m_pins)   Ui::fillPinCombo(c, true);
    for (auto *c : m_relays) Ui::fillPinCombo(c, true);
    m_loading = false;
    refresh();
    refreshPinMarks();
}

void OutputEditor::refreshPinMarks()
{
    for (auto *c : m_pins)   Ui::markUsedPins(c, true, m_index, Ui::pinShare(m_cfg));
    for (auto *c : m_relays) Ui::markUsedPins(c, true, m_index);
}

void OutputEditor::setOutput(int index, const OutputConfig &c)
{
    const bool other = (index != m_index) || (frogOf(c) != frogOf(m_cfg));
    m_index = index;
    m_cfg   = c;
    if (other || frogOf(c) != FrogNone)
        m_frog = frogOf(c);              // keep a chosen-but-empty frog mode while editing
    refresh();
}

void OutputEditor::setInputs(const QStringList &names, const QVector<quint8> &types)
{
    if (names == m_inputNames && types == m_inputTypes)
        return;
    m_inputNames = names;
    m_inputTypes = types;
    m_loading = true;
    fillBlockCombos();
    m_loading = false;
}

void OutputEditor::fillBlockCombos()
{
    const bool block = m_cfg.type == BlockSignal;
    m_blockOwn->clear();
    m_blockNext->clear();
    m_followInput->clear();
    m_blockNext->addItem(tr("— none"), -1);
    for (int i = 0; i < NumInputs; ++i)
    {
        const QIcon icon = Ui::swatchIcon(Ui::inputTypeColor(m_inputTypes.value(i, InputNotInUse)));
        for (QComboBox *c : { m_blockOwn, m_blockNext, m_followInput })
            c->addItem(icon, m_inputNames.value(i), i);
    }
    Ui::selectData(m_blockOwn,  block ? m_cfg.blockOwn() : 0);
    Ui::selectData(m_blockNext, block ? m_cfg.blockNext() : -1);
    Ui::selectData(m_followInput, qMax(0, m_cfg.servoFollows()));
}

void OutputEditor::setWarnings(const QStringList &warnings, const QString &note)
{
    m_warn->setText(warnings.join(QStringLiteral("\n")));
    m_warn->setVisible(!warnings.isEmpty());
    m_note->setText(note);
    m_note->setVisible(!note.isEmpty());
}

void OutputEditor::setConnected(bool connected)
{
    m_connected = connected;
    m_test0->setEnabled(connected);
    m_test1->setEnabled(connected);
    refresh();
}

void OutputEditor::setLiveAvailable(bool available)
{
    m_liveAvailable = available;
    refresh();
}

void OutputEditor::refresh()
{
    m_loading = true;
    const OutputTypeInfo &info = outputTypeInfo(m_cfg.type);

    m_title->setText(tr("Output %1").arg(m_index));
    Ui::selectData(m_type, m_cfg.type);
    m_help->setText(outputTypeHelp(m_cfg.type));

    for (int i = 0; i < 4; ++i) Ui::selectData(m_pins[i],   m_cfg.pin(i));
    for (int i = 0; i < 3; ++i) Ui::selectData(m_relays[i], m_cfg.pin(4 + i));
    Ui::selectData(m_frogMode, int(m_frog));
    const bool mixed = (m_cfg.relayA != PinNA || m_cfg.relayB != PinNA) && m_cfg.relayC != PinNA;
    m_frogNote->setText(tr("Both electrofrog and unifrog relays were set. Only one frog mode is allowed: "
                           "the unifrog relay is removed when you change this output."));
    m_frogNote->setVisible(mixed);

    m_switchTime->setValue(int(qMin<quint32>(m_cfg.switchTime(), 65535)));
    m_swingPulse->setValue(m_cfg.swingPulse());
    m_swingPause->setValue(m_cfg.swingPause());
    m_servoMin->setValue(m_cfg.servoMin());
    m_servoMax->setValue(m_cfg.servoMax());
    m_servoSpeed->setValue(qMax<int>(1, m_cfg.servoSpeed()));
    m_pwm->setValue(m_cfg.pwmValue());
    m_fade->setValue(m_cfg.data[1]);
    m_blinkHz->setValue(m_cfg.data[1] ? m_cfg.data[1] / 10.0 : 1.0);
    if (m_cfg.type == BlockSignal)
    {
        Ui::selectData(m_blockOwn,  m_cfg.blockOwn());
        Ui::selectData(m_blockNext, m_cfg.blockNext());
        for (int k = 0; k < 3; ++k)
            m_blockAspect[k]->setValue(m_cfg.data[k]);
    }
    m_follow->setChecked(m_cfg.servoFollows() >= 0);
    if (m_cfg.servoFollows() >= 0)
        Ui::selectData(m_followInput, m_cfg.servoFollows());
    m_followSwap->setChecked(m_cfg.servoFollowSwap());
    m_state->setCurrentIndex(m_cfg.state ? 1 : 0);

    const bool liveOk = m_connected && m_liveAvailable;
    m_live->setEnabled(liveOk);
    m_live->setToolTip(liveOk ? QString()
                              : tr("Needs a connected board"));
    if (!liveOk) m_live->setChecked(false);

    updateVisibility();
    updateTimeLabels();
    m_raw->setText(QStringLiteral("payload  %1")
                       .arg(Proto::toHex(encodeOutput(m_index, m_cfg))));
    m_loading = false;
}

void OutputEditor::updateVisibility()
{
    const OutputTypeInfo &info = outputTypeInfo(m_cfg.type);
    for (int i = 0; i < 4; ++i)
    {
        const bool used = i < info.pinCount;
        m_pins[i]->setVisible(used);
        if (QWidget *label = m_pinForm->labelForField(m_pins[i]))
            label->setVisible(used);
    }
    m_pins[0]->setEnabled(m_cfg.type != SoundEffect);   // the DFPlayer is always on TX (D1)
    m_soundInfo->setVisible(m_cfg.type == SoundEffect);
    m_pinBox->setVisible(info.pinCount > 0);
    m_pinBox->setTitle(info.needsPwm ? tr("Pins (PWM capable: D3 D5 D6 D9 D10 D11)") : tr("Pins"));

    m_relayBox->setVisible(info.relays);
    const bool showRelay[3] = { m_frog == FrogElectro, m_frog == FrogElectro, m_frog == FrogUni };
    for (int i = 0; i < 3; ++i)
    {
        m_relays[i]->setVisible(showRelay[i]);
        if (QWidget *label = m_relayForm->labelForField(m_relays[i]))
            label->setVisible(showRelay[i]);
    }

    const Page page = pageFor(info.params);
    m_params->setCurrentIndex(page);
    m_paramBox->setVisible(page != PageNone);
    m_followBox->setVisible(m_cfg.servoFollows() >= 0);
    for (int k = 0; k < 3; ++k)
        m_blockText[k]->setText(Ui::aspectText(m_cfg.data[k]));
    const bool next = m_cfg.blockNext() >= 0;  // without a next block that aspect is never shown
    m_blockRow[1]->setEnabled(next);
    if (auto *form = qobject_cast<QFormLayout *>(m_blockRow[1]->parentWidget()->layout()))
        if (QWidget *label = form->labelForField(m_blockRow[1]))
            label->setEnabled(next);
    m_stateBox->setVisible(m_cfg.type != BlockSignal);   // no stored state: the aspect is computed
}

void OutputEditor::updateTimeLabels()
{
    const OutputTypeInfo &info = outputTypeInfo(m_cfg.type);
    const int unit = qMax(1, info.timeUnitMs);
    m_switchTimeMs->setText(tr("× %1 ms  =  %2 ms").arg(unit).arg(qulonglong(m_cfg.switchTime()) * unit));
    m_swingMs->setText(tr("× %1 ms  →  on %2 ms, pause %3 ms")
                           .arg(unit).arg(m_cfg.swingPulse() * unit).arg(m_cfg.swingPause() * unit));
    const int sweep = qAbs(int(m_cfg.servoMax()) - int(m_cfg.servoMin())) * qMax<int>(1, m_cfg.servoSpeed());
    m_servoTime->setText(tr("Full sweep ≈ %1 ms").arg(sweep));
    const int fade = m_cfg.type == Pwm ? m_cfg.data[1] : 0;
    m_fadeNote->setText(fade ? tr("A full change (0 → 255) takes %1 s, a half change %2 s.").arg(fade).arg(fade / 2.0)
                             : tr("A new brightness is set at once."));
}

void OutputEditor::collect()
{
    const OutputTypeInfo &info = outputTypeInfo(m_cfg.type);

    // Unused pins are cleared: the firmware calls pinMode() on every pin != NA.
    for (int i = 0; i < 4; ++i)
        m_cfg.setPin(i, i < info.pinCount ? quint8(Ui::currentData(m_pins[i])) : PinNA);
    if (m_cfg.type == SoundEffect)
        m_cfg.pinA = SoundPin;

    // Frog interlocking: electrofrog (relay A + B) or unifrog (relay C), never both
    const bool relays = info.relays;
    m_cfg.relayA = (relays && m_frog == FrogElectro) ? quint8(Ui::currentData(m_relays[0])) : PinNA;
    m_cfg.relayB = (relays && m_frog == FrogElectro) ? quint8(Ui::currentData(m_relays[1])) : PinNA;
    m_cfg.relayC = (relays && m_frog == FrogUni)     ? quint8(Ui::currentData(m_relays[2])) : PinNA;

    switch (info.params)
    {
    case ParamKind::SwitchTime:
        m_cfg.setSwitchTime(quint32(m_switchTime->value()));
        break;
    case ParamKind::Swing:
        m_cfg.setSwing(quint16(m_swingPulse->value()), quint16(m_swingPause->value()));
        break;
    case ParamKind::Servo:
        m_cfg.data[0] = quint8(m_servoMin->value());
        m_cfg.data[1] = quint8(m_servoMax->value());
        m_cfg.data[2] = quint8(m_servoSpeed->value());
        if (!m_follow->isChecked())
            m_cfg.data[3] = 0;
        else
        {
            // data[3] = 0x80 | 0x40 (swap) | detector input; just switched on: the first sensor input
            int in = Ui::currentData(m_followInput);
            if (m_cfg.servoFollows() < 0)
            {
                in = qMax(0, int(m_inputTypes.indexOf(quint8(Sensor))));
                m_loading = true;
                Ui::selectData(m_followInput, in);
                m_loading = false;
            }
            m_cfg.data[3] = quint8(0x80 | (m_followSwap->isChecked() ? 0x40 : 0) | (in & 0x0F));
        }
        break;
    case ParamKind::PwmValue:
        m_cfg.data[0] = quint8(m_pwm->value());
        m_cfg.data[1] = quint8(m_fade->value());
        break;
    case ParamKind::FourTone:
        m_cfg.data[1] = quint8(qRound(m_blinkHz->value() * 10));   // data[0] (aspect) stays as it is
        break;
    case ParamKind::Block:
        for (int k = 0; k < 3; ++k)
            m_cfg.data[k] = quint8(m_blockAspect[k]->value());
        m_cfg.setBlockInputs(Ui::currentData(m_blockOwn), Ui::currentData(m_blockNext));
        break;
    default:
        break;
    }
    m_cfg.state = quint8(m_state->currentIndex());

    m_loading = true;
    m_frogNote->setVisible(false);
    updateVisibility();
    updateTimeLabels();
    m_raw->setText(QStringLiteral("payload  %1").arg(Proto::toHex(encodeOutput(m_index, m_cfg))));
    m_loading = false;

    emit changed(m_index, m_cfg);
}
