#!/usr/bin/env python3
"""Build pinned Prism for Linux/Orca and stage its runtime beside bt3_64.

Requires CMake, a C++23 compiler, pkg-config and glibmm/giomm-2.68 development files.
Only source code is downloaded. No game assets are involved.
"""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
COMMIT = "a08ea340dc4063ce48ac4b6834571dcfb2d50d7c"
SHA256 = "b9898912904cab967fc7046988025d2271ee5da6e745d2d841497f2b1243669b"


def main():
    cache = Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache")) / "bt3-prism" / COMMIT
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / "source.tar.gz"
    if not archive.exists():
        with urllib.request.urlopen(f"https://codeload.github.com/ethindp/prism/tar.gz/{COMMIT}") as response:
            archive.write_bytes(response.read())
    if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        raise SystemExit(f"Prism source checksum mismatch: {archive}")
    source = cache / f"prism-{COMMIT}"
    if not source.exists():
        with tarfile.open(archive) as contents:
            contents.extractall(cache, filter="data")
    subprocess.run(["pkg-config", "--exists", "glibmm-2.68 >= 2.68", "giomm-2.68 >= 2.68"], check=True)
    build = cache / "build"
    subprocess.run([
        "cmake", "-S", str(source), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release",
        "-DBUILD_SHARED_LIBS=ON", "-DPRISM_ENABLE_GDEXTENSION=OFF",
        "-DPRISM_ENABLE_TESTS=OFF", "-DPRISM_ENABLE_DEMOS=OFF",
        "-DCMAKE_CXX_FLAGS=",
    ], check=True)
    subprocess.run(["cmake", "--build", str(build), "--parallel", "4"], check=True)
    out = ROOT / "port/build"
    (out / "lib").mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / "libprism.so", out / "lib/libprism.so")
    notices = out / "licenses/prism"
    notices.mkdir(parents=True, exist_ok=True)
    for name in ("LICENSE", "NOTICE"):
        shutil.copy2(source / name, notices / name)
    shutil.copy2(ROOT / "port/third_party/prism/SOURCE", notices / "SOURCE")
    shutil.copytree(source / "LICENSES", notices / "LICENSES", dirs_exist_ok=True)
    print("Prism staged:", out / "lib/libprism.so")


if __name__ == "__main__":
    main()
