#!/usr/bin/env python3
"""
Build everything into release/ (run from the repository root, or from anywhere):

    python build.py

1. Empties release/ (it only holds build output, it is not in git).
2. Runs every build script of the project, in this order:
       software/*/src/build.py     firmware (arduino-cli)  -> release/<name>.hex
       software/*/build.py         other parts, e.g. the GUI -> release/
   Each script puts its own result in release/.

The same file works in other repositories with this layout.
"""

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RELEASE_DIR = ROOT / "release"


def build_scripts():
    return sorted(ROOT.glob("software/*/src/build.py")) + sorted(ROOT.glob("software/*/build.py"))


def build():
    scripts = build_scripts()
    if not scripts:
        sys.exit("[FAIL] no build scripts found (software/*/src/build.py, software/*/build.py)")
    RELEASE_DIR.mkdir(exist_ok=True)
    for item in RELEASE_DIR.iterdir():          # empty it; the folder itself may be open in Explorer
        try:
            shutil.rmtree(item) if item.is_dir() else item.unlink()
        except PermissionError:
            sys.exit(f"[FAIL] {item} is in use: close the program using it (GPIOcoreGUI.exe?) and try again")
    for script in scripts:
        print(f"\n[INFO] === {script.relative_to(ROOT).as_posix()} ===", flush=True)
        if subprocess.run([sys.executable, "-B", str(script)], cwd=script.parent).returncode != 0:
            sys.exit(f"[FAIL] {script.relative_to(ROOT).as_posix()} failed")
    print(f"\n[OK] BUILD READY in {RELEASE_DIR}")


if __name__ == "__main__":
    build()
