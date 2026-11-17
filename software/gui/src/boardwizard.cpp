#include "boardwizard.h"
#include "infotexts.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWizardPage>

#include <functional>

using namespace Gpio;

namespace {
// A page whose Next button follows a condition, with an action when it is shown
class Page : public QWizardPage
{
public:
    std::function<bool()> complete;
    std::function<void()> init;
    bool isComplete() const override { return complete ? complete() : true; }
    void initializePage() override { if (init) init(); }
    void changed() { emit completeChanged(); }
};

QLabel *helpLabel(const QString &text, const char *style = "help")
{
    auto *l = new QLabel(text);
    l->setWordWrap(true);
    l->setObjectName(QLatin1String(style));
    return l;
}
} // namespace

BoardWizard::BoardWizard(quint8 boardType, QWidget *parent) : QWizard(parent)
{
    setObjectName(QStringLiteral("boardWizard"));
    setWindowTitle(tr("New board wizard"));
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnStartPage);
    resize(640, 520);

    // 1. Board
    auto *board = new Page;
    board->setTitle(tr("Which board do you use?"));
    board->setSubTitle(tr("A few questions, then the program sets up your board. You can change everything afterwards."));
    {
        auto *v = new QVBoxLayout(board);
        const QString texts[3] = { tr("Arduino Nano"), tr("Arduino Uno"), tr("DCCnext (ArCoMoRa): ports P1–P16 on screw terminals") };
        const quint8 types[3] = { BoardNano, BoardUno, BoardDccNext };
        for (int k = 0; k < 3; ++k)
        {
            m_board[k] = new QRadioButton(texts[k]);
            m_board[k]->setChecked(types[k] == boardType || (k == 0 && boardType != BoardUno && boardType != BoardDccNext));
            v->addWidget(m_board[k]);
        }
        auto *form = new QFormLayout;
        m_name = new QLineEdit;
        m_name->setMaxLength(MaxBoardName);
        m_name->setPlaceholderText(tr("optional, max. 16 letters, e.g. Station-left"));
        form->addRow(tr("Name"), m_name);
        v->addSpacing(8);
        v->addLayout(form);
        v->addStretch(1);
        auto *infoRow = new QHBoxLayout;
        infoRow->addWidget(helpLabel(tr("Only the basics are used: servos, turnout coils, lights, buttons and DCC.")), 1);
        infoRow->addWidget(Info::button(QStringLiteral("wizard")));
        v->addLayout(infoRow);
    }
    addPage(board);

    // 2. What do you want to control?
    auto *what = new Page;
    what->setTitle(tr("What do you want to control?"));
    what->setSubTitle(tr("How many of each? Every item gets an output of its own."));
    {
        auto *form = new QFormLayout(what);
        const QString labels[5] = { tr("Turnouts with a servo"), tr("Turnouts with two coils"),
                                    tr("Turnouts with a stall motor (MP1, Tortoise)"), tr("Lights"), tr("Uncouplers") };
        const char *const names[5] = { "servos", "coils", "stallMotors", "lights", "uncouplers" };
        const QString tips[5] = { tr("1 pin each"), tr("2 pins each: a short pulse on one coil or the other"),
                                  tr("2 pins each: the motor runs one way or the other"), tr("1 pin each: on or off"),
                                  tr("1 pin each: a short pulse") };
        for (int k = 0; k < 5; ++k)
        {
            m_count[k] = new QSpinBox;
            m_count[k]->setObjectName(QLatin1String(names[k]));
            m_count[k]->setRange(0, NumOutputs);
            m_count[k]->setToolTip(tips[k]);
            auto *row = new QHBoxLayout;
            row->addWidget(m_count[k]);
            row->addWidget(helpLabel(tips[k]), 1);
            form->addRow(labels[k], row);
        }
        m_fit = helpLabel(QString(), "error");
        m_fit->setObjectName(QStringLiteral("error"));
        m_fit->hide();
        form->addRow(m_fit);
        what->complete = [this] { return choice().items() > 0 && fitText(false).isEmpty(); };
        for (QSpinBox *s : m_count)
            connect(s, QOverload<int>::of(&QSpinBox::valueChanged), what, [this, what] {
                m_fit->setText(fitText(false));
                m_fit->setVisible(!m_fit->text().isEmpty());
                what->changed();
            });
    }
    addPage(what);

    // 3. How do you control them?
    auto *how = new Page;
    how->setTitle(tr("How do you control them?"));
    how->setSubTitle(tr("With DCC from a command station or iTrain, with buttons on the board, or both."));
    {
        auto *v = new QVBoxLayout(how);
        m_dcc = new QCheckBox(tr("With DCC: every item gets a turnout (accessory) address"));
        m_dcc->setChecked(true);
        v->addWidget(m_dcc);
        auto *addrRow = new QHBoxLayout;
        addrRow->setContentsMargins(24, 0, 0, 0);
        m_addrFrom = new QRadioButton(tr("Addresses one after another, from"));
        m_addrFrom->setChecked(true);
        m_firstAddr = new QSpinBox;
        m_firstAddr->setObjectName(QStringLiteral("firstAddress"));
        m_firstAddr->setRange(1, 4094 - NumOutputs);
        m_firstAddr->setValue(1);
        addrRow->addWidget(m_addrFrom);
        addrRow->addWidget(m_firstAddr);
        addrRow->addStretch(1);
        v->addLayout(addrRow);
        m_addrEmpty = new QRadioButton(tr("I'll set the addresses myself (leave them empty)"));
        auto *emptyRow = new QHBoxLayout;
        emptyRow->setContentsMargins(24, 0, 0, 0);
        emptyRow->addWidget(m_addrEmpty);
        v->addLayout(emptyRow);
        auto *addrGroup = new QButtonGroup(how);
        addrGroup->addButton(m_addrFrom);
        addrGroup->addButton(m_addrEmpty);
        v->addSpacing(10);
        const QString texts[3] = { tr("No buttons"), tr("A push button per item: every press switches it over"),
                                   tr("A switch per item: on = straight / on, off = curved / off") };
        auto *buttonGroup = new QButtonGroup(how);
        for (int k = 0; k < 3; ++k)
        {
            m_buttons[k] = new QRadioButton(texts[k]);
            buttonGroup->addButton(m_buttons[k]);
            v->addWidget(m_buttons[k]);
        }
        m_buttons[0]->setChecked(true);
        auto *toggleNote = helpLabel(tr("With push buttons every DCC command for the address also switches the item over."));
        v->addWidget(toggleNote);
        m_fit2 = helpLabel(QString(), "error");
        m_fit2->hide();
        v->addWidget(m_fit2);
        v->addStretch(1);
        auto update = [this, how, toggleNote] {
            m_addrFrom->setEnabled(m_dcc->isChecked());
            m_addrEmpty->setEnabled(m_dcc->isChecked());
            m_firstAddr->setEnabled(m_dcc->isChecked() && m_addrFrom->isChecked());
            toggleNote->setVisible(m_dcc->isChecked() && m_buttons[1]->isChecked());
            m_fit2->setText(fitText(true));
            m_fit2->setVisible(!m_fit2->text().isEmpty());
            how->changed();
        };
        for (QAbstractButton *b : { static_cast<QAbstractButton *>(m_dcc), static_cast<QAbstractButton *>(m_addrFrom),
                                    static_cast<QAbstractButton *>(m_buttons[0]), static_cast<QAbstractButton *>(m_buttons[1]),
                                    static_cast<QAbstractButton *>(m_buttons[2]) })
            connect(b, &QAbstractButton::toggled, how, update);
        how->complete = [this] { return fitText(true).isEmpty(); };
        how->init = update;
    }
    addPage(how);

    // 4. Pins
    auto *pins = new Page;
    pins->setTitle(tr("Which pins?"));
    pins->setSubTitle(tr("The outputs come first, then the buttons."));
    {
        auto *v = new QVBoxLayout(pins);
        m_pinsInOrder = new QRadioButton;
        m_pinsInOrder->setChecked(true);
        m_pinsEmpty = new QRadioButton(tr("I'll choose the pins myself (leave them empty)"));
        v->addWidget(m_pinsInOrder);
        v->addWidget(m_pinsEmpty);
        v->addWidget(helpLabel(tr("In order means the next free pins of this board, one after another. The Pin map tab "
                                  "shows them afterwards; you can change any pin later.")));
        v->addStretch(1);
        pins->init = [this] {
            const WizardChoice c = choice();
            m_pinsInOrder->setText(tr("Assign pins in order from %1").arg(pinNameOn(c.boardType, wizardPins(c.boardType).value(0))));
        };
    }
    addPage(pins);

    // 5. Summary
    auto *done = new Page;
    done->setTitle(tr("Summary"));
    done->setSubTitle(tr("Finish sets up the board. A Next steps list above the canvas shows what is left to do."));
    {
        auto *v = new QVBoxLayout(done);
        m_summary = new QLabel;
        m_summary->setObjectName(QStringLiteral("wizardSummary"));
        m_summary->setWordWrap(true);
        m_summary->setTextFormat(Qt::RichText);
        v->addWidget(m_summary);
        v->addStretch(1);
        done->init = [this] { m_summary->setText(summary()); };
    }
    addPage(done);
}

WizardChoice BoardWizard::choice() const
{
    WizardChoice c;
    c.boardType   = m_board[1]->isChecked() ? quint8(BoardUno) : m_board[2]->isChecked() ? quint8(BoardDccNext) : quint8(BoardNano);
    c.name        = m_name->text();
    c.servos      = m_count[0]->value();
    c.coils       = m_count[1]->value();
    c.stallMotors = m_count[2]->value();
    c.lights      = m_count[3]->value();
    c.uncouplers  = m_count[4]->value();
    c.dcc         = m_dcc->isChecked();
    c.firstAddress = m_addrFrom->isChecked() ? m_firstAddr->value() : 0;
    c.buttons     = m_buttons[1]->isChecked() ? WizardChoice::ToggleButtons
                  : m_buttons[2]->isChecked() ? WizardChoice::SwitchButtons : WizardChoice::NoButtons;
    c.pinsInOrder = m_pinsInOrder->isChecked();
    return c;
}

QString BoardWizard::fitText(bool withButtons) const
{
    WizardChoice c = choice();
    if (!withButtons)
        c.buttons = WizardChoice::NoButtons;
    const int have = wizardPins(c.boardType).size();
    if (c.items() > NumOutputs)
        return tr("That is %1 items, but a board has %2 outputs. Choose fewer.").arg(c.items()).arg(NumOutputs);
    if (c.pinsNeeded() <= have)
        return QString();
    return withButtons ? tr("With a button per item this needs %1 pins, but the %2 has %3. Choose fewer items on the "
                            "previous page, or no buttons.").arg(c.pinsNeeded()).arg(boardTypeName(c.boardType)).arg(have)
                       : tr("This needs %1 pins, but the %2 has %3. Choose fewer.")
                             .arg(c.pinsNeeded()).arg(boardTypeName(c.boardType)).arg(have);
}

QString BoardWizard::summary() const
{
    const WizardChoice c = choice();
    const BoardConfig b = wizardBoard(c);
    QString html = tr("<p><b>%1</b>%2 · %3 of %4 pins used</p>")
                       .arg(boardTypeName(c.boardType), c.name.isEmpty() ? QString() : QStringLiteral(" “%1”").arg(c.name.toHtmlEscaped()))
                       .arg(c.pinsInOrder ? c.pinsNeeded() : 0).arg(wizardPins(c.boardType).size());
    html += QStringLiteral("<table cellspacing=\"0\" cellpadding=\"2\">");
    for (int o = 0; o < NumOutputs; ++o)
    {
        const OutputConfig &out = b.outputs[o];
        if (out.type == OutputNotInUse)
            continue;
        QStringList pins;
        for (int k = 0; k < outputTypeInfo(out.type).pinCount; ++k)
            pins << (out.pin(k) == PinNA ? tr("pin to choose") : pinNameOn(c.boardType, out.pin(k)));
        QStringList control;
        const InputConfig &in = b.inputs[o];
        if (in.type != InputNotInUse)
        {
            if (in.address != AddrNone) control << tr("DCC %1").arg(in.address);
            else if (c.dcc)             control << tr("DCC address to set");
            if (c.buttons != WizardChoice::NoButtons)
                control << (in.pin == PinNA ? tr("button pin to choose")
                                            : (c.buttons == WizardChoice::ToggleButtons ? tr("button %1") : tr("switch %1"))
                                                  .arg(pinNameOn(c.boardType, in.pin)));
        }
        html += QStringLiteral("<tr><td>%1&nbsp;&nbsp;</td><td><b>%2</b>&nbsp;&nbsp;</td><td>%3&nbsp;&nbsp;</td><td>%4</td></tr>")
                    .arg(tr("Out %1").arg(o + 1), out.name.toHtmlEscaped(), pins.join(QStringLiteral(", ")),
                         control.join(QStringLiteral(", ")));
    }
    html += QStringLiteral("</table>");
    html += tr("<p>Finish replaces the configuration on screen (you are asked first if it has your own work in it). "
               "Then save it with Ctrl+S.</p>");
    return html;
}
