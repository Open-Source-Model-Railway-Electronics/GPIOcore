#pragma once
// Configuration model of a GPIOcore board, mirroring output.h / input.h of
// the firmware (including the bitfield widths, so the GUI shows what the
// device will actually store).

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <array>

namespace Gpio {

constexpr int     NumOutputs = 16;
constexpr int     NumInputs  = 16;
constexpr int     NumPins    = 22;     // D0..D13, A0..A5 (= 14..19), A6/A7 (= 20/21, Nano only, analog input only)
constexpr quint8  PinNA      = 0x3F;   // NA in config.h (6-bit pin fields)
constexpr quint16 AddrNone   = 0x0FFF; // 12-bit address field, 0xFFFF truncates to this

// ── Outputs ─────────────────────────────────────────────────────────────────
enum OutputType : quint8
{
    SingleSteady, SinglePulse, SingleFlashing,
    DoubleSteady, DoublePulse, DoubleFlashing,
    SwingLight, FirePlaceLight, TubeLight, WelderLight,
    FourToneSignal, ServoType, Pwm, OutputNotInUse,
    SoundEffect,    // DFPlayer Mini on TX (D1): the input chooses the track (custom values, route step, DCC extended)
    OutputTypeCount
};

enum class ParamKind { None, SwitchTime, Swing, Servo, PwmValue, FourTone };
constexpr quint8 SoundPin = 1;   // a sound effect output always uses TX (D1)

struct OutputTypeInfo
{
    const char *key;        // stable id for JSON
    const char *name;       // display name
    int         pinCount;   // how many of pinA..pinD are used
    bool        relays;     // electrofrog / unifrog relays apply
    bool        needsPwm;   // pins must be PWM capable
    ParamKind   params;
    int         timeUnitMs; // for SwitchTime / Swing: firmware multiplies the raw value by this
    const char *help;
};

const OutputTypeInfo &outputTypeInfo(quint8 type);
// Translated texts (context "Gpio", see i18n/)
QString trGpio(const char *text);
QString outputTypeName(quint8 type);
QString outputTypeHelp(quint8 type);

struct OutputConfig
{
    quint8 pinA   = PinNA, pinB   = PinNA, pinC   = PinNA, pinD = PinNA;
    quint8 relayA = PinNA, relayB = PinNA, relayC = PinNA;
    quint8 type   = OutputNotInUse;
    quint8 state  = 0;
    std::array<quint8, 4> data { { 0, 0, 0, 0 } };   // union, AVR little endian
    QString name;                          // GUI only (board file), not stored on the board
    QStringList lampColors;                // GUI only: colours of signal lamps 1-4 ("#RRGGBB"), empty = default

    // union views
    quint32 switchTime() const;
    void    setSwitchTime(quint32 v);
    quint16 swingPulse() const  { return quint16(data[0] | data[1] << 8); }
    quint16 swingPause() const  { return quint16(data[2] | data[3] << 8); }
    void    setSwing(quint16 pulse, quint16 pause);
    quint8  servoMin()     const { return data[0]; }
    quint8  servoMax()     const { return data[1]; }
    quint8  servoSpeed()   const { return data[2]; }
    quint8  servoPattern() const { return data[3]; }
    quint8  pwmValue()     const { return data[0]; }
    quint8  pwmFade()      const { return data[1]; }   // s for a full 0-255 change, 0 = at once
    // 4-aspect signal as automatic block signal (data[3] bit7): data[2] = own block input | next << 4,
    // data[3] = 0x80 | 0x40 (has a next block) | green lamp << 4 | yellow lamp << 2 | red lamp (0-3 = A-D)
    bool    blockSignal()  const { return type == FourToneSignal && (data[3] & 0x80); }
    int     blockOwn()     const { return data[2] & 0x0F; }
    int     blockNext()    const { return (data[3] & 0x40) ? data[2] >> 4 : -1; }   // -1 = none
    int     blockLamp(int k) const { return (data[3] >> (2 * k)) & 3; }        // k: 0 red, 1 yellow, 2 green
    void    setBlockSignal(int own, int next, int red, int yellow, int green);    // next -1 = none

    quint8  pin(int slot) const;           // 0..3 = A..D, 4..6 = relay A..C
    void    setPin(int slot, quint8 pin);

    OutputConfig normalized() const;       // apply firmware bitfield widths
    bool operator==(const OutputConfig &o) const;
    bool operator!=(const OutputConfig &o) const { return !(*this == o); }

    QJsonObject toJson() const;
    static OutputConfig fromJson(const QJsonObject &o);
};

// Payload incl. index byte, 14 bytes.
QByteArray encodeOutput(int index, const OutputConfig &c);
bool       decodeOutput(const QByteArray &payload, int &index, OutputConfig &c);

// ── Inputs ──────────────────────────────────────────────────────────────────
// Values as in config.h. Pins use INPUT_PULLUP: pressed = LOW = falling edge.
enum InputType : quint8
{
    Momentary,      // "toggle": toggles the output on the falling edge / every DCC command
    Latching,       // "absolute": output follows the input level / DCC direction
    Route,          // route on a button press (falling edge) / any DCC command (0 and 1)
    InputNotInUse,
    Sensor,         // 4: occupancy detector, only reports its state (automatic block signals)
    ReservedFeedback,   // 5: was the recorder's feedback sensor, the firmware reads it as not in use
    RouteStraight,  // route on a button press / a DCC 'straight' (1) command only
    RouteCurved,    // route on a button press / a DCC 'curved' (0) command only
    InputTypeCount
};

inline bool isDirectType(quint8 t) { return t == Momentary || t == Latching; }
inline bool isRouteType(quint8 t)  { return t == Route || t == RouteStraight || t == RouteCurved; }

struct InputTypeInfo
{
    const char *key;
    const char *name;
    bool        usesAddress;
    bool        usesOutput;
    bool        usesRoute;
    const char *help;
};

const InputTypeInfo &inputTypeInfo(quint8 type);
QString inputTypeName(quint8 type);
QString inputTypeHelp(quint8 type);

// ── Route steps (4 bytes each, see PROTOCOL.md) ────────────────────────────
constexpr int     MaxRouteSteps    = 16;      // per route (protocol limit)
constexpr quint16 MaxStepDelay     = 0x3FFF;  // 14 bits of 0.1 s = 1638.3 s
constexpr quint16 DefaultStepDelay = 3;       // 0.3 s

struct RouteStep
{
    int     output = 0;                       // 0..15
    quint8  value  = 1;                       // state 0/1, or a raw value (raw = true)
    bool    raw    = false;                   // false: curved/straight, true: aspect / angle / PWM
    quint16 delay  = DefaultStepDelay;        // wait after this step, 0.1 s units
    bool    values = false;                   // STEP_VALUES (0x20): a custom value of a toggle / absolute input

    bool operator==(const RouteStep &o) const
    { return output == o.output && value == o.value && raw == o.raw && delay == o.delay && values == o.values; }
    bool operator!=(const RouteStep &o) const { return !(*this == o); }
};

QByteArray encodeStep(const RouteStep &s);              // 4 bytes
RouteStep  decodeStep(const char *bytes);

// What a route step can set on an output of this type
enum class StepKind { State, ServoStateOrAngle, Aspect, PwmValue, Track };   // Track: sound effect, 0 = stop
StepKind   stepKind(quint8 outputType);
RouteStep  defaultStep(int output, quint8 outputType);
constexpr quint8 ServoAngleMin = 10, ServoAngleMax = 170;

// Analog inputs (firmware config.h ANALOG_*): the pin byte holds 0x20 | mode << 3 | channel
// instead of a pin number; the threshold is stored as value / 8 and sent as value / 4 in payload byte 6.
enum AnalogMode : quint8 { AnalogNone, AnalogBelow, AnalogAbove, AnalogAt };
constexpr int AnalogMaxThreshold = 1016;   // 7 bits x 8
constexpr int AnalogHysteresis   = 20;     // below / above: released 20 counts past the threshold
constexpr int AnalogWindow       = 10;     // at: threshold +/- 10
constexpr int AnalogMaxValue     = 1023;   // analogRead()

struct InputConfig
{
    quint16 address  = AddrNone;
    quint8  pin      = PinNA;                 // analog: the real pin, A0-A7 = 14-21
    quint8  type     = InputNotInUse;
    quint8  outputId = 0;
    quint8  analog   = AnalogNone;            // AnalogMode: compare the voltage instead of reading a button
    quint16 threshold = 512;                  // analog: 0-1016 in steps of 8
    QVector<RouteStep> route;                 // source of truth for routes
    QString name;                             // GUI only (board file), not stored on the board

    bool   isAnalog() const      { return analog != AnalogNone; }
    int    analogChannel() const { return pin - 14; }    // 0-7 = A0-A7
    quint8 pinByte() const;                   // the pin field on the wire / in EEPROM
    void   setPinByte(quint8 b);
    static int roundThreshold(int t);         // to a multiple of 8, 0-1016

    // Custom values of a toggle / absolute input: its route slot holds exactly 2 steps marked
    // 'values', step 0 for state 0 (off / curved) and step 1 for state 1 (on / straight).
    bool hasCustomValues() const;
    // on: start from the defaults for this output type (sound: off = stop, on = track 1); off: no steps.
    void setCustomValues(bool on, quint8 outputType);
    // After an edit: a direct input keeps either its 2 value steps (on its own output, no wait) or an
    // empty route, so the board's slot is cleared; other inputs drop a pair of value steps.
    void syncCustomValues();

    InputConfig normalized() const;
    bool operator==(const InputConfig &o) const;
    bool operator!=(const InputConfig &o) const { return !(*this == o); }

    QJsonObject toJson() const;
    static InputConfig fromJson(const QJsonObject &o);
};

QByteArray encodeInput(int index, const InputConfig &c);
bool       decodeInput(const QByteArray &payload, int &index, InputConfig &c);

// ── Board settings (GET_BOARD 0xC0 / SET_BOARD 0xCF) ────────────────────────
enum BoardType : quint8 { BoardUno, BoardNano, BoardDccNext, BoardJackOfAllTrades, BoardTypeCount };
enum Interlock : quint8 { InterlockOff, InterlockOn, InterlockAuto };
constexpr int MaxBoardName = 16;

struct BoardSettings
{
    QString name;                       // identifies the board, max 16 ASCII characters
    quint8  type      = BoardNano;
    bool    timing    = false;          // per-step wait times in routes (off: fixed delay), firmware default
    quint8  interlock = InterlockAuto;
    quint8  delay     = DefaultStepDelay; // 0.1 s units, used when timing is off

    bool operator==(const BoardSettings &o) const
    { return name == o.name && type == o.type && timing == o.timing
          && interlock == o.interlock && delay == o.delay; }
    bool operator!=(const BoardSettings &o) const { return !(*this == o); }

    QJsonObject toJson() const;
    static BoardSettings fromJson(const QJsonObject &o);
};
QString    boardTypeName(quint8 type);
QString    cleanBoardName(const QString &name);       // ASCII, no path characters, max 16
QByteArray encodeBoard(const BoardSettings &b);       // payload without the length byte
bool       decodeBoard(const QByteArray &payload, BoardSettings &b);

// ── Whole board ─────────────────────────────────────────────────────────────
struct BoardConfig
{
    BoardSettings board;
    std::array<OutputConfig, NumOutputs> outputs;
    std::array<InputConfig,  NumInputs>  inputs;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject &o, BoardConfig &out, QString *error);
};

// ── Board / pins (Arduino Uno / Nano, or DCCnext) ──────────────────────────
struct BoardOptions
{
    quint8 type           = BoardNano;
    bool dccNext          = false;   // ArCoMoRa DCCnext: ports 1-10 = D3-D12, 11-16 = A0-A5, no D13
    bool serialPinsUsable = true;    // decoder releases D0/D1 after its boot window
};
BoardOptions &boardOptions();
void          setBoardType(quint8 type);   // also sets dccNext

QString pinName(quint8 pin);            // "D5", "A0", "—"
QString pinLabel(quint8 pin);           // pinName plus "port n" on a DCCnext
int     dccNextPort(quint8 pin);        // 1..16, or 0 when the pin is not a DCCnext port
bool    isPwmPin(quint8 pin);           // same list as isPwmPin() in output.cpp
QString pinReservedReason(quint8 pin);  // non-empty: the pin cannot be used (conflict)
QString pinNote(quint8 pin);            // non-empty: usable, but with a caveat (D0/D1)
QString pinOutputReason(quint8 pin);    // non-empty: the pin cannot be an output (A6/A7)
bool    isServoTimerPin(quint8 pin);    // D9/D10: no PWM while a servo exists (Servo library, timer 1)

// ── Presets ─────────────────────────────────────────────────────────────────
QStringList presetNames();
// A preset as a complete set of outputs and inputs (computed by the GUI, written as normal data).
// Pins from D3 upwards, skipping D13 (config LED); used inputs: absolute, DCC 1.., input n -> output n.
BoardConfig presetBoard(int preset);

} // namespace Gpio
