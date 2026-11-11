import tkinter as tk
from PIL import Image, ImageTk

class StatusIconFrame(tk.Frame):
    def __init__(self, parent, appdata, output_frame):
        super().__init__(parent)
        self.appdata = appdata
        self.output_frame = output_frame

        # Load all icons
        self.icons = {}
        icon_specs = [
            ("singleSteady", "singleSteady.png", 40),
            ("singlePulse", "singlePulse.png", 40),
            ("singleFlashing", "singleFlash.png", 40),
            ("doubleSteady", "doubleSteady.png", 40),
            ("doublePulse", "doublePulse.png", 40),
            ("doubleFlashing", "doubleFlash.png", 40),
            ("servoType", "servo.png", 40),
            ("singleRelay", "singleRelais.png", 50),
            ("doubleRelay", "doubleRelay.png", 100),
        ]
        for key, file, size in icon_specs:
            self.icons[key] = self.load_icon(file, size)

        # Create icon labels for each pin 0-15
        self.icon_labels = []
        for i in range(16):
            row_frame = tk.Frame(self)
            row_frame.pack(side=tk.TOP, pady=2, anchor="w")

            tk.Label(row_frame, text=str(i), width=2).pack(side=tk.LEFT)
            icon_label = tk.Label(row_frame)
            icon_label.pack(side=tk.LEFT, padx=5)
            self.icon_labels.append(icon_label)

        self.update_icons()

    def load_icon(self, path, size):
        try:
            img = Image.open(path)
            w, h = img.size
            scale_factor = (size*2) / max(w, h)  # VERDUBBELD
            new_size = (int(w * scale_factor), int(h * scale_factor))
            img = img.resize(new_size, Image.Resampling.LANCZOS)
            return ImageTk.PhotoImage(img)
        except Exception as e:
            print(f"[ERROR] Failed to load {path}: {e}")
            return None


    def update_icons(self):
        pin_to_icon = {}

        for idx, output_row in enumerate(self.output_frame.rows):
            pins = {}
            for p in ["pinA", "rpinA", "rpinC"]:
                val = output_row[p].get().strip()
                pins[p] = int(val) if val.isdigit() and 0 <= int(val) < 16 else None
                print(f"[DEBUG] Output {idx} {p}: raw='{val}' parsed={pins[p]}")

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

            # Placement logic: pinA > rpinA > rpinC
            if pins["pinA"] is not None and img_key:
                pin_to_icon[pins["pinA"]] = img_key
                print(f"[DEBUG] Output {idx}: Assigned {img_key} to pinA {pins['pinA']}")
            elif pins["rpinA"] is not None:
                pin_to_icon[pins["rpinA"]] = "doubleRelay"
                print(f"[DEBUG] Output {idx}: Assigned doubleRelay to relayPinA {pins['rpinA']}")
            elif pins["rpinC"] is not None:
                pin_to_icon[pins["rpinC"]] = "singleRelay"
                print(f"[DEBUG] Output {idx}: Assigned singleRelay to relayPinC {pins['rpinC']}")
            else:
                print(f"[DEBUG] Output {idx}: No valid pins found")

        # Apply to GUI
        for pin in range(16):
            label = self.icon_labels[pin]
            if pin in pin_to_icon:
                key = pin_to_icon[pin]
                icon = self.icons.get(key)
                if icon:
                    label.config(image=icon)
                    label.image = icon
                    print(f"[DEBUG] Placed {key} icon at pin {pin}")
                else:
                    label.config(image="")
                    label.image = None
                    print(f"[DEBUG] No icon found for key {key} at pin {pin}")
            else:
                label.config(image="")
                label.image = None
                print(f"[DEBUG] Cleared icon at pin {pin}")

        self.after(5000, self.update_icons)
