#include "uihelpers.h"

#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QStyleFactory>
#include <QGuiApplication>
#include <QHash>
#include <QRegularExpression>
#include <QSettings>
#include <QStyleHints>

using namespace Gpio;

namespace Ui {

QColor lampColor(const OutputConfig &o, int lamp)
{
    static const QColor defaults[4] = { QColor(0xEF, 0x44, 0x44), QColor(0x22, 0xC5, 0x5E),
                                        QColor(0xFA, 0xCC, 0x15), QColor(0xF8, 0xFA, 0xFC) };
    const QColor c(o.lampColors.value(lamp));
    return c.isValid() ? c : defaults[qBound(0, lamp, 3)];
}

// ── Theme ───────────────────────────────────────────────────────────────────
static bool g_dark = false;

static QString g_themeOverride;

void setThemeOverride(const QString &theme) { g_themeOverride = theme; }

QString themeSetting()
{
    if (!g_themeOverride.isEmpty())
        return g_themeOverride;
    return QSettings().value(QStringLiteral("ui/theme"), QStringLiteral("auto")).toString();
}

static bool systemIsDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return false;
#endif
}

bool isDark() { return g_dark; }

QColor themed(const QColor &light)
{
    if (!g_dark)
        return light;
    // light-theme colour -> dark-theme colour (slate scale); accent colours stay the same
    static const QHash<QRgb, QRgb> map = {
        { 0xFFF4F6F9, 0xFF0F172A }, { 0xFFFFFFFF, 0xFF1E293B }, { 0xFFF8FAFC, 0xFF18222F },
        { 0xFFF1F5F9, 0xFF273449 }, { 0xFFEEF2F7, 0xFF2A3A52 }, { 0xFFE2E8F0, 0xFF334155 },
        { 0xFFCBD5E1, 0xFF475569 }, { 0xFF0F172A, 0xFFE2E8F0 }, { 0xFF1F2937, 0xFFE5E7EB },
        { 0xFF334155, 0xFFCBD5E1 }, { 0xFF475569, 0xFF94A3B8 }, { 0xFF64748B, 0xFF94A3B8 },
        { 0xFFFEF3C7, 0xFF422006 }, { 0xFF92400E, 0xFFFCD34D }, { 0xFFFEE2E2, 0xFF450A0A },
        { 0xFF991B1B, 0xFFFCA5A5 }, { 0xFFDCFCE7, 0xFF052E16 }, { 0xFF166534, 0xFF86EFAC },
        { 0xFFEFF6FF, 0xFF172554 }, { 0xFFDBEAFE, 0xFF1E3A8A }, { 0xFFFCE7F3, 0xFF500724 },
        { 0xFF9D174D, 0xFFF9A8D4 }, { 0xFFBFD0F5, 0xFF1E3A8A }, { 0xFFA0A8B4, 0xFF64748B },
        { 0xFF94A3B8, 0xFF64748B }, { 0xFF93C5FD, 0xFF3B82F6 }, { 0xFFFCA5A5, 0xFFB91C1C },
    };
    const auto it = map.constFind(light.rgb() | 0xFF000000u);
    if (it == map.constEnd())
        return light;
    QColor c = QColor::fromRgb(*it);
    c.setAlpha(light.alpha());
    return c;
}

void setThemeSetting(const QString &theme)
{
    QSettings().setValue(QStringLiteral("ui/theme"), theme);
    applyAppStyle(*qApp);
}

void applyAppStyle(QApplication &app)
{
    const QString theme = themeSetting();
    g_dark = theme == QLatin1String("dark") || (theme == QLatin1String("auto") && systemIsDark());
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    static bool hooked = false;
    if (!hooked)
    {
        hooked = true;     // follow Windows when the theme is automatic
        QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &app, [&app] {
            if (themeSetting() == QLatin1String("auto"))
                applyAppStyle(app);
        });
    }
#endif
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    p.setColor(QPalette::Window,          themed(QColor(0xF4, 0xF6, 0xF9)));
    p.setColor(QPalette::WindowText,      themed(QColor(0x1F, 0x29, 0x37)));
    p.setColor(QPalette::Base,            themed(QColor(Qt::white)));
    p.setColor(QPalette::AlternateBase,   themed(QColor(0xF8, 0xFA, 0xFC)));
    p.setColor(QPalette::Text,            themed(QColor(0x1F, 0x29, 0x37)));
    p.setColor(QPalette::Button,          themed(QColor(0xFF, 0xFF, 0xFF)));
    p.setColor(QPalette::ButtonText,      themed(QColor(0x1F, 0x29, 0x37)));
    p.setColor(QPalette::Highlight,       themed(QColor(0x25, 0x63, 0xEB)));
    p.setColor(QPalette::HighlightedText, QColor(Qt::white));
    p.setColor(QPalette::ToolTipBase,     QColor(0x1F, 0x29, 0x37));   // TOOLTIP
    p.setColor(QPalette::ToolTipText,     QColor(Qt::white));
    p.setColor(QPalette::PlaceholderText, themed(QColor(0x94, 0xA3, 0xB8)));
    p.setColor(QPalette::Disabled, QPalette::Text,       themed(QColor(0xA0, 0xA8, 0xB4)));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, themed(QColor(0xA0, 0xA8, 0xB4)));
    app.setPalette(p);

    QString css = QStringLiteral(R"(
        QMainWindow { background: #F4F6F9; }
        QToolBar { background: #FFFFFF; border: none; border-bottom: 1px solid #E2E8F0; spacing: 6px; padding: 6px; }
        QToolBar QToolButton { padding: 5px 10px; border-radius: 6px; }
        QToolBar QToolButton:hover { background: #EEF2F7; }
        QToolButton#primary { background: #2563EB; color: white; font-weight: 600; }
        QToolButton#primary:hover { background: #1D4ED8; }
        QToolButton#primary:disabled { background: #BFD0F5; color: #F1F5F9; }
        QGroupBox { background: #FFFFFF; border: 1px solid #E2E8F0; border-radius: 8px; margin-top: 14px; padding: 10px 10px 8px 10px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #475569; font-weight: 600; }
        QTableWidget { background: #FFFFFF; border: 1px solid #E2E8F0; border-radius: 8px; gridline-color: #EEF2F7; selection-background-color: #DBEAFE; selection-color: #1F2937; }
        QHeaderView::section { background: #F8FAFC; border: none; border-bottom: 1px solid #E2E8F0; padding: 5px 6px; color: #64748B; font-weight: 600; }
        QTabWidget::pane { border: none; }
        QTabBar::tab { padding: 7px 16px; margin-right: 2px; border-top-left-radius: 6px; border-top-right-radius: 6px; color: #64748B; }
        QTabBar::tab:selected { background: #FFFFFF; color: #1F2937; font-weight: 600; border: 1px solid #E2E8F0; border-bottom: none; }
        QLabel#title { font-size: 15pt; font-weight: 600; color: #0F172A; }
        QToolButton#info { border: 1px solid #93C5FD; border-radius: 10px; color: #2563EB; background: #EFF6FF;
                           font-weight: 700; font-family: Georgia, 'Times New Roman', serif; font-style: italic; padding: 0; }
        QToolButton#info:hover { background: #DBEAFE; border-color: #2563EB; }
        QFrame#infoPopup { background: #FFFFFF; border: 1px solid #CBD5E1; border-radius: 8px; }
        QLabel#infoText { color: #1F2937; }
        QWidget#sidePanel { background: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 10px; }
        QToolButton#panelClose { border: none; color: #64748B; font-size: 14pt; padding: 0 6px; }
        QToolButton#stepDelete { border: 1px solid #FCA5A5; border-radius: 5px; background: #FFFFFF; padding: 2px; }
        QToolButton#stepDelete:hover { background: #FEE2E2; border-color: #DC2626; }
        QToolButton#panelClose:hover { color: #0F172A; }
        QLabel#help  { color: #64748B; }
        QLabel#warn  { background: #FEF3C7; color: #92400E; border: 1px solid #FCD34D; border-radius: 6px; padding: 6px 8px; }
        QLabel#error { background: #FEE2E2; color: #991B1B; border: 1px solid #FCA5A5; border-radius: 6px; padding: 6px 8px; }
        QLabel#chip     { border-radius: 9px; padding: 2px 10px; background: #E2E8F0; color: #334155; }
        QLabel#chipOk   { border-radius: 9px; padding: 2px 10px; background: #DCFCE7; color: #166534; }
        QLabel#chipWarn { border-radius: 9px; padding: 2px 10px; background: #FEF3C7; color: #92400E; }
        QLabel#mono  { font-family: Consolas, 'Courier New', monospace; color: #475569; }
        QPlainTextEdit#log { font-family: Consolas, 'Courier New', monospace; font-size: 9pt; background: #0F172A; color: #E2E8F0; border: none; }
        QPushButton { padding: 5px 12px; border: 1px solid #CBD5E1; border-radius: 6px; background: #FFFFFF; }
        QPushButton:hover { background: #F1F5F9; }
        QPushButton#test { border-color: #93C5FD; color: #1D4ED8; }
        QComboBox, QSpinBox { padding: 3px 6px; border: 1px solid #CBD5E1; border-radius: 5px; background: white; min-height: 20px; }
        QComboBox:disabled, QSpinBox:disabled { background: #F1F5F9; }
        QScrollArea { border: none; background: transparent; }
        QStatusBar { background: #FFFFFF; border-top: 1px solid #E2E8F0; }
        QCheckBox::indicator, QRadioButton::indicator { width: 15px; height: 15px; border: 2px solid #8B9AB0; background: #FFFFFF; }
        QCheckBox::indicator { border-radius: 4px; }
        QRadioButton::indicator { border-radius: 9px; }
        QCheckBox::indicator:hover, QRadioButton::indicator:hover { border-color: #2563EB; }
        QCheckBox::indicator:checked { background: #2563EB; border-color: #2563EB; image: url(:/ui/check.svg); }
        QRadioButton::indicator:checked { width: 9px; height: 9px; border: 5px solid #2563EB; border-radius: 9px; background: #FFFFFF; }
        QCheckBox::indicator:disabled, QRadioButton::indicator:disabled { border-color: #CBD5E1; background: #F1F5F9; }
        QCheckBox::indicator:checked:disabled { background: #93C5FD; border-color: #93C5FD; }
        QToolButton#modeConfigure, QToolButton#modeSimulate, QToolButton#modeLive {
            border: 2px solid; border-radius: 7px; padding: 4px 12px; font-weight: 700; margin: 0 2px; }
        QToolButton#modeConfigure { border-color: #DC2626; color: #DC2626; }
        QToolButton#modeSimulate  { border-color: #CA8A04; color: #A16207; }
        QToolButton#modeLive      { border-color: #16A34A; color: #15803D; }
        QToolButton#modeConfigure:checked { background: #DC2626; color: white; }
        QToolButton#modeSimulate:checked  { background: #EAB308; color: #111827; border-color: #EAB308; }
        QToolButton#modeLive:checked      { background: #16A34A; color: white; }
        QToolButton#modeConfigure:hover:!checked, QToolButton#modeSimulate:hover:!checked,
        QToolButton#modeLive:hover:!checked { background: #EEF2F7; }
    )");
    if (g_dark)
    {
        css.replace(QStringLiteral("background: white"), QStringLiteral("background: #FFFFFF"));
        static const QRegularExpression hex(QStringLiteral("#[0-9A-Fa-f]{6}\\b"));
        QString out;
        int last = 0;
        auto it = hex.globalMatch(css);
        while (it.hasNext())
        {
            const auto m = it.next();
            out += css.mid(last, m.capturedStart() - last);
            out += themed(QColor(m.captured())).name(QColor::HexRgb).toUpper();
            last = m.capturedEnd();
        }
        css = out + css.mid(last);
        css += QStringLiteral(" QPlainTextEdit#log { background: #020617; }");
    }
    app.setStyleSheet(css);
    for (QWidget *w : QApplication::allWidgets())
        w->update();
}

QColor outputTypeColor(quint8 type)
{
    switch (type)
    {
    case SingleSteady:   return QColor(0x3B, 0x82, 0xF6);
    case SinglePulse:    return QColor(0x63, 0x66, 0xF1);
    case SingleFlashing: return QColor(0xF5, 0x9E, 0x0B);
    case DoubleSteady:   return QColor(0x0E, 0xA5, 0xE9);
    case DoublePulse:    return QColor(0x8B, 0x5C, 0xF6);
    case DoubleFlashing: return QColor(0xF9, 0x73, 0x16);
    case SwingLight:     return QColor(0xEF, 0x44, 0x44);
    case FirePlaceLight: return QColor(0xEA, 0x58, 0x0C);
    case TubeLight:      return QColor(0x06, 0xB6, 0xD4);
    case WelderLight:    return QColor(0xA8, 0x55, 0xF7);
    case FourToneSignal: return QColor(0x22, 0xC5, 0x5E);
    case ServoType:      return QColor(0x10, 0xB9, 0x81);
    case Pwm:            return QColor(0xEC, 0x48, 0x99);
    case OutputNotInUse: return QColor(0xCB, 0xD5, 0xE1);
    case SoundEffect:    return QColor(0x0D, 0x94, 0x88);
    }
    return QColor(0x94, 0xA3, 0xB8);
}

QColor inputTypeColor(quint8 type)
{
    switch (type)
    {
    case Momentary:     return QColor(0x3B, 0x82, 0xF6);
    case Latching:      return QColor(0x10, 0xB9, 0x81);
    case Route:
    case RouteStraight:
    case RouteCurved:   return QColor(0x8B, 0x5C, 0xF6);
    case InputNotInUse: return QColor(0xCB, 0xD5, 0xE1);
    case Recorder:      return QColor(0xEF, 0x44, 0x44);
    case FeedbackInput: return QColor(0xF5, 0x9E, 0x0B);
    }
    return QColor(0x94, 0xA3, 0xB8);
}

QColor inputColor(int index)
{
    // 16 distinct mid-saturation hues, readable as wires on white
    static const QColor palette[16] = {
        QColor(0xE5, 0x39, 0x35), QColor(0x1E, 0x88, 0xE5), QColor(0x43, 0xA0, 0x47), QColor(0xFB, 0x8C, 0x00),
        QColor(0x8E, 0x24, 0xAA), QColor(0x00, 0x89, 0x7B), QColor(0xD8, 0x1B, 0x60), QColor(0x39, 0x49, 0xAB),
        QColor(0x7C, 0xB3, 0x42), QColor(0xF4, 0x51, 0x1E), QColor(0x00, 0xAC, 0xC1), QColor(0x6D, 0x4C, 0x41),
        QColor(0xC0, 0xCA, 0x33), QColor(0x5E, 0x35, 0xB1), QColor(0x03, 0x9B, 0xE5), QColor(0x54, 0x6E, 0x7A),
    };
    return palette[qBound(0, index, 15)];
}

QIcon swatchIcon(const QColor &color)
{
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(color.darker(120));
    p.setBrush(color);
    p.drawRoundedRect(QRectF(1.5, 1.5, 11, 11), 3, 3);
    return QIcon(pm);
}

void fillPinCombo(QComboBox *combo, bool output)
{
    combo->clear();
    combo->addItem(trGpio("—  not used"), int(PinNA));
    for (quint8 pin = 0; pin < NumPins; ++pin)
    {
        QString text = pinLabel(pin);
        if (isPwmPin(pin)) text += QStringLiteral("  ~PWM");
        QString reserved = pinReservedReason(pin);
        if (reserved.isEmpty() && output) reserved = pinOutputReason(pin);
        const QString note     = pinNote(pin);
        if ((pin == 20 || pin == 21) && !reserved.isEmpty())
            continue;                    // A6/A7 only appear where they can be used
        if (!reserved.isEmpty()) text += QStringLiteral("  (%1)").arg(reserved);
        else if (pin < 2)          text += QStringLiteral("  (%1)").arg(trGpio("serial pin"));
        else if (pin >= 20)        text += QStringLiteral("  (%1)").arg(trGpio("analog input only"));
        combo->addItem(text, int(pin));
        combo->setItemData(combo->count() - 1, text, Qt::UserRole + 1);       // text without 'in use'
        if (!reserved.isEmpty())
            combo->setItemData(combo->count() - 1, QColor(0xB4, 0x53, 0x09), Qt::ForegroundRole);
        if (!note.isEmpty())
            combo->setItemData(combo->count() - 1, note, Qt::ToolTipRole);
    }
}

void selectData(QComboBox *combo, int value, const QString &unknownText)
{
    int i = combo->findData(value);
    if (i < 0)
    {
        combo->addItem(unknownText.isEmpty() ? QStringLiteral("⚠ invalid (%1)").arg(value) : unknownText, value);
        i = combo->count() - 1;
        combo->setItemData(i, QColor(0xB9, 0x1C, 0x1C), Qt::ForegroundRole);
    }
    combo->setCurrentIndex(i);
}

int currentData(const QComboBox *combo)
{
    return combo->currentData().toInt();
}

static BoardConfig g_pinBoard;
static bool        g_pinBoardSet = false;

void setPinBoard(const BoardConfig &board)
{
    g_pinBoard = board;
    g_pinBoardSet = true;
}

QString aspectText(int aspect)
{
    QStringList parts;
    for (int k = 0; k < 4; ++k)
    {
        const bool on = (aspect >> k) & 1, flash = (aspect >> (k + 4)) & 1;
        if (!on && !flash) continue;
        parts << (flash ? (on ? QObject::tr("lamp %1 flashing").arg(k + 1)
                              : QObject::tr("lamp %1 flashing, opposite phase").arg(k + 1))
                        : QObject::tr("lamp %1 on").arg(k + 1));
    }
    return parts.isEmpty() ? QObject::tr("all lamps off") : parts.join(QStringLiteral(", "));
}

int pickAspect(QWidget *parent, int current, int output)
{
    QDialog d(parent);
    d.setWindowTitle(QObject::tr("Signal aspect"));
    auto *form = new QFormLayout(&d);
    auto *intro = new QLabel(QObject::tr("Choose per lamp. Lamp 1 is pin A, lamp 2 pin B, and so on."));
    intro->setWordWrap(true);
    form->addRow(intro);
    QVector<QComboBox *> lamps;
    auto *value = new QLabel;
    auto *meaning = new QLabel;
    meaning->setWordWrap(true);
    meaning->setObjectName(QStringLiteral("help"));
    auto compute = [&lamps] {
        int v = 0;
        for (int k = 0; k < 4; ++k)
        {
            const int m = lamps[k]->currentIndex();
            if (m == 1 || m == 2) v |= 1 << k;
            if (m == 2 || m == 3) v |= 1 << (k + 4);
        }
        return v;
    };
    auto show = [&] {
        const int v = compute();
        value->setText(QObject::tr("Value: %1  (0x%2)").arg(v).arg(v, 2, 16, QLatin1Char('0')));
        meaning->setText(aspectText(v));
    };
    for (int k = 0; k < 4; ++k)
    {
        auto *c = new QComboBox;
        c->addItems({ QObject::tr("off"), QObject::tr("on"), QObject::tr("flashing"),
                      QObject::tr("flashing, opposite phase") });
        const bool on = (current >> k) & 1, flash = (current >> (k + 4)) & 1;
        c->setCurrentIndex(flash ? (on ? 2 : 3) : (on ? 1 : 0));
        QColor col = g_pinBoardSet && output >= 0 && output < NumOutputs
                         ? lampColor(g_pinBoard.outputs[output], k) : lampColor(OutputConfig(), k);
        auto *lbl = new QLabel;
        lbl->setPixmap(swatchIcon(col).pixmap(14, 14));
        auto *row = new QHBoxLayout;
        row->addWidget(lbl);
        row->addWidget(new QLabel(QObject::tr("Lamp %1").arg(k + 1)));
        auto *rowW = new QWidget;
        rowW->setLayout(row);
        row->setContentsMargins(0, 0, 0, 0);
        form->addRow(rowW, c);
        lamps << c;
        QObject::connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), &d, show);
    }
    form->addRow(value);
    form->addRow(meaning);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    form->addRow(bb);
    show();
    return d.exec() == QDialog::Accepted ? compute() : -1;
}

void markUsedPins(QComboBox *combo, bool forOutput, int selfIndex, int share)
{
    if (!g_pinBoardSet)
        return;
    const auto usage = pinUsage(g_pinBoard);
    const QColor red(0xDC, 0x26, 0x26);
    for (int i = 0; i < combo->count(); ++i)
    {
        const int pin = combo->itemData(i).toInt();
        const QString base = combo->itemData(i, Qt::UserRole + 1).toString();
        if (pin < 0 || pin >= NumPins || base.isEmpty())
            continue;
        QStringList users, shared;
        for (const PinUse &u : usage[pin])
        {
            if (u.output == forOutput && u.index == selfIndex)
                continue;                                    // this input / output itself
            (share != ShareNone && u.share == share ? shared : users)
                << (u.output ? QObject::tr("Out %1").arg(u.index) : QObject::tr("In %1").arg(u.index));
        }
        users.removeDuplicates();
        shared.removeDuplicates();
        if (users.isEmpty())
        {
            combo->setItemText(i, shared.isEmpty() ? base
                                                   : base + QStringLiteral("  — ") + QObject::tr("shared with: %1")
                                                                                         .arg(shared.join(QStringLiteral(", "))));
            if (pinReservedReason(quint8(pin)).isEmpty())
                combo->setItemData(i, QVariant(), Qt::ForegroundRole);
        }
        else
        {
            combo->setItemText(i, base + QStringLiteral("  — ") + QObject::tr("in use: %1").arg(users.join(QStringLiteral(", "))));
            combo->setItemData(i, red, Qt::ForegroundRole);
        }
    }
}

int pinShare(const InputConfig &c)  { return c.isAnalog() ? ShareAnalog : ShareNone; }
int pinShare(const OutputConfig &c) { return c.type == SoundEffect ? ShareSound : ShareNone; }

bool pinClash(const QVector<PinUse> &users)
{
    for (int a = 0; a < users.size(); ++a)
        for (int b = a + 1; b < users.size(); ++b)
            if (!sharesPin(users[a], users[b]))
                return true;
    return false;
}

QVector<QVector<PinUse>> pinUsage(const BoardConfig &board)
{
    QVector<QVector<PinUse>> use(NumPins);
    static const char *roles[7] = { QT_TRANSLATE_NOOP("Gpio", "A"), QT_TRANSLATE_NOOP("Gpio", "B"), QT_TRANSLATE_NOOP("Gpio", "C"),
                                    QT_TRANSLATE_NOOP("Gpio", "D"), QT_TRANSLATE_NOOP("Gpio", "relay A"),
                                    QT_TRANSLATE_NOOP("Gpio", "relay B"), QT_TRANSLATE_NOOP("Gpio", "relay C") };

    for (int i = 0; i < NumOutputs; ++i)
    {
        const OutputConfig &c = board.outputs[i];
        const OutputTypeInfo &info = outputTypeInfo(c.type);
        for (int s = 0; s < 7; ++s)
        {
            const bool active = (s < 4) ? (s < info.pinCount) : info.relays;
            const quint8 pin = c.pin(s);
            if (active && pin < NumPins)
                use[pin].append({ true, i, trGpio(roles[s]), pinShare(c) });
        }
    }
    for (int i = 0; i < NumInputs; ++i)
    {
        const InputConfig &c = board.inputs[i];
        if (c.type != InputNotInUse && c.pin < NumPins)
            use[c.pin].append({ false, i, QStringLiteral("input"), pinShare(c) });
    }
    return use;
}

QString outputSummaryPins(const OutputConfig &c)
{
    const OutputTypeInfo &info = outputTypeInfo(c.type);
    QStringList parts;
    for (int s = 0; s < info.pinCount; ++s)
        parts << pinName(c.pin(s));
    return parts.join(QStringLiteral("  "));
}

QString outputSummaryRelays(const OutputConfig &c)
{
    if (!outputTypeInfo(c.type).relays)
        return QString();
    QStringList parts;
    if (c.relayA != PinNA || c.relayB != PinNA)
        parts << QStringLiteral("EF %1/%2").arg(pinName(c.relayA), pinName(c.relayB));
    if (c.relayC != PinNA)
        parts << QStringLiteral("UF %1").arg(pinName(c.relayC));
    return parts.join(QStringLiteral("  "));
}

QString outputSummaryParams(const OutputConfig &c)
{
    const OutputTypeInfo &info = outputTypeInfo(c.type);
    switch (info.params)
    {
    case ParamKind::SwitchTime:
        return QStringLiteral("%1 ms").arg(qulonglong(c.switchTime()) * info.timeUnitMs);
    case ParamKind::Swing:
        return QStringLiteral("%1 / %2 ms").arg(c.swingPulse() * info.timeUnitMs).arg(c.swingPause() * info.timeUnitMs);
    case ParamKind::Servo:
        return QStringLiteral("%1°–%2°, %3 ms/°").arg(c.servoMin()).arg(c.servoMax()).arg(c.servoSpeed());
    case ParamKind::PwmValue:
        return QStringLiteral("value %1").arg(c.pwmValue());
    case ParamKind::FourTone:
        return QStringLiteral("on %1  blink %2")
            .arg(c.pwmValue() & 0x0F, 4, 2, QLatin1Char('0'))
            .arg(c.pwmValue() >> 4, 4, 2, QLatin1Char('0'));
    case ParamKind::None:
        break;
    }
    return QString();
}

QString inputSummaryTarget(const InputConfig &c)
{
    const InputTypeInfo &info = inputTypeInfo(c.type);
    if (info.usesRoute)
        return c.route.isEmpty() ? trGpio("(empty route)")
                                 : trGpio("%1 steps").arg(c.route.size());
    if (c.type == FeedbackInput)
        return trGpio("feedback id %1").arg(c.outputId);
    if (info.usesOutput)
        return trGpio("Output %1").arg(c.outputId);
    return QString();
}

QString stepValueText(const RouteStep &s, quint8 outputType)
{
    if (!s.raw)
        return s.value ? QStringLiteral("S") : QStringLiteral("C");
    switch (stepKind(outputType))
    {
    case StepKind::ServoStateOrAngle: return QStringLiteral("%1°").arg(s.value);
    case StepKind::Aspect:            return QStringLiteral("A%1").arg(s.value);
    case StepKind::PwmValue:          return QStringLiteral("P%1").arg(s.value);
    case StepKind::Track:             return s.value ? QStringLiteral("♪%1").arg(s.value) : QStringLiteral("♪■");
    default:                          return QString::number(s.value);
    }
}

QString delayText(quint16 delay)
{
    return QStringLiteral("%1 s").arg(delay / 10.0, 0, 'f', 1);
}

} // namespace Ui
