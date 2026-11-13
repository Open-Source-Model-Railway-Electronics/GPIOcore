#include "outputeditor.h"
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

enum Page { PageNone, PageTime, PageSwing, PageServo, PagePwm };

static Page pageFor(ParamKind k)
{
    switch (k)
    {
    case ParamKind::SwitchTime: return PageTime;
    case ParamKind::Swing:      return PageSwing;
    case ParamKind::Servo:      return PageServo;
    case ParamKind::PwmValue:   return PagePwm;
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
        m_params->addWidget(w);
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
        m_params->addWidget(w);
    }
    root->addWidget(m_paramBox);

    m_legacyNote = new QLabel(tr("This board runs old firmware (output opcode 0x2D, 13 bytes): "
                                 "the 4th data byte is not transferred. Update the firmware (review F1)."));
    m_legacyNote->setObjectName(QStringLiteral("warn"));
    m_legacyNote->setWordWrap(true);
    root->addWidget(m_legacyNote);

    // State + test
    auto *stateBox = new QGroupBox(tr("State"));
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
        case FourToneSignal: break;                              // keep the aspect the board has
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
    for (auto *s : { m_switchTime, m_swingPulse, m_swingPause, m_servoMin, m_servoMax, m_servoSpeed, m_pwm })
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged), this, onEdit);

    // live servo positions while adjusting
    auto live = [this](int angle) {
        if (!m_loading && m_live->isChecked() && m_live->isEnabled())
            emit liveServo(m_index, angle);
    };
    connect(m_servoMin, QOverload<int>::of(&QSpinBox::valueChanged), this, live);
    connect(m_servoMax, QOverload<int>::of(&QSpinBox::valueChanged), this, live);
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
    for (auto *c : m_pins)   Ui::markUsedPins(c, true, m_index);
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

void OutputEditor::setLegacyFormat(bool legacy)
{
    m_legacy = legacy;
    refresh();
}

void OutputEditor::setWarnings(const QStringList &warnings)
{
    m_warn->setText(warnings.join(QStringLiteral("\n")));
    m_warn->setVisible(!warnings.isEmpty());
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
    m_swingPause->setMaximum(m_legacy ? 255 : 65535);
    m_swingPause->setValue(m_cfg.swingPause());
    m_servoMin->setValue(m_cfg.servoMin());
    m_servoMax->setValue(m_cfg.servoMax());
    m_servoSpeed->setValue(qMax<int>(1, m_cfg.servoSpeed()));
    m_pwm->setValue(m_cfg.pwmValue());
    m_state->setCurrentIndex(m_cfg.state ? 1 : 0);

    const bool liveOk = m_connected && m_liveAvailable;
    m_live->setEnabled(liveOk);
    m_live->setToolTip(liveOk ? QString()
                              : tr("Needs a connected board that speaks protocol v2 (SERVO_LIVE)"));
    if (!liveOk) m_live->setChecked(false);

    updateVisibility();
    updateTimeLabels();
    m_raw->setText(QStringLiteral("payload  %1")
                       .arg(Proto::toHex(encodeOutput(m_index, m_cfg, m_legacy))));
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

    bool legacyMatters = false;
    if (m_legacy)
    {
        switch (info.params)
        {
        case ParamKind::Swing:      legacyMatters = true; break;
        case ParamKind::SwitchTime: legacyMatters = m_cfg.switchTime() > 0xFFFFFF; break;
        default: break;
        }
    }
    m_legacyNote->setVisible(legacyMatters);
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
}

void OutputEditor::collect()
{
    const OutputTypeInfo &info = outputTypeInfo(m_cfg.type);

    // Unused pins are cleared: the firmware calls pinMode() on every pin != NA.
    for (int i = 0; i < 4; ++i)
        m_cfg.setPin(i, i < info.pinCount ? quint8(Ui::currentData(m_pins[i])) : PinNA);

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
        // data[3] (servoPattern) is not used by the firmware and is kept as it is
        m_cfg.data[0] = quint8(m_servoMin->value());
        m_cfg.data[1] = quint8(m_servoMax->value());
        m_cfg.data[2] = quint8(m_servoSpeed->value());
        break;
    case ParamKind::PwmValue:
        m_cfg.data[0] = quint8(m_pwm->value());
        break;
    default:
        break;
    }
    m_cfg.state = quint8(m_state->currentIndex());

    m_loading = true;
    m_frogNote->setVisible(false);
    updateVisibility();
    updateTimeLabels();
    m_raw->setText(QStringLiteral("payload  %1").arg(Proto::toHex(encodeOutput(m_index, m_cfg, m_legacy))));
    m_loading = false;

    emit changed(m_index, m_cfg);
}
