#!/usr/bin/env python3
"""
Build everything into release/ (run from the repository root, or from anywhere):

    python build.py

1. Empties release/ (it only holds build output, it is not in git).
2. Runs every build script of the project, in this order:
       software/*/src/build.py     firmware (arduino-cli)  -> release/<name>.hex
       software/*/build.py         other parts, e.g. the GUI -> release/
   Each script puts its own result in release/.
3. With Inno Setup and installer/<repo>.iss: the installer <repo>-v<version>-setup.exe in the root
   (the version from software/*/CMakeLists.txt; older setups in the root are removed).

The same file works in other repositories with this layout.
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RELEASE_DIR = ROOT / "release"
REPO = "IOcore"                  # the product name (the local folder may still have another name)


def build_scripts():
    return sorted(ROOT.glob("software/*/src/build.py")) + sorted(ROOT.glob("software/*/build.py"))


def empty(folder):
    """Remove everything in folder. A subfolder that is open (e.g. in Explorer) is emptied and kept."""
    for item in folder.iterdir():
        try:
            shutil.rmtree(item) if item.is_dir() else item.unlink()
        except PermissionError:
            if not item.is_dir():
                sys.exit(f"[FAIL] {item} is in use: close the program using it (IOcoreGUI.exe?) and try again")
            empty(item)


def project_version():
    """The version in software/*/CMakeLists.txt (release.py writes it there), or None."""
    for p in sorted(ROOT.glob("software/*/CMakeLists.txt")):
        m = re.search(r"project\(\s*\w+\s+VERSION\s+(\d+(?:\.\d+)*)", p.read_text(encoding="utf-8"))
        if m:
            return m.group(1)
    return None


def find_iscc():
    """Inno Setup compiler, or None (then no installer is made)."""
    for p in (os.environ.get("ISCC"), shutil.which("ISCC"),
              r"C:\Program Files\Inno Setup 7\ISCC.exe", r"C:\Program Files (x86)\Inno Setup 7\ISCC.exe",
              r"C:\Program Files\Inno Setup 6\ISCC.exe", r"C:\Program Files (x86)\Inno Setup 6\ISCC.exe"):
        if p and Path(p).is_file():
            return p
    return None


def make_setup(version=None):
    """installer/<REPO>.iss -> <REPO>-v<version>-setup.exe in the root; None without Inno Setup or script."""
    iss = ROOT / "installer" / f"{REPO}.iss"
    iscc = find_iscc()
    version = version or project_version()
    if not iss.is_file() or not iscc or not version:
        return None
    for old in ROOT.glob(f"{REPO}-v*-setup.exe"):
        try:
            old.unlink()
        except PermissionError:
            sys.exit(f"[FAIL] {old.name} is in use: close it and try again")
    with tempfile.TemporaryDirectory() as tmp:   # outside Dropbox: it locks a file that is being written
        res = subprocess.run([iscc, "-Q", f"-DAppVersion={version}", f"-DSourceDir={RELEASE_DIR}",
                              f"-DOutputDir={tmp}", str(iss)], cwd=ROOT)
        if res.returncode != 0:
            sys.exit("[FAIL] Inno Setup could not build the installer")
        for made in Path(tmp).glob("*-setup.exe"):
            return Path(shutil.copy2(made, ROOT / made.name))
    return None


def build():
    scripts = build_scripts()
    if not scripts:
        sys.exit("[FAIL] no build scripts found (software/*/src/build.py, software/*/build.py)")
    RELEASE_DIR.mkdir(exist_ok=True)
    empty(RELEASE_DIR)                          # the folder itself may be open in Explorer
    for script in scripts:
        print(f"\n[INFO] === {script.relative_to(ROOT).as_posix()} ===", flush=True)
        if subprocess.run([sys.executable, "-B", str(script)], cwd=script.parent).returncode != 0:
            sys.exit(f"[FAIL] {script.relative_to(ROOT).as_posix()} failed")
    setup = make_setup()
    print(f"\n[OK] BUILD READY in {RELEASE_DIR}"
          + (f"\n[OK] installer: {setup.name}" if setup else "\n[INFO] no installer (Inno Setup not found)"))


if __name__ == "__main__":
    build()
