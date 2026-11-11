import tkinter as tk
from PIL import Image, ImageTk

class StatusIconFrame(tk.Canvas):
    def __init__(self, parent, appdata, output_frame):
        super().__init__(parent, width=600, height=900, bg="white", highlightthickness=0)
        self.appdata = appdata
        self.output_frame = output_frame

        # Load icons
        self.icons = {
            "singleSteady": self.load_icon("singleSteady.png", 40),
            "singlePulse": self.load_icon("singlePulse.png", 40),
            "singleFlashing": self.load_icon("singleFlash.png", 40),
            "doubleSteady": self.load_icon("doubleSteady.png", 40, double=True),
            "doublePulse": self.load_icon("doublePulse.png", 40, double=True),
            "doubleFlashing": self.load_icon("doubleFlash.png", 40, double=True),
            "servoType": self.load_icon("servo.png", 40),
            "singleRelay": self.load_icon("singleRelais.png", 40),
            "doubleRelay": self.load_icon("doubleRelay.png", 40, double=True),
        }

        self.icon_items = {}

        self.ball_items = []
        self.draw_static_layout()
        self.update_icons()

    def load_icon(self, path, target_height, double=False, extra_scale=1.0):
        try:
            img = Image.open(path)
            w, h = img.size

            # Verdubbel hoogte als double=True
            if double:
                target_height *= 2

            scale_factor = (target_height / h) * extra_scale
            target_width = int(w * scale_factor)
            target_height = int(h * scale_factor)

            img = img.resize((target_width, target_height), Image.Resampling.LANCZOS)

            return ImageTk.PhotoImage(img)
        except Exception as e:
            print(f"[ERROR] Failed to load {path}: {e}")
            return None

    def draw_static_layout(self):
        for i in range(16):
            y = 30 + i * 50
            self.create_text(20, y, text=str(i), anchor="center")
            ball = self.create_oval(40, y-20, 80, y+20, fill="grey")
            self.ball_items.append(ball)

    def update_balls(self, override_colors=None):
        """
        Kleur bollen op basis van override_colors (lijst met 16 kleuren).
        """
        if override_colors:
            for i, color in enumerate(override_colors):
                try:
                    self.itemconfig(self.ball_items[i], fill=color)
                except Exception as e:
                    print(f"[ERROR] set_ball_color({i}): {e}")

    def update_icons(self):
        # Clear existing icons
        for item in self.icon_items.values():
            self.delete(item)
        self.icon_items.clear()

        # Build mapping from pins to icons
        pin_to_icon = {}
        for idx, output_row in enumerate(self.output_frame.rows):
            pins = {}
            for p in ["pinA", "rpinA", "rpinC"]:
                val = output_row[p].get().strip()
                pins[p] = int(val) if val.isdigit() and 0 <= int(val) < 16 else None

            type_str = output_row["type"].get().lower()

            img_key = None
            if "single" in type_str and "steady" in type_str:
                img_key = "singleSteady"
            elif "single" in type_str and "pulse" in type_str:
                img_key = "singlePulse"
            elif "single" in type_str and "flash" in type_str:
                img_key = "singleFlashing"
            elif "double" in type_str and "steady" in type_str:
                img_key = "doubleSteady"
            elif "double" in type_str and "pulse" in type_str:
                img_key = "doublePulse"
            elif "double" in type_str and "flash" in type_str:
                img_key = "doubleFlashing"
            elif "servo" in type_str:
                img_key = "servoType"

            # Place icons based on priority
            if pins["pinA"] is not None and img_key:
                pin_to_icon[pins["pinA"]] = (img_key, False)
            if pins["rpinA"] is not None:
                pin_to_icon[pins["rpinA"]] = ("doubleRelay", True)
            if pins["rpinC"] is not None:
                pin_to_icon[pins["rpinC"]] = ("singleRelay", False)

        # Draw icons on canvas
        for pin, (key, is_double_relay) in pin_to_icon.items():
            icon = self.icons.get(key)
            if icon:
                y = 30 + pin * 50
                if is_double_relay or "double" in key:
                    img_id = self.create_image(200, y + 25, image=icon, anchor="center")
                else:
                    img_id = self.create_image(200, y, image=icon, anchor="center")
                self.icon_items[pin] = img_id
