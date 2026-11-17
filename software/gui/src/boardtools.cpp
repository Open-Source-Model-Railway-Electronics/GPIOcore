#include "boardtools.h"
#include "connections.h"

#include <QCoreApplication>

namespace Gpio {

static QString tr(const char *text) { return QCoreApplication::translate("Gpio", text); }

// ── Partial export / import ─────────────────────────────────────────────────
BoardConfig exportPart(const BoardConfig &board, int sections)
{
    BoardConfig e;                                   // every slot 'not in use'
    if (sections & PartBoard)
        e.board = board.board;
    e.board.type = board.board.type;                 // the pins only mean something with the board type
    for (int o = 0; o < NumOutputs; ++o)
        if ((sections & PartOutputs) && board.outputs[o].type != OutputNotInUse)
            e.outputs[o] = board.outputs[o];
    for (int i = 0; i < NumInputs; ++i)
        if ((sections & PartInputs) && board.inputs[i].type != InputNotInUse)
            e.inputs[i] = board.inputs[i];
    return e;
}

bool isFreeOutput(const BoardConfig &board, int output)
{
    return board.outputs[output].type == OutputNotInUse && outputSources(board, output).isEmpty();
}

bool isFreeInput(const BoardConfig &board, int input)
{
    return board.inputs[input].type == InputNotInUse;
}

ImportResult planImport(const BoardConfig &board, const BoardConfig &file, int sections, bool sameNumbers)
{
    ImportResult r;
    r.outputs.fill(-1, NumOutputs);
    r.inputs.fill(-1, NumInputs);
    int needOut = 0, needIn = 0, freeOut = 0, freeIn = 0, nextOut = 0, nextIn = 0;
    for (int o = 0; o < NumOutputs; ++o)
        freeOut += isFreeOutput(board, o);
    for (int i = 0; i < NumInputs; ++i)
        freeIn += isFreeInput(board, i);
    for (int o = 0; (sections & PartOutputs) && o < NumOutputs; ++o)
    {
        if (file.outputs[o].type == OutputNotInUse)
            continue;
        ++needOut;
        while (!sameNumbers && nextOut < NumOutputs && !isFreeOutput(board, nextOut))
            ++nextOut;
        if (sameNumbers)
            r.outputs[o] = o;
        else if (nextOut < NumOutputs)
            r.outputs[o] = nextOut++;
    }
    for (int i = 0; (sections & PartInputs) && i < NumInputs; ++i)
    {
        if (file.inputs[i].type == InputNotInUse)
            continue;
        ++needIn;
        while (!sameNumbers && nextIn < NumInputs && !isFreeInput(board, nextIn))
            ++nextIn;
        if (sameNumbers)
            r.inputs[i] = i;
        else if (nextIn < NumInputs)
            r.inputs[i] = nextIn++;
    }
    if (!sameNumbers && (needOut > freeOut || needIn > freeIn))
    {
        r.error = tr("Not enough free slots: the file has %1 output(s) and %2 input(s) in use, this board has %3 free "
                     "output(s) and %4 free input(s). Nothing was imported.")
                      .arg(needOut).arg(needIn).arg(freeOut).arg(freeIn);
        r.outputs.fill(-1);
        r.inputs.fill(-1);
    }
    return r;
}

static int mapped(const QVector<int> &map, int index)
{
    return map.value(index, -1) >= 0 ? map[index] : index;   // not imported: the user's own item
}

OutputConfig importedOutput(const BoardConfig &file, int index, const ImportResult &plan)
{
    OutputConfig c = file.outputs[index];
    if (c.type == BlockSignal)
        c.setBlockInputs(mapped(plan.inputs, c.blockOwn()), c.blockNext() >= 0 ? mapped(plan.inputs, c.blockNext()) : -1);
    else if (c.servoFollows() >= 0)
        c.data[3] = quint8((c.data[3] & 0xF0) | (mapped(plan.inputs, c.servoFollows()) & 0x0F));
    return c;
}

InputConfig importedInput(const BoardConfig &file, int index, const ImportResult &plan)
{
    InputConfig c = file.inputs[index];
    c.outputId = quint8(mapped(plan.outputs, c.outputId));
    for (RouteStep &s : c.route)                     // route steps and the steps of custom values
        s.output = mapped(plan.outputs, s.output);
    return c;
}

// ── New board wizard ────────────────────────────────────────────────────────
int WizardChoice::pinsNeeded() const
{
    return servos + 2 * coils + 2 * stallMotors + lights + uncouplers + (buttons != NoButtons ? items() : 0);
}

QVector<quint8> wizardPins(quint8 boardType)
{
    Q_UNUSED(boardType);                             // Nano, Uno and DCCnext: D3-D12, A0-A5 (= P1-P16)
    QVector<quint8> pins;
    for (quint8 p = 3; p <= 12; ++p) pins << p;
    for (quint8 p = 14; p <= 19; ++p) pins << p;
    return pins;
}

// "1–4", "D9–D12": the items are numbered and the pins taken in order, so first–last is exact
static QString span(const QStringList &names)
{
    return names.size() > 1 && names.first() != names.last() ? QStringLiteral("%1–%2").arg(names.first(), names.last())
                                                             : names.value(0);
}

BoardConfig wizardBoard(const WizardChoice &c)
{
    BoardConfig b;                                   // everything 'not in use'
    b.board.type = c.boardType;
    b.board.name = cleanBoardName(c.name);
    const QVector<quint8> pins = wizardPins(c.boardType);
    int nextPin = 0;
    auto takePin = [&]() -> quint8 { return c.pinsInOrder && nextPin < pins.size() ? pins[nextPin++] : PinNA; };

    struct Kind { int count; quint8 type; const char *name; };
    const Kind kinds[] = {
        { c.servos,      ServoType,    QT_TRANSLATE_NOOP("Gpio", "Servo turnout %1") },
        { c.coils,       DoublePulse,  QT_TRANSLATE_NOOP("Gpio", "Coil turnout %1") },
        { c.stallMotors, DoubleSteady, QT_TRANSLATE_NOOP("Gpio", "Stall motor turnout %1") },
        { c.lights,      SingleSteady, QT_TRANSLATE_NOOP("Gpio", "Light %1") },
        { c.uncouplers,  SinglePulse,  QT_TRANSLATE_NOOP("Gpio", "Uncoupler %1") },
    };
    int items = 0;
    QStringList outNames, servoNames, outPins, buttonPins, inNames;
    for (const Kind &k : kinds)
        for (int n = 0; n < k.count && items < NumOutputs; ++n, ++items)
        {
            OutputConfig &o = b.outputs[items];
            o.type = k.type;
            o.name = tr(k.name).arg(n + 1);
            o.pinA = takePin();
            if (outputTypeInfo(k.type).pinCount == 2)
                o.pinB = takePin();
            if (k.type == ServoType)   o.data = { { 80, 100, 20, 0 } };   // as the output settings start them
            if (k.type == DoublePulse) o.setSwitchTime(25);               // 250 ms
            if (k.type == SinglePulse) o.setSwitchTime(5);                // 500 ms
            outNames << QString::number(items + 1);
            if (k.type == ServoType) servoNames << QString::number(items + 1);
            for (int s = 0; s < outputTypeInfo(k.type).pinCount; ++s)
                if (o.pin(s) != PinNA) outPins << pinNameOn(c.boardType, o.pin(s));
        }
    const bool inputs = c.dcc || c.buttons != WizardChoice::NoButtons;
    for (int i = 0; inputs && i < items; ++i)
    {
        InputConfig &in = b.inputs[i];
        in.type     = c.buttons == WizardChoice::ToggleButtons ? Momentary : Latching;
        in.outputId = quint8(i);
        in.address  = c.dcc && c.firstAddress > 0 ? quint16(c.firstAddress + i) : AddrNone;
        in.pin      = c.buttons != WizardChoice::NoButtons ? takePin() : PinNA;
        in.name     = b.outputs[i].name;
        inNames << QString::number(i + 1);
        if (in.pin != PinNA) buttonPins << pinNameOn(c.boardType, in.pin);
    }

    // the "Next steps" list
    auto add = [&b](const QString &text) { b.nextSteps.append({ text, false }); };
    const bool buttons = c.buttons != WizardChoice::NoButtons;
    if (items && !c.pinsInOrder)
        add(tr("Choose pins for Out %1 (output settings, or the Pin map tab)").arg(span(outNames)));
    if (buttons && !c.pinsInOrder)
        add(tr("Choose the button pins for In %1 (input settings)").arg(span(inNames)));
    if (!outPins.isEmpty())
        add(tr("Wire Out %1 to %2 (the Pin map tab shows which pin is which)").arg(span(outNames), span(outPins)));
    if (!buttonPins.isEmpty())
        add((c.buttons == WizardChoice::ToggleButtons ? tr("Wire the buttons to %1 and GND")
                                                      : tr("Wire the switches to %1 and GND")).arg(span(buttonPins)));
    if (!servoNames.isEmpty())
        add(c.boardType == BoardNano
                ? tr("Set the end positions of servo Out %1 (Live slider, phone remote or loco 9999)").arg(span(servoNames))
                : tr("Set the end positions of servo Out %1 (Live slider or phone remote)").arg(span(servoNames)));
    if (c.dcc && c.firstAddress > 0 && items)
        add(tr("Set turnout addresses %1 in your command station / iTrain")
                .arg(span({ QString::number(c.firstAddress), QString::number(c.firstAddress + items - 1) })));
    else if (c.dcc && items)
        add(tr("Set the DCC addresses of In %1 (input settings)").arg(span(inNames)));
    add(tr("Try it in Simulate"));
    add(tr("Connect the board and use Write all to put this configuration on it"));
    return b;
}

} // namespace Gpio
