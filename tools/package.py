#!/usr/bin/env python3
"""Assemble an x86_64 release directory from a Qt build and verified Temurin archive."""
import argparse
from collections import deque
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import zipfile

JRE = {
    "linux": "2edef9e4f81ec5ee426d023336d4cfbd7b4644e99884e7a73a6a867b70b17acf",
    "windows": "3260a1b9899f30ac80d737daafc053042c525249f52d066101fbf81d78d73f25",
}


def java(archive, stage, platform):
    with archive.open("rb") as file:
        if hashlib.file_digest(file, "sha256").hexdigest() != JRE[platform]:
            raise ValueError("Expected the pinned Temurin 25.0.5.1+2 x64 JRE archive")
    unpacked = stage / ".runtime"
    if platform == "linux":
        with tarfile.open(archive) as file:
            file.extractall(unpacked, filter="data")
    else:
        with zipfile.ZipFile(archive) as file:
            file.extractall(unpacked)
    [runtime] = unpacked.iterdir()
    runtime.rename(stage / "runtime")
    unpacked.rmdir()


def deploy_linux(qt_lib, plugins, stage):
    library = stage / "lib"
    library.mkdir()
    roots = ["Core", "Gui", "Widgets", "Network", "DBus", "XcbQpa", "OpenGL", "WaylandClient"]
    for name in roots:
        source = qt_lib / f"libQt6{name}.so.6"
        if source.exists():
            shutil.copy2(source, library)
    for group, names in {
        "platforms": ["libqxcb.so", "libqwayland.so"],
        "tls": ["libqopensslbackend.so", "libqcertonlybackend.so"],
        "wayland-shell-integration": ["libxdg-shell.so"],
    }.items():
        target = stage / "plugins" / group
        target.mkdir(parents=True)
        for name in names:
            source = plugins / group / name
            if source.exists():
                shutil.copy2(source, target)
    script = stage / "启动 GTNH.sh"
    script.write_text(
        '#!/bin/sh\n'
        'set -eu\n'
        'app_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
        'export LD_LIBRARY_PATH="$app_dir/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\n'
        'exec "$app_dir/GtnhLauncher" "$@"\n',
        encoding="utf-8",
    )
    script.chmod(0o755)


def deploy_windows(qt_bin, plugins, stage, objdump):
    library = {file.name.lower(): file for file in qt_bin.glob("*.dll")}
    queue = deque([stage / "GtnhLauncher.exe"])
    for group, name in (("platforms", "qwindows.dll"), ("tls", "qschannelbackend.dll")):
        target = stage / "plugins" / group
        target.mkdir(parents=True)
        queue.append(Path(shutil.copy2(plugins / group / name, target)))
    included = set()
    while queue:
        binary = queue.popleft()
        imports = subprocess.check_output([objdump, "-p", binary], text=True)
        for name in re.findall(r"DLL Name: (\S+)", imports):
            key = name.lower()
            if key in included:
                continue
            if key in library:
                included.add(key)
                queue.append(Path(shutil.copy2(library[key], stage)))
            elif key.startswith(("qt6", "libgcc", "libstdc++", "libwinpthread", "libssp")):
                raise FileNotFoundError(f"Missing {name}, imported by {binary.name}")


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("platform", choices=JRE)
parser.add_argument("build", type=Path)
parser.add_argument("qt_lib", type=Path, help="Qt lib directory on Linux, bin directory on Windows")
parser.add_argument("plugins", type=Path)
parser.add_argument("jre", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--server-mods", type=Path, required=True, help="Directory with the pinned Galaxy client add-ons")
parser.add_argument("--objdump", default="objdump", help="objdump with PE support for Windows deployment")
args = parser.parse_args()

stage = args.output
stage.mkdir(parents=True, exist_ok=False)
java(args.jre, stage, args.platform)
name = "GtnhLauncher.exe" if args.platform == "windows" else "GtnhLauncher"
for file in (name, "Extract.class"):
    shutil.copy2(args.build / file, stage)
(stage / "qt.conf").write_text("[Paths]\nPlugins=plugins\n", encoding="utf-8")
if args.platform == "windows":
    deploy_windows(args.qt_lib, args.plugins, stage, args.objdump)
else:
    deploy_linux(args.qt_lib, args.plugins, stage)
repository = Path(__file__).resolve().parent.parent
mods = json.loads((repository / "resources/pack.json").read_text())["galaxyMods"]
overlay = stage / "galaxy-mods"
overlay.mkdir()
for name, digest in mods.items():
    source = args.server_mods / name
    if hashlib.sha256(source.read_bytes()).hexdigest() != digest:
        raise ValueError(f"Incorrect Galaxy mod bytes: {name}")
    shutil.copy2(source, overlay)

for file in ("README.md", "LICENSE", "THIRD-PARTY.md"):
    shutil.copy2(repository / file, stage)
