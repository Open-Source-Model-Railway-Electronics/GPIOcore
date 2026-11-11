import tkinter as tk

def type_string_from_code(code):
    if code == 0: return "momentary"
    elif code == 1: return "latching"
    else: return "route"

def parse_set_input_data(payload, appdata, input_frame, silent=False):
    index = payload[0]
    addr = (payload[1] << 8) | payload[2]
    pin = payload[3]
    type_ = payload[4]
    output = payload[5]
    linked_outputs = payload[6:14]

    if 0 <= index < len(appdata.inputs):
        row = appdata.inputs[index]
        row["pin"].delete(0, 'end')
        row["pin"].insert(0, str(pin))
        row["dcc"].delete(0, 'end')
        row["dcc"].insert(0, str(addr))
        row["type"].set("momentary" if type_ == 0 else "latching" if type_ == 1 else "route")
        row["output"].delete(0, 'end')
        row["output"].insert(0, str(output))

        for j, val in enumerate(linked_outputs):
            num = val & 0x7F
            is_straight = (val & 0x80) != 0
            lo_entry = row["lo_entries"][j]
            lo_entry["num"].delete(0, 'end')
            lo_entry["num"].insert(0, str(num))
            if is_straight:
                lo_entry["straight"].set(1)
                lo_entry["curved"].set(0)
            else:
                lo_entry["straight"].set(0)
                lo_entry["curved"].set(1)

        input_frame.update_lo_fields(row)

        if not silent:
            row["dirty"] = True

def parse_set_output_data(payload, appdata):
    index = payload[0]
    pinA = payload[1]
    pinB = payload[2]
    relayPinA = payload[3]
    relayPinB = payload[4]
    relayPinC = payload[5]
    type_ = payload[6]
    state = payload[7]
    union_bytes = payload[8:12]

    type_options = [
        "Single Steady", "Single Pulse", "Single Flashing",
        "Double Steady", "Double Pulse", "Double Flashing",
        "Servo", "PWM"
    ]

    if 0 <= index < len(appdata.outputs):
        row = appdata.outputs[index]

        row["pinA"].delete(0, tk.END)
        row["pinA"].insert(0, str(pinA))
        row["pinB"].delete(0, tk.END)
        row["pinB"].insert(0, str(pinB))
        row["rpinA"].delete(0, tk.END)
        row["rpinA"].insert(0, str(relayPinA))
        row["rpinB"].delete(0, tk.END)
        row["rpinB"].insert(0, str(relayPinB))
        row["rpinC"].delete(0, tk.END)
        row["rpinC"].insert(0, str(relayPinC))
        row["state"].delete(0, tk.END)
        row["state"].insert(0, str(state))

        if 0 <= type_ < len(type_options):
            type_name = type_options[type_]
        else:
            type_name = "Unknown"

        row["type"].set(type_name)
        row["type_dropdown"].set(type_name)

        dyn_frame = row["dynamic"]
        for widget in dyn_frame.winfo_children():
            widget.destroy()

        if type_name.lower() == "servo":
            servoMin = union_bytes[0]
            servoMax = union_bytes[1]
            servoSpeed = union_bytes[2]
            servoPlace = union_bytes[3]

            for label, val in zip(["Min", "Max", "Speed", "Place"], [servoMin, servoMax, servoSpeed, servoPlace]):
                tk.Label(dyn_frame, text=label, width=6).pack(side=tk.LEFT)
                e = tk.Entry(dyn_frame, width=4)
                e.insert(0, str(val))
                e.pack(side=tk.LEFT)

        elif type_name.lower() == "pwm":
            pwmMax = union_bytes[0]
            pwmFading = union_bytes[1]

            for label, val in zip(["Max", "Fading"], [pwmMax, pwmFading]):
                tk.Label(dyn_frame, text=label, width=6).pack(side=tk.LEFT)
                e = tk.Entry(dyn_frame, width=4)
                e.insert(0, str(val))
                e.pack(side=tk.LEFT)

        else:
            switchTime = (union_bytes[0] << 24) | (union_bytes[1] << 16) | (union_bytes[2] << 8) | union_bytes[3]
            tk.Label(dyn_frame, text="Switch Time").pack(side=tk.LEFT)
            e = tk.Entry(dyn_frame, width=8)
            e.insert(0, str(switchTime))
            e.pack(side=tk.LEFT)
