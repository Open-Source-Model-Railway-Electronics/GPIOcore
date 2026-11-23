#!/usr/bin/env python3
"""
Build the IOcore firmware and put the hex straight in <repo>/release/IOcore.hex.
Also fills <repo>/release/bootloader/ for boards burnt through ICSP: the Nano bootloader, the firmware
with that bootloader (IOcore_with_bootloader.hex) and burn-bootloader.bat (see bootloader/README.md).

    python src/build.py            (from software/IOcore, or from anywhere)

Compiles in a temporary folder that is removed afterwards: no build/ folder is left behind.
Needs arduino-cli on PATH (or ARDUINO_CLI=<path to arduino-cli>).
"""

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

FQBN = "arduino:avr:nano"

SKETCH_DIR = Path(__file__).resolve().parent.parent      # software/IOcore
REPO_ROOT = SKETCH_DIR.parent.parent                     # the IOcore repository
RELEASE_DIR = REPO_ROOT / "release"
HEX_NAME = "IOcore.hex"
BOOT_SRC = SKETCH_DIR / "bootloader"                     # optiboot hex, burn script, README
BOOT_DIR = RELEASE_DIR / "bootloader"
BOOT_HEX_NAME = "IOcore_with_bootloader.hex"


def find_arduino_cli():
    exe = os.environ.get("ARDUINO_CLI") or shutil.which("arduino-cli")
    if not exe:
        sys.exit("[FAIL] arduino-cli not found: put it on PATH or set ARDUINO_CLI")
    return exe


def build():
    cli = find_arduino_cli()
    print(f"[INFO] Building {SKETCH_DIR.name} ({FQBN})")
    out_dir = Path(tempfile.mkdtemp(prefix="iocore_fw_"))
    try:
        code = subprocess.run([cli, "compile", "-b", FQBN, "--output-dir", str(out_dir), str(SKETCH_DIR)]).returncode
        if code != 0:
            sys.exit("[FAIL] BUILD FAILED!!!")
        hex_file = out_dir / f"{SKETCH_DIR.name}.ino.hex"      # the one without bootloader (for avrdude -D)
        if not hex_file.is_file():
            sys.exit(f"[FAIL] no hex file made: {hex_file}")
        RELEASE_DIR.mkdir(exist_ok=True)
        target = RELEASE_DIR / HEX_NAME
        shutil.copy2(hex_file, target)
        boot_hex = out_dir / f"{SKETCH_DIR.name}.ino.with_bootloader.hex"   # firmware + Optiboot (ICSP)
        shutil.copytree(BOOT_SRC, BOOT_DIR, dirs_exist_ok=True)
        if boot_hex.is_file():
            shutil.copy2(boot_hex, BOOT_DIR / BOOT_HEX_NAME)
        print(f"[OK] BUILD SUCCESS!!! -> {target}")
        print(f"[OK] bootloader files -> {BOOT_DIR}")
        return target
    finally:
        shutil.rmtree(out_dir, ignore_errors=True)


if __name__ == "__main__":
    build()
