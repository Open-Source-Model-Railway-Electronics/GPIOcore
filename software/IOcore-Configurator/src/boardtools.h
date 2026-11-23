#pragma once
// Whole-configuration tools without UI: partial export / import (File menu).

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

} // namespace Gpio
