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

} // namespace Gpio
