#!/usr/bin/env python3
"""An unavailable required standard-library header must stop configuration clearly."""
import pathlib
import subprocess
import sys
import tempfile

source = pathlib.Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix="vita-p00-bad-library-") as root:
    root = pathlib.Path(root)
    headers = root / "headers"
    headers.mkdir()
    (headers / "expected").write_text('#error "independent P00 test: std::expected unavailable"\n')
    cmd = ["cmake", "-S", str(source), "-B", str(root / "build"),
           "-DBUILD_TESTING=OFF", f"-DCMAKE_CXX_FLAGS={sys.argv[2] if len(sys.argv) > 2 else chr(32)} -I{headers}"]
    if len(sys.argv) > 1:
        cmd.append(f"-DCMAKE_CXX_COMPILER={sys.argv[1]}")
    result = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(result.stdout)
    if result.returncode == 0:
        raise SystemExit("FAIL: configuration accepted missing std::expected")
    if "C++23" not in result.stdout or "expected" not in result.stdout:
        raise SystemExit("FAIL: configuration did not give a clear required-library diagnostic")
    print("PASS: missing required standard library fails with an actionable diagnostic")
