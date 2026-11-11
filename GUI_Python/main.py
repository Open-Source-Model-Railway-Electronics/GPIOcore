import tkinter as tk
from tkinter import ttk
from tkinter import messagebox
from input_gui import InputFrame
from output_gui import OutputFrame
from status_balls import StatusIconFrame
import Transceiver
import serial.tools.list_ports
from parser import parse_set_input_data, parse_set_output_data

class AppData:
    def __init__(self):
        self.inputs = []
        self.outputs = []
        self.status_balls = []
        self.transceiver = None
        self.connected = False
        self.send_queue = []
        self.getdata_queue = []

appdata = AppData()

preset_options = [
    "just servos", "servo unifrog relay", "servo electrofrog relays",
    "single steady", "double pulse", "double pulse w/ unifrog relay",
    "double pulse w/ electrofrog relay", "double steady"
]

def message_received_handler(msg, length):
    if msg.command == Transceiver.OPCODE_ACK:
        if appdata.send_queue:
            appdata.send_queue.pop(0)
            send_next_in_queue()
        if appdata.getdata_queue:
            appdata.getdata_queue.pop(0)
            send_next_getdata()
        return

    if msg.command == Transceiver.OPCODE_SET_INPUT_DATA:
        parse_set_input_data(msg.payload, appdata, input_frame, silent=True)
        if appdata.getdata_queue:
            appdata.getdata_queue.pop(0)
            send_next_getdata()
    elif msg.command == Transceiver.OPCODE_SET_OUTPUT_DATA:
        parse_set_output_data(msg.payload, appdata)
        if appdata.getdata_queue:
            appdata.getdata_queue.pop(0)
            send_next_getdata()
    elif (msg.command & 0x50) == Transceiver.OPCODE_SEND_VERSION:
        queue_getdata_requests()
        send_next_getdata()
    elif (msg.command == 0x70 ): # ACK
        print("ACK received")
    elif (msg.command == 0x71 ): # NACK
        print("NACK received" + (str) )
        error_map = {
            1: "Invalid checksum",
            2: "Invalid length",
            3: "Timeout",
        }
        if msg.payload:
            err_code = msg.payload[0]
            err_msg = error_map.get(err_code, f"Unknown error code {err_code}")

        messagebox.showerror("Communication Error", f"NACK received:\n{err_msg}")

def queue_getdata_requests():
    for i in range(16):
        mess = Transceiver.Message()
        mess.command = Transceiver.OPCODE_GET_INPUT_DATA
        mess.payload[0] = i
        appdata.getdata_queue.append((mess, 'input', i))
    for i in range(16):
        mess = Transceiver.Message()
        mess.command = Transceiver.OPCODE_GET_OUTPUT_DATA
        mess.payload[0] = i
        appdata.getdata_queue.append((mess, 'output', i))

def send_next_getdata():
    if appdata.getdata_queue:
        mess, typ, idx = appdata.getdata_queue[0]
        appdata.transceiver.sendMessage(mess)

def send_next_in_queue():
    if appdata.send_queue:
        mess, typ, idx = appdata.send_queue[0]
        appdata.transceiver.sendMessage(mess)

def send_data():
    if not appdata.transceiver:
        return

    appdata.send_queue.clear()

    for i, row in enumerate(appdata.inputs):
        if row["dirty"]:
            mess = Transceiver.Message()
            mess.command = Transceiver.OPCODE_SET_INPUT_DATA
            mess.payload[0] = i
            addr = int(row["dcc"].get()) if row["dcc"].get().isdigit() else 0
            mess.payload[1] = (addr >> 8) & 0xFF
            mess.payload[2] = addr & 0xFF
            mess.payload[3] = int(row["pin"].get()) if row["pin"].get().isdigit() else 0
            mess.payload[4] = 0 if row["type"].get() == "momentary" else 1 if row["type"].get() == "latching" else 2
            mess.payload[5] = int(row["output"].get()) if row["output"].get().isdigit() else 0

            for j, e in enumerate(row["lo_entries"]):
                num = int(e["num"].get()) if e["num"].get().isdigit() else 0
                straight = e["straight"].get()
                mess.payload[6 + j] = (num & 0x7F) | (0x80 if straight else 0x00)

            appdata.send_queue.append((mess, 'input', i))
            row["dirty"] = False

    for i, row in enumerate(appdata.outputs):
        if row["dirty"]:
            mess = Transceiver.Message()
            mess.command = Transceiver.OPCODE_SET_OUTPUT_DATA
            mess.payload[0] = i
            mess.payload[1] = int(row["pinA"].get()) if row["pinA"].get().isdigit() else 0
            mess.payload[2] = int(row["pinB"].get()) if row["pinB"].get().isdigit() else 0
            mess.payload[3] = int(row["rpinA"].get()) if row["rpinA"].get().isdigit() else 0
            mess.payload[4] = int(row["rpinB"].get()) if row["rpinB"].get().isdigit() else 0
            mess.payload[5] = int(row["rpinC"].get()) if row["rpinC"].get().isdigit() else 0
            mess.payload[6] = 0
            mess.payload[7] = int(row["state"].get()) if row["state"].get().isdigit() else 0

            for j in range(4):
                mess.payload[8 + j] = 0

            appdata.send_queue.append((mess, 'output', i))
            row["dirty"] = False

    if appdata.send_queue:
        send_next_in_queue()

def send_preset():
    selected_preset = preset_var.get()
    if not appdata.transceiver:
        return

    mess = Transceiver.Message()
    mess.command = Transceiver.OPCODE_SET_PRESET
    mess.payload[0] = preset_options.index(selected_preset) if selected_preset in preset_options else 0
    appdata.transceiver.sendMessage(mess)

def update_all_colors(status_frame):
    # === Fase 1: Outputs nemen kleur over van gekoppelde inputs ===
    output_colors = []
    for idx, output_row in enumerate(appdata.outputs):
        kleur = output_row["lbl"].cget("bg")
        for input_row in appdata.inputs:
            output_str = input_row["output"].get()
            if output_str.isdigit() and int(output_str) == idx:
                kleur = input_row["label"].cget("bg")
                break
        output_row["lbl"].config(bg=kleur)
        output_colors.append(kleur)

    # === Fase 2: Bollen nemen kleur over van output blokjes ===
    for pin in range(16):
        kleur = "grey"
        for idx, output_row in enumerate(appdata.outputs):
            for p in ["pinA", "pinB", "rpinA", "rpinB", "rpinC"]:
                val = output_row[p].get()
                if val.isdigit() and int(val) == pin:
                    kleur = output_colors[idx]
                    break
            if kleur != "grey":
                break
        set_ball_color(status_frame, pin, kleur)

def set_ball_color(status_frame, pin, color):
    try:
        if 0 <= pin < len(status_frame.ball_items):
            status_frame.itemconfig(status_frame.ball_items[pin], fill=color)
    except Exception as e:
        print(f"[ERROR] set_ball_color({pin}): {e}")

def main():
    global appdata, input_frame, preset_var
    root = tk.Tk()
    root.title("Jack Of All Trades")
    root.geometry("1800x900")

    main_container = tk.Frame(root)
    main_container.pack(fill=tk.BOTH, expand=True)

    left_frame = tk.Frame(main_container)
    left_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

    right_frame = tk.Frame(main_container)
    right_frame.pack(side=tk.RIGHT, fill=tk.Y, padx=10)

    inputs_outputs_frame = tk.Frame(left_frame)
    inputs_outputs_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

    input_frame = InputFrame(inputs_outputs_frame, appdata)
    input_frame.grid(row=0, column=0, sticky="nsew")
    appdata.inputs = input_frame.rows

    output_frame = OutputFrame(inputs_outputs_frame)
    output_frame.grid(row=1, column=0, sticky="nsew")
    appdata.outputs = output_frame.rows

    status_frame = StatusIconFrame(right_frame, appdata, output_frame)
    status_frame.pack(side=tk.TOP, fill=tk.Y, pady=10)
    appdata.status_balls = status_frame.ball_items

    # Re-enable icons
    status_frame.update_icons()

    com_frame = tk.Frame(right_frame)
    com_frame.pack(pady=5, anchor="n")
    tk.Label(com_frame, text="Select COM Port:").pack(side=tk.LEFT)
    com_var = tk.StringVar()
    ports = [port.device for port in serial.tools.list_ports.comports()]
    com_dropdown = ttk.Combobox(com_frame, textvariable=com_var, values=ports, width=10)
    com_dropdown.pack(side=tk.LEFT)
    connect_button = tk.Button(com_frame, text="Connect", command=lambda: connect_serial())
    connect_button.pack(side=tk.LEFT)
    status_label = tk.Label(right_frame, text="No COM port connected.", fg="red")
    status_label.pack()

    preset_frame = tk.Frame(right_frame)
    preset_frame.pack(pady=5, anchor="n")
    tk.Label(preset_frame, text="Preset").pack(side=tk.LEFT)
    preset_var = tk.StringVar()
    preset_dropdown = ttk.Combobox(preset_frame, textvariable=preset_var, values=preset_options, width=30)
    preset_dropdown.pack(side=tk.LEFT)
    preset_dropdown.set(preset_options[0])
    send_preset_button = tk.Button(preset_frame, text="Send", command=send_preset)
    send_preset_button.pack(side=tk.LEFT)

    def connect_serial():
        if not appdata.connected:
            port = com_var.get()
            if port:
                appdata.transceiver = Transceiver.Transceiver(port=port, baudrate=115200)
                appdata.transceiver.messageReceived = message_received_handler
                status_label.config(text=f"Connected to {port}", fg="green")
                connect_button.config(text="Disconnect")
                appdata.connected = True
            else:
                status_label.config(text="No COM port selected.", fg="red")
        else:
            if appdata.transceiver:
                appdata.transceiver.close()
                appdata.transceiver = None
            status_label.config(text="Disconnected.", fg="red")
            connect_button.config(text="Connect")
            appdata.connected = False

    def periodic_receive():
        if appdata.transceiver:
            appdata.transceiver.receive()\

    def periodic_send():
        send_data()
        root.after(333, periodic_send)

    def periodic_update():
        update_all_colors(status_frame)
        status_frame.update_icons()  # voeg deze regel toe
        root.after(500, periodic_update)

    periodic_send()
    periodic_receive()
    periodic_update()
    root.mainloop()

if __name__ == "__main__":
    main()
