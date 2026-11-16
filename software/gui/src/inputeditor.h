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
    // warnings (red) and a note (blue, information only, e.g. D0/D1)
    void setWarnings(const QStringList &warnings, const QString &note = QString());
    void refreshPinCombos();
    void refreshPinMarks();                   // pins used elsewhere in red
    // Analog inputs: the live reading of a channel (GET_ANALOG), and whether there is one at all.
    void setAnalogReading(int channel, int value);
    void setLiveReadingAvailable(bool available);
    int  analogReading() const { return m_nowValue; }   // -1 = none yet

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
    // The value editor of a route step or a custom value: curved / straight [/ angle], or a raw value
    void showStepValue(QComboBox *mode, QSpinBox *value, QToolButton *pick, const Gpio::RouteStep &s,
                       Gpio::StepKind kind);
    static void readStepValue(const QComboBox *mode, const QSpinBox *value, Gpio::StepKind kind,
                              Gpio::RouteStep &s);
    void refresh();
    void collect();
    void rebuildSteps();
    void updateSteps();
    QString stepSignature() const;
    void fillOutputCombo(QComboBox *combo);
    void addOutputItems(QComboBox *combo);   // "Out n · type"; block signals disabled (not a target)
    quint8 outputType(int output) const { return m_outputTypes.value(output, Gpio::OutputNotInUse); }

    int               m_index = 0;
    Gpio::InputConfig m_cfg;
    bool              m_loading = false;
    QStringList       m_outputNames;
    QVector<quint8>   m_outputTypes;
    Gpio::RouteLimits m_limits;
    bool              m_timing = true;
    int               m_stepsElsewhere = 0;
    QString           m_signature;

    QLabel       *m_title = nullptr;
    QLabel       *m_help  = nullptr;
    QLabel       *m_warn  = nullptr;
    QLabel       *m_note  = nullptr;
    QComboBox    *m_function = nullptr;
    QGroupBox    *m_modeBox  = nullptr;
    QRadioButton *m_optA     = nullptr;   // toggle
    QRadioButton *m_optB     = nullptr;   // absolute
    QToolButton  *m_infoDirect = nullptr;
    QGroupBox    *m_srcBox  = nullptr;
    QCheckBox    *m_useAddr = nullptr;
    QSpinBox     *m_addr    = nullptr;
    QComboBox    *m_pin     = nullptr;
    QWidget      *m_pinRow  = nullptr;
    QComboBox    *m_readAs  = nullptr;    // digital, or analog below / above / at a value
    QComboBox    *m_channel = nullptr;    // analog pin A0-A6
    QWidget      *m_channelRow = nullptr;
    QSpinBox     *m_threshold = nullptr;
    QWidget      *m_thresholdRow = nullptr;
    QLabel       *m_range   = nullptr;    // what the threshold means: pressed / released at ...
    QLabel       *m_now     = nullptr;    // live reading
    QPushButton  *m_useValue = nullptr;
    QWidget      *m_nowRow  = nullptr;
    int           m_nowValue = -1;
    bool          m_liveReading = false;
    void          updateAnalog();
    QGroupBox    *m_targetBox = nullptr;
    QComboBox    *m_output = nullptr;
    QCheckBox    *m_custom = nullptr;      // custom values of a toggle / absolute input
    QWidget      *m_customRow = nullptr;
    QWidget      *m_valueRow[2] {};        // off / curved, on / straight
    QComboBox    *m_valueMode[2] {};
    QSpinBox     *m_value[2] {};
    QToolButton  *m_valuePick[2] {};
    QGroupBox    *m_routeBox = nullptr;
    QComboBox    *m_routeTrigger = nullptr;   // any / straight only / curved only
    QWidget      *m_routeTriggerRow = nullptr;
    QLabel       *m_routeTriggerLabel = nullptr;
    QLabel       *m_routeCount = nullptr;
    QGridLayout  *m_stepGrid = nullptr;
    QPushButton  *m_addStep = nullptr;
    QVector<StepRow> m_rows;
    QLabel       *m_raw = nullptr;
};
