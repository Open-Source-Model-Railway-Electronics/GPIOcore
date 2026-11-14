#include "connections.h"

namespace Gpio {

void setRouteSteps(InputConfig &c, const QVector<RouteStep> &steps)
{
    c.route = steps.mid(0, MaxRouteSteps);
    c.syncLinkedFromRoute();
}

bool inputConnected(const InputConfig &c)
{
    if (isDirectType(c.type))
        return c.outputId < NumOutputs;
    if (isRouteType(c.type))
        return !c.route.isEmpty();
    return false;
}

QVector<int> outputSources(const BoardConfig &board, int output)
{
    QVector<int> direct, route;
    for (int i = 0; i < NumInputs; ++i)
    {
        const InputConfig &c = board.inputs[i];
        if (isDirectType(c.type) && c.outputId == output)
            direct.append(i);
        else if (isRouteType(c.type))
            for (const RouteStep &s : c.route)
                if (s.output == output) { route.append(i); break; }
    }
    return direct + route;
}

int totalRouteSteps(const BoardConfig &board)
{
    int n = 0;
    for (const InputConfig &c : board.inputs)
        if (isRouteType(c.type) || c.hasCustomValues())   // custom values use 2 steps of the pool
            n += c.route.size();
    return n;
}

int recorderInput(const BoardConfig &board, int except)
{
    for (int i = 0; i < NumInputs; ++i)
        if (i != except && board.inputs[i].type == Recorder)
            return i;
    return -1;
}

ConnectResult connectOutput(BoardConfig &board, int input, int output, const RouteLimits &limits)
{
    if (input < 0 || input >= NumInputs || output < 0 || output >= NumOutputs)
        return ConnectResult::NotConnectable;

    InputConfig &c = board.inputs[input];
    if (c.type == Recorder || c.type == FeedbackInput)
        return ConnectResult::NotConnectable;

    // a sound output plays a track chosen by the input: custom values on (off = stop, on = track 1)
    auto values = [&c, &board](quint8 before) {
        const quint8 now = board.outputs[c.outputId].type;
        if (now == SoundEffect && !c.hasCustomValues())
            c.setCustomValues(true, now);
        else if (c.hasCustomValues() && stepKind(now) != stepKind(before))
            c.setCustomValues(true, now);            // other kind of value: start from its defaults
        c.syncCustomValues();
    };
    if (c.type == InputNotInUse)
    {
        c.type     = Latching;
        c.outputId = quint8(output);
        c.route.clear();
        values(board.outputs[output].type);
        return ConnectResult::Connected;
    }

    if (isDirectType(c.type))
    {
        if (c.outputId == output)
            return ConnectResult::AlreadyConnected;
        const quint8 before = board.outputs[c.outputId].type;
        c.outputId = quint8(output);
        values(before);
        return ConnectResult::Replaced;
    }

    // route: the same output may appear more than once (e.g. a servo moving
    // through several positions), so every drop adds a step
    const int maxSteps = qMin(limits.maxSteps, limits.v1 ? MaxRouteStepsV1 : MaxRouteSteps);
    if (c.route.size() >= maxSteps)
        return ConnectResult::RouteFull;
    if (totalRouteSteps(board) >= limits.pool)
        return ConnectResult::PoolFull;

    RouteStep s = defaultStep(output, board.outputs[output].type);
    if (limits.v1)
    {
        s.raw   = false;
        s.value = 1;
        s.delay = V1StepDelay;
    }
    QVector<RouteStep> steps = c.route;
    steps.append(s);
    setRouteSteps(c, steps);
    return ConnectResult::Connected;
}

void disconnect(BoardConfig &board, int input, int step)
{
    if (input < 0 || input >= NumInputs)
        return;
    InputConfig &c = board.inputs[input];
    if (step < 0)
    {
        if (isDirectType(c.type))
            c.type = InputNotInUse;      // pin / address are kept
        return;
    }
    if (step < c.route.size())
    {
        QVector<RouteStep> steps = c.route;
        steps.remove(step);
        setRouteSteps(c, steps);
    }
}

void toggleStepState(BoardConfig &board, int input, int step)
{
    if (input < 0 || input >= NumInputs)
        return;
    InputConfig &c = board.inputs[input];
    if (step < 0 || step >= c.route.size())
        return;
    QVector<RouteStep> steps = c.route;
    if (steps[step].raw)
        return;                          // raw values are edited in the side panel
    steps[step].value ^= 1;
    setRouteSteps(c, steps);
}

void moveStep(BoardConfig &board, int input, int step, int delta)
{
    if (input < 0 || input >= NumInputs)
        return;
    InputConfig &c = board.inputs[input];
    QVector<RouteStep> steps = c.route;
    const int to = step + delta;
    if (step < 0 || step >= steps.size() || to < 0 || to >= steps.size())
        return;
    std::swap(steps[step], steps[to]);
    setRouteSteps(c, steps);
}

} // namespace Gpio
