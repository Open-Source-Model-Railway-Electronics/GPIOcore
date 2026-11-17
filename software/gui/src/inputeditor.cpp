#include "inputeditor.h"
#include "infotexts.h"
#include "protocol.h"
#include "uihelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

using namespace Gpio;

enum Function { FnDirect, FnRoute, FnUnused, FnSensor };
enum StepMode { ModeCurved, ModeStraight, ModeAngle };

static Function functionOf(quint8 type)
{
    if (isDirectType(type))    return FnDirect;
    if (isRouteType(type))     return FnRoute;
    if (type == Sensor)        return FnSensor;
    return FnUnused;
}

static QHBoxLayout *withInfo(QWidget *w, const QString &key)
{
    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(w, 1);
    row->addWidget(Info::button(key));
    return row;
}

InputEditor::InputEditor(QWidget *parent) : QWidget(parent)
{
    for (int i = 0; i < NumOutputs; ++i)
    {
        m_outputNames << tr("Output %1").arg(i + 1);
        m_outputTypes << SingleSteady;
    }
    buildUi();
    setInput(0, InputConfig());
}

void InputEditor::fillOutputCombo(QComboBox *combo)
{
    const int current = combo->count() ? Ui::currentData(combo) : -1;
    combo->clear();
    addOutputItems(combo);
    if (current >= 0)
        Ui::selectData(combo, current);
}

void InputEditor::addOutputItems(QComboBox *combo)
{
    auto *model = qobject_cast<QStandardItemModel *>(combo->model());
    for (int i = 0; i < NumOutputs; ++i)
    {
        combo->addItem(Ui::swatchIcon(Ui::outputTypeColor(outputType(i))),
                       tr("Out %1 · %2").arg(i + 1).arg(outputTypeName(outputType(i))), i);
        if (outputType(i) == BlockSignal && model)        // it sets its own aspect: not a target
        {
            model->item(combo->count() - 1)->setEnabled(false);
            combo->setItemData(combo->count() - 1, tr("A block signal sets its own aspect from its detectors"), Qt::ToolTipRole);
        }
    }
}

void InputEditor::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(8);

    auto *head = new QHBoxLayout;
    m_title = new QLabel;
    m_title->setObjectName(QStringLiteral("title"));
    m_function = new QComboBox;
    m_function->setMinimumWidth(130);
    m_function->addItem(Ui::swatchIcon(Ui::inputTypeColor(Latching)),      tr("Direct control"), int(FnDirect));
    m_function->addItem(Ui::swatchIcon(Ui::inputTypeColor(Route)),         tr("Route"),          int(FnRoute));
    m_function->addItem(Ui::swatchIcon(Ui::inputTypeColor(Sensor)),        inputTypeName(Sensor), int(FnSensor));
    m_function->addItem(Ui::swatchIcon(Ui::inputTypeColor(InputNotInUse)), tr("Not in use"),     int(FnUnused));
    head->addWidget(m_title);
    head->addStretch();
    head->addWidget(new QLabel(tr("Function")));
    head->addWidget(m_function);
    head->addWidget(Info::button(QStringLiteral("input.function")));
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

    // Behaviour of direct control: toggle / absolute. A route has no choice:
    // it starts on a button press (falling edge) or on any DCC command.
    m_modeBox = new QGroupBox(tr("Behaviour"));
    auto *modeLay = new QVBoxLayout(m_modeBox);
    auto *modeHead = new QHBoxLayout;
    modeHead->addStretch();
    m_infoDirect = Info::button(QStringLiteral("input.behaviour.direct"));
    modeHead->addWidget(m_infoDirect);
    modeLay->addLayout(modeHead);
    m_optA = new QRadioButton;
    m_optB = new QRadioButton;
    modeLay->addWidget(m_optA);
    modeLay->addWidget(m_optB);
    root->addWidget(m_modeBox);

    // Source: DCC address and/or physical pin
    m_srcBox = new QGroupBox(tr("Triggered by"));
    auto *srcForm = new QFormLayout(m_srcBox);
    m_useAddr = new QCheckBox(tr("DCC accessory address"));
    m_addr    = new QSpinBox;
    m_addr->setRange(1, 4094);
    m_addr->setMinimumWidth(90);
    m_addr->setKeyboardTracking(false);
    auto *addrRow = new QHBoxLayout;
    addrRow->addWidget(m_useAddr);
    addrRow->addWidget(m_addr);
    addrRow->addStretch();
    addrRow->addWidget(Info::button(QStringLiteral("input.dcc")));
    srcForm->addRow(tr("DCC"), addrRow);
    // routes: which DCC command starts the route (so two routes can share one address)
    m_routeTrigger = new QComboBox;
    m_routeTrigger->addItem(Ui::swatchIcon(QColor(0x8B, 0x5C, 0xF6)), tr("any command (straight and curved)"), int(Route));
    m_routeTrigger->addItem(Ui::swatchIcon(QColor(0x25, 0x63, 0xEB)), tr("straight only"), int(RouteStraight));
    m_routeTrigger->addItem(Ui::swatchIcon(QColor(0xDC, 0x26, 0x26)), tr("curved only"), int(RouteCurved));
    m_routeTrigger->setToolTip(tr("With 'straight only' and 'curved only' two routes can share one DCC address, "
                                  "e.g. a level crossing: curved = close, straight = open. A button always starts the route."));
    m_routeTriggerRow = new QWidget;
    QHBoxLayout *trig = withInfo(m_routeTrigger, QStringLiteral("route.steps"));
    trig->setContentsMargins(0, 0, 0, 0);
    m_routeTriggerRow->setLayout(trig);
    m_routeTriggerLabel = new QLabel(tr("Route starts on"));
    srcForm->addRow(m_routeTriggerLabel, m_routeTriggerRow);
    // digital (button to GND) or analog (a voltage compared with a threshold, e.g. a resistor ladder)
    m_readAs = new QComboBox;
    m_readAs->addItem(tr("Digital — button to GND"),                     int(AnalogNone));
    m_readAs->addItem(tr("Analog — below a value"),                      int(AnalogBelow));
    m_readAs->addItem(tr("Analog — above a value"),                      int(AnalogAbove));
    m_readAs->addItem(tr("Analog — at a value (resistor ladder)"),       int(AnalogAt));
    srcForm->addRow(tr("Read as"), withInfo(m_readAs, QStringLiteral("input.analog")));
    auto rowWidget = [](QLayout *lay) {
        auto *w = new QWidget;
        lay->setContentsMargins(0, 0, 0, 0);
        w->setLayout(lay);
        return w;
    };
    m_pin = new QComboBox;
    m_pin->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_pin->setMinimumContentsLength(10);
    m_pinRow = rowWidget(withInfo(m_pin, QStringLiteral("input.pin")));
    srcForm->addRow(tr("Input pin"), m_pinRow);
    m_channel = new QComboBox;
    m_channel->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_channel->setMinimumContentsLength(10);
    m_channelRow = rowWidget(withInfo(m_channel, QStringLiteral("input.analog")));
    srcForm->addRow(tr("Analog pin"), m_channelRow);
    m_threshold = new QSpinBox;
    m_threshold->setRange(0, AnalogMaxThreshold);
    m_threshold->setSingleStep(8);
    m_threshold->setMinimumWidth(90);
    m_threshold->setKeyboardTracking(false);
    m_threshold->setToolTip(tr("0 = 0 V, 1023 = 5 V. Stored in steps of 8."));
    m_range = new QLabel;
    m_range->setObjectName(QStringLiteral("help"));
    m_range->setWordWrap(true);
    auto *thrLay = new QHBoxLayout;
    thrLay->addWidget(m_threshold);
    thrLay->addWidget(m_range, 1);
    m_thresholdRow = rowWidget(thrLay);
    srcForm->addRow(tr("Threshold"), m_thresholdRow);
    m_now = new QLabel;
    m_useValue = new QPushButton(tr("Use this value"));
    m_useValue->setToolTip(tr("Take the reading as the threshold (rounded to a step of 8)"));
    auto *nowLay = new QHBoxLayout;
    nowLay->addWidget(m_now, 1);
    nowLay->addWidget(m_useValue);
    m_nowRow = rowWidget(nowLay);
    srcForm->addRow(tr("Live reading"), m_nowRow);
    root->addWidget(m_srcBox);

    // Direct target
    m_targetBox = new QGroupBox(tr("Controls"));
    auto *tf = new QFormLayout(m_targetBox);
    m_output = new QComboBox;
    m_output->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_output->setMinimumContentsLength(10);
    tf->addRow(tr("Output"), m_output);
    // custom values: per state a value like a route step (angle, aspect, PWM, sound track) instead of 0 / 1
    m_custom = new QCheckBox(tr("Custom values"));
    m_custom->setToolTip(tr("Off / curved and on / straight each send their own value, like a route step"));
    auto *customLay = new QHBoxLayout;
    customLay->setContentsMargins(0, 0, 0, 0);
    customLay->addWidget(m_custom, 1);
    customLay->addWidget(Info::button(QStringLiteral("input.values")));
    m_customRow = new QWidget;
    m_customRow->setLayout(customLay);
    tf->addRow(m_customRow);
    const QString valueLabels[2] = { tr("Off / curved"), tr("On / straight") };
    for (int k = 0; k < 2; ++k)
    {
        m_valueMode[k] = new QComboBox;
        m_valueMode[k]->setSizeAdjustPolicy(QComboBox::AdjustToContents);   // filled per output type later
        m_value[k] = new QSpinBox;
        m_value[k]->setMinimumWidth(80);
        m_value[k]->setKeyboardTracking(false);
        m_valuePick[k] = new QToolButton;
        m_valuePick[k]->setText(QStringLiteral("▾"));
        m_valuePick[k]->setToolTip(tr("Choose the aspect lamp by lamp"));
        auto *lay = new QHBoxLayout;
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(4);
        lay->addWidget(m_valueMode[k]);
        lay->addWidget(m_value[k]);
        lay->addWidget(m_valuePick[k]);
        lay->addStretch();
        m_valueRow[k] = new QWidget;
        m_valueRow[k]->setLayout(lay);
        tf->addRow(valueLabels[k], m_valueRow[k]);
        connect(m_valuePick[k], &QToolButton::clicked, this, [this, k] {
            const int v = Ui::pickAspect(this, m_value[k]->value(), m_cfg.outputId);
            if (v >= 0) m_value[k]->setValue(v);
        });
    }
    root->addWidget(m_targetBox);

    // Route steps
    m_routeBox = new QGroupBox(tr("Route steps"));
    auto *routeLay = new QVBoxLayout(m_routeBox);
    auto *routeHead = new QHBoxLayout;
    m_routeCount = new QLabel;
    m_routeCount->setObjectName(QStringLiteral("help"));
    routeHead->addWidget(m_routeCount, 1);
    routeHead->addWidget(Info::button(QStringLiteral("route.steps")));
    routeLay->addLayout(routeHead);
    m_stepGrid = new QGridLayout;
    m_stepGrid->setHorizontalSpacing(6);
    m_stepGrid->setVerticalSpacing(4);
    routeLay->addLayout(m_stepGrid);
    m_addStep = new QPushButton(tr("+ Add step"));
    m_addStep->setToolTip(tr("Or drag an output onto this input in the canvas"));
    auto *addRow = new QHBoxLayout;
    addRow->addWidget(m_addStep);
    addRow->addStretch();
    routeLay->addLayout(addRow);
    root->addWidget(m_routeBox);

    m_raw = new QLabel;
    m_raw->setObjectName(QStringLiteral("mono"));
    m_raw->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_raw);
    root->addStretch();

    refreshPinCombos();
    fillOutputCombo(m_output);

    auto onEdit = [this] { if (!m_loading) collect(); };
    connect(m_function, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onEdit);
    connect(m_routeTrigger, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onEdit);
    connect(m_optA, &QRadioButton::toggled, this, [onEdit](bool on) { if (on) onEdit(); });
    connect(m_optB, &QRadioButton::toggled, this, [onEdit](bool on) { if (on) onEdit(); });
    connect(m_pin,    QOverload<int>::of(&QComboBox::currentIndexChanged), this, onEdit);
    connect(m_readAs, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onEdit);
    connect(m_channel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onEdit);
    connect(m_threshold, QOverload<int>::of(&QSpinBox::valueChanged),    this, onEdit);
    connect(m_useValue, &QPushButton::clicked, this, [this] {
        if (m_nowValue >= 0)
            m_threshold->setValue(InputConfig::roundThreshold(m_nowValue));
    });
    connect(m_output, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onEdit);
    connect(m_addr,   QOverload<int>::of(&QSpinBox::valueChanged),         this, onEdit);
    connect(m_useAddr, &QCheckBox::toggled, this, onEdit);
    connect(m_custom,  &QCheckBox::toggled, this, onEdit);
    for (int k = 0; k < 2; ++k)
    {
        connect(m_valueMode[k], QOverload<int>::of(&QComboBox::currentIndexChanged), this, onEdit);
        connect(m_value[k],     QOverload<int>::of(&QSpinBox::valueChanged),         this, onEdit);
    }
    connect(m_addStep, &QPushButton::clicked, this, [this] {
        const int out = m_cfg.route.isEmpty() ? 0 : m_cfg.route.last().output;
        m_cfg.route.append(defaultStep(out, outputType(out)));
        refresh();
        emit changed(m_index, m_cfg);
    });
}

void InputEditor::refreshPinCombos()
{
    m_loading = true;
    Ui::fillPinCombo(m_pin);
    Ui::fillPinCombo(m_channel);          // the same pins and rules, only A0-A7 that exist here
    for (int i = m_channel->count() - 1; i >= 0; --i)
    {
        const int pin = m_channel->itemData(i).toInt();
        if (pin < 14 || pin >= NumPins)
            m_channel->removeItem(i);
    }
    m_loading = false;
    refresh();
    refreshPinMarks();
}

void InputEditor::refreshPinMarks()
{
    Ui::markUsedPins(m_pin, false, m_index);
    Ui::markUsedPins(m_channel, false, m_index, Ui::ShareAnalog);   // other analog inputs may share a channel
}

void InputEditor::setAnalogReading(int channel, int value)
{
    if (!m_cfg.isAnalog() || channel != m_cfg.analogChannel())
        return;
    m_nowValue = value;
    updateAnalog();
}

void InputEditor::setLiveReadingAvailable(bool available)
{
    if (available == m_liveReading)
        return;
    m_liveReading = available;
    if (!available)
        m_nowValue = -1;
    updateAnalog();
}

void InputEditor::updateAnalog()
{
    const bool analog = m_cfg.isAnalog();
    for (QWidget *w : { m_pinRow, m_channelRow, m_thresholdRow, m_nowRow })
    {
        const bool show = (w == m_pinRow) ? !analog : analog;
        w->setVisible(show);
        if (auto *form = qobject_cast<QFormLayout *>(m_srcBox->layout()))
            if (QWidget *label = form->labelForField(w))
                label->setVisible(show);
    }
    if (!analog)
        return;
    const int t = m_cfg.threshold;
    switch (m_cfg.analog)
    {
    case AnalogBelow:
        m_range->setText(tr("Pressed below %1, released again from %2.").arg(t).arg(t + AnalogHysteresis));
        break;
    case AnalogAbove:
        m_range->setText(tr("Pressed above %1, released again at %2 or lower.").arg(t).arg(qMax(0, t - AnalogHysteresis)));
        break;
    default:
        m_range->setText(tr("Pressed from %1 to %2.").arg(qMax(0, t - AnalogWindow)).arg(qMin(AnalogMaxValue, t + AnalogWindow)));
        break;
    }
    if (!m_liveReading)
    {
        m_now->setText(tr("Now: —"));
        m_now->setToolTip(tr("Needs a board with analog input support (connected or simulated)."));
    }
    else
    {
        m_now->setText(m_nowValue >= 0 ? tr("Now: %1").arg(m_nowValue) : tr("Now: …"));
        m_now->setToolTip(QString());
    }
    m_useValue->setEnabled(m_liveReading && m_nowValue >= 0);
}

void InputEditor::setOutputs(const QStringList &names, const QVector<quint8> &types)
{
    m_outputNames = names;
    m_outputTypes = types;
    m_loading = true;
    fillOutputCombo(m_output);
    for (const StepRow &r : m_rows)
        fillOutputCombo(r.output);
    m_loading = false;
    refresh();
}

void InputEditor::setRouteLimits(const RouteLimits &limits, int stepsUsedElsewhere)
{
    m_limits = limits;
    m_stepsElsewhere = stepsUsedElsewhere;
    refresh();
}

void InputEditor::setInput(int index, const InputConfig &c)
{
    m_index = index;
    if (index != m_index || c.analog != m_cfg.analog || (c.isAnalog() && c.pin != m_cfg.pin))
        m_nowValue = -1;                 // another channel: wait for its reading
    m_cfg   = c;
    refresh();
}

void InputEditor::setWarnings(const QStringList &warnings, const QString &note)
{
    m_warn->setText(warnings.join(QStringLiteral("\n")));
    m_warn->setVisible(!warnings.isEmpty());
    m_note->setText(note);
    m_note->setVisible(!note.isEmpty());
}

// ── Route step rows ─────────────────────────────────────────────────────────
QString InputEditor::stepSignature() const
{
    QStringList parts;
    for (const RouteStep &s : m_cfg.route)
        parts << QStringLiteral("%1:%2").arg(s.output).arg(int(stepKind(outputType(s.output))));
    return parts.join(QLatin1Char(','));
}

void InputEditor::rebuildSteps()
{
    // remove old rows
    while (QLayoutItem *item = m_stepGrid->takeAt(0))
    {
        if (QWidget *w = item->widget())
        {
            w->hide();
            w->deleteLater();            // a row button may be the sender of this rebuild
        }
        delete item;
    }
    m_rows.clear();

    const QString heads[4] = { tr("#"), tr("Output"), tr("Value"), tr("Wait after") };
    for (int c = 0; c < 4; ++c)
    {
        auto *h = new QLabel(heads[c]);
        h->setObjectName(QStringLiteral("help"));
        m_stepGrid->addWidget(h, 0, c);
    }

    if (m_cfg.route.isEmpty())
    {
        // Always one row open: choosing an output here creates step 1
        auto *number = new QLabel(QStringLiteral("1"));
        auto *pick = new QComboBox;
        pick->setObjectName(QStringLiteral("firstStep"));
        pick->addItem(tr("— choose an output —"), -1);
        addOutputItems(pick);
        auto *hint = new QLabel(tr("or drag an output onto this input"));
        hint->setObjectName(QStringLiteral("help"));
        m_stepGrid->addWidget(number, 1, 0);
        m_stepGrid->addWidget(pick,   1, 1);
        m_stepGrid->addWidget(hint,   1, 2, 1, 5);
        connect(pick, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, pick] {
            const int out = pick->currentData().toInt();
            if (m_loading || out < 0) return;
            m_cfg.route.append(defaultStep(out, outputType(out)));
            refresh();
            emit changed(m_index, m_cfg);
        });
        return;
    }

    for (int i = 0; i < m_cfg.route.size(); ++i)
    {
        StepRow r;
        r.number = new QLabel(QString::number(i + 1));
        r.output = new QComboBox;
        fillOutputCombo(r.output);
        r.output->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        r.output->setMinimumContentsLength(9);
        r.mode   = new QComboBox;
        r.mode->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        r.mode->setMinimumContentsLength(7);
        r.value  = new QSpinBox;
        r.value->setMinimumWidth(70);
        r.value->setKeyboardTracking(false);
        r.wait   = new QDoubleSpinBox;
        r.wait->setRange(0.0, MaxStepDelay / 10.0);
        r.wait->setDecimals(1);
        r.wait->setSingleStep(0.1);
        r.wait->setSuffix(tr(" s"));
        r.wait->setMinimumWidth(80);
        r.wait->setKeyboardTracking(false);
        r.up     = new QToolButton; r.up->setText(QStringLiteral("▲"));     r.up->setToolTip(tr("Earlier"));
        r.down   = new QToolButton; r.down->setText(QStringLiteral("▼"));   r.down->setToolTip(tr("Later"));
        r.remove = new QToolButton;
        r.remove->setObjectName(QStringLiteral("stepDelete"));
        r.remove->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
        r.remove->setToolTip(tr("Delete step %1").arg(i + 1));

        auto *valueBox = new QWidget;
        auto *vl = new QHBoxLayout(valueBox);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(4);
        vl->addWidget(r.mode);
        vl->addWidget(r.value);
        r.pick = new QToolButton;
        r.pick->setText(QStringLiteral("▾"));
        r.pick->setToolTip(tr("Choose the aspect lamp by lamp"));
        vl->addWidget(r.pick);
        connect(r.pick, &QToolButton::clicked, this, [this, i] {
            if (i >= m_cfg.route.size() || i >= m_rows.size()) return;
            const int v = Ui::pickAspect(this, m_rows[i].value->value(), m_cfg.route[i].output);
            if (v >= 0) m_rows[i].value->setValue(v);
        });

        const int row = i + 1;
        m_stepGrid->addWidget(r.number, row, 0);
        m_stepGrid->addWidget(r.output, row, 1);
        m_stepGrid->addWidget(valueBox, row, 2);
        m_stepGrid->addWidget(r.wait,   row, 3);
        m_stepGrid->addWidget(r.up,     row, 4);
        m_stepGrid->addWidget(r.down,   row, 5);
        m_stepGrid->addWidget(r.remove, row, 6);
        m_rows.append(r);

        auto onEdit = [this] { if (!m_loading) collect(); };
        connect(r.output, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onEdit);
        connect(r.mode,   QOverload<int>::of(&QComboBox::currentIndexChanged), this, onEdit);
        connect(r.value,  QOverload<int>::of(&QSpinBox::valueChanged),         this, onEdit);
        connect(r.wait,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onEdit);
        connect(r.up, &QToolButton::clicked, this, [this, i] {
            if (i == 0) return;
            std::swap(m_cfg.route[i], m_cfg.route[i - 1]);
            refresh(); emit changed(m_index, m_cfg);
        });
        connect(r.down, &QToolButton::clicked, this, [this, i] {
            if (i + 1 >= m_cfg.route.size()) return;
            std::swap(m_cfg.route[i], m_cfg.route[i + 1]);
            refresh(); emit changed(m_index, m_cfg);
        });
        connect(r.remove, &QToolButton::clicked, this, [this, i] {
            if (i >= m_cfg.route.size()) return;
            m_cfg.route.remove(i);
            refresh(); emit changed(m_index, m_cfg);
        });
    }
    m_stepGrid->setColumnStretch(1, 3);
    m_stepGrid->setColumnStretch(2, 2);
}

void InputEditor::updateSteps()
{
    const int n = m_cfg.route.size();
    for (int i = 0; i < n && i < m_rows.size(); ++i)
    {
        const RouteStep &s = m_cfg.route[i];
        StepRow &r = m_rows[i];
        const StepKind kind = stepKind(outputType(s.output));

        Ui::selectData(r.output, s.output);
        showStepValue(r.mode, r.value, r.pick, s, kind);
        r.wait->setValue(s.delay / 10.0);
        r.wait->setVisible(m_timing);
        r.up->setEnabled(i > 0);
        r.down->setEnabled(i + 1 < n);
    }

    const int maxSteps = qMin(m_limits.maxSteps, MaxRouteSteps);
    const int total = m_stepsElsewhere + n;
    m_routeCount->setText(tr("%1 / %2 steps  ·  board total %3 / %4")
                              .arg(n).arg(maxSteps).arg(total).arg(m_limits.pool));
    m_addStep->setEnabled(n < maxSteps && total < m_limits.pool);
}

void InputEditor::showStepValue(QComboBox *mode, QSpinBox *value, QToolButton *pick, const RouteStep &s, StepKind kind)
{
    pick->setVisible(kind == StepKind::Aspect);
    value->setToolTip(QString());
    value->setSpecialValueText(QString());
    mode->clear();
    if (kind == StepKind::State || kind == StepKind::ServoStateOrAngle)
    {
        mode->addItem(tr("Curved (0)"),   int(ModeCurved));
        mode->addItem(tr("Straight (1)"), int(ModeStraight));
        if (kind == StepKind::ServoStateOrAngle)
            mode->addItem(tr("Angle"), int(ModeAngle));
        mode->setCurrentIndex(s.raw ? ModeAngle : (s.value ? ModeStraight : ModeCurved));
        mode->setVisible(true);
    }
    else
        mode->setVisible(false);

    switch (kind)
    {
    case StepKind::ServoStateOrAngle:
        value->setRange(ServoAngleMin, ServoAngleMax);
        value->setPrefix(QString());
        value->setSuffix(QStringLiteral("°"));
        value->setVisible(s.raw);
        value->setValue(s.raw ? s.value : (ServoAngleMin + ServoAngleMax) / 2);
        break;
    case StepKind::Aspect:
        value->setRange(0, 255);
        value->setPrefix(tr("aspect "));
        value->setSuffix(QString());
        value->setVisible(true);
        value->setValue(s.value);
        value->setToolTip(Ui::aspectText(s.value));
        break;
    case StepKind::PwmValue:
        value->setRange(0, 255);
        value->setPrefix(tr("PWM "));
        value->setSuffix(QString());
        value->setVisible(true);
        value->setValue(s.value);
        break;
    case StepKind::Track:
        value->setRange(0, 255);
        value->setPrefix(tr("track "));
        value->setSuffix(QString());
        value->setSpecialValueText(tr("stop"));
        value->setToolTip(tr("MP3 track 1-255 (0001.mp3 …), 0 = stop"));
        value->setVisible(true);
        value->setValue(s.value);
        break;
    case StepKind::State:
        value->setVisible(false);
        break;
    }
}

void InputEditor::readStepValue(const QComboBox *mode, const QSpinBox *value, StepKind kind, RouteStep &s)
{
    switch (kind)
    {
    case StepKind::State:
        s.raw = false;
        s.value = quint8(mode->currentIndex() == ModeStraight);
        break;
    case StepKind::ServoStateOrAngle:
        if (mode->currentIndex() == ModeAngle)
        {
            s.raw = true;
            s.value = quint8(value->value());
        }
        else
        {
            s.raw = false;
            s.value = quint8(mode->currentIndex() == ModeStraight);
        }
        break;
    case StepKind::Aspect:
    case StepKind::PwmValue:
    case StepKind::Track:
        s.raw = true;
        s.value = quint8(value->value());
        break;
    }
}

void InputEditor::refresh()
{
    m_loading = true;
    const InputTypeInfo &info = inputTypeInfo(m_cfg.type);

    m_title->setText(tr("Input %1").arg(m_index + 1));
    const Function fn = functionOf(m_cfg.type);
    Ui::selectData(m_function, int(fn));
    Ui::selectData(m_routeTrigger, isRouteType(m_cfg.type) ? int(m_cfg.type) : int(Route));
    m_help->setText(inputTypeHelp(m_cfg.type));

    m_modeBox->setVisible(fn == FnDirect);
    if (fn == FnDirect)
    {
        m_optA->setText(tr("Toggle — every press switches the output over"));
        m_optB->setText(tr("Absolute — the output follows the input"));
        (m_cfg.type == Momentary ? m_optA : m_optB)->setChecked(true);
    }

    const bool hasAddr = m_cfg.address != AddrNone && m_cfg.address != 0;
    m_useAddr->setChecked(hasAddr);
    m_addr->setValue(hasAddr ? m_cfg.address : qMax(1, m_index + 1));
    m_addr->setEnabled(hasAddr);
    m_useAddr->setEnabled(info.usesAddress);
    Ui::selectData(m_readAs, m_cfg.analog);
    if (m_cfg.isAnalog())
        Ui::selectData(m_channel, m_cfg.pin);
    else
        Ui::selectData(m_pin, m_cfg.pin);
    m_threshold->setValue(m_cfg.threshold);
    updateAnalog();
    Ui::selectData(m_output, m_cfg.outputId);

    m_targetBox->setVisible(info.usesOutput);
    const bool values = m_cfg.hasCustomValues();
    m_customRow->setVisible(fn == FnDirect);
    m_custom->setChecked(values);
    for (int k = 0; k < 2; ++k)
    {
        const bool show = fn == FnDirect && values;
        m_valueRow[k]->setVisible(show);
        if (auto *form = qobject_cast<QFormLayout *>(m_targetBox->layout()))
            if (QWidget *label = form->labelForField(m_valueRow[k]))
                label->setVisible(show);
        if (show)
            showStepValue(m_valueMode[k], m_value[k], m_valuePick[k], m_cfg.route[k], stepKind(outputType(m_cfg.outputId)));
    }
    m_routeBox->setVisible(fn == FnRoute);
    m_routeTriggerRow->setVisible(fn == FnRoute);
    m_routeTriggerLabel->setVisible(fn == FnRoute);
    m_routeTrigger->setEnabled(m_useAddr->isChecked() || m_cfg.address != AddrNone);

    const QString sig = stepSignature();
    if (sig != m_signature || m_rows.size() != m_cfg.route.size() || m_cfg.route.isEmpty())
    {
        m_signature = sig;
        rebuildSteps();
    }
    updateSteps();

    m_raw->setText(QStringLiteral("payload  %1").arg(Proto::toHex(encodeInput(m_index, m_cfg))));
    m_loading = false;
}

void InputEditor::collect()
{
    const quint8 oldType = m_cfg.type, oldOutput = m_cfg.outputId;
    const bool hadValues = m_cfg.hasCustomValues();
    const bool a = m_optA->isChecked();
    const Function fn = Function(Ui::currentData(m_function));
    switch (fn)
    {
    case FnDirect:   m_cfg.type = a ? Momentary : Latching;  break;
    case FnRoute:    m_cfg.type = quint8(Ui::currentData(m_routeTrigger));   break;
    case FnUnused:   m_cfg.type = InputNotInUse;             break;
    case FnSensor:   m_cfg.type = Sensor;                    break;
    }
    m_cfg.address  = m_useAddr->isChecked() ? quint16(m_addr->value()) : AddrNone;
    const quint8 analog = quint8(Ui::currentData(m_readAs));
    if (analog != m_cfg.analog)
    {
        // digital <-> analog: keep the pin when it can be both, otherwise start on A0
        if (analog != AnalogNone && (m_cfg.pin < 14 || m_cfg.pin >= NumPins))
            m_cfg.pin = 14;
        m_cfg.analog = analog;
    }
    else
        m_cfg.pin = quint8(Ui::currentData(analog != AnalogNone ? m_channel : m_pin));
    m_cfg.threshold = quint16(InputConfig::roundThreshold(m_threshold->value()));
    m_cfg.outputId = quint8(Ui::currentData(m_output) & 0x0F);

    for (int i = 0; fn == FnRoute && i < m_rows.size() && i < m_cfg.route.size(); ++i)
    {
        const StepRow &r = m_rows[i];
        RouteStep &s = m_cfg.route[i];
        const int out = Ui::currentData(r.output);
        if (out != s.output)
        {
            // another output: start with the default value for its type, keep the wait
            const quint16 delay = s.delay;
            s = defaultStep(out, outputType(out));
            s.delay = delay;
            continue;
        }
        readStepValue(r.mode, r.value, stepKind(outputType(s.output)), s);
        s.delay = quint16(std::lround(r.wait->value() * 10.0));
    }
    if (fn == FnDirect)
    {
        const quint8 t = outputType(m_cfg.outputId);
        const bool newTarget = oldOutput != m_cfg.outputId || !isDirectType(oldType);
        if (!m_custom->isChecked() && hadValues && isDirectType(oldType))
            m_cfg.route.clear();                                      // switched off
        else if (m_custom->isChecked() && !hadValues)
            m_cfg.setCustomValues(true, t);                           // switched on: defaults for the output
        else if (hadValues && stepKind(t) != stepKind(outputType(oldOutput)))
            m_cfg.setCustomValues(true, t);                           // another kind of output: its defaults
        else if (hadValues)
            for (int k = 0; k < 2; ++k)
                readStepValue(m_valueMode[k], m_value[k], stepKind(t), m_cfg.route[k]);
        if (t == SoundEffect && newTarget && !m_cfg.hasCustomValues())
            m_cfg.setCustomValues(true, t);                           // a sound output plays the input's tracks
    }
    m_cfg.syncCustomValues();
    if (fn == FnSensor)
    {
        m_cfg.address = AddrNone;                                     // a sensor has no DCC address
        m_cfg.route.clear();                                          // nor a route
    }

    refresh();
    emit changed(m_index, m_cfg);
}

void InputEditor::setTimingEnabled(bool on)
{
    m_timing = on;
    setInput(m_index, m_cfg);
}
