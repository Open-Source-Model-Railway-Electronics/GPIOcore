"""GPIOcore screen-recording deck (16:9) with narration in the speaker notes; opens in LibreOffice Impress.

    pip install python-pptx
    python docs/make_presentation.py      -> docs/GPIOcore-presentation.pptx (uses docs/images)
"""
import sys
from pathlib import Path
from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR

REPO = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent   # the repository
IMG = REPO / "docs" / "images"
OUT = REPO / "docs" / "GPIOcore-presentation.pptx"

SLATE = RGBColor(0x1F, 0x2A, 0x33)   # dominant dark
INK = RGBColor(0x26, 0x32, 0x3C)
MUTED = RGBColor(0x5B, 0x6B, 0x78)
LIGHT = RGBColor(0xF1, 0xF4, 0xF6)
WHITE = RGBColor(0xFF, 0xFF, 0xFF)
AMBER = RGBColor(0xF2, 0xA9, 0x00)   # accent (the simulation colour)
RED = RGBColor(0xC6, 0x28, 0x28)
GREEN = RGBColor(0x2E, 0x7D, 0x32)
BLUE = RGBColor(0x1E, 0x63, 0xB5)
HEAD, BODY = "Cambria", "Calibri"

prs = Presentation()
prs.slide_width, prs.slide_height = Inches(13.333), Inches(7.5)
BLANK = prs.slide_layouts[6]


def bg(slide, color):
    f = slide.background.fill
    f.solid()
    f.fore_color.rgb = color


def text(slide, x, y, w, h, runs, size=16, color=INK, font=BODY, bold=False, align=PP_ALIGN.LEFT,
         anchor=MSO_ANCHOR.TOP, name=None, space=6):
    """runs: str, or list of paragraphs; a paragraph is str or list of (text, {bold,color,size}) runs."""
    tb = slide.shapes.add_textbox(Inches(x), Inches(y), Inches(w), Inches(h))
    if name:
        tb.name = name
    tf = tb.text_frame
    tf.word_wrap = True
    tf.vertical_anchor = anchor
    tf.margin_left = tf.margin_right = tf.margin_top = tf.margin_bottom = 0
    paras = runs if isinstance(runs, list) else [runs]
    for i, para in enumerate(paras):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.alignment = align
        p.space_after = Pt(space)
        for t, o in ([(para, {})] if isinstance(para, str) else para):
            r = p.add_run()
            r.text = t
            r.font.size = Pt(o.get("size", size))
            r.font.bold = o.get("bold", bold)
            r.font.name = o.get("font", font)
            r.font.color.rgb = o.get("color", color)
    return tb


def title(slide, t, color=SLATE):
    text(slide, 0.6, 0.45, 12.1, 0.9, t, size=36, font=HEAD, bold=True, color=color, name="Title")


def card(slide, x, y, w, h, fill=LIGHT, name=None):
    s = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(x), Inches(y), Inches(w), Inches(h))
    s.adjustments[0] = 0.08
    s.fill.solid()
    s.fill.fore_color.rgb = fill
    s.line.fill.background()
    s.shadow.inherit = False
    if name:
        s.name = name
    return s


def dot(slide, x, y, d, color, label, size=18):
    s = slide.shapes.add_shape(MSO_SHAPE.OVAL, Inches(x), Inches(y), Inches(d), Inches(d))
    s.fill.solid()
    s.fill.fore_color.rgb = color
    s.line.fill.background()
    s.shadow.inherit = False
    tf = s.text_frame
    tf.margin_left = tf.margin_right = tf.margin_top = tf.margin_bottom = 0
    tf.vertical_anchor = MSO_ANCHOR.MIDDLE
    p = tf.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    r = p.add_run()
    r.text = label
    r.font.size, r.font.bold, r.font.name = Pt(size), True, BODY
    r.font.color.rgb = WHITE
    return s


def shot(slide, file, x, y, w, alt):
    pic = slide.shapes.add_picture(str(IMG / file), Inches(x), Inches(y), width=Inches(w))
    pic.line.color.rgb = RGBColor(0xC9, 0xD2, 0xD9)
    pic.line.width = Pt(1)
    pic.name = "Screenshot"
    pic._element.nvPicPr.cNvPr.set("descr", alt)
    return pic


def points(slide, x, y, w, items, size=16):
    """items: (header, description) with a small amber dot."""
    for i, (head, desc) in enumerate(items):
        yy = y + i * 1.0
        dot(slide, x, yy + 0.06, 0.22, AMBER, "")
        text(slide, x + 0.4, yy, w - 0.4, 0.95,
             [[(head, {"bold": True, "color": SLATE})], [(desc, {"color": MUTED, "size": size - 2})]],
             size=size, space=2)


def notes(slide, t):
    slide.notes_slide.notes_text_frame.text = t


def screenshot_slide(heading, file, alt, items, narration):
    s = prs.slides.add_slide(BLANK)
    bg(s, WHITE)
    title(s, heading)
    points(s, 0.6, 1.75, 3.7, items)
    shot(s, file, 4.55, 1.6, 8.2, alt)
    notes(s, narration)
    return s


# 1 Title
s = prs.slides.add_slide(BLANK)
bg(s, SLATE)
text(s, 0.8, 2.0, 11.5, 1.3, "GPIOcore", size=66, font=HEAD, bold=True, color=WHITE)
text(s, 0.8, 3.3, 11.5, 0.9, "A DCC accessory decoder on an Arduino Nano", size=28, color=AMBER)
text(s, 0.8, 4.2, 11.5, 0.8, "16 inputs · 16 outputs · routes · configured with a PC program, no code",
     size=20, color=RGBColor(0xC9, 0xD2, 0xD9))
text(s, 0.8, 6.4, 11.5, 0.5, "Open-Source-Model-Railway-Electronics", size=14, color=RGBColor(0x9A, 0xA8, 0xB4))
notes(s, "Welcome. In this video I show GPIOcore: a DCC accessory decoder that runs on a plain Arduino Nano. "
         "It has sixteen inputs and sixteen outputs, it can run routes, and you set it all up with a program "
         "on your PC. No programming needed.")

# 2 What it does
s = prs.slides.add_slide(BLANK)
bg(s, WHITE)
title(s, "One Nano, sixteen outputs, any job")
cols = [
    (BLUE, "Turnouts", "Servo with speed and frog relays, or a coil pair (double pulse)"),
    (AMBER, "Lights and effects", "Steady, flashing, swing light, fireplace, TL tube, welder, PWM"),
    (RED, "Signals", "4-aspect signal, aspect and blink rhythm from DCC extended"),
    (GREEN, "Inputs and routes", "Buttons, switches, track contacts; routes of up to 16 steps"),
]
for i, (c, h, d) in enumerate(cols):
    x = 0.6 + i * 3.08
    card(s, x, 1.85, 2.85, 3.5)
    dot(s, x + 0.35, 2.2, 0.75, c, str(i + 1), size=24)
    text(s, x + 0.35, 3.25, 2.2, 0.6, h, size=20, bold=True, color=SLATE, font=HEAD)
    text(s, x + 0.35, 3.95, 2.2, 2.0, d, size=15, color=MUTED)
notes(s, "What can it do? Every output can be a servo for a turnout, a coil pair, a light or a light effect, "
         "or a four-aspect signal. Inputs are push buttons, switches or track contacts, and an input can start "
         "a route that sets up to sixteen outputs one after another.")

# 3 What you need
s = prs.slides.add_slide(BLANK)
bg(s, WHITE)
title(s, "What you need")
points(s, 0.6, 1.8, 5.6, [
    ("Arduino Nano or Uno", "ATmega328P; CH340 clones work too"),
    ("DCC on D2", "through an optocoupler circuit"),
    ("Optional: button on A7", "10 kΩ pull-up to 5 V; D13 is the config LED"),
    ("A Windows PC", "with a USB cable; the program needs no install"),
])
card(s, 7.0, 1.8, 5.75, 4.2, fill=SLATE)
text(s, 7.4, 2.1, 5.0, 0.6, "Reserved pins", size=22, bold=True, color=WHITE, font=HEAD)
for i, (pin, use, c) in enumerate([("D2", "DCC input", BLUE), ("D13", "config LED", AMBER), ("A7", "config button", RED)]):
    dot(s, 7.4, 2.95 + i * 0.85, 0.6, c, pin, size=14)
    text(s, 8.25, 3.0 + i * 0.85, 4.2, 0.5, use, size=18, color=WHITE)
text(s, 7.4, 5.45, 5.0, 0.4, "All other pins are free for inputs and outputs", size=14,
     color=RGBColor(0xC9, 0xD2, 0xD9))
notes(s, "You need an Arduino Nano or Uno, a DCC signal on pin D2, and a Windows PC. Optionally a push button "
         "on A7 for setting servos on the layout. Three pins are reserved: D2 for DCC, D13 for the config LED "
         "and A7 for the config button. All the other pins are yours.")

# 4 Getting started
s = prs.slides.add_slide(BLANK)
bg(s, WHITE)
title(s, "Up and running in four steps")
steps = [("Unpack", "the release zip and start GPIOcoreGUI.exe"),
         ("Flash", "Board → Flash firmware…, pick the port, Flash"),
         ("Connect", "choose the port in the toolbar, Connect"),
         ("Configure", "Read board, or start from a preset or a file")]
for i, (h, d) in enumerate(steps):
    x = 0.6 + i * 3.08
    dot(s, x, 2.1, 0.9, SLATE if i < 3 else AMBER, str(i + 1), size=28)
    if i < 3:
        line = s.shapes.add_connector(1, Inches(x + 1.0), Inches(2.55), Inches(x + 2.98), Inches(2.55))
        line.line.color.rgb = RGBColor(0xC9, 0xD2, 0xD9)
        line.line.width = Pt(2)
    text(s, x, 3.25, 2.8, 0.6, h, size=22, bold=True, color=SLATE, font=HEAD)
    text(s, x, 3.9, 2.7, 1.6, d, size=16, color=MUTED)
notes(s, "Getting started takes four steps. Unpack the zip and start the program. Flash the firmware with "
         "Board, Flash firmware. Pick the port and click Connect. Then read the board, or start from a preset "
         "or a board file.")

# 5 Modes
s = prs.slides.add_slide(BLANK)
bg(s, WHITE)
title(s, "Always one of three modes")
modes = [(RED, "Configure", "Change everything. Connected: every change goes to the board at once."),
         (AMBER, "Simulate", "Your configuration on a simulated board. Click inputs, watch outputs."),
         (GREEN, "Live test", "Click inputs and watch the real board. Connects by itself.")]
for i, (c, h, d) in enumerate(modes):
    x = 0.6 + i * 4.1
    card(s, x, 1.85, 3.85, 3.0)
    dot(s, x + 0.35, 2.2, 0.7, c, "", size=12)
    text(s, x + 1.25, 2.3, 2.4, 0.6, h, size=24, bold=True, color=SLATE, font=HEAD)
    text(s, x + 0.35, 3.25, 3.2, 2.2, d, size=17, color=MUTED)
text(s, 0.6, 5.3, 12.1, 0.6, [[("The frame around the drawing has the colour of the mode. ", {"color": INK}),
                                ("Simulate and Live test are look-only.", {"bold": True, "color": SLATE})]], size=16)
notes(s, "The program is always in one of three modes. Configure, red when connected: you change things and "
         "every change goes straight to the board. Simulate, yellow: try your configuration without a board. "
         "Live test, green: press inputs and watch the real board. In Simulate and Live test you cannot change "
         "anything, so you cannot break your setup by accident.")

# 6-9 screenshots
screenshot_slide("Servos and frogs", "servo-output.png", "A servo output in the GPIOcore Configurator", [
    ("Two end positions", "curved and straight, 10–170°"),
    ("Speed", "milliseconds per degree"),
    ("Live", "the servo follows the slider"),
    ("Frog relays", "unifrog or electrofrog"),
], "Click an output to edit it. For a servo you set the curved and straight positions and the speed. "
   "Turn on Live and the servo follows the slider while you adjust it. Frog relays are switched for you, "
   "halfway the movement.")
screenshot_slide("Routes", "route-input.png", "A route input with six steps", [
    ("Up to 16 steps", "each with a value and a wait time"),
    ("Any value", "straight, curved, an angle or an aspect"),
    ("Starts on", "any command, straight only or curved only"),
    ("Drag to add", "from an output to the route input"),
], "An input can be a route. Each step sets an output: straight, curved, a servo angle or a signal aspect, "
   "with a wait time after it. You choose whether the route starts on any DCC command, or only on straight "
   "or curved. You can also drag from an output to the route to add a step.")
screenshot_slide("Signals", "signal-output.png", "A 4-aspect signal output", [
    ("Four lamps on A–D", "lamp 1 = bit 0 = pin A"),
    ("Aspect", "from DCC extended or a route step"),
    ("Blink rhythm", "set once per signal, in Hz"),
    ("Remembered", "the last aspect survives a restart"),
], "A four-aspect signal has its lamps on pins A to D. The aspect comes from a DCC extended packet or a "
   "route step; the aspect picker shows you which lamps are on. The blink rhythm is set once per signal.")
screenshot_slide("The pin map", "pin-map.png", "The pin map tab", [
    ("Every pin at a glance", "what uses it, PWM pins marked"),
    ("Conflicts in red", "two users on one pin"),
    ("Taking over", "a used pin moves to the new user"),
    ("Click a card", "to edit that input or output"),
], "The pin map shows every pin of the board and what uses it. A conflict is red. When you choose a pin "
   "that is already in use, the new input or output takes it over and the old one becomes not in use.")

# 10 Phone + config button
s = prs.slides.add_slide(BLANK)
bg(s, WHITE)
title(s, "Set servos at the layout")
for i, (c, h, lines) in enumerate([
    (BLUE, "Phone remote", ["Board → Phone remote… shows a QR code",
                            "Same Wi-Fi network, no app needed",
                            "Slide curved or straight: the servo moves and the position is stored",
                            "The slider in the program moves along"]),
    (AMBER, "Config button + throttle", ["Hold A7 for 2 s: config mode, D13 blinks",
                                         "Short press: next servo",
                                         "Loco 9999: speed knob moves the servo",
                                         "F1 curved · F2 straight · F3 throw · F4 next"]),
]):
    x = 0.6 + i * 6.2
    card(s, x, 1.85, 5.9, 3.9)
    dot(s, x + 0.35, 2.2, 0.7, c, str(i + 1), size=22)
    text(s, x + 1.25, 2.3, 4.4, 0.6, h, size=24, bold=True, color=SLATE, font=HEAD)
    text(s, x + 0.35, 3.25, 5.2, 3.0, [[(l, {})] for l in lines], size=17, color=INK, space=10)
notes(s, "Setting servo positions is easiest at the layout itself. Open the phone remote and scan the QR code: "
         "slide the curved or straight position and the servo moves along. Or use the config button: hold it "
         "two seconds, select loco 9999 on your throttle, and use the speed knob and F1 and F2 to store the "
         "positions.")

# 11 Closing
s = prs.slides.add_slide(BLANK)
bg(s, SLATE)
text(s, 0.8, 2.3, 11.5, 1.1, "Try it yourself", size=54, font=HEAD, bold=True, color=WHITE)
text(s, 0.8, 3.5, 11.5, 0.8, "github.com/Open-Source-Model-Railway-Electronics/GPIOcore", size=24, color=AMBER)
text(s, 0.8, 4.4, 11.5, 0.8, "Firmware, configurator and manual · free to use (CC0)", size=20,
     color=RGBColor(0xC9, 0xD2, 0xD9))
notes(s, "That was GPIOcore. Everything is on GitHub: the firmware, the configurator and the manual, free to use. "
         "Thanks for watching.")

prs.save(OUT)
print(OUT)
