#!/usr/bin/env python3
"""Independent raw-artifact oracle; does not import the benchmark analyzer."""
import argparse
import csv
import json
import math
from collections import defaultdict
from pathlib import Path


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * fraction) - 1)] if ordered else None


def inspect(directory):
    directory = Path(directory)
    summary = json.loads((directory / 'summary.json').read_text())
    def read(name):
        with (directory / name).open(newline='') as file:
            return [{key: int(value) for key, value in row.items()} for row in csv.DictReader(file)]
    trace, peer = read('trace.csv'), read('peer.csv')
    errors = []
    for name, actual in [('trace_rows', len(trace)), ('peer_rows', len(peer))]:
        if summary[name] != actual:
            errors.append(f'{name}: summary {summary[name]} != raw {actual}')
    for name in ['trace_overflow', 'peer_trace_overflow', 'runtime_error', 'peer_error', 'peer_bad', 'pending_overwrite', 'capture_io_error']:
        if summary.get(name, 0): errors.append(f'{name}={summary[name]}')
    for name, actual in [('trace_generated', len(trace)), ('peer_generated', len(peer))]:
        if name in summary and summary[name] != actual: errors.append(f'{name} does not match captured rows')
    start, end = summary['measurement_start_ns'], summary['measurement_end_ns']
    if end <= start or abs((end-start)/1e9-summary['duration_seconds']) > 1e-8:
        errors.append('measurement interval does not match duration')
    sent = {}
    for row in peer:
        if row['kind'] in (0, 1):
            key = (row['sid'], row['mid'])
            if key in sent: errors.append(f'duplicate send key {key}')
            sent[key] = row
            if not row['success'] or not start <= row['monotonic_ns'] < end:
                errors.append(f'invalid offered send {key}')
    normal = sum(row['kind'] == 0 for row in sent.values())
    burst = sum(row['kind'] == 1 for row in sent.values())
    if (normal, burst) != (summary['normal_sent'], summary['burst_sent']): errors.append('send counters differ from raw rows')
    if burst != summary['burst_count'] * 64: errors.append('incomplete or misreported 64-command burst')
    groups = defaultdict(list)
    for row in trace:
        groups[tuple(row[k] for k in ('generation','operation','peer','sid','mid'))].append(row)
    seen_wire_keys = set()
    validated_ns, recorded_ns, pre_dispatch_ns, backend_ns, consumption_ns = [], [], [], [], []
    completed = successful = received = validated = 0
    for key, rows in groups.items():
        wire_key = key[-2:]
        if wire_key not in sent: errors.append(f'trace without offered command {wire_key}')
        if wire_key in seen_wire_keys: errors.append(f'multiple operation identities for {wire_key}')
        seen_wire_keys.add(wire_key)
        stages = defaultdict(list)
        for row in rows:
            stages[row['stage']].append(row)
            if row['stage'] not in range(5): errors.append(f'unknown stage {key}')
            if row['monotonic_ns'] <= 0 or row['simulated']: errors.append(f'invalid/simulated measurement {key}')
        if len(stages[0]) != 1 or any(len(stages[n]) > 1 for n in (1, 4)):
            errors.append(f'duplicate/missing command stage {key}'); continue
        received += 1
        rx = stages[0][0]['monotonic_ns']
        if wire_key in sent and rx < sent[wire_key]['monotonic_ns']: errors.append(f'receive precedes send {key}')
        if stages[1]:
            validated += 1
            val = stages[1][0]['monotonic_ns']
            if val < rx: errors.append(f'validation precedes receive {key}')
            validated_ns.append(val-rx)
        if not stages[4]: continue  # Report admission loss/incomplete work explicitly.
        completed += 1
        final = stages[4][0]
        if not stages[1]: errors.append(f'recorded without admission {key}'); continue
        rec = final['monotonic_ns']
        if rec < val: errors.append(f'recorded precedes validation {key}')
        recorded_ns.append(rec-rx)
        dispatch = {(r['cif'],r['bit']):r for r in stages[2]}
        done = {(r['cif'],r['bit']):r for r in stages[3]}
        if len(dispatch) != len(stages[2]) or len(done) != len(stages[3]): errors.append(f'duplicate field stages {key}')
        if final['status'] == 1:
            successful += 1
            if len(dispatch) != summary['writable_fields_per_command'] or set(dispatch) != set(done): errors.append(f'incomplete successful fields {key}')
        for field, event in done.items():
            if field not in dispatch: errors.append(f'completion without dispatch {key}'); continue
            if not val <= dispatch[field]['monotonic_ns'] <= event['monotonic_ns'] <= rec:
                errors.append(f'field stage ordering {key}')
        if dispatch and done:
            first = min(r['monotonic_ns'] for r in dispatch.values())
            last = max(r['monotonic_ns'] for r in done.values())
            pre_dispatch_ns.append(first-rx);backend_ns.append(last-first);consumption_ns.append(rec-last)
    def stats(values):
        return {'count':len(values),'p50_ns':percentile(values,.5),'p99_ns':percentile(values,.99),'max_ns':max(values) if values else None}
    expected_normal = math.ceil(summary['duration_seconds'] * summary['control_rate'] - 1e-9)
    load_exact = normal == expected_normal
    if summary['mode'] == 'normal':
        load_exact &= summary['sample_rate'] == 1_000_000 and summary['control_rate'] == 100
    elif summary['mode'] == 'overload':
        load_exact &= summary['sample_rate'] == 1_200_000 and summary['control_rate'] == 120
    zero_allocations = not any(value for key,value in summary.items() if key.startswith('critical_') and 'alloc' in key)
    data = summary['data']
    data_load = len(data)==4 and len({row['sid'] for row in data})==4 and all(abs(row['accepted_packets_measured']*summary['samples_per_packet']+row['skipped_samples_measured']-summary['duration_seconds']*summary['sample_rate']) <= 2*summary['samples_per_packet'] for row in data)
    data_clean = data_load and len(data)==4 and all(not row['skipped_samples_measured'] and not row['send_failures'] and not row['peer_invalid'] and not row['peer_overlap_samples'] and row['accepted_packets_measured']>0 for row in data)
    return {'integrity_errors':errors,'offered':len(sent),'normal_offered':normal,'expected_normal_offered':expected_normal,'burst_offered':burst,'received':received,'validated':validated,'recorded':completed,'successful':successful,'offered_not_received':len(sent)-received,'received_not_recorded':received-completed,'validation':stats(validated_ns),'recording':stats(recorded_ns),'pre_dispatch':stats(pre_dispatch_ns),'aggregate_backend_span':stats(backend_ns),'completion_consumption':stats(consumption_ns),'load_exact':bool(load_exact),'framework_within_cap':summary['framework_bytes']<=67_108_864,'zero_instrumented_allocations':zero_allocations,'data_load_reconciled':data_load,'data_no_framework_drop':data_clean,'local_control_targets_met':bool(recorded_ns) and percentile(validated_ns,.99)<=1_000_000 and percentile(recorded_ns,.99)<=2_000_000 and successful==len(sent),'duration_at_least_30min':summary['duration_seconds']>=1800,'deployment_qualified':False}

if __name__ == '__main__':
    parser=argparse.ArgumentParser();parser.add_argument('directory');parser.add_argument('--output');args=parser.parse_args()
    result=inspect(args.directory);text=json.dumps(result,indent=2)+'\n'
    if args.output: Path(args.output).write_text(text)
    print(text,end='')
    raise SystemExit(bool(result['integrity_errors']))
