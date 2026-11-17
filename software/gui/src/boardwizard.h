#pragma once
// File > New board wizard: a few questions for beginners (board, what to control, how, pins), then
// MainWindow::applyWizard() builds the configuration and its "Next steps" list (Beginner features only).

#include "boardtools.h"

#include <QWizard>

class QCheckBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QSpinBox;

class BoardWizard : public QWizard
{
    Q_OBJECT
public:
    explicit BoardWizard(quint8 boardType, QWidget *parent = nullptr);

    Gpio::WizardChoice choice() const;     // what the pages show now

private:
    QString fitText(bool withButtons) const;   // empty when the pins are enough
    QString summary() const;

    QRadioButton *m_board[3] {};             // Nano, Uno, DCCnext
    QLineEdit    *m_name = nullptr;
    QSpinBox     *m_count[5] {};             // servos, coil turnouts, stall motors, lights, uncouplers
    QLabel       *m_fit = nullptr;           // page 2: does not fit
    QCheckBox    *m_dcc = nullptr;
    QRadioButton *m_addrFrom = nullptr;      // addresses from ...
    QRadioButton *m_addrEmpty = nullptr;     // ... or leave them empty
    QSpinBox     *m_firstAddr = nullptr;
    QRadioButton *m_buttons[3] {};           // none, toggle buttons, switches
    QLabel       *m_fit2 = nullptr;          // page 3: with the buttons it does not fit
    QRadioButton *m_pinsInOrder = nullptr;
    QRadioButton *m_pinsEmpty = nullptr;
    QLabel       *m_summary = nullptr;
};
