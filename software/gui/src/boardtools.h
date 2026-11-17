#pragma once
// Whole-configuration tools without UI: partial export / import (File menu) and the configuration the
// new board wizard builds.

#include "gpioconfig.h"

#include <QStringList>
#include <QVector>

namespace Gpio {

// ── Partial export / import ─────────────────────────────────────────────────
enum PartSection { PartOutputs = 1, PartInputs = 2, PartBoard = 4 };

// The in-use items of the chosen sections (with names, lamp colours, analog settings, custom values and
// routes); every other slot is 'not in use'. Without PartBoard only the board type is kept (it gives the
// pins their meaning), the other board settings are the defaults.
BoardConfig exportPart(const BoardConfig &board, int sections);

struct ImportResult
{
    QVector<int> outputs;   // per output of the file: the slot it went to, -1 = not imported
    QVector<int> inputs;    // per input of the file
    QString      error;     // not empty: nothing was changed (not enough free slots)
};

// Free slots for an import: an output that is not in use and that no input drives; an input not in use.
bool isFreeOutput(const BoardConfig &board, int output);
bool isFreeInput(const BoardConfig &board, int input);

// Plans where the in-use items of the chosen sections of 'file' go in 'board'. sameNumbers: each to its
// own number; otherwise to the first free slots in order.
ImportResult planImport(const BoardConfig &board, const BoardConfig &file, int sections, bool sameNumbers);

// The item of 'file' as it is placed by 'plan': references to other imported items (input -> output,
// route steps and custom values, block signal detectors, semaphore arm detector) get their new numbers;
// references to items that were not imported stay as they are.
OutputConfig importedOutput(const BoardConfig &file, int index, const ImportResult &plan);
InputConfig  importedInput(const BoardConfig &file, int index, const ImportResult &plan);

// ── New board wizard ────────────────────────────────────────────────────────
struct WizardChoice
{
    quint8  boardType  = BoardNano;
    QString name;                       // board name, may be empty
    int     servos = 0;                 // turnouts with a servo
    int     coils = 0;                  // coil turnouts: two coils, double pulse
    int     stallMotors = 0;            // MP1 / Tortoise: double steady
    int     lights = 0;                 // single steady
    int     uncouplers = 0;             // single pulse
    bool    dcc = true;                 // a DCC accessory address per item
    int     firstAddress = 1;           // numbered on over all items; 0 = leave the addresses empty
    enum Buttons { NoButtons, ToggleButtons, SwitchButtons };
    int     buttons = NoButtons;        // one button (or switch) per item
    bool    pinsInOrder = true;         // false: leave the pins empty

    int items() const { return servos + coils + stallMotors + lights + uncouplers; }
    int pinsNeeded() const;             // outputs plus buttons
};

// The pins a beginner can use on this board type, in the board's own order (D3-D12, A0-A5 = P1-P16).
QVector<quint8> wizardPins(quint8 boardType);

// The configuration for the choice (Beginner features only), and its "Next steps" list.
BoardConfig wizardBoard(const WizardChoice &choice);

} // namespace Gpio
