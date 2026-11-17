#include "gpioconfig.h"

#include <QCoreApplication>

#include <QJsonArray>

namespace Gpio {

// ── Type tables ─────────────────────────────────────────────────────────────
static const OutputTypeInfo kOutputTypes[OutputTypeCount + 1] =
{
    // key               name                pins relays pwm  params                 unit
    { "singleSteady",    QT_TRANSLATE_NOOP("Gpio", "Single steady"),     1, true,  false, ParamKind::None,       0,
      QT_TRANSLATE_NOOP("Gpio", "One pin follows the state.") },
    { "singlePulse",     QT_TRANSLATE_NOOP("Gpio", "Single pulse"),      1, true,  false, ParamKind::SwitchTime, 100,
      QT_TRANSLATE_NOOP("Gpio", "One pin is pulsed for the switch time (firmware: value x 100 ms).") },
    { "singleFlashing",  QT_TRANSLATE_NOOP("Gpio", "Single flashing"),   1, false, false, ParamKind::SwitchTime, 10,
      QT_TRANSLATE_NOOP("Gpio", "Pin A blinks while the state is on (value x 10 ms per half period).") },
    { "doubleSteady",    QT_TRANSLATE_NOOP("Gpio", "Double steady"),     2, true,  false, ParamKind::None,       0,
      QT_TRANSLATE_NOOP("Gpio", "Pin A = state, pin B = inverted state.") },
    { "doublePulse",     QT_TRANSLATE_NOOP("Gpio", "Double pulse"),      2, true,  false, ParamKind::SwitchTime, 10,
      QT_TRANSLATE_NOOP("Gpio", "Turnout coil pair: pin A or B is pulsed (value x 10 ms).") },
    { "doubleFlashing",  QT_TRANSLATE_NOOP("Gpio", "Double flashing"),   2, false, false, ParamKind::SwitchTime, 10,
      QT_TRANSLATE_NOOP("Gpio", "Pins A and B blink alternately (value x 10 ms per half period).") },
    { "swingLight",      QT_TRANSLATE_NOOP("Gpio", "Swing light"),       2, false, false, ParamKind::Swing,      10,
      QT_TRANSLATE_NOOP("Gpio", "Level crossing style: A, B, pause (values x 10 ms).") },
    { "firePlaceLight",  QT_TRANSLATE_NOOP("Gpio", "Fireplace"),         2, false, true,  ParamKind::None,       0,
      QT_TRANSLATE_NOOP("Gpio", "Random flicker on PWM pins A and B.") },
    { "tubeLight",       QT_TRANSLATE_NOOP("Gpio", "TL tube"),           1, false, true,  ParamKind::None,       0,
      QT_TRANSLATE_NOOP("Gpio", "Fluorescent tube start-up flicker on PWM pin A.") },
    { "welderLight",     QT_TRANSLATE_NOOP("Gpio", "Welder"),            1, false, false, ParamKind::None,       0,
      QT_TRANSLATE_NOOP("Gpio", "Random welding flashes on pin A.") },
    { "fourToneSignal",  QT_TRANSLATE_NOOP("Gpio", "4-aspect signal"),   4, false, false, ParamKind::FourTone,   0,
      QT_TRANSLATE_NOOP("Gpio", "Four LEDs A-D. The aspect comes from DCC extended (signal) packets or a route; the board keeps the last aspect in EEPROM.") },
    { "servo",           QT_TRANSLATE_NOOP("Gpio", "Servo"),             1, true,  false, ParamKind::Servo,      0,
      QT_TRANSLATE_NOOP("Gpio", "Servo on pin A, sweeps between min and max.") },
    { "pwm",             QT_TRANSLATE_NOOP("Gpio", "PWM"),               1, false, true,  ParamKind::PwmValue,   0,
      QT_TRANSLATE_NOOP("Gpio", "PWM brightness on pin A. With a fade time it changes slowly to every new value.") },
    { "notInUse",        QT_TRANSLATE_NOOP("Gpio", "Not in use"),        0, false, false, ParamKind::None,       0,
      QT_TRANSLATE_NOOP("Gpio", "Output disabled.") },
    { "soundEffect",     QT_TRANSLATE_NOOP("Gpio", "Sound effect"),      1, false, false, ParamKind::None,       0,
      QT_TRANSLATE_NOOP("Gpio", "Plays an MP3 track on a DFPlayer Mini wired to TX (D1). The input chooses the track: its custom "
                                "values, a route step or a DCC extended value (0 = stop); without custom values on = track 1, "
                                "off = stop. It only plays when no PC is connected: with the GUI connected, D1 is the USB line "
                                "and the board only reports the track.") },
    { "blockSignal",     QT_TRANSLATE_NOOP("Gpio", "Block signal"),      4, false, false, ParamKind::Block,      0,
      QT_TRANSLATE_NOOP("Gpio", "Automatic signal with lamps A-D (B-D optional). The board chooses the aspect itself from two "
                                "block detectors: its own block and the next one. Inputs, routes and DCC cannot set it.") },
    { "unknown",         QT_TRANSLATE_NOOP("Gpio", "Unknown type"),      4, true,  false, ParamKind::None,       0,
      QT_TRANSLATE_NOOP("Gpio", "Type value not known by this GUI.") },
};

const OutputTypeInfo &outputTypeInfo(quint8 type)
{
    return kOutputTypes[type < OutputTypeCount ? type : OutputTypeCount];
}

static const InputTypeInfo kInputTypes[InputTypeCount + 1] =
{
    // key          name          addr   output route
    { "momentary",  QT_TRANSLATE_NOOP("Gpio", "Toggle"),       true,  true,  false, QT_TRANSLATE_NOOP("Gpio", "Each press (falling edge) or DCC command toggles the output.") },
    { "latching",   QT_TRANSLATE_NOOP("Gpio", "Absolute"),     true,  true,  false, QT_TRANSLATE_NOOP("Gpio", "The output follows the input level or the DCC direction.") },
    { "route",      QT_TRANSLATE_NOOP("Gpio", "Route"),        true,  false, true,  QT_TRANSLATE_NOOP("Gpio", "Sets up to 16 outputs one after another, each step with its own wait time. Starts when the button is pressed, or on any DCC command for its address (straight and curved).") },
    { "notInUse",   QT_TRANSLATE_NOOP("Gpio", "Not in use"),   false, false, false, QT_TRANSLATE_NOOP("Gpio", "Input disabled.") },
    { "sensor",     QT_TRANSLATE_NOOP("Gpio", "Sensor (occupancy)"), false, false, false, QT_TRANSLATE_NOOP("Gpio", "An occupancy detector or other sensor. It switches nothing itself; automatic block signals read it.") },
    // 5: reserved (was the recorder's feedback input); old files ("feedback", also "recorder") and boards load it as not in use
    { "feedback",   QT_TRANSLATE_NOOP("Gpio", "Not in use"),   false, false, false, QT_TRANSLATE_NOOP("Gpio", "Input disabled.") },
    { "routeStraight", QT_TRANSLATE_NOOP("Gpio", "Route"),     true,  false, true,  QT_TRANSLATE_NOOP("Gpio", "Route that starts on a button press, or only on a DCC 'straight' command for its address.") },
    { "routeCurved",   QT_TRANSLATE_NOOP("Gpio", "Route"),     true,  false, true,  QT_TRANSLATE_NOOP("Gpio", "Route that starts on a button press, or only on a DCC 'curved' command for its address.") },
    { "unknown",    QT_TRANSLATE_NOOP("Gpio", "Unknown"),      true,  true,  true,  QT_TRANSLATE_NOOP("Gpio", "Type value not known by this GUI.") },
};

const InputTypeInfo &inputTypeInfo(quint8 type)
{
    return kInputTypes[type < InputTypeCount ? type : InputTypeCount];
}

QString trGpio(const char *text) { return QCoreApplication::translate("Gpio", text); }
QString outputTypeName(quint8 type) { return trGpio(outputTypeInfo(type).name); }
QString outputTypeHelp(quint8 type) { return trGpio(outputTypeInfo(type).help); }
QString inputTypeName(quint8 type)  { return trGpio(inputTypeInfo(isRouteType(type) ? quint8(Route) : type).name); }
QString inputTypeHelp(quint8 type)  { return trGpio(inputTypeInfo(type).help); }

// ── OutputConfig ────────────────────────────────────────────────────────────
quint32 OutputConfig::switchTime() const
{
    return quint32(data[0]) | quint32(data[1]) << 8 | quint32(data[2]) << 16 | quint32(data[3]) << 24;
}

void OutputConfig::setSwitchTime(quint32 v)
{
    for (int i = 0; i < 4; ++i)
        data[i] = quint8(v >> (8 * i));
}

void OutputConfig::setBlockInputs(int own, int next)
{
    data[3] = quint8((own & 0x0F) | ((next >= 0 ? next : own) & 0x0F) << 4);
}

QVector<int> OutputConfig::watchedInputs() const
{
    if (type == BlockSignal)
        return blockNext() >= 0 ? QVector<int>{ blockOwn(), blockNext() } : QVector<int>{ blockOwn() };
    if (servoFollows() >= 0)
        return { servoFollows() };
    return {};
}

void OutputConfig::setSwing(quint16 pulse, quint16 pause)
{
    data[0] = quint8(pulse);  data[1] = quint8(pulse >> 8);
    data[2] = quint8(pause);  data[3] = quint8(pause >> 8);
}

quint8 OutputConfig::pin(int slot) const
{
    switch (slot)
    {
    case 0: return pinA;   case 1: return pinB;   case 2: return pinC;   case 3: return pinD;
    case 4: return relayA; case 5: return relayB; case 6: return relayC;
    }
    return PinNA;
}

void OutputConfig::setPin(int slot, quint8 p)
{
    switch (slot)
    {
    case 0: pinA   = p; break;  case 1: pinB   = p; break;
    case 2: pinC   = p; break;  case 3: pinD   = p; break;
    case 4: relayA = p; break;  case 5: relayB = p; break;
    case 6: relayC = p; break;
    }
}

OutputConfig OutputConfig::normalized() const
{
    OutputConfig c = *this;
    for (int s = 0; s < 7; ++s)
        c.setPin(s, c.pin(s) & 0x3F);   // 6-bit fields
    c.type  &= 0x0F;                     // 4-bit field
    c.state &= 0x01;                     // 1-bit field
    return c;
}

bool OutputConfig::operator==(const OutputConfig &o) const
{
    for (int s = 0; s < 7; ++s)
        if (pin(s) != o.pin(s)) return false;
    return type == o.type && state == o.state && invertFrog == o.invertFrog && data == o.data;
}

static QJsonValue pinToJson(quint8 p) { return p == PinNA ? QJsonValue() : QJsonValue(int(p)); }
static quint8 pinFromJson(const QJsonValue &v) { return v.isDouble() ? quint8(v.toInt() & 0x3F) : PinNA; }

QJsonObject OutputConfig::toJson() const
{
    QJsonObject o;
    if (!name.isEmpty()) o["name"] = name;
    if (!lampColors.isEmpty()) o["lampColors"] = QJsonArray::fromStringList(lampColors);
    o["type"]   = QString::fromUtf8(outputTypeInfo(type).key);
    o["pinA"]   = pinToJson(pinA);   o["pinB"]   = pinToJson(pinB);
    o["pinC"]   = pinToJson(pinC);   o["pinD"]   = pinToJson(pinD);
    o["relayA"] = pinToJson(relayA); o["relayB"] = pinToJson(relayB);
    o["relayC"] = pinToJson(relayC);
    o["state"]  = int(state);
    if (invertFrog) o["invertFrog"] = true;   // only when set: older files stay as they are
    QJsonArray d;
    for (quint8 b : data) d.append(int(b));
    o["data"]   = d;
    return o;
}

OutputConfig OutputConfig::fromJson(const QJsonObject &o)
{
    OutputConfig c;
    c.name = o["name"].toString();
    for (const QJsonValue &v : o["lampColors"].toArray())
        c.lampColors << v.toString();
    const QString key = o["type"].toString();
    for (quint8 t = 0; t < OutputTypeCount; ++t)
        if (key == QLatin1String(kOutputTypes[t].key)) c.type = t;
    c.pinA   = pinFromJson(o["pinA"]);   c.pinB   = pinFromJson(o["pinB"]);
    c.pinC   = pinFromJson(o["pinC"]);   c.pinD   = pinFromJson(o["pinD"]);
    c.relayA = pinFromJson(o["relayA"]); c.relayB = pinFromJson(o["relayB"]);
    c.relayC = pinFromJson(o["relayC"]);
    c.state  = quint8(o["state"].toInt() & 1);
    c.invertFrog = o["invertFrog"].toBool();
    const QJsonArray d = o["data"].toArray();
    for (int i = 0; i < 4 && i < d.size(); ++i)
        c.data[i] = quint8(d.at(i).toInt());
    if (c.type == FourToneSignal && (c.data[3] & 0x80))
        c.data[2] = c.data[3] = 0;       // the removed automatic mode (now the block signal type): a manual signal
    return c;
}

QByteArray encodeOutput(int index, const OutputConfig &c)
{
    QByteArray p;
    p.append(char(index));
    for (int s = 0; s < 7; ++s)
        p.append(char(c.pin(s)));
    p.append(char(c.type));
    p.append(char(c.state | (c.invertFrog ? 0x02 : 0)));   // bit1: frog relays inverted
    for (quint8 b : c.data)
        p.append(char(b));
    return p;   // 14 bytes
}

bool decodeOutput(const QByteArray &p, int &index, OutputConfig &c)
{
    if (p.size() < 14)
        return false;
    auto at = [&](int i) { return quint8(p.at(i)); };
    index = at(0);
    for (int s = 0; s < 7; ++s)
        c.setPin(s, at(1 + s));
    c.type  = at(8);
    c.state = at(9) & 0x01;
    c.invertFrog = at(9) & 0x02;
    for (int i = 0; i < 4; ++i)
        c.data[i] = at(10 + i);
    return index >= 0 && index < NumOutputs;
}

// ── Route steps ─────────────────────────────────────────────────────────────
// byte 0: bits 0-3 output id, bit 4 raw value, bit 5 custom value (STEP_VALUES), bits 6-7 reserved (0)
// byte 1: value
// byte 2: delay bits 0-7    byte 3: bits 0-5 = delay bits 8-13, bits 6-7 reserved (0)
QByteArray encodeStep(const RouteStep &s)
{
    const quint16 d = qMin<quint16>(s.delay, MaxStepDelay);
    QByteArray b(4, '\0');
    b[0] = char((s.output & 0x0F) | (s.raw ? 0x10 : 0) | (s.values ? 0x20 : 0));
    b[1] = char(s.value);
    b[2] = char(d & 0xFF);
    b[3] = char((d >> 8) & 0x3F);
    return b;
}

RouteStep decodeStep(const char *bytes)
{
    const quint8 *b = reinterpret_cast<const quint8 *>(bytes);
    RouteStep s;
    s.output = b[0] & 0x0F;
    s.raw    = (b[0] & 0x10) != 0;
    s.values = (b[0] & 0x20) != 0;
    s.value  = b[1];
    s.delay  = quint16(b[2] | (b[3] & 0x3F) << 8);
    return s;
}

StepKind stepKind(quint8 outputType)
{
    switch (outputType)
    {
    case ServoType:      return StepKind::ServoStateOrAngle;
    case FourToneSignal: return StepKind::Aspect;
    case Pwm:            return StepKind::PwmValue;
    case SoundEffect:    return StepKind::Track;
    }
    return StepKind::State;
}

RouteStep defaultStep(int output, quint8 outputType)
{
    RouteStep s;
    s.output = output;
    switch (stepKind(outputType))
    {
    case StepKind::Aspect:   s.raw = true;  s.value = 0;   break;
    case StepKind::PwmValue: s.raw = true;  s.value = 255; break;
    case StepKind::Track:    s.raw = true;  s.value = 1;   break;
    default:                 s.raw = false; s.value = 1;   break;
    }
    return s;
}

// ── InputConfig ─────────────────────────────────────────────────────────────
quint8 InputConfig::pinByte() const
{
    if (!isAnalog() || pin < 14 || pin >= 22)
        return pin;
    return quint8(0x20 | (analog - 1) << 3 | (pin - 14));
}

void InputConfig::setPinByte(quint8 b)
{
    const int mode = (b >> 3) & 3;
    if (b != PinNA && (b & 0x20) && mode <= 2)   // firmware isAnalogInput()
    {
        analog = quint8(AnalogBelow + mode);
        pin    = quint8(14 + (b & 7));
    }
    else
    {
        analog = AnalogNone;
        pin    = b;
    }
}

int InputConfig::roundThreshold(int t)
{
    return qBound(0, (t + 4) / 8 * 8, AnalogMaxThreshold);
}

bool InputConfig::hasCustomValues() const
{
    return isDirectType(type) && route.size() == 2 && route[0].values && route[1].values;
}

void InputConfig::setCustomValues(bool on, quint8 outputType)
{
    route.clear();
    for (int k = 0; on && k < 2; ++k)
    {
        RouteStep s = defaultStep(outputId, outputType);
        if (!s.raw)
            s.value = quint8(k);                     // curved / straight, as without custom values
        else if (k == 0 && (stepKind(outputType) == StepKind::PwmValue || stepKind(outputType) == StepKind::Track))
            s.value = 0;                             // off: dark / stop
        s.delay  = 0;
        s.values = true;
        route.append(s);
    }
}

void InputConfig::syncCustomValues()
{
    const bool pair = route.size() == 2 && route[0].values && route[1].values;
    if (isDirectType(type))
    {
        if (!pair)
            route.clear();
        for (RouteStep &s : route)
        {
            s.output = outputId & 0x0F;
            s.delay  = 0;
        }
    }
    else
    {
        if (pair)
            route.clear();
        for (RouteStep &s : route)
            s.values = false;
    }
}

InputConfig InputConfig::normalized() const
{
    InputConfig c = *this;
    c.address  &= 0x0FFF;   // 12 bits
    c.pin      &= 0x3F;     //  6 bits
    c.type     &= 0x07;     //  3 bits
    if (c.type == ReservedFeedback)
        c.type = InputNotInUse;          // the feedback input was removed, like in the firmware
    c.outputId &= 0x0F;     //  4 bits
    if (c.analog > AnalogAt || c.pin < 14 || c.pin >= 22)
        c.analog = AnalogNone;
    c.threshold = quint16(roundThreshold(c.threshold));   // 7 bits x 8
    if (c.route.size() > MaxRouteSteps)
        c.route.resize(MaxRouteSteps);
    for (RouteStep &s : c.route)
    {
        s.output &= 0x0F;
        s.delay   = qMin<quint16>(s.delay, MaxStepDelay);
    }
    return c;
}

bool InputConfig::operator==(const InputConfig &o) const
{
    return address == o.address && pin == o.pin && type == o.type
        && outputId == o.outputId && route == o.route && analog == o.analog
        && (!isAnalog() || threshold == o.threshold);
}

QJsonObject InputConfig::toJson() const
{
    QJsonObject o;
    if (!name.isEmpty()) o["name"] = name;
    o["type"]    = QString::fromUtf8(inputTypeInfo(type).key);
    o["address"] = address == AddrNone ? QJsonValue() : QJsonValue(int(address));
    o["pin"]     = isAnalog() ? QJsonValue() : pinToJson(pin);
    if (isAnalog())
    {
        static const char *const modes[3] = { "below", "above", "at" };
        QJsonObject a;
        a["mode"]      = QString::fromLatin1(modes[(analog - 1) % 3]);
        a["channel"]   = analogChannel();
        a["threshold"] = int(threshold);
        o["analog"]    = a;
    }
    o["output"]  = int(outputId);
    QJsonArray steps;
    if (hasCustomValues())                   // [ off / curved, on / straight ]
    {
        QJsonArray values;
        for (const RouteStep &s : route)
        {
            QJsonObject v;
            v["value"] = int(s.value);
            v["raw"]   = s.raw;
            values.append(v);
        }
        o["values"] = values;
    }
    for (const RouteStep &s : route)
    {
        if (s.values) continue;
        QJsonObject step;
        step["output"] = s.output;
        step["value"]  = int(s.value);
        step["raw"]    = s.raw;
        step["delay"]  = int(s.delay);       // 0.1 s units
        steps.append(step);
    }
    o["route"] = steps;
    return o;
}

InputConfig InputConfig::fromJson(const QJsonObject &o)
{
    InputConfig c;
    c.name = o["name"].toString();
    const QString key = o["type"].toString();
    for (quint8 t = 0; t < InputTypeCount; ++t)
        if (key == QLatin1String(kInputTypes[t].key)) c.type = t;
    if (key == QLatin1String("routeFalling"))
        c.type = Route;                  // files from before the edge choice was dropped
    c.address  = o["address"].isDouble() ? quint16(o["address"].toInt() & 0x0FFF) : AddrNone;
    c.pin      = pinFromJson(o["pin"]);
    const QJsonObject a = o["analog"].toObject();
    const QString mode  = a["mode"].toString();
    if (mode == QLatin1String("below") || mode == QLatin1String("above") || mode == QLatin1String("at"))
    {
        c.analog    = mode == QLatin1String("below") ? AnalogBelow : mode == QLatin1String("above") ? AnalogAbove : AnalogAt;
        c.pin       = quint8(14 + (a["channel"].toInt() & 7));
        c.threshold = quint16(roundThreshold(a["threshold"].toInt(512)));
    }
    c.outputId = quint8(o["output"].toInt() & 0x0F);
    const QJsonArray steps = o["route"].toArray();
    for (int i = 0; i < MaxRouteSteps && i < steps.size(); ++i)
    {
        const QJsonObject step = steps.at(i).toObject();
        RouteStep s;
        s.output = step["output"].toInt() & 0x0F;
        s.value  = quint8(step.contains("value") ? step["value"].toInt() : step["state"].toInt());  // "state": older files
        s.raw    = step["raw"].toBool(false);
        s.delay  = quint16(step.contains("delay") ? step["delay"].toInt() : DefaultStepDelay);
        c.route.append(s);
    }
    const QJsonArray values = o["values"].toArray();
    if (isDirectType(c.type) && values.size() == 2)
    {
        c.route.clear();
        for (const QJsonValue &v : values)
        {
            RouteStep s;
            s.output = c.outputId;
            s.value  = quint8(v.toObject()["value"].toInt());
            s.raw    = v.toObject()["raw"].toBool(false);
            s.delay  = 0;
            s.values = true;
            c.route.append(s);
        }
    }
    return c.normalized();
}

QByteArray encodeInput(int index, const InputConfig &c)
{
    QByteArray p;
    p.append(char(index));
    p.append(char(c.address >> 8));
    p.append(char(c.address & 0xFF));
    p.append(char(c.pinByte()));
    p.append(char(c.type));
    p.append(char(c.outputId));
    // payload 6 = analog threshold / 4 (0xFF for a digital input), 7-13 unused (0xFF)
    p.append(c.isAnalog() ? char(InputConfig::roundThreshold(c.threshold) / 4) : char(0xFF));
    p.append(7, char(0xFF));
    return p;   // 14 bytes
}

bool decodeInput(const QByteArray &p, int &index, InputConfig &c)
{
    if (p.size() < 14)
        return false;
    auto at = [&](int i) { return quint8(p.at(i)); };
    index      = at(0);
    c.address  = quint16(at(1) << 8 | at(2));
    c.setPinByte(at(3));
    c.type     = at(4) == ReservedFeedback ? quint8(InputNotInUse) : at(4);
    c.outputId = at(5);
    if (c.isAnalog())
        c.threshold = quint16(InputConfig::roundThreshold(at(6) * 4));   // payload 6 = threshold / 4
    c.route.clear();                     // the route follows with GET_ROUTE; payload 7-13 are unused
    return index >= 0 && index < NumInputs;
}

// ── BoardConfig ─────────────────────────────────────────────────────────────
QJsonObject BoardConfig::toJson() const
{
    QJsonObject root;
    root["format"]  = QStringLiteral("GPIOcore-config");
    root["version"] = 2;
    root["about"]   = QStringLiteral(
        "GPIOcore board file. 'board' = settings stored on the board; outputs[i] / inputs[i] = output / input i (0-15). "
        "Pins: 0-13 = D0-D13, 14-19 = A0-A5, 20-21 = A6-A7 (Nano, analog input only), null = not used; pin 2 is the DCC input. "
        "An analog input has 'analog': {mode below / above / at, channel 0-7 = A0-A7, threshold 0-1016} instead of a pin. "
        "A toggle / absolute input may have 'values': [off / curved, on / straight], each {value, raw} like a route step. "
        "Output data[4] depends on the type, see PROTOCOL.md section 2.1 (PWM: data[0] brightness, data[1] fade time in s). Input address: DCC accessory address or null. "
        "Route steps: output 0-15, value = 0 curved / 1 straight, or with raw = true an aspect / servo angle 10-170 / PWM 0-255, "
        "delay = wait after the step in 0.1 s. 'name' fields are only kept in this file.");
    root["board"]   = board.toJson();
    QJsonArray outs, ins;
    for (int i = 0; i < NumOutputs; ++i) { QJsonObject o = outputs[i].toJson(); o["index"] = i; outs.append(o); }
    for (int i = 0; i < NumInputs; ++i)  { QJsonObject o = inputs[i].toJson();  o["index"] = i; ins.append(o); }
    root["outputs"] = outs;
    root["inputs"]  = ins;
    return root;
}

bool BoardConfig::fromJson(const QJsonObject &root, BoardConfig &out, QString *error)
{
    if (root["format"].toString() != QLatin1String("GPIOcore-config"))
    {
        if (error) *error = QStringLiteral("Not a GPIOcore configuration file.");
        return false;
    }
    const QJsonArray outs = root["outputs"].toArray();
    const QJsonArray ins  = root["inputs"].toArray();
    if (outs.size() != NumOutputs || ins.size() != NumInputs)
    {
        if (error) *error = QStringLiteral("Expected %1 outputs and %2 inputs.").arg(NumOutputs).arg(NumInputs);
        return false;
    }
    for (int i = 0; i < NumOutputs; ++i) out.outputs[i] = OutputConfig::fromJson(outs.at(i).toObject());
    for (int i = 0; i < NumInputs;  ++i) out.inputs[i]  = InputConfig::fromJson(ins.at(i).toObject());
    out.board = root.contains("board") ? BoardSettings::fromJson(root["board"].toObject()) : BoardSettings();
    return true;
}

// ── Board settings ──────────────────────────────────────────────────────────
static const char *const kBoardKeys[BoardTypeCount] = { "uno", "nano", "dccnext", "jackOfAllTrades" };
static const char *const kInterlockKeys[3] = { "off", "on", "auto" };

QString boardTypeName(quint8 type)
{
    switch (type)
    {
    case BoardUno:             return trGpio("Arduino Uno");
    case BoardNano:            return trGpio("Arduino Nano");
    case BoardDccNext:         return trGpio("DCCnext");
    case BoardJackOfAllTrades: return trGpio("Jack of all trades");
    }
    return QStringLiteral("?");
}

QString cleanBoardName(const QString &name)
{
    QString out;
    for (QChar c : name.trimmed())
        if (c.unicode() >= 0x20 && c.unicode() < 0x7F && !QStringLiteral("\\/:*?\"<>|").contains(c))
            out += c;
    return out.left(MaxBoardName);
}

QJsonObject BoardSettings::toJson() const
{
    QJsonObject o;
    o["name"]      = name;
    o["type"]      = QString::fromLatin1(kBoardKeys[type < BoardTypeCount ? type : BoardNano]);
    o["timing"]    = timing;
    o["interlock"] = QString::fromLatin1(kInterlockKeys[interlock < 3 ? interlock : InterlockAuto]);
    o["delay"]     = int(delay);
    return o;
}

BoardSettings BoardSettings::fromJson(const QJsonObject &o)
{
    BoardSettings b;
    b.name = cleanBoardName(o["name"].toString());
    const QString t = o["type"].toString();
    for (quint8 i = 0; i < BoardTypeCount; ++i)
        if (t == QLatin1String(kBoardKeys[i])) b.type = i;
    b.timing   = o["timing"].toBool(false);
    const QString il = o["interlock"].toString();
    for (quint8 i = 0; i < 3; ++i)
        if (il == QLatin1String(kInterlockKeys[i])) b.interlock = i;
    b.delay = quint8(qBound(0, o["delay"].toInt(DefaultStepDelay), 255));
    return b;
}

QByteArray encodeBoard(const BoardSettings &b)
{
    QByteArray p;
    p.append(char(b.type));
    p.append(char((b.timing ? 0x02 : 0) | ((b.interlock & 0x03) << 2)));
    p.append(char(b.delay));
    p.append(cleanBoardName(b.name).toLatin1());
    return p;
}

bool decodeBoard(const QByteArray &p, BoardSettings &b)
{
    if (p.size() < 3)
        return false;
    b.type      = quint8(p.at(0)) < BoardTypeCount ? quint8(p.at(0)) : quint8(BoardNano);
    // bit 0 (was 'recorder on') is ignored
    b.timing    = p.at(1) & 0x02;
    b.interlock = quint8((p.at(1) >> 2) & 0x03);
    if (b.interlock > InterlockAuto) b.interlock = InterlockAuto;
    b.delay     = quint8(p.at(2));
    b.name      = cleanBoardName(QString::fromLatin1(p.mid(3)));
    return true;
}

// ── Board / pins ────────────────────────────────────────────────────────────
BoardOptions &boardOptions()
{
    static BoardOptions options;
    return options;
}

QString pinName(quint8 pin)
{
    if (pin < 14)       return QStringLiteral("D%1").arg(pin);
    if (pin < NumPins)  return QStringLiteral("A%1").arg(pin - 14);   // A0..A7
    if (pin == PinNA)   return QStringLiteral("—");
    return QStringLiteral("?%1").arg(pin);
}

int dccNextPort(quint8 pin)
{
    if (pin >= 3 && pin <= 12)       return pin - 2;     // port 1-10 = D3-D12
    if (pin >= 14 && pin <= 19)      return pin - 3;     // port 11-16 = A0-A5
    return 0;
}

QString pinLabel(quint8 pin)
{
    if (boardOptions().dccNext && dccNextPort(pin))
        return QStringLiteral("%1 · port %2").arg(pinName(pin)).arg(dccNextPort(pin));
    return pinName(pin);
}

bool isPwmPin(quint8 pin)
{
    return pin == 3 || pin == 5 || pin == 6 || pin == 9 || pin == 10 || pin == 11;
}

QString pinReservedReason(quint8 pin)
{
    const BoardOptions &b = boardOptions();
    switch (pin)
    {
    case 0:  return b.serialPinsUsable ? QString() : trGpio("Serial RX (GUI link)");
    case 1:  return b.serialPinsUsable ? QString() : trGpio("Serial TX (GUI link)");
    case 2:  return trGpio("DCC input");
    case 13: return trGpio("config LED");
    case 20: return b.type == BoardNano ? QString() : trGpio("only on an Arduino Nano");
    case 21: return trGpio("config button");
    }
    return QString();
}

QString pinOutputReason(quint8 pin)
{
    return (pin == 20 || pin == 21) ? trGpio("analog input only") : QString();
}

bool isServoTimerPin(quint8 pin)
{
    return pin == 9 || pin == 10;
}

void setBoardType(quint8 type)
{
    boardOptions().type    = type < BoardTypeCount ? type : quint8(BoardNano);
    boardOptions().dccNext = (type == BoardDccNext);
}

QString pinNote(quint8 pin)
{
    if (pin < 2 && boardOptions().serialPinsUsable)
        return trGpio("shared with serial: only active when no GUI is connected");
    if (pin == 20 || pin == 21)
        return trGpio("analog input without pull-up: add a 10 kOhm resistor to 5 V");
    return QString();
}

BoardConfig presetBoard(int preset)
{
    BoardConfig b;                                 // everything 'not in use' to start with
    quint8 pin = 3;
    auto next = [&pin]() -> quint8 { if (pin == 13) ++pin; return pin++; };
    auto out = [&b](int i, quint8 type, quint8 a, quint8 pb, quint8 ra, quint8 rb, quint8 rc) {
        OutputConfig &c = b.outputs[i];
        c.type = type; c.pinA = a; c.pinB = pb; c.relayA = ra; c.relayB = rb; c.relayC = rc; c.state = 0;
        if (type == ServoType)                         c.data = { { 80, 100, 20, 0 } };
        if (type == SinglePulse || type == DoublePulse) c.setSwitchTime(50);
    };
    const quint8 X = PinNA;
    int used = 0;
    switch (preset)
    {
    case 0: used = 16; for (int i = 0; i < used; ++i) { quint8 a = next(); out(i, ServoType, a, X, X, X, X); } break;
    case 1: used = 8;  for (int i = 0; i < used; ++i) { quint8 a = next(), c = next(); out(i, ServoType, a, X, X, X, c); } break;
    case 2: used = 5;  for (int i = 0; i < used; ++i) { quint8 a = next(), ra = next(), rb = next(); out(i, ServoType, a, X, ra, rb, X); } break;
    case 3: used = 16; for (int i = 0; i < used; ++i) { quint8 a = next(); out(i, SingleSteady, a, X, X, X, X); } break;
    case 4: used = 8;  for (int i = 0; i < used; ++i) { quint8 a = next(), p2 = next(); out(i, DoublePulse, a, p2, X, X, X); } break;
    case 5: used = 5;  for (int i = 0; i < used; ++i) { quint8 a = next(), p2 = next(), c = next(); out(i, DoublePulse, a, p2, X, X, c); } break;
    case 6: used = 4;  for (int i = 0; i < used; ++i) { quint8 a = next(), p2 = next(), ra = next(), rb = next(); out(i, DoublePulse, a, p2, ra, rb, X); } break;
    case 7: used = 8;  for (int i = 0; i < used; ++i) { quint8 a = next(), p2 = next(); out(i, DoubleSteady, a, p2, X, X, X); } break;
    case 8: used = 1;  out(0, SingleSteady, next(), X, X, X, X); break;    // empty: 1 input, 1 output
    default: break;
    }
    for (int i = 0; i < used; ++i)
    {
        InputConfig &c = b.inputs[i];
        c.type = Latching; c.address = quint16(1 + i); c.outputId = quint8(i);
    }
    return b;
}

QStringList presetNames()
{
    return {
        trGpio("0 – 16 servos"),
        trGpio("1 – 8 servos + unifrog relay"),
        trGpio("2 – 5 servos + electrofrog relays"),
        trGpio("3 – 16 single steady"),
        trGpio("4 – 8 double pulse"),
        trGpio("5 – 5 double pulse + unifrog relay"),
        trGpio("6 – 4 double pulse + electrofrog relays"),
        trGpio("7 – 8 double steady"),
        trGpio("8 – Empty: 1 input, 1 output"),
    };
}

} // namespace Gpio
