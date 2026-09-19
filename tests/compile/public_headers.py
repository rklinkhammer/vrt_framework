"""Compile every public header independently; no link or runtime dependencies."""
import pathlib
import shlex
import subprocess
import sys
import tempfile


compiler, include_root, compiler_flags = sys.argv[1:]
root = pathlib.Path(include_root).resolve()
headers = sorted(root.rglob("*.hpp"))
with tempfile.TemporaryDirectory(prefix="vita-public-headers-") as directory:
    source = pathlib.Path(directory) / "header.cpp"
    for header in headers:
        name = header.relative_to(root).as_posix()
        source.write_text(f"#include <{name}>\n", encoding="utf-8")
        result = subprocess.run(
            [compiler, *shlex.split(compiler_flags), "-std=c++23", "-fno-exceptions", "-fno-rtti",
             "-Wall", "-Wextra", "-Wpedantic", "-fsyntax-only",
             "-I", str(root), str(source)],
            capture_output=True, text=True,
        )
        if result.returncode:
            print(f"FAIL: {name}\n{result.stdout}{result.stderr}")
            sys.exit(result.returncode)
print(f"PASS: {len(headers)} standalone public headers")
