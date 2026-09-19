#!/usr/bin/env python3
"""Cross-check a real capture and reject independently corrupted copies."""
import argparse
import csv
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from verify_run import inspect
parser=argparse.ArgumentParser();parser.add_argument('capture');args=parser.parse_args()
source=Path(args.capture).resolve()
analyzer=Path(__file__).resolve().parents[3]/'bench/analyze.py'
with tempfile.TemporaryDirectory() as directory:
    target=Path(directory)
    originals={name:(source/name).read_bytes() for name in ['summary.json','trace.csv','peer.csv']}
    def reset():
        for name,data in originals.items():(target/name).write_bytes(data)
    def check(invalid=False):
        completed=subprocess.run([sys.executable,str(analyzer),str(target),'--minimum-duration-seconds','0'],capture_output=True,text=True)
        assert completed.returncode==(2 if invalid else 1),completed.stdout+completed.stderr
        return json.loads((target/'analysis.json').read_text())
    reset();result=check();independent=inspect(target)
    assert not independent['integrity_errors']
    for ours,theirs in [('validation','validation'),('recording','receive_to_recorded'),('pre_dispatch','predispatch'),('aggregate_backend_span','backend_span'),('completion_consumption','completion_consumption')]:
        for statistic in ['p50_ns','p99_ns','max_ns']:
            assert independent[ours][statistic]==result['latency'][theirs][statistic]
    for name,value in [('capture_io_error',True),('trace_overflow',1),('trace_generated',999999)]:
        reset();summary=json.loads((target/'summary.json').read_text());summary[name]=value;(target/'summary.json').write_text(json.dumps(summary));check(True)
    for mutation in ['duplicate','missing','reversed','identity','simulated']:
        reset();print('checking',mutation)
        with (target/'trace.csv').open(newline='') as file:
            reader=csv.DictReader(file);fields=reader.fieldnames;rows=list(reader)
        if mutation=='duplicate':rows.append(rows[0].copy())
        elif mutation=='missing':rows.pop(3)
        elif mutation=='reversed':rows[3]['monotonic_ns']='1'
        elif mutation=='identity':rows[3]['operation']='99999999'
        else:rows[3]['simulated']='1'
        with (target/'trace.csv').open('w',newline='') as file:
            writer=csv.DictWriter(file,fieldnames=fields);writer.writeheader();writer.writerows(rows)
        # Keep counters consistent to exercise deeper checks, not row count alone.
        summary=json.loads((target/'summary.json').read_text());summary['trace_rows']=summary['trace_generated']=len(rows);(target/'summary.json').write_text(json.dumps(summary));check(True)
print('production analyzer matches independent paired oracle and rejects eight corruptions')
