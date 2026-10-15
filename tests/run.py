#!/usr/bin/env python3
"""Run shared production logic on the host after `pio run -e lamp`."""
from pathlib import Path
import subprocess
import os

root = Path(__file__).resolve().parents[1]
json_headers = root / ".pio/libdeps/lamp/ArduinoJson/src"
if not json_headers.exists():
    raise SystemExit("Bitte zuerst pio run -e lamp ausfuehren (ArduinoJson-Abhaengigkeit).")
out = root / ("build/core_test.exe" if os.name == "nt" else "build/core_test")
out.parent.mkdir(exist_ok=True)
sanitizers = [] if os.name == "nt" else ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
subprocess.run([
    "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
    *sanitizers, "-g",
    "-I", str(root / "include"), "-I", str(json_headers),
    str(root / "tests/core_test.cpp"), "-o", str(out),
], check=True)
subprocess.run([str(out)], check=True)
