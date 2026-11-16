#!/usr/bin/env python3
"""
Build the GUI for end users into <repo>/release/:

    GPIOcoreGUI.exe       the configuration program (Release build) + the Qt files it needs
    gpiocore_fw_host.exe  the firmware compiled for the PC: the GUI's simulator runs it
    tools/avrdude/        the upload program the GUI uses to put the firmware on a Nano
    examples/             example board files

    python software/gui/build.py         (the root build.py also builds the firmware)

The GUI is built in a temporary folder that is removed afterwards.
Qt is found in C:/Qt (newest 6.x with MinGW) or via QT_DIR=<...>/mingw_64.
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

GUI_DIR = Path(__file__).resolve().parent                    # software/gui
REPO_ROOT = GUI_DIR.parent.parent
RELEASE_DIR = REPO_ROOT / "release"
EXE = "GPIOcoreGUI.exe"
FW_HOST = "gpiocore_fw_host.exe"                             # must be next to EXE (Simulate mode)


def fail(msg):
    sys.exit(f"[FAIL] {msg}")


def version_key(p):
    return [int(x) for x in re.findall(r"\d+", p.name)]


def find_qt():
    """(qt prefix e.g. C:/Qt/6.10.1/mingw_64, [extra PATH dirs: compiler, cmake, ninja])"""
    qt = os.environ.get("QT_DIR")
    root = Path(os.environ.get("QT_ROOT", "C:/Qt"))
    if not qt:
        kits = sorted(root.glob("6.*/mingw_64"), key=lambda p: version_key(p.parent))
        if not kits:
            fail(f"no Qt 6 MinGW kit found in {root} (set QT_DIR)")
        qt = kits[-1]
    qt = Path(qt)
    extra = [qt / "bin"]
    compilers = sorted(root.glob("Tools/mingw*_64/bin"), key=lambda p: version_key(p.parent))
    if compilers:
        extra.insert(0, compilers[-1])
    for tool in ("Tools/CMake_64/bin", "Tools/Ninja"):
        if (root / tool).is_dir():
            extra.append(root / tool)
    return qt, extra


def run(cmd, env):
    cmd = [str(c) for c in cmd]
    # Windows looks the program up in our own PATH, not in env's: resolve it here
    cmd[0] = shutil.which(cmd[0], path=env["PATH"]) or fail(f"{cmd[0]} not found")
    print("[RUN] " + " ".join(cmd), flush=True)
    if subprocess.run(cmd, env=env).returncode != 0:
        fail(f"{Path(str(cmd[0])).name} failed")


def build_gui():
    qt, extra = find_qt()
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join(str(p) for p in extra) + os.pathsep + env.get("PATH", "")
    print(f"[INFO] Qt: {qt}", flush=True)
    build_dir = Path(tempfile.mkdtemp(prefix="gpiocore_gui_"))
    try:
        run(["cmake", "-S", GUI_DIR, "-B", build_dir, "-G", "Ninja",
             "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_PREFIX_PATH={qt}"], env)
        run(["cmake", "--build", build_dir, "--target", "GPIOcoreGUI", "gpiocore_fw_host"], env)
        exe = build_dir / EXE
        if not exe.is_file():
            fail(f"{EXE} was not built")
        if not (build_dir / FW_HOST).is_file():
            fail(f"{FW_HOST} was not built")
        RELEASE_DIR.mkdir(exist_ok=True)
        shutil.copy2(exe, RELEASE_DIR / EXE)
        shutil.copy2(build_dir / FW_HOST, RELEASE_DIR / FW_HOST)   # statically linked, no DLLs
        # Qt libraries, plugins, Qt's own translations and the MinGW runtime next to the exe
        run([qt / "bin" / "windeployqt.exe", "--release", "--no-opengl-sw", "--no-system-d3d-compiler",
             "--dir", RELEASE_DIR, RELEASE_DIR / EXE], env)
    finally:
        shutil.rmtree(build_dir, ignore_errors=True)

    for sub in ("tools", "examples"):
        if (GUI_DIR / sub).is_dir():
            shutil.copytree(GUI_DIR / sub, RELEASE_DIR / sub, dirs_exist_ok=True)
    print(f"[OK] GUI -> {RELEASE_DIR / EXE}")


if __name__ == "__main__":
    build_gui()
