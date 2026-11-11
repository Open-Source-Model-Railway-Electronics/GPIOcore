import tkinter as tk
from tkinter import ttk

class OutputFrame(tk.Frame):
    def __init__(self, parent):
        super().__init__(parent)

        self.output_colors = self.generate_output_colors()
        self.type_options = [
            "Single Steady", "Single Pulse", "Single Flashing",
            "Double Steady", "Double Pulse", "Double Flashing",
            "Servo", "PWM", "Not used"
        ]

        # Headers
        tk.Label(self, text="Output", anchor="center").grid(row=1, column=0)
        tk.Label(self, text="Port").grid(row=0, column=1, columnspan=2)
        tk.Label(self, text="A").grid(row=1, column=1)
        tk.Label(self, text="B").grid(row=1, column=2)
        tk.Label(self, text="Electrofrog").grid(row=0, column=3, columnspan=2)
        tk.Label(self, text="A").grid(row=1, column=3)
        tk.Label(self, text="B").grid(row=1, column=4)
        tk.Label(self, text="Unifrog").grid(row=0, column=5)
        tk.Label(self, text="Relay").grid(row=1, column=5)
        tk.Label(self, text="State").grid(row=1, column=6)
        tk.Label(self, text="Type").grid(row=1, column=7)
        tk.Label(self, text="", anchor="center").grid(row=1, column=8)

        self.rows = []

        for i in range(16):
            row_index = i * 2 + 2

            lbl = tk.Label(self, text=f"Output {i}", bg=self.output_colors[i], width=10)
            lbl.grid(row=row_index, column=0, sticky="nsew", padx=1, pady=1)

            pinA = self.make_entry(row_index, 1)
            pinB = self.make_entry(row_index, 2)
            rpinA = self.make_entry(row_index, 3)
            rpinB = self.make_entry(row_index, 4)
            rpinC = self.make_entry(row_index, 5)

            state = tk.Entry(self, width=4)
            state.insert(0, "1")
            state.grid(row=row_index, column=6, padx=1, pady=1, ipady=3)

            type_var = tk.StringVar()
            type_dropdown = ttk.Combobox(self, textvariable=type_var, values=self.type_options, width=12)
            type_dropdown.set(self.type_options[0])
            type_dropdown.grid(row=row_index, column=7, padx=1, pady=1)

            dynamic_frame = tk.Frame(self)
            dynamic_frame.grid(row=row_index, column=8, padx=1, pady=1)

            row = {
                "lbl": lbl, "pinA": pinA, "pinB": pinB,
                "rpinA": rpinA, "rpinB": rpinB, "rpinC": rpinC,
                "state": state, "type": type_var, "dynamic": dynamic_frame,
                "type_dropdown": type_dropdown,
                "dirty": False
            }
            self.rows.append(row)

            # Bindings for dirty tracking
            for widget in [pinA, pinB, rpinA, rpinB, rpinC, state]:
                widget.bind("<KeyRelease>", self.make_dirty_callback(i))
            type_dropdown.bind("<<ComboboxSelected>>", self.make_type_callback(i, dynamic_frame, type_var, pinB))

            self.update_dynamic(dynamic_frame, type_var, pinB)

    def make_entry(self, row, col, width=4):
        e = tk.Entry(self, width=width)
        e.insert(0, "NA")
        e.grid(row=row, column=col, padx=1, pady=1, ipady=3)
        return e

    def make_dirty_callback(self, index):
        return lambda e: self.mark_dirty(index)

    def make_type_callback(self, index, frame, type_var, pinB):
        def callback(event):
            self.update_dynamic(frame, type_var, pinB)
            self.mark_dirty(index)
        return callback

    def update_dynamic(self, frame, type_var, pinB):
        for widget in frame.winfo_children():
            widget.destroy()

        t = type_var.get().lower()
        if t == "servo":
            for h in ["Min", "Max", "Speed", "Place"]:
                tk.Label(frame, text=h, width=6).pack(side=tk.LEFT)
                tk.Entry(frame, width=4).pack(side=tk.LEFT)
        elif t == "pwm":
            for h in ["Max", "Fading"]:
                tk.Label(frame, text=h, width=6).pack(side=tk.LEFT)
                tk.Entry(frame, width=4).pack(side=tk.LEFT)
        else:
            tk.Label(frame, text="Switch Time", width=10).pack(side=tk.LEFT)
            tk.Entry(frame, width=6).pack(side=tk.LEFT)

        # Correct pinB enable/disable logic
        # if "double" in t:
        #     pinB.config(state="normal")
        # else:
        #     pinB.config(state="normal")  # enable temporarily to clear
        #     pinB.delete(0, tk.END)
        #     pinB.insert(0, "NA")
        #     pinB.config(state="disabled")

    def mark_dirty(self, index):
        self.rows[index]["dirty"] = True

    def generate_output_colors(self):
        return [
            "#00FFFF",  # Aqua (lichtst)
            "#00E6E0",  # Licht cyan
            "#00CCCC",  # Cyan
            "#00B28F",  # Teal groen
            "#009966",  # Sterk teal
            "#007A3D",  # Medium donker groen
            "#006400",  # Donker groen
            "#0066FF",  # Bright Blue
            "#004CFF",  # Strong Blue
            "#0033CC",  # Diep Blauw
            "#002699",  # Donker Blauw (donkerste toegestaan)
            "#0033CC",  # Minder donker dan #002699 maar donker blauw
            "#004CFF",  # Sterk blauw opnieuw
            "#0066FF",  # Bright blue opnieuw
            "#0099FF",  # Dodger blue
            "#00CCFF"   # Sky blue
        ]


    # def set_output_color(self, index, color):
        # if 0 <= index < len(self.rows):
            # self.rows[index]["lbl"].config(bg=color)
