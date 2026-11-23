#!/usr/bin/env python3
"""
Release a new version (run from the repository root, or from anywhere):

    python release.py              ask the version and notes, build, then publish
    python release.py --no-build   publish what is in release/ now (it must contain the new version!)
    python release.py --dry-run    show what would happen; changes nothing

Steps:
1. Asks the version (e.g. 0.2, v0.2 or 0.2.1) and the release notes.
2. Writes the version into the project:
       software/**/src/version.h     const char * version = "v0.2" ;   (the firmware reports it)
       software/*/CMakeLists.txt     project(<name> VERSION 0.2 ...)    (the GUI shows it)
3. Builds everything into release/ (build.py), so the files carry the new version.
4. Asks for a final OK, then: commit "Release v0.2", tag v0.2, push both.
5. Makes a GitHub release v0.2 with release/ as <repo>-v0.2.zip, the setup.exe (build.py puts it in the root),
   plus the .hex files on their own.

Needs git and the GitHub CLI (gh auth login). The same file works in other repositories with this layout.
"""

import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RELEASE_DIR = ROOT / "release"
REPO = "IOcore"                  # the product name (the local folder may still have another name)
sys.path.insert(0, str(ROOT))
import build as builder         # noqa: E402  (build.py next to this file: make_setup)


def fail(msg):
    sys.exit(f"[FAIL] {msg}")


def git(*args, check=True):
    res = subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True)
    if check and res.returncode != 0:
        fail(f"git {' '.join(args)}: {res.stderr.strip()}")
    return res.stdout.strip()


def ask(question):
    return input(question).strip()


def version_files(version):
    """[(path, new text)] for every file whose version changes."""
    tag, number = "v" + version, version
    changes = []
    for p in ROOT.glob("software/**/src/version.h"):
        old = p.read_text(encoding="utf-8")
        new = re.sub(r'(const\s+char\s*\*\s*version\s*=\s*")[^"]*(")', rf"\g<1>{tag}\g<2>", old)
        if new != old:
            changes.append((p, new))
    for p in ROOT.glob("software/*/CMakeLists.txt"):
        old = p.read_text(encoding="utf-8")
        new = re.sub(r"(project\(\s*\w+\s+VERSION\s+)\d+(?:\.\d+)*", rf"\g<1>{number}", old, count=1)
        if new != old:
            changes.append((p, new))
    return changes


def make_zip(tag, out_dir):
    base = out_dir / f"{REPO}-{tag}"
    staged = out_dir / "stage" / f"{REPO}-{tag}"
    shutil.copytree(RELEASE_DIR, staged)
    return Path(shutil.make_archive(str(base), "zip", root_dir=staged.parent))


def main():
    dry = "--dry-run" in sys.argv
    build_first = "--no-build" not in sys.argv
    for tool in ("git", "gh"):
        if not shutil.which(tool):
            fail(f"{tool} not found")
    if Path(git("rev-parse", "--show-toplevel")).resolve() != ROOT:
        fail(f"{ROOT} is not the root of a git repository")

    dirty = git("status", "--porcelain", "--untracked-files=no")   # untracked folders (e.g. hardware drafts) don't matter
    if dirty:
        print("[WARN] Uncommitted changes (they would be in the build, but not in the tag):\n" + dirty)
        if not dry and ask("Continue anyway? [y/N] ").lower() != "y":
            fail("commit or stash your changes first")

    last = git("describe", "--tags", "--abbrev=0", check=False) or "none"
    version = ask(f"Version to release (last: {last}), e.g. 0.2: ").lstrip("vV")
    if not re.fullmatch(r"\d+\.\d+(\.\d+)?", version):              # 0.2, or 0.2.1 for a quick fix
        fail(f"'{version}' is not a version like 0.2")
    tag = "v" + version
    if git("tag", "--list", tag):
        fail(f"tag {tag} already exists")
    print("Release notes; finish with an empty line:")
    lines = []
    while (line := input()) != "":
        lines.append(line)
    notes = "\n".join(lines) or f"{REPO} {tag}"
    pre_default = version.startswith("0.")                   # v0.x: not yet tested on enough hardware
    answer = ask(f"Mark as pre-release (beta)? [{'Y/n' if pre_default else 'y/N'}] ").lower()
    prerelease = answer == "y" or ( answer == "" and pre_default )

    changes = version_files(version)
    for p, text in changes:
        print(f"[INFO] version {tag} -> {p.relative_to(ROOT).as_posix()}")
        if not dry:
            p.write_text(text, encoding="utf-8")

    if build_first and not dry:
        if subprocess.run([sys.executable, "-B", str(ROOT / "build.py")], cwd=ROOT).returncode != 0:
            fail("build failed (the version files are changed but nothing is committed)")
    files = [p for p in RELEASE_DIR.rglob("*") if p.is_file()] if RELEASE_DIR.is_dir() else []
    if not files:
        fail("release/ is empty: run build.py first")
    hexes = sorted(RELEASE_DIR.glob("*.hex"))
    size = sum(p.stat().st_size for p in files) / 1e6

    with tempfile.TemporaryDirectory() as tmp:
        archive = make_zip(tag, Path(tmp))
        setup = None if dry else (ROOT / f"{REPO}-v{version}-setup.exe")   # build.py made it
        if setup and not setup.is_file():
            setup = builder.make_setup(version)                           # --no-build
        print(f"\n  release : {REPO} {tag}{'  (pre-release)' if prerelease else ''}"
              f"\n  zip     : {archive.name} ({len(files)} files, {size:.1f} MB unpacked)"
              f"\n  setup   : {setup.name if setup else '- (no Inno Setup or installer script)'}"
              f"\n  also    : {', '.join(p.name for p in hexes) or '-'}\n  notes   : {notes.splitlines()[0]}")
        if dry:
            print("\n[DRY RUN] nothing committed, tagged, pushed or published")
            return
        if ask("\nCommit, tag, push and publish this on GitHub? [y/N] ").lower() != "y":
            fail("stopped: the version files are changed, nothing is committed")

        if changes:
            git("add", *[str(p) for p, _ in changes])
            git("commit", "-m", f"Release {tag}", "-m", notes)
        git("tag", "-a", tag, "-m", notes)
        git("push")
        git("push", "origin", tag)
        assets = [str(archive)] + ([str(setup)] if setup else []) + [str(h) for h in hexes]
        res = subprocess.run(["gh", "release", "create", tag, *assets,
                              "--title", f"{REPO} {tag}", "--notes", notes,
                              *(["--prerelease"] if prerelease else [])], cwd=ROOT)
        if res.returncode != 0:
            fail(f"GitHub release failed; the tag is pushed. Retry with: gh release create {tag} <zip>")
    print(f"\n[OK] {REPO} {tag} released")


if __name__ == "__main__":
    main()
