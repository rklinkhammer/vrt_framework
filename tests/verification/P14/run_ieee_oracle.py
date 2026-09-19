#!/usr/bin/env python3
"""Optional pinned external numerical oracle; never fetches or builds dependencies."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--reference', type=Path, default=Path('artifacts/P14-softfloat-reference.json'))
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
reference = json.loads(args.reference.read_text())
expected = 'a0c6494cdc11865811dec815d5c0049fba9d82a8'
if reference['revision'] != expected:
    raise SystemExit('Unexpected reference revision')
actual = subprocess.check_output(['git', '-C', reference['path'], 'rev-parse', 'HEAD'], text=True).strip()
if actual != expected:
    raise SystemExit('Reference checkout revision mismatch')
with tempfile.TemporaryDirectory(prefix='vita-independent-ieee-') as directory:
    binary = str(Path(directory) / 'oracle')
    command = ['clang++', '-std=c++23', '-O2', '-Wall', '-Wextra', '-Werror',
               '-fno-exceptions', '-fno-rtti', '-Iinclude',
               '-I' + str(Path(reference['path']) / 'source/include'),
               'tests/verification/P14/ieee_oracle.cpp', reference['library'], '-o', binary]
    if args.sanitize:
        command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run(command, check=True)
    subprocess.run([binary], check=True)
