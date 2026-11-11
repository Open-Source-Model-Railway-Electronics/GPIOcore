import tkinter as tk
from tkinter import ttk

root = tk.Tk()
root.title("Jack Of All Trades")
root.geometry("1500x900")

# Dummy serial selector bovenin
com_frame = tk.Frame(root)
com_frame.pack(pady=5)
tk.Label(com_frame, text="Select COM Port:").pack(side=tk.LEFT)
com_dropdown = ttk.Combobox(com_frame, width=10)
com_dropdown.pack(side=tk.LEFT)
connect_button = tk.Button(com_frame, text="Connect")
connect_button.pack(side=tk.LEFT)
status_label = tk.Label(root, text="No COM port connected.", fg="red")
status_label.pack()

# ---------- INPUTS FRAME ----------
inputs_frame = tk.LabelFrame(root, text="Inputs")
inputs_frame.pack(side=tk.LEFT, padx=10, pady=10)

# Header row inputs
header_row = tk.Frame(inputs_frame)
header_row.pack(fill=tk.X)
headers = ["Pin", "DCC", "Type", "OutputID"] + [f"LO{i} Out State" for i in range(8)] + ["Input"]
widths = [4,6,8,6] + [6,3]*8 + [10]

for h, w in zip(headers, widths):
    tk.Label(header_row, text=h, width=w).pack(side=tk.LEFT, padx=1)

input_entries = []
input_colors = ["#ff9999", "#ffcc99", "#ffff99", "#99ff99", "#66ffcc", "#66ccff",
                "#9999ff", "#cc99ff", "#ff99ff", "#ff6666", "#ff9966", "#ffff66",
                "#ccff66", "#99ff66", "#66ff66", "#33ff33"]

def update_input_type(event, idx):
    entry_set = input_entries[idx]
    selected = entry_set["type_var"].get()
    for frame in entry_set["linkedOutputFrames"]:
        frame.pack_forget()
    if selected == "route":
        for frame in entry_set["linkedOutputFrames"]:
            frame.pack(side=tk.LEFT, padx=1)

for i in range(16):
    row = tk.Frame(inputs_frame)
    row.pack(fill=tk.X, pady=2)

    pin_entry = tk.Entry(row, width=4)
    pin_entry.insert(0, str(i))
    pin_entry.pack(side=tk.LEFT, padx=1)

    dcc_entry = tk.Entry(row, width=6)
    dcc_entry.insert(0, str(i))
    dcc_entry.pack(side=tk.LEFT, padx=1)

    type_var = tk.StringVar(value="latching")
    type_dropdown = ttk.Combobox(row, textvariable=type_var, values=["momentary", "latching", "route"], width=8)
    type_dropdown.pack(side=tk.LEFT, padx=1)

    outid_entry = tk.Entry(row, width=6)
    outid_entry.insert(0, str(i))
    outid_entry.pack(side=tk.LEFT, padx=1)

    linked_outputs = []
    linkedOutputFrames = []
    for j in range(8):
        lo_frame = tk.Frame(row)
        lo_out_entry = tk.Entry(lo_frame, width=6)
        lo_out_entry.insert(0, str(j))
        lo_out_entry.pack(side=tk.LEFT)
        lo_state_entry = tk.Entry(lo_frame, width=3)
        lo_state_entry.insert(0, "1")
        lo_state_entry.pack(side=tk.LEFT)
        linked_outputs.append( (lo_out_entry, lo_state_entry) )
        linkedOutputFrames.append(lo_frame)

    for frame in linkedOutputFrames:
        frame.pack_forget()

    lbl = tk.Label(row, text=f"Input {i}", bg=input_colors[i], width=10)
    lbl.pack(side=tk.LEFT, padx=1)

    type_dropdown.bind("<<ComboboxSelected>>", lambda event, idx=i: update_input_type(event, idx))

    input_entries.append({
        "pin": pin_entry,
        "dcc": dcc_entry,
        "type_var": type_var,
        "type_dropdown": type_dropdown,
        "outputID": outid_entry,
        "linkedOutputs": linked_outputs,
        "linkedOutputFrames": linkedOutputFrames
    })

# ---------- SPACING FRAME ----------
spacer = tk.Frame(root, width=100)
spacer.pack(side=tk.LEFT)

# ---------- OUTPUTS FRAME ----------
outputs_frame = tk.LabelFrame(root, text="Outputs")
outputs_frame.pack(side=tk.LEFT, padx=10, pady=10)

# Header row outputs
header_row = tk.Frame(outputs_frame)
header_row.pack(fill=tk.X)
headers = ["Output", "PinA", "PinB", "RPinA", "RPinB", "RPinC", "Type", "State", "swTime", "svSpd", "svMin", "svMax", "svPat", "PWM"]
for h in headers:
    tk.Label(header_row, text=h, width=6).pack(side=tk.LEFT, padx=1)

output_entries = []
output_colors = ["#cc99ff", "#99ff66", "#ffcc66", "#ff6666", "#ff99cc", "#cc66ff",
                 "#6666ff", "#66ccff", "#66ffff", "#66ffcc", "#99ff99", "#99ffcc",
                 "#ff9966", "#ff6699", "#ff66cc", "#ff6666"]

def update_output_type(event, output_widgets):
    selected_type = output_widgets["type_var"].get()
    for widget in output_widgets["dynamic_widgets"]:
        widget.pack_forget()
    if selected_type == "servo":
        for widget in output_widgets["servo_widgets"]:
            widget.pack(side=tk.LEFT, padx=1)
    elif selected_type == "pwm":
        output_widgets["pwmVal"].pack(side=tk.LEFT, padx=1)
    elif selected_type == "flash":
        output_widgets["switchTime"].pack(side=tk.LEFT, padx=1)

for i in range(16):
    row = tk.Frame(outputs_frame)
    row.pack(fill=tk.X, pady=2)

    lbl = tk.Label(row, text=f"Output {i}", bg=output_colors[i], width=10)
    lbl.pack(side=tk.LEFT, padx=1)

    pinA = tk.Entry(row, width=4) ; pinA.insert(0, str(i)); pinA.pack(side=tk.LEFT, padx=1)
    pinB = tk.Entry(row, width=4) ; pinB.insert(0, str(i)); pinB.pack(side=tk.LEFT, padx=1)
    rpinA = tk.Entry(row, width=4); rpinA.insert(0, "NA"); rpinA.pack(side=tk.LEFT, padx=1)
    rpinB = tk.Entry(row, width=4); rpinB.insert(0, "NA"); rpinB.pack(side=tk.LEFT, padx=1)
    rpinC = tk.Entry(row, width=4); rpinC.insert(0, "NA"); rpinC.pack(side=tk.LEFT, padx=1)

    type_var = tk.StringVar(value="flash")
    type_dropdown = ttk.Combobox(row, textvariable=type_var, values=["servo", "pwm", "flash"], width=6)
    type_dropdown.pack(side=tk.LEFT, padx=1)

    state_entry = tk.Entry(row, width=3); state_entry.insert(0, "1"); state_entry.pack(side=tk.LEFT, padx=1)
    swtime_entry = tk.Entry(row, width=6); swtime_entry.insert(0, "1000")
    servo_speed_entry = tk.Entry(row, width=4); servo_speed_entry.insert(0, "50")
    servo_min_entry = tk.Entry(row, width=4); servo_min_entry.insert(0, "10")
    servo_max_entry = tk.Entry(row, width=4); servo_max_entry.insert(0, "170")
    servo_pattern_entry = tk.Entry(row, width=4); servo_pattern_entry.insert(0, "0")
    pwmval_entry = tk.Entry(row, width=4); pwmval_entry.insert(0, "128")

    output_widgets = {
        "type_var": type_var,
        "type_dropdown": type_dropdown,
        "switchTime": swtime_entry,
        "pwmVal": pwmval_entry,
        "servo_widgets": [servo_speed_entry, servo_min_entry, servo_max_entry, servo_pattern_entry],
        "dynamic_widgets": [swtime_entry, pwmval_entry, servo_speed_entry, servo_min_entry, servo_max_entry, servo_pattern_entry]
    }

    type_dropdown.bind("<<ComboboxSelected>>", lambda event, ow=output_widgets: update_output_type(event, ow))

    output_entries.append(output_widgets)

root.mainloop()
