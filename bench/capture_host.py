#!/usr/bin/env python3
"""Capture local benchmark provenance; never infer deployment qualification."""
import argparse
import datetime
import json
import pathlib
import platform
import subprocess


def command(args):
    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=10)
        return {"command": args, "exit_code": result.returncode,
                "stdout": result.stdout.strip(), "stderr": result.stderr.strip()}
    except (OSError, subprocess.TimeoutExpired) as error:
        return {"command": args, "unavailable": str(error)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    record = {
        "captured_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "scope": "local host provenance; not deployment qualification",
        "system": platform.system(), "release": platform.release(),
        "version": platform.version(), "machine": platform.machine(),
        "compiler": command([args.compiler, "--version"]),
        "source_revision": command(["git", "rev-parse", "HEAD"]),
        "required_run_metadata": [
            "source manifest", "build flags", "affinity and scheduling",
            "clock binding and actual measurement clock", "socket buffer sizes",
            "addresses and MTU", "copy path", "warmup and measured duration",
            "offered load and loss method", "latency observations",
            "native memory ledger", "allocation and profiling overhead"],
    }
    if platform.system() == "Darwin":
        record["os_build"] = command(["sw_vers"])
        record["cpu_and_memory"] = command(
            ["sysctl", "machdep.cpu.brand_string", "hw.ncpu", "hw.memsize"])
    elif platform.system() == "Linux":
        record["cpu"] = command(["lscpu"])
        record["memory"] = command(["getconf", "_PHYS_PAGES"])
        record["page_size"] = command(["getconf", "PAGESIZE"])
    text = json.dumps(record, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text)
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
