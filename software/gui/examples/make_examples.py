"""Generate IOcore example board files into software/gui/examples."""
import json
import sys
from pathlib import Path

OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent   # this folder
PINS = [3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16, 17, 18, 19]   # D3-D12, A0-A5 (D2 = DCC, D13 = LED)
ABOUT = ("IOcore board file. 'board' = settings stored on the board; outputs[i] / inputs[i] = output / input i (0-15). "
         "Pins: 0-13 = D0-D13, 14-19 = A0-A5, 20-21 = A6-A7 (Nano, analog input only), null = not used; pin 2 is the DCC input. "
         "See PROTOCOL.md for data[] per type and README.md for how to use the program.")


def out(type_="notInUse", a=None, b=None, c=None, d=None, ra=None, rb=None, rc=None, data=(0, 0, 0, 0), name="", state=0):
    o = {"type": type_, "pinA": a, "pinB": b, "pinC": c, "pinD": d,
         "relayA": ra, "relayB": rb, "relayC": rc, "state": state, "data": list(data)}
    if name:
        o["name"] = name
    return o


def inp(type_="notInUse", address=None, pin=None, output=0, route=(), name="", analog=None):
    i = {"type": type_, "address": address, "pin": pin, "output": output, "route": list(route)}
    if analog:
        i["analog"] = analog
    if name:
        i["name"] = name
    return i


def step(output, value, raw=False, delay=3):
    return {"output": output, "value": value, "raw": raw, "delay": delay}


def board(outputs, inputs, timing=False, interlock="auto", delay=3, type_="nano"):
    outputs = outputs + [out() for _ in range(16 - len(outputs))]
    inputs = inputs + [inp() for _ in range(16 - len(inputs))]
    for i, o in enumerate(outputs):
        o["index"] = i
    for i, x in enumerate(inputs):
        x["index"] = i
    return {"format": "IOcore-config", "version": 2, "about": ABOUT,
            "board": {"name": "", "type": type_, "timing": timing, "interlock": interlock, "delay": delay},
            "outputs": outputs, "inputs": inputs}


def save(name, cfg):
    (OUT / name).write_text(json.dumps(cfg, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(name)


SERVO = (80, 100, 20, 0)          # curved 80, straight 100, 20 ms per degree
BARRIER = (20, 110, 40, 0)        # curved = barrier down (20), straight = up (110), slow
SWING = (50, 0, 0, 0)             # light effect 'swing' (data[3] = 0): each lamp 0.5 s, no pause


def crossing(barriers):
    """AHOB (with half barriers) or AKI (lights and bell only)."""
    outs = [dict(out("lightEffect", 3, 4, data=SWING, name="Flashing lights"), effect="swing"),
            out("soundEffect", 1, name="Bell (DFPlayer, track 1)")]
    close = [step(0, 1, delay=0), step(1, 1, raw=True, delay=40 if barriers else 0)]
    open_ = []
    if barriers:
        outs += [out("servo", 5, data=BARRIER, state=1, name="Barrier left"),
                 out("servo", 6, data=BARRIER, state=1, name="Barrier right")]
        close += [step(2, 0, delay=5), step(3, 0, delay=40), step(1, 0, raw=True, delay=0)]   # bell stops when down
        open_ += [step(2, 1, delay=5), step(3, 1, delay=30)]
    open_ += [step(0, 0, delay=0), step(1, 0, raw=True, delay=0)]
    ins = [inp("route", 50, 7, route=close, name="Train approaching (sensor or DCC 50)"),
           inp("route", 51, 8, route=open_, name="Train has passed (sensor or DCC 51)")]
    return board(outs, ins, timing=True)


save("ahob-level-crossing.json", crossing(True))
save("aki-level-crossing.json", crossing(False))

# servo decoder with electrofrog relays: servo pin + relay A + relay B on following pins (5 servos)
outs, ins = [], []
for k in range(5):
    p = PINS[3 * k: 3 * k + 3]
    outs.append(out("servo", p[0], ra=p[1], rb=p[2], data=SERVO, name=f"Turnout {k + 1}"))
    ins.append(inp("latching", k + 1, None, k, name=f"Turnout {k + 1} (DCC {k + 1})"))
save("servo-decoder-electrofrog.json", board(outs, ins))

# servo decoder with unifrog relay: servo pin + relay C on the next pin (8 servos)
outs, ins = [], []
for k in range(8):
    p = PINS[2 * k: 2 * k + 2]
    outs.append(out("servo", p[0], rc=p[1], data=SERVO, name=f"Turnout {k + 1}"))
    ins.append(inp("latching", k + 1, None, k, name=f"Turnout {k + 1} (DCC {k + 1})"))
save("servo-decoder-unifrog.json", board(outs, ins))

# 16 servos, DCC 1-16
save("servo-decoder-16.json", board(
    [out("servo", PINS[k], data=SERVO, name=f"Servo {k + 1}") for k in range(16)],
    [inp("latching", k + 1, None, k, name=f"DCC {k + 1}") for k in range(16)]))

# 16 single steady outputs, DCC 1-16
save("switch-decoder-16.json", board(
    [out("singleSteady", PINS[k], name=f"Output {k + 1}") for k in range(16)],
    [inp("latching", k + 1, None, k, name=f"DCC {k + 1}") for k in range(16)]))

# inputs only: 8 route inputs (DCC 101-108), pins and steps to fill in yourself
save("inputs-routes-8.json", board(
    [], [inp("route", 101 + k, None, 0, name=f"Route {k + 1} (DCC {101 + k})") for k in range(8)]))

# inputs only: resistor ladder, 8 buttons on A0; set each threshold with Live reading -> Use this value
save("inputs-resistor-ladder-8.json", board(
    [], [inp("route", None, None, 0, name=f"Ladder button {k + 1}",
             analog={"mode": "at", "channel": 0, "threshold": 64 + 128 * k}) for k in range(8)]))
