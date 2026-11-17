#pragma once
#include "gpioconfig.h"

#include <QVector>
#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QTimer;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QStackedWidget;

class OutputEditor : public QWidget
{
    Q_OBJECT
public:
    explicit OutputEditor(QWidget *parent = nullptr);

    void setOutput(int index, const Gpio::OutputConfig &c);
    Gpio::OutputConfig config() const { return m_cfg; }
    int  index() const { return m_index; }

    // warnings (red) and a note (blue, information only, e.g. D0/D1)
    void setWarnings(const QStringList &warnings, const QString &note = QString());
    void setConnected(bool connected);
    void setLiveAvailable(bool available);   // board supports SERVO_LIVE
    void refreshPinCombos();                  // board profile changed (DCCnext, D0/D1)
    void refreshPinMarks();                   // pins used elsewhere in red
    // Input names ("Input 3 · Sensor (occupancy)") and types, for the detectors of a block signal / semaphore arm
    void setInputs(const QStringList &names, const QVector<quint8> &types);

    enum Frog { FrogNone, FrogElectro, FrogUni };

signals:
    void changed(int index, const Gpio::OutputConfig &c);
    void testRequested(int index, int state);
    void liveServo(int index, int angle);

private:
    void buildUi();
    void refresh();            // config -> widgets
    void collect();            // widgets -> config, emits changed
    void updateVisibility();
    void updateTimeLabels();
    void fillBlockCombos();
    static Frog frogOf(const Gpio::OutputConfig &c);

    int                m_index = 0;
    Gpio::OutputConfig m_cfg;
    bool               m_loading = false;
    bool               m_connected = false;
    bool               m_liveAvailable = false;
    Frog               m_frog = FrogNone;

    QLabel         *m_title = nullptr;
    QLabel         *m_help = nullptr;
    QLabel         *m_warn = nullptr;
    QLabel         *m_note = nullptr;
    QComboBox      *m_type = nullptr;

    QGroupBox      *m_pinBox = nullptr;
    QFormLayout    *m_pinForm = nullptr;
    QComboBox      *m_pins[4] {};

    QGroupBox      *m_relayBox = nullptr;
    QFormLayout    *m_relayForm = nullptr;
    QComboBox      *m_frogMode = nullptr;
    QComboBox      *m_relays[3] {};          // electrofrog A, electrofrog B, unifrog
    QLabel         *m_frogNote = nullptr;
    QCheckBox      *m_invertFrog = nullptr;  // relay pins driven inverted (relay modules that switch on LOW)

    QGroupBox      *m_paramBox = nullptr;
    QStackedWidget *m_params = nullptr;
    QSpinBox       *m_switchTime = nullptr;
    QLabel         *m_switchTimeMs = nullptr;
    QSpinBox       *m_swingPulse = nullptr;
    QSpinBox       *m_swingPause = nullptr;
    QLabel         *m_swingMs = nullptr;
    QSpinBox       *m_servoMin = nullptr;
    QSlider        *m_servoMinSlider = nullptr;
    QSpinBox       *m_servoMax = nullptr;
    QSlider        *m_servoMaxSlider = nullptr;
    QSpinBox       *m_servoSpeed = nullptr;
    QLabel         *m_servoTime = nullptr;
    QCheckBox      *m_follow = nullptr;      // servo: semaphore arm, follows a detector (data[3] bit7)
    QWidget        *m_followBox = nullptr;
    QComboBox      *m_followInput = nullptr; // data[3] & 0x0F
    QCheckBox      *m_followSwap = nullptr;  // data[3] bit6: occupied = straight
    QCheckBox      *m_live = nullptr;
    QTimer         *m_liveTimer = nullptr;   // live servo: at most 4 updates per second
    int             m_livePending = -1;      // newest angle not sent yet
    QSpinBox       *m_pwm = nullptr;
    QSlider        *m_pwmSlider = nullptr;
    QSpinBox       *m_fade = nullptr;        // PWM: data[1] = fade time in s for a full 0-255 change
    QLabel         *m_fadeNote = nullptr;
    QDoubleSpinBox *m_blinkHz = nullptr;     // signal: blink frequency (data[1] = Hz x 10)
    QComboBox      *m_blockOwn = nullptr;    // block signal: input of its own block detector
    QComboBox      *m_blockNext = nullptr;   // input of the next block detector, or none
    QSpinBox       *m_blockAspect[3] {};     // aspect: own block occupied, next block occupied, both free
    QLabel         *m_blockText[3] {};       // what that aspect shows
    QWidget        *m_blockRow[3] {};
    QStringList     m_inputNames;
    QVector<quint8> m_inputTypes;
    QWidget        *m_soundInfo = nullptr;   // sound effect: wiring and SD card help

    QGroupBox      *m_stateBox = nullptr;
    QComboBox      *m_state = nullptr;
    QPushButton    *m_test0 = nullptr;
    QPushButton    *m_test1 = nullptr;
    QLabel         *m_raw = nullptr;
};
