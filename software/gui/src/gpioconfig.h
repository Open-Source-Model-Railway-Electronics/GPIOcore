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
constexpr int     MaxLinked  = 8;      // MAX_OUTPUTS in config.h
constexpr int     NumPins    = 22;     // D0..D13, A0..A5 (= 14..19), A6/A7 (= 20/21, Nano only, analog input only)
constexpr quint8  PinNA      = 0x3F;   // NA in config.h (6-bit pin fields)
constexpr quint16 AddrNone   = 0x0FFF; // 12-bit address field, 0xFFFF truncates to this
constexpr quint8  LinkNone   = 0xFF;   // linkedOutput: bits 6-0 = 0x7F means "end of route"

// ── Outputs ─────────────────────────────────────────────────────────────────
enum OutputType : quint8
{
    SingleSteady, SinglePulse, SingleFlashing,
    DoubleSteady, DoublePulse, DoubleFlashing,
    SwingLight, FirePlaceLight, TubeLight, WelderLight,
    FourToneSignal, ServoType, Pwm, OutputNotInUse,
    OutputTypeCount
};

enum class ParamKind { None, SwitchTime, Swing, Servo, PwmValue, FourTone };

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

    quint8  pin(int slot) const;           // 0..3 = A..D, 4..6 = relay A..C
    void    setPin(int slot, quint8 pin);

    OutputConfig normalized() const;       // apply firmware bitfield widths
    bool operator==(const OutputConfig &o) const;
    bool operator!=(const OutputConfig &o) const { return !(*this == o); }

    QJsonObject toJson() const;
    static OutputConfig fromJson(const QJsonObject &o);
};

// Payload incl. index byte. Legacy (firmware 0x2D) carries only 13 bytes:
// data[3] is not transmitted.
QByteArray encodeOutput(int index, const OutputConfig &c, bool legacy);
bool       decodeOutput(const QByteArray &payload, int &index, OutputConfig &c, bool legacy);

// ── Inputs ──────────────────────────────────────────────────────────────────
// Values as in config.h. Pins use INPUT_PULLUP: pressed = LOW = falling edge.
enum InputType : quint8
{
    Momentary,      // "toggle": toggles the output on the falling edge / every DCC command
    Latching,       // "absolute": output follows the input level / DCC direction
    Route,          // route on a button press (falling edge) / any DCC command (0 and 1)
    InputNotInUse,
    Recorder,
    FeedbackInput,
    InputTypeCount
};

// Type 6 was "route on the falling edge" in the GUI of 2026-11-10. The firmware
// no longer has it; it is read as a plain route and written back as Route.
constexpr quint8 OldRouteFalling = 6;

inline bool isDirectType(quint8 t) { return t == Momentary || t == Latching; }
inline bool isRouteType(quint8 t)  { return t == Route || t == OldRouteFalling; }

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

// ── Route steps (protocol v2, 4 bytes each, see PROTOCOL.md) ───────────────
constexpr int     MaxRouteSteps    = 16;      // per route (protocol limit)
constexpr int     MaxRouteStepsV1  = 8;       // linkedOutput[] of the current firmware
constexpr quint16 MaxStepDelay     = 0x3FFF;  // 14 bits of 0.1 s = 1638.3 s
constexpr quint16 DefaultStepDelay = 3;       // 0.3 s
constexpr quint16 V1StepDelay      = 5;       // current firmware: fixed 500 ms

struct RouteStep
{
    int     output = 0;                       // 0..15
    quint8  value  = 1;                       // state 0/1, or a raw value (raw = true)
    bool    raw    = false;                   // false: curved/straight, true: aspect / angle / PWM
    quint16 delay  = DefaultStepDelay;        // wait after this step, 0.1 s units

    bool operator==(const RouteStep &o) const
    { return output == o.output && value == o.value && raw == o.raw && delay == o.delay; }
    bool operator!=(const RouteStep &o) const { return !(*this == o); }
};

QByteArray encodeStep(const RouteStep &s);              // 4 bytes
RouteStep  decodeStep(const char *bytes);

// What a route step can set on an output of this type
enum class StepKind { State, ServoStateOrAngle, Aspect, PwmValue };
StepKind   stepKind(quint8 outputType);
RouteStep  defaultStep(int output, quint8 outputType);
constexpr quint8 ServoAngleMin = 10, ServoAngleMax = 170;

struct InputConfig
{
    quint16 address  = AddrNone;
    quint8  pin      = PinNA;
    quint8  type     = InputNotInUse;
    quint8  outputId = 0;
    QVector<RouteStep> route;                 // source of truth for routes
    QString name;                             // GUI only (board file), not stored on the board
    std::array<quint8, MaxLinked> linked;     // v1 wire format: bit7 = state, bits 6-0 = output id

    InputConfig() { linked.fill(LinkNone); }

    void syncLinkedFromRoute();               // route -> linked (first 8 steps, state only)
    void routeFromLinked();                   // linked -> route (v1 firmware)

    static bool   linkUsed(quint8 v)  { return (v & 0x7F) != 0x7F; }
    static quint8 linkId(quint8 v)    { return v & 0x7F; }
    static quint8 linkState(quint8 v) { return v >> 7; }
    static quint8 makeLink(quint8 id, quint8 state) { return quint8((id & 0x7F) | (state ? 0x80 : 0)); }

    InputConfig asV1() const;                 // what a v1 board stores of this input
    InputConfig normalized() const;
    bool operator==(const InputConfig &o) const;
    bool operator!=(const InputConfig &o) const { return !(*this == o); }

    QJsonObject toJson() const;
    static InputConfig fromJson(const QJsonObject &o);
};

QByteArray encodeInput(int index, const InputConfig &c);
bool       decodeInput(const QByteArray &payload, int &index, InputConfig &c);

// ── Board settings (protocol v2: GET_BOARD 0xC0 / SET_BOARD 0xCF) ───────────
enum BoardType : quint8 { BoardUno, BoardNano, BoardDccNext, BoardJackOfAllTrades, BoardTypeCount };
enum Interlock : quint8 { InterlockOff, InterlockOn, InterlockAuto };
constexpr int MaxBoardName = 16;

struct BoardSettings
{
    QString name;                       // identifies the board, max 16 ASCII characters
    quint8  type      = BoardNano;
    bool    recorder  = true;           // recorder function enabled
    bool    timing    = true;           // per-step wait times in routes (off: fixed 'delay')
    quint8  interlock = InterlockAuto;
    quint8  delay     = DefaultStepDelay; // 0.1 s units, used when timing is off

    bool operator==(const BoardSettings &o) const
    { return name == o.name && type == o.type && recorder == o.recorder && timing == o.timing
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
    bool serialPinsUsable = true;    // decoder releases D0/D1 after its boot window (protocol v2)
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

// ── Presets (OPCODE_SET_PRESET in messageParser.cpp) ───────────────────────
QStringList presetNames();

} // namespace Gpio
