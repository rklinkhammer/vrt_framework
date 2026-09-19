#!/usr/bin/env python3
"""Independent artifact mutations and paired-latency arithmetic."""
import copy
import csv
import json
import tempfile
from pathlib import Path
from verify_run import inspect

TRACE = ['generation','operation','peer','sid','mid','stage','monotonic_ns','cif','bit','status','simulated']
PEER = ['monotonic_ns','sid','mid','kind','success']
start = 1_000_000_000
summary = dict(mode='normal', duration_seconds=.02, measurement_start_ns=start, measurement_end_ns=start+20_000_000, control_rate=100, sample_rate=1_000_000, samples_per_packet=256, writable_fields_per_command=1, normal_sent=2, burst_sent=0, burst_count=0, trace_rows=10, peer_rows=2, trace_generated=10, peer_generated=2, framework_bytes=1000, critical_cpp_allocations=0, critical_c_allocations=0, data=[dict(sid=i,accepted_packets_measured=78, skipped_samples_measured=0,send_failures=0,peer_invalid=0,peer_overlap_samples=0) for i in range(1,5)])
trace=[]
peer=[]
# Widely separated absolute timestamps: percentile subtraction would give 10,
# but the actual per-command maximum/p99 is 1000 ns.
for mid,base,latency in [(1,start+1000,1000),(2,start+10_000_000,10)]:
    peer.append(dict(zip(PEER,[base-1,101,mid,0,1])))
    for stage,delta in [(0,0),(1,1),(2,2),(3,3),(4,latency)]:
        trace.append(dict(zip(TRACE,[1,mid,1,101,mid,stage,base+delta,0,21,1,0])))

def run(s=summary,t=trace,p=peer):
    with tempfile.TemporaryDirectory() as directory:
        path=Path(directory)
        (path/'summary.json').write_text(json.dumps(s))
        for name,fields,rows in [('trace.csv',TRACE,t),('peer.csv',PEER,p)]:
            with (path/name).open('w',newline='') as file:
                writer=csv.DictWriter(file,fieldnames=fields);writer.writeheader();writer.writerows(rows)
        return inspect(path)

result=run()
assert not result['integrity_errors'] and result['recording']['p99_ns']==1000
assert result['data_load_reconciled'] and result['local_control_targets_met']
for field in ['capture_io_error','trace_overflow','peer_trace_overflow']:
    altered=copy.deepcopy(summary);altered[field]=1
    assert run(altered)['integrity_errors']
altered=copy.deepcopy(summary);altered['trace_generated']=11
assert run(altered)['integrity_errors']
altered=copy.deepcopy(trace);altered[3]['monotonic_ns']=start
assert run(t=altered)['integrity_errors']
altered=copy.deepcopy(trace);altered[3]['stage']=2
assert run(t=altered)['integrity_errors']
altered=copy.deepcopy(trace);altered[3]['simulated']=1
assert run(t=altered)['integrity_errors']
altered=copy.deepcopy(trace);altered[3]['operation']=999
assert run(t=altered)['integrity_errors']
altered=copy.deepcopy(summary);altered['data'][0]['accepted_packets_measured']=1
assert not run(altered)['data_load_reconciled']
# Honest performance failures remain valid captures.
altered=copy.deepcopy(trace);altered[-1]['monotonic_ns']+=3_000_000
result=run(t=altered)
assert not result['integrity_errors'] and not result['local_control_targets_met']
print('independent raw-artifact oracle mutations passed')
