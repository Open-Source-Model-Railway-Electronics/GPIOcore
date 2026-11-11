import tkinter as tk
from tkinter import ttk

class InputFrame(tk.Frame):
    def __init__(self, parent, appdata):
        super().__init__(parent)

        self.input_colors = self.generate_input_colors()
        self.appdata = appdata

        headers = ["Clr", "Pin", "DCC", "Type", "Output"] + [f"Out {i+1}" for i in range(8)]
        widths = [10, 3, 4, 12, 4] + [12]*8

        for c, h in enumerate(headers):
            tk.Label(self, text=h, width=widths[c], anchor="center").grid(row=0, column=c, padx=1, pady=1, sticky="nsew")

        for c in range(5):
            tk.Label(self, text="", width=widths[c]).grid(row=1, column=c)

        for j in range(8):
            frame = tk.Frame(self)
            frame.grid(row=1, column=5+j, padx=1, pady=0)
            tk.Label(frame, text="Nr", width=3).pack(side=tk.LEFT)
            tk.Label(frame, text="C", width=2).pack(side=tk.LEFT)
            tk.Label(frame, text="S", width=2).pack(side=tk.LEFT)

        self.rows = []

        for i in range(16):
            row_index = i + 2

            color = self.input_colors[i]
            lbl = tk.Label(self, text=f"Input {i}", bg=color, fg="black", width=10)
            lbl.grid(row=row_index, column=0, sticky="nsew", padx=1, pady=1)

            pin = tk.Entry(self, width=3)
            pin.insert(0, "NA")
            pin.grid(row=row_index, column=1, padx=1, pady=1, ipady=3)

            dcc = tk.Entry(self, width=4)
            dcc.insert(0, "NA")
            dcc.grid(row=row_index, column=2, padx=1, pady=1, ipady=3)

            type_var = tk.StringVar()
            type_dropdown = ttk.Combobox(self, textvariable=type_var, values=["momentary", "latching", "route"], width=12)
            type_dropdown.set("momentary")
            type_dropdown.grid(row=row_index, column=3, padx=1, pady=1)

            output = tk.Entry(self, width=4)
            output.insert(0, "NA")
            output.grid(row=row_index, column=4, padx=1, pady=1, ipady=3)

            lo_entries = []
            for j in range(8):
                frame = tk.Frame(self)
                frame.grid(row=row_index, column=5+j, padx=1, pady=0)

                out_num = tk.Entry(frame, width=3)
                out_num.pack(side=tk.LEFT, padx=1)

                var_c = tk.IntVar()
                var_s = tk.IntVar()

                def toggle_c(vs=var_s, vc=var_c):
                    if vc.get():
                        vs.set(0)

                def toggle_s(vc=var_c, vs=var_s):
                    if vs.get():
                        vc.set(0)

                c = tk.Checkbutton(frame, variable=var_c, width=2, command=toggle_c, padx=0, pady=0)
                c.pack(side=tk.LEFT, padx=0)

                s = tk.Checkbutton(frame, variable=var_s, width=2, command=toggle_s, padx=0, pady=0)
                s.pack(side=tk.LEFT, padx=0)

                lo_entries.append({
                    "num": out_num,
                    "curved": var_c,
                    "straight": var_s
                })

            input_row = {
                "pin": pin, "dcc": dcc, "type": type_var, "output": output,
                "lo_entries": lo_entries, "label": lbl,
                "dirty": False
            }
            self.rows.append(input_row)

            # Bindings with immediate color update
            pin.bind("<KeyRelease>", lambda e, r=input_row, i=i: [self.mark_dirty(i), self.update_lo_fields(r)])
            dcc.bind("<KeyRelease>", lambda e, r=input_row, i=i: [self.mark_dirty(i), self.update_lo_fields(r)])
            output.bind("<KeyRelease>", lambda e, r=i: self.mark_dirty(r))
            type_dropdown.bind("<<ComboboxSelected>>", lambda e, r=input_row, i=i: [self.mark_dirty(i), self.update_lo_fields(r)])

            for entry in lo_entries:
                entry["num"].bind("<KeyRelease>", lambda e, r=i: self.mark_dirty(r))
                entry["curved"].trace_add("write", lambda *args, r=i: self.mark_dirty(r))
                entry["straight"].trace_add("write", lambda *args, r=i: self.mark_dirty(r))

            self.update_lo_fields(input_row)

    def update_lo_fields(self, row):
        try:
            addr_str = row["dcc"].get().strip()
            pin_str = row["pin"].get().strip()

            addr = int(addr_str) if addr_str.isdigit() else None
            pin = int(pin_str) if pin_str.isdigit() else None

            valid_pin = pin is not None and 0 <= pin <= 15
            valid_dcc = addr is not None and 1 <= addr <= 4095

            idx = int(row["label"].cget("text").split()[1])

            if valid_pin or valid_dcc:
                row["label"].config(bg=self.input_colors[idx], fg="black")
            else:
                row["label"].config(bg="#CCCCCC", fg="black")

        except Exception as e:
            print(f"[DEBUG] kleur update failed: {e}")

    def mark_dirty(self, index):
        self.rows[index]["dirty"] = True

    def generate_input_colors(self):
        return [
            "#E34234",  # Tomato Red
            "#FF4500",  # OrangeRed
            "#FF6347",  # Tomato
            "#FF7F50",  # Coral
            "#FF8C00",  # DarkOrange
            "#FFA500",  # Orange
            "#FFB733",  # Light Orange
            "#FFCC00",  # Gold
            "#FFD700",  # GoldenRod
            "#FFE033",  # Light Yellow-Gold
            "#FFF000",  # Yellow
            "#FFFF00",  # Bright Yellow
            "#FFFF66",  # Light Yellow
            "#FFFF99",  # Pale Yellow
            "#FFFFCC",  # Very Pale Yellow
            "#FFFFFF"   # White (optional final light)
        ]
