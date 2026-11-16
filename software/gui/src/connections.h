#pragma once
// Input -> output connections ("wires") on top of BoardConfig.
//
// An input either controls one output directly (toggle / absolute, via
// outputId) or runs a route: up to 16 steps, each with an output, a value
// and a delay (see RouteStep in gpioconfig.h).

#include "gpioconfig.h"

#include <QVector>

namespace Gpio {

inline const QVector<RouteStep> &routeSteps(const InputConfig &c) { return c.route; }
void setRouteSteps(InputConfig &c, const QVector<RouteStep> &steps);

// True when the input drives at least one output (used for its colour).
bool inputConnected(const InputConfig &c);

// Inputs that drive this output: direct inputs first, then route inputs.
QVector<int> outputSources(const BoardConfig &board, int output);

// Total number of route steps on the board (decoders may have a shared pool).
int totalRouteSteps(const BoardConfig &board);

struct RouteLimits
{
    int maxSteps = MaxRouteSteps;            // per route
    int pool     = NumInputs * MaxRouteSteps; // all routes together
};

enum class ConnectResult
{
    Connected,          // new wire
    Replaced,           // direct input: previous output replaced
    AlreadyConnected,   // nothing changed
    RouteFull,          // route already has the maximum number of steps
    PoolFull,           // decoder has no room for more steps
    NotConnectable,     // no such input / output
};

// Drag an output onto an input. A 'not in use' input becomes 'absolute'.
// Route inputs get a new step with a default value for the output type.
ConnectResult connectOutput(BoardConfig &board, int input, int output,
                            const RouteLimits &limits = RouteLimits());

// step = -1: remove the direct wire (input becomes 'not in use');
// otherwise remove that route step.
void disconnect(BoardConfig &board, int input, int step);
void toggleStepState(BoardConfig &board, int input, int step);
void moveStep(BoardConfig &board, int input, int step, int delta);

} // namespace Gpio
