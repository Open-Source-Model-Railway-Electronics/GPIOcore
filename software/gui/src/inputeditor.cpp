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

enum Function { FnDirect, FnRoute, FnUnused, FnRecorder, FnFeedback };
enum StepMode { ModeCurved, ModeStraight, ModeAngle };

static Function functionOf(quint8 type)
{
    if (isDirectType(type))    return FnDirect;
    if (isRouteType(type))     return FnRoute;
    if (type == Recorder)      return FnRecorder;
    if (type == FeedbackInput) return FnFeedback;
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
        m_outputNames << tr("Output %1").arg(i);
        m_outputTypes << SingleSteady;
    }
    buildUi();
    setInput(0, InputConfig());
}

void InputEditor::fillOutputCombo(QComboBox *combo)
{
    const int current = combo->count() ? Ui::currentData(combo) : -1;
    combo->clear();
    for (int i = 0; i < NumOutputs; ++i)
        combo->addItem(Ui::swatchIcon(Ui::outputTypeColor(outputType(i))),
                       tr("Out %1 · %2").arg(i).arg(outputTypeName(outputType(i))), i);
    if (current >= 0)
        Ui::selectData(combo, current);
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
    m_function->addItem(Ui::swatchIcon(Ui::inputTypeColor(InputNotInUse)), tr("Not in use"),     int(FnUnused));
    m_function->addItem(Ui::swatchIcon(Ui::inputTypeColor(Recorder)),      tr("Recorder"),       int(FnRecorder));
    m_function->addItem(Ui::swatchIcon(Ui::inputTypeColor(FeedbackInput)), tr("Feedback"),       int(FnFeedback));
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
    m_pin = new QComboBox;
    m_pin->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_pin->setMinimumContentsLength(10);
    srcForm->addRow(tr("Input pin"), withInfo(m_pin, QStringLiteral("input.pin")));
    root->addWidget(m_srcBox);

    // Direct target / feedback id
    m_targetBox = new QGroupBox(tr("Controls"));
    auto *tf = new QFormLayout(m_targetBox);
    m_output = new QComboBox;
    m_output->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_output->setMinimumContentsLength(10);
    m_targetLabel = new QLabel(tr("Output"));
    auto *targetRow = new QHBoxLayout;
    targetRow->addWidget(m_output, 1);
    m_infoFeedback = Info::button(QStringLiteral("input.feedback"));
    targetRow->addWidget(m_infoFeedback);
    tf->addRow(m_targetLabel, targetRow);
    root->addWidget(m_targetBox);

    // Recorder explanation
    m_recorderBox = new QGroupBox(tr("Recorder"));
    auto *recLay = new QHBoxLayout(m_recorderBox);
    auto *recText = new QLabel(tr("Long press: record on / off.  Short press: play on / off."));
    recText->setObjectName(QStringLiteral("help"));
    recText->setWordWrap(true);
    recLay->addWidget(recText, 1);
    recLay->addWidget(Info::button(QStringLiteral("input.recorder")));
    root->addWidget(m_recorderBox);

    // Route steps
    m_routeBox = new QGroupBox(tr("Route steps"));
    auto *routeLay = new QVBoxLayout(m_routeBox);
    auto *routeHead = new QHBoxLayout;
    m_routeCount = new QLabel;
    m_routeCount->setObjectName(QStringLiteral("help"));
    routeHead->addWidget(m_routeCount, 1);
    routeHead->addWidget(Info::button(QStringLiteral("route.steps")));
    routeLay->addLayout(routeHead);
    m_routeV1 = new QLabel(tr("This board speaks protocol v1: max. 8 steps, curved / straight only, "
                              "fixed 0.5 s per step."));
    m_routeV1->setObjectName(QStringLiteral("warn"));
    m_routeV1->setWordWrap(true);
    routeLay->addWidget(m_routeV1);
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
    connect(m_output, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onEdit);
    connect(m_addr,   QOverload<int>::of(&QSpinBox::valueChanged),         this, onEdit);
    connect(m_useAddr, &QCheckBox::toggled, this, onEdit);
    connect(m_addStep, &QPushButton::clicked, this, [this] {
        const int out = m_cfg.route.isEmpty() ? 0 : m_cfg.route.last().output;
        RouteStep s = defaultStep(out, outputType(out));
        if (m_limits.v1) { s.raw = false; s.value = 1; s.delay = V1StepDelay; }
        m_cfg.route.append(s);
        m_cfg.syncLinkedFromRoute();
        refresh();
        emit changed(m_index, m_cfg);
    });
}

void InputEditor::refreshPinCombos()
{
    m_loading = true;
    Ui::fillPinCombo(m_pin);
    m_loading = false;
    refresh();
    refreshPinMarks();
}

void InputEditor::refreshPinMarks()
{
    Ui::markUsedPins(m_pin, false, m_index);
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

void InputEditor::setRecorderTaken(bool taken)
{
    m_recorderTaken = taken;
    if (auto *model = qobject_cast<QStandardItemModel *>(m_function->model()))
    {
        QStandardItem *item = model->item(FnRecorder);
        item->setEnabled(!taken);
        item->setToolTip(taken ? tr("There already is a recorder input; only one is allowed") : QString());
    }
}

void InputEditor::setInput(int index, const InputConfig &c)
{
    m_index = index;
    m_cfg   = c;
    refresh();
}

void InputEditor::setWarnings(const QStringList &warnings)
{
    m_warn->setText(warnings.join(QStringLiteral("\n")));
    m_warn->setVisible(!warnings.isEmpty());
}

// ── Route step rows ─────────────────────────────────────────────────────────
QString InputEditor::stepSignature() const
{
    QStringList parts;
    parts << QString::number(m_limits.v1);
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
        for (int i = 0; i < NumOutputs; ++i)
            pick->addItem(Ui::swatchIcon(Ui::outputTypeColor(outputType(i))),
                          tr("Out %1 · %2").arg(i).arg(outputTypeName(outputType(i))), i);
        auto *hint = new QLabel(tr("or drag an output onto this input"));
        hint->setObjectName(QStringLiteral("help"));
        m_stepGrid->addWidget(number, 1, 0);
        m_stepGrid->addWidget(pick,   1, 1);
        m_stepGrid->addWidget(hint,   1, 2, 1, 5);
        connect(pick, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, pick] {
            const int out = pick->currentData().toInt();
            if (m_loading || out < 0) return;
            RouteStep s = defaultStep(out, outputType(out));
            if (m_limits.v1) { s.raw = false; s.value = 1; s.delay = V1StepDelay; }
            m_cfg.route.append(s);
            m_cfg.syncLinkedFromRoute();
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
            m_cfg.syncLinkedFromRoute(); refresh(); emit changed(m_index, m_cfg);
        });
        connect(r.down, &QToolButton::clicked, this, [this, i] {
            if (i + 1 >= m_cfg.route.size()) return;
            std::swap(m_cfg.route[i], m_cfg.route[i + 1]);
            m_cfg.syncLinkedFromRoute(); refresh(); emit changed(m_index, m_cfg);
        });
        connect(r.remove, &QToolButton::clicked, this, [this, i] {
            if (i >= m_cfg.route.size()) return;
            m_cfg.route.remove(i);
            m_cfg.syncLinkedFromRoute(); refresh(); emit changed(m_index, m_cfg);
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
        const StepKind kind = m_limits.v1 ? StepKind::State : stepKind(outputType(s.output));

        Ui::selectData(r.output, s.output);
        r.pick->setVisible(kind == StepKind::Aspect);
        r.value->setToolTip(QString());
        r.mode->clear();
        if (kind == StepKind::State || kind == StepKind::ServoStateOrAngle)
        {
            r.mode->addItem(tr("Curved (0)"),   int(ModeCurved));
            r.mode->addItem(tr("Straight (1)"), int(ModeStraight));
            if (kind == StepKind::ServoStateOrAngle)
                r.mode->addItem(tr("Angle"), int(ModeAngle));
            r.mode->setCurrentIndex(s.raw ? ModeAngle : (s.value ? ModeStraight : ModeCurved));
            r.mode->setVisible(true);
        }
        else
            r.mode->setVisible(false);

        switch (kind)
        {
        case StepKind::ServoStateOrAngle:
            r.value->setRange(ServoAngleMin, ServoAngleMax);
            r.value->setPrefix(QString());
            r.value->setSuffix(QStringLiteral("°"));
            r.value->setVisible(s.raw);
            r.value->setValue(s.raw ? s.value : (ServoAngleMin + ServoAngleMax) / 2);
            break;
        case StepKind::Aspect:
            r.value->setRange(0, 255);
            r.value->setPrefix(tr("aspect "));
            r.value->setSuffix(QString());
            r.value->setVisible(true);
            r.value->setValue(s.value);
            r.value->setToolTip(Ui::aspectText(s.value));
            break;
        case StepKind::PwmValue:
            r.value->setRange(0, 255);
            r.value->setPrefix(tr("PWM "));
            r.value->setSuffix(QString());
            r.value->setVisible(true);
            r.value->setValue(s.value);
            break;
        case StepKind::State:
            r.value->setVisible(false);
            break;
        }
        r.wait->setValue(s.delay / 10.0);
        r.wait->setEnabled(!m_limits.v1);
        r.wait->setVisible(m_timing);
        r.up->setEnabled(i > 0);
        r.down->setEnabled(i + 1 < n);
    }

    const int maxSteps = qMin(m_limits.maxSteps, m_limits.v1 ? MaxRouteStepsV1 : MaxRouteSteps);
    const int total = m_stepsElsewhere + n;
    m_routeCount->setText(tr("%1 / %2 steps  ·  board total %3 / %4")
                              .arg(n).arg(maxSteps).arg(total).arg(m_limits.pool));
    m_addStep->setEnabled(n < maxSteps && total < m_limits.pool);
    m_routeV1->setVisible(m_limits.v1);
}

void InputEditor::refresh()
{
    m_loading = true;
    const InputTypeInfo &info = inputTypeInfo(m_cfg.type);

    m_title->setText(tr("Input %1").arg(m_index));
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
    Ui::selectData(m_pin, m_cfg.pin);
    Ui::selectData(m_output, m_cfg.outputId);

    m_targetBox->setVisible(info.usesOutput);
    m_targetLabel->setText(m_cfg.type == FeedbackInput ? tr("Feedback id") : tr("Output"));
    m_infoFeedback->setVisible(m_cfg.type == FeedbackInput);
    m_recorderBox->setVisible(fn == FnRecorder);
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
    const bool a = m_optA->isChecked();
    Function fn = Function(Ui::currentData(m_function));
    if (fn == FnRecorder && m_recorderTaken)
        fn = functionOf(m_cfg.type);         // only one recorder per board
    switch (fn)
    {
    case FnDirect:   m_cfg.type = a ? Momentary : Latching;  break;
    case FnRoute:    m_cfg.type = quint8(Ui::currentData(m_routeTrigger));   break;
    case FnUnused:   m_cfg.type = InputNotInUse;             break;
    case FnRecorder: m_cfg.type = Recorder;                  break;
    case FnFeedback: m_cfg.type = FeedbackInput;             break;
    }
    m_cfg.address  = m_useAddr->isChecked() ? quint16(m_addr->value()) : AddrNone;
    m_cfg.pin      = quint8(Ui::currentData(m_pin));
    m_cfg.outputId = quint8(Ui::currentData(m_output) & 0x0F);

    for (int i = 0; i < m_rows.size() && i < m_cfg.route.size(); ++i)
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
            if (m_limits.v1) { s.raw = false; s.value = 1; }
            continue;
        }
        const StepKind kind = m_limits.v1 ? StepKind::State : stepKind(outputType(s.output));
        switch (kind)
        {
        case StepKind::State:
            s.raw = false;
            s.value = quint8(r.mode->currentIndex() == ModeStraight);
            break;
        case StepKind::ServoStateOrAngle:
            if (r.mode->currentIndex() == ModeAngle)
            {
                s.raw = true;
                s.value = quint8(r.value->value());
            }
            else
            {
                s.raw = false;
                s.value = quint8(r.mode->currentIndex() == ModeStraight);
            }
            break;
        case StepKind::Aspect:
        case StepKind::PwmValue:
            s.raw = true;
            s.value = quint8(r.value->value());
            break;
        }
        s.delay = m_limits.v1 ? V1StepDelay : quint16(std::lround(r.wait->value() * 10.0));
    }
    m_cfg.syncLinkedFromRoute();

    refresh();
    emit changed(m_index, m_cfg);
}

void InputEditor::setTimingEnabled(bool on)
{
    m_timing = on;
    setInput(m_index, m_cfg);
}
