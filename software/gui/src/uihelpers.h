#pragma once
#include "gpioconfig.h"

#include <QColor>
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
};
QVector<QVector<PinUse>> pinUsage(const Gpio::BoardConfig &board);

QString outputSummaryPins(const Gpio::OutputConfig &c);
QString outputSummaryRelays(const Gpio::OutputConfig &c);
QString outputSummaryParams(const Gpio::OutputConfig &c);
QString inputSummaryTarget(const Gpio::InputConfig &c);

// Short route step value for wire labels: "S", "C", "45°", "A3", "P128"
QString stepValueText(const Gpio::RouteStep &s, quint8 outputType);
QString delayText(quint16 delay);   // 0.1 s units -> "0.3 s"

} // namespace Ui
