#pragma once
#include "gpioconfig.h"

#include <QColor>
class QWidget;
#include <QIcon>
#include <QString>
#include <QVector>

class QApplication;
class QComboBox;

namespace Ui {

void   applyAppStyle(QApplication &app);
// Theme: "auto" (follows Windows), "light" or "dark"; stored in QSettings "ui/theme".
QString themeSetting();
// Colour of signal lamp 0-3: the board file's choice, or red / green / yellow / white.
QColor  lampColor(const Gpio::OutputConfig &o, int lamp);
void    setThemeOverride(const QString &theme);  // this run only, not saved (--theme)
void    setThemeSetting(const QString &theme);   // and applies it
bool    isDark();
QColor  themed(const QColor &light);              // the dark-mode equivalent of a light-theme colour

QColor outputTypeColor(quint8 type);
QColor inputTypeColor(quint8 type);
QColor inputColor(int index);          // unique colour per input (0..15), used for its wires and outputs
QIcon  swatchIcon(const QColor &color);

// Pin combo: "—" plus D0..D13, A0..A5. Item data = pin number.
void   fillPinCombo(QComboBox *combo, bool output = false);
// Select the item whose data equals value; adds a warning item if missing.
void   selectData(QComboBox *combo, int value, const QString &unknownText = QString());
int    currentData(const QComboBox *combo);

// Who uses which pin
struct PinUse
{
    bool    output = true;
    int     index  = 0;
    QString role;          // "A", "B", "relay A", "input"
    int     share  = 0;    // users with the same non-zero share may use one pin together
};
enum PinShare { ShareNone, ShareAnalog, ShareSound };   // analog inputs on one channel, sound outputs on D1
inline bool sharesPin(const PinUse &a, const PinUse &b) { return a.share != ShareNone && a.share == b.share; }
int  pinShare(const Gpio::InputConfig &c);
int  pinShare(const Gpio::OutputConfig &c);
bool pinClash(const QVector<PinUse> &users);   // two users of this pin that cannot share it
QVector<QVector<PinUse>> pinUsage(const Gpio::BoardConfig &board);
// Pins in use elsewhere are shown red with who uses them. setPinBoard() is called by the main
// window after every change; markUsedPins() updates a combo's texts without refilling it.
void setPinBoard(const Gpio::BoardConfig &board);
// 4-aspect signal value: bits 0-3 lamp 1-4 on (pin A-D), bits 4-7 flashing; flashing without 'on' =
// opposite phase. aspectText() explains a value, pickAspect() lets you choose lamp by lamp
// (returns -1 on cancel; 'output' gives the lamp colours).
QString aspectText(int aspect);
int     pickAspect(QWidget *parent, int current, int output);
// share: users with the same share (analog inputs, sound outputs) are listed as 'shared', not red.
void markUsedPins(QComboBox *combo, bool forOutput, int selfIndex, int share = ShareNone);

QString outputSummaryPins(const Gpio::OutputConfig &c);
QString outputSummaryRelays(const Gpio::OutputConfig &c);
QString outputSummaryParams(const Gpio::OutputConfig &c);
QString inputSummaryTarget(const Gpio::InputConfig &c);

// Short route step value for wire labels: "S", "C", "45°", "A3", "P128"
QString stepValueText(const Gpio::RouteStep &s, quint8 outputType);
QString delayText(quint16 delay);   // 0.1 s units -> "0.3 s"

} // namespace Ui
