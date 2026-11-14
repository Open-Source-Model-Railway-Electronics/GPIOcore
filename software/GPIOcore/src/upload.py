#!/usr/bin/env python3
"""
Upload <repo>/release/GPIOcore.hex to an Arduino Nano with the avrdude that ships with the GUI.

    python src/upload.py            pick the port: the only USB serial port, or choose from a list
    python src/upload.py COM9       use this port
    python src/upload.py --build    build first (src/build.py), then upload

Close the GPIOcore GUI connection (or the serial monitor) first: the port can only be used once.
"""

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

SKETCH_DIR = Path(__file__).resolve().parent.parent      # software/GPIOcore
REPO_ROOT = SKETCH_DIR.parent.parent
HEX_FILE = REPO_ROOT / "release" / "GPIOcore.hex"
AVRDUDE_DIR = REPO_ROOT / "software" / "gui" / "tools" / "avrdude"
BAUD = "115200"       # new Nano bootloader (Optiboot); use 57600 for the old bootloader


def usb_ports():
    """[(port, description)] of USB serial ports."""
    try:
        from serial.tools import list_ports
        return [(p.device, p.description) for p in list_ports.comports() if p.vid is not None]
    except ImportError:
        pass
    cli = os.environ.get("ARDUINO_CLI") or shutil.which("arduino-cli")
    if not cli:
        return []
    out = subprocess.run([cli, "board", "list", "--format", "json"], capture_output=True, text=True).stdout
    try:
        data = json.loads(out or "[]")
    except ValueError:
        return []
    if isinstance(data, dict):                    # arduino-cli 1.x
        data = data.get("detected_ports", [])
    ports = []
    for entry in data:
        port = entry.get("port", {})
        if port.get("protocol") == "serial" and port.get("properties", {}).get("vid"):
            ports.append((port["address"], port.get("protocol_label", "")))
    return ports


def choose_port():
    ports = usb_ports()
    if not ports:
        sys.exit("[FAIL] No USB serial port found. Is the Nano plugged in?")
    if len(ports) == 1:
        return ports[0][0]
    for i, (dev, desc) in enumerate(ports, 1):
        print(f"  {i}: {dev}  {desc}")
    choice = input("Port number: ").strip()
    if not choice.isdigit() or not 1 <= int(choice) <= len(ports):
        sys.exit("[FAIL] No port chosen")
    return ports[int(choice) - 1][0]


def upload(port):
    exe = AVRDUDE_DIR / ("avrdude.exe" if os.name == "nt" else "avrdude")
    if not exe.is_file():
        exe = shutil.which("avrdude")
        if not exe:
            sys.exit(f"[FAIL] avrdude not found in {AVRDUDE_DIR} or on PATH")
    if not HEX_FILE.is_file():
        sys.exit(f"[FAIL] {HEX_FILE} not found: build first (python src/build.py)")
    avr_port = port
    if os.name == "nt" and port.upper().startswith("COM") and int(port[3:] or 0) > 9:
        avr_port = "\\\\.\\" + port                       # COM10 and up
    cmd = [str(exe)]
    conf = Path(exe).with_name("avrdude.conf")
    if conf.is_file():
        cmd += ["-C", str(conf)]
    cmd += ["-p", "atmega328p", "-c", "arduino", "-P", avr_port, "-b", BAUD, "-D",
            "-U", f"flash:w:{HEX_FILE}:i"]
    print(f"[INFO] Uploading {HEX_FILE.name} to {port}", flush=True)
    if subprocess.run(cmd).returncode != 0:
        sys.exit("[FAIL] UPLOAD FAILED!!! (port in use by the GUI? old bootloader: set BAUD to 57600)")
    print("[OK] UPLOAD SUCCESS!!!")


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if "--build" in sys.argv:
        sys.dont_write_bytecode = True       # no __pycache__ in the sketch folder
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import build
        build.build()
    upload(args[0] if args else choose_port())
