#!/usr/bin/env python3
"""Translate the pinned GTNH client ZIP metadata to the launcher's two targets."""
import argparse
import hashlib
import json
from pathlib import Path
from urllib.parse import urlsplit
from zipfile import ZipFile

PACK_URL = "https://downloads.gtnewhorizons.com/Multi_mc_downloads/GT_New_Horizons_2.9.0_Java_17-26.zip"
PACK_HASH = "6d0ebe7e6be18fb74b4fc920916cc5710e142a3f885072d324da05244d398b16"

# Same bytes as the tested Galaxy client. Build inputs are staged outside Git.
GALAXY_MODS = {
    "chainengineering-0.1.0-alpha.7.jar": "891bb67c225f9546fe595e97c31d826138b1469aa84540bf8ed48b5b22db2c08",
    "127-qz_uilib-1.8.2.jar": "4d8e33ac9447bf2ed9f266822867082cbe386850504e9f44859a41f178e5268f",
}


def applies(library, platform):
    rules = library.get("rules")
    if rules is None:
        return True
    allowed = False
    for rule in rules:
        os = rule.get("os", {})
        if set(os) - {"name"} or set(rule) - {"os", "action"}:
            raise ValueError(f"Unsupported library rule: {rule}")
        if not os or os["name"] == platform:
            allowed = rule["action"] == "allow"
    return allowed


def artifact(library, platform):
    group, name, version, *classifier = library["name"].split(":")
    if library.get("MMC-hint") == "local":
        return {"path": f"pack/libraries/{name}-{version}-{classifier[0]}.jar"}
    downloads = library["downloads"]
    native = library.get("natives", {}).get(platform)
    if native:
        download = downloads["classifiers"][native.replace("${arch}", "64")]
    else:
        download = downloads["artifact"]
    url = download["url"]
    if url.startswith("https://repo1.maven.org/maven2/"):
        url = url.replace("repo1.maven.org", "repo.maven.apache.org", 1)
    filename = urlsplit(url).path.rsplit("/", 1)[1]
    path = f"libraries/{group.replace('.', '/')}/{name}/{version}/{filename}"
    entry = {"path": path, "url": url, "sha1": download["sha1"], "size": download["size"]}
    if native:
        entry["extract"] = library["extract"]["exclude"]
    return entry


def generate(path):
    with path.open("rb") as stream:
        if hashlib.file_digest(stream, "sha256").hexdigest() != PACK_HASH:
            raise ValueError("Input is not the pinned official GTNH ZIP")
    with ZipFile(path) as archive:
        instance = next(name.removesuffix("mmc-pack.json") for name in archive.namelist()
                        if name.endswith("/mmc-pack.json"))
        components = json.loads(archive.read(instance + "mmc-pack.json"))["components"]
        patches = [json.loads(archive.read(instance + "patches/" + item["uid"] + ".json"))
                   for item in components]
    minecraft = next(patch for patch in patches if patch["uid"] == "net.minecraft")
    # Component order takes precedence over standalone metadata's numeric order.
    libraries = {}
    for patch in patches:
        for library in patch.get("libraries", []):
            group, name, _, *classifier = library["name"].split(":")
            # A later component replaces an earlier version of the same library.
            # Forge 1.7.10 requires its Guava 17 in place of vanilla Guava 15.
            libraries[group, name, tuple(classifier)] = library
    libraries = [*libraries.values(), minecraft["mainJar"]]
    return {
        "archive": {"url": PACK_URL, "sha256": PACK_HASH, "size": path.stat().st_size},
        "instance": instance,
        "galaxyMods": GALAXY_MODS,
        "assetIndex": minecraft["assetIndex"],
        "jvm": [arg for patch in patches for arg in patch.get("+jvmArgs", [])],
        "main": next(patch["mainClass"] for patch in reversed(patches) if "mainClass" in patch),
        "arguments": minecraft["minecraftArguments"].split(),
        "tweakers": [name for patch in patches for name in patch.get("+tweakers", [])],
        "libraries": {os: [artifact(lib, os) for lib in libraries if applies(lib, os)]
                      for os in ("linux", "windows")},
    }


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("archive", type=Path)
args = parser.parse_args()
print(json.dumps(generate(args.archive), indent=2))
