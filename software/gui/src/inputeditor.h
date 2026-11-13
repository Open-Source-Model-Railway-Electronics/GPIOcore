#pragma once
#include "connections.h"
#include "gpioconfig.h"

#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QGridLayout;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QToolButton;

class InputEditor : public QWidget
{
    Q_OBJECT
public:
    explicit InputEditor(QWidget *parent = nullptr);

    void setInput(int index, const Gpio::InputConfig &c);
    Gpio::InputConfig config() const { return m_cfg; }
    int  index() const { return m_index; }

    // Output names ("Output 3 · Servo") and types, for pickers and step values
    void setOutputs(const QStringList &names, const QVector<quint8> &types);
    void setOutputNames(const QStringList &names) { setOutputs(names, m_outputTypes); }
    void setRouteLimits(const Gpio::RouteLimits &limits, int stepsUsedElsewhere);
    void setTimingEnabled(bool on);         // board setting: own wait time per step
    void setRecorderTaken(bool taken);        // another input already is the recorder
    void setWarnings(const QStringList &warnings);
    void refreshPinCombos();
    void refreshPinMarks();                   // pins used elsewhere in red

signals:
    void changed(int index, const Gpio::InputConfig &c);

private:
    struct StepRow
    {
        QLabel         *number = nullptr;
        QComboBox      *output = nullptr;
        QComboBox      *mode   = nullptr;     // curved / straight [/ angle]
        QSpinBox       *value  = nullptr;     // aspect, PWM or servo angle
        QToolButton    *pick   = nullptr;     // aspect: choose lamp by lamp
        QDoubleSpinBox *wait   = nullptr;
        QToolButton    *up = nullptr, *down = nullptr, *remove = nullptr;
    };

    void buildUi();
    void refresh();
    void collect();
    void rebuildSteps();
    void updateSteps();
    QString stepSignature() const;
    void fillOutputCombo(QComboBox *combo);
    quint8 outputType(int output) const { return m_outputTypes.value(output, Gpio::OutputNotInUse); }

    int               m_index = 0;
    Gpio::InputConfig m_cfg;
    bool              m_loading = false;
    QStringList       m_outputNames;
    QVector<quint8>   m_outputTypes;
    Gpio::RouteLimits m_limits;
    bool              m_timing = true;
    int               m_stepsElsewhere = 0;
    bool              m_recorderTaken = false;
    QString           m_signature;

    QLabel       *m_title = nullptr;
    QLabel       *m_help  = nullptr;
    QLabel       *m_warn  = nullptr;
    QComboBox    *m_function = nullptr;
    QGroupBox    *m_modeBox  = nullptr;
    QRadioButton *m_optA     = nullptr;   // toggle
    QRadioButton *m_optB     = nullptr;   // absolute
    QToolButton  *m_infoDirect = nullptr;
    QGroupBox    *m_srcBox  = nullptr;
    QCheckBox    *m_useAddr = nullptr;
    QSpinBox     *m_addr    = nullptr;
    QComboBox    *m_pin     = nullptr;
    QGroupBox    *m_targetBox = nullptr;
    QLabel       *m_targetLabel = nullptr;
    QComboBox    *m_output = nullptr;
    QToolButton  *m_infoFeedback = nullptr;
    QGroupBox    *m_recorderBox = nullptr;
    QGroupBox    *m_routeBox = nullptr;
    QComboBox    *m_routeTrigger = nullptr;   // any / straight only / curved only
    QWidget      *m_routeTriggerRow = nullptr;
    QLabel       *m_routeTriggerLabel = nullptr;
    QLabel       *m_routeCount = nullptr;
    QLabel       *m_routeV1 = nullptr;
    QGridLayout  *m_stepGrid = nullptr;
    QPushButton  *m_addStep = nullptr;
    QVector<StepRow> m_rows;
    QLabel       *m_raw = nullptr;
};
