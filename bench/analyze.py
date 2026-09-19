#!/usr/bin/env python3
"""Bounded-memory P13 raw-evidence analyzer. Exit 0 pass, 1 measured fail, 2 invalid.

CSV input and joins stream through a temporary on-disk SQLite database with a
4 MiB page cache. Exact percentiles use disk-backed ordering of paired samples;
no approximate histogram or independently subtracted percentile is used.
"""
import argparse
import collections
import csv
from decimal import Decimal, InvalidOperation
import json
import math
from pathlib import Path
import sqlite3
import tempfile

TRACE = ['generation', 'operation', 'peer', 'sid', 'mid', 'stage',
         'monotonic_ns', 'cif', 'bit', 'status', 'simulated']
PEER = ['monotonic_ns', 'sid', 'mid', 'kind', 'success', 'ack_cam']
LIMIT = (1 << 63) - 1


class Evidence:
    def __init__(self):
        self.invalid = collections.Counter()
        self.failures = collections.Counter()
        self.examples = []

    def issue(self, code, detail='', invalid=True):
        (self.invalid if invalid else self.failures)[code] += 1
        if len(self.examples) < 32:
            self.examples.append({'code': code, 'detail': str(detail)[:256],
                                  'invalid': invalid})


def integer(value):
    if isinstance(value, bool):
        raise ValueError('boolean where integer required')
    if isinstance(value, int):
        result = value
    elif isinstance(value, str) and value.isascii() and value.isdecimal():
        result = int(value)
    else:
        raise ValueError('nonnegative decimal integer required')
    if not 0 <= result <= LIMIT:
        raise ValueError('integer outside supported host-clock/count range')
    return result


def ns(value):
    number = Decimal(str(value)) * 1_000_000_000
    if not number.is_finite() or number < 0 or number > LIMIT:
        raise ValueError('invalid duration')
    return int(number)


def rows(path, header, maximum, optional_tail=False):
    with path.open(newline='', encoding='utf-8') as source:
        reader = csv.reader(source)
        actual = next(reader, None)
        legacy = optional_tail and actual == header[:-1]
        if actual != header and not legacy:
            raise ValueError(f'{path.name}: incorrect CSV header')
        for line, row in enumerate(reader, 2):
            if line - 1 > maximum:
                raise ValueError(f'{path.name}: configured row bound exceeded')
            if len(row) != len(actual):
                raise ValueError(f'{path.name}:{line}: wrong column count')
            try:
                yield line, tuple(integer(value) for value in row) + ((0,) if legacy else ())
            except ValueError as error:
                raise ValueError(f'{path.name}:{line}: {error}') from error


def percentile(db, name):
    count = db.execute('SELECT count(*) FROM metric WHERE name=?', (name,)).fetchone()[0]
    if not count:
        return {'samples': 0, 'p50_ns': None, 'p90_ns': None,
                'p99_ns': None, 'max_ns': None}
    result = {'samples': count}
    for label, numerator in [('p50_ns', 50), ('p90_ns', 90), ('p99_ns', 99), ('max_ns', 100)]:
        rank = (count * numerator + 99) // 100 - 1
        result[label] = db.execute(
            'SELECT value FROM metric WHERE name=? ORDER BY value LIMIT 1 OFFSET ?',
            (name, rank)).fetchone()[0]
    return result


def analyze(directory, minimum, maximum, evidence):
    if (directory / 'summary.json').stat().st_size > 1_048_576:
        raise ValueError('summary exceeds bounded metadata size')
    csv.field_size_limit(128)
    with (directory / 'summary.json').open(encoding='utf-8') as source:
        summary = json.load(source)
    if summary.get('schema') != 1 or summary.get('mode') not in ('normal', 'overload'):
        raise ValueError('unsupported summary schema/mode (components has a separate report)')
    required = ['measurement_start_ns', 'measurement_end_ns', 'control_rate',
                'normal_sent', 'burst_sent', 'burst_count', 'burst_size', 'trace_rows',
                'peer_rows', 'ack_v', 'ack_x', 'ack_s', 'ack_failed', 'framework_bytes',
                'trace_overflow', 'peer_trace_overflow', 'runtime_error', 'peer_error',
                'pending_overwrite', 'peer_bad', 'critical_cpp_allocations']
    numbers = {key: integer(summary[key]) for key in required}
    start, end = numbers['measurement_start_ns'], numbers['measurement_end_ns']
    if end <= start or ns(summary['duration_seconds']) != end - start:
        raise ValueError('measurement window and duration disagree')
    duration = end - start
    period = ns(summary['burst_period_seconds'])
    expected_normal = (duration * numbers['control_rate'] + 999_999_999) // 1_000_000_000
    expected_bursts = (duration - 1) // period if period else 0
    expected_burst = expected_bursts * numbers['burst_size']
    if numbers['normal_sent'] + numbers['burst_sent'] > maximum:
        raise ValueError('configured command bound exceeded')
    for key in ('trace_overflow', 'peer_trace_overflow'):
        if numbers[key]:
            evidence.issue(key, numbers[key])
    for key in ('capture_io_error', 'trace_generated', 'peer_generated', 'critical_c_allocations'):
        if key not in summary:
            evidence.issue('missing_capture_integrity_evidence', key)
    if summary.get('capture_io_error') not in (None, False, 0):
        evidence.issue('capture_io_error')
    for key in ('runtime_error', 'peer_error', 'peer_bad', 'critical_cpp_allocations'):
        if numbers[key]:
            evidence.issue(key, numbers[key], invalid=False)
    for key in ('critical_c_allocations', 'critical_aligned_allocations'):
        if key in summary and integer(summary[key]):
            evidence.issue(key, summary[key], invalid=False)
    if summary.get('c_allocation_coverage_available') is not True:
        evidence.issue('c_allocation_coverage_unavailable', invalid=False)
    ledger = summary.get('budget_rows')
    if not isinstance(ledger, list) or not ledger:
        evidence.issue('missing_actual_budget_ledger')
    else:
        categories = set()
        charged_total = reserved_total = 0
        for row in ledger:
            category, reserved, charged = (integer(row[key]) for key in ('category', 'reserved', 'charged'))
            if category in categories or charged > reserved:
                evidence.issue('invalid_budget_row', category)
            categories.add(category)
            charged_total += charged
            reserved_total += reserved
        if charged_total != numbers['framework_bytes'] or reserved_total > 64 * 1024 * 1024:
            evidence.issue('budget_ledger_mismatch')
    if numbers['framework_bytes'] > 64 * 1024 * 1024:
        evidence.issue('framework_budget_exceeded', numbers['framework_bytes'], invalid=False)
    if duration < ns(minimum):
        evidence.issue('required_duration_not_met', f'{duration / 1e9} < {minimum}', invalid=False)
    expected_rate = 100 if summary['mode'] == 'normal' else 120
    if numbers['control_rate'] != expected_rate or numbers['burst_size'] != 64:
        evidence.issue('incorrect_control_workload', invalid=False)
    for key, expected in [('normal_sent', expected_normal), ('burst_sent', expected_burst),
                          ('burst_count', expected_bursts)]:
        if numbers[key] != expected:
            evidence.issue('offered_workload_mismatch', f'{key}: {numbers[key]} != {expected}', invalid=False)
    if (summary.get('iq_streams'), summary.get('samples_per_packet'), summary.get('command_bytes'),
            summary.get('cam')) != (4, 256, 44, 0xa91f0000):
        evidence.issue('incorrect_benchmark_shape', invalid=False)
    expected_sample_rate = 1_000_000 if summary['mode'] == 'normal' else 1_200_000
    if integer(summary['sample_rate']) != expected_sample_rate:
        evidence.issue('incorrect_iq_offered_rate', invalid=False)
    writable = integer(summary.get('writable_fields_per_command', 1))
    if not 1 <= writable <= 4:
        raise ValueError('unsupported bounded writable field count')
    legacy_pending = summary.get('peer_capture_policy') != 'stateless_checked_raw_v1'
    legacy_slots = [None] * 512
    eviction_counts = collections.Counter()
    counts = collections.Counter()
    phase = collections.Counter()
    metrics = ['validation', 'receive_to_recorded', 'predispatch', 'backend_span',
               'completion_consumption', 'send_to_receive', 'send_to_ack_x']
    with tempfile.TemporaryDirectory(prefix='vita-p13-analysis-') as temporary:
        db = sqlite3.connect(str(Path(temporary) / 'evidence.sqlite'))
        db.executescript('''PRAGMA cache_size=-4096; PRAGMA temp_store=FILE;
          PRAGMA journal_mode=OFF; PRAGMA synchronous=OFF;
          CREATE TABLE command(sid INTEGER,mid INTEGER,sent INTEGER,kind INTEGER,PRIMARY KEY(sid,mid));
          CREATE TABLE ack(sid INTEGER,mid INTEGER,kind INTEGER,time INTEGER,success INTEGER,cam INTEGER,PRIMARY KEY(sid,mid,kind));
          CREATE TABLE trace(sid INTEGER,mid INTEGER,stage INTEGER,cif INTEGER,bit INTEGER,
             generation INTEGER,operation INTEGER,peer INTEGER,time INTEGER,status INTEGER,simulated INTEGER,
             seq INTEGER,UNIQUE(sid,mid,stage,cif,bit));
          CREATE INDEX trace_command ON trace(sid,mid,seq);
          CREATE TABLE eviction(sid INTEGER,mid INTEGER,time INTEGER,phases INTEGER);
          CREATE TABLE metric(name TEXT,value INTEGER);''')
        for line, row in rows(directory / 'peer.csv', PEER, maximum * 8, optional_tail=True):
            time, sid, mid, kind, success, ack_cam = row
            counts['peer_rows'] += 1
            if not sid or not mid or kind > 4 or success > 1 or ack_cam > 0xffffffff:
                evidence.issue('invalid_peer_value', line)
                continue
            try:
                if kind <= 1:
                    if ack_cam:
                        evidence.issue('send_has_ack_cam', line)
                    if not success:
                        evidence.issue('unsuccessful_send_record', line)
                    if legacy_pending:
                        previous = legacy_slots[mid % 512]
                        if previous and previous[2] != 7:
                            db.execute('INSERT INTO eviction VALUES(?,?,?,?)', (previous[0], previous[1], time, previous[2]))
                            eviction_counts['reconstructed'] += 1
                        legacy_slots[mid % 512] = [sid, mid, 0]
                    db.execute('INSERT INTO command VALUES(?,?,?,?)', (sid, mid, time, kind))
                    counts['normal_sent' if kind == 0 else 'burst_sent'] += 1
                    if counts['normal_sent'] + counts['burst_sent'] > maximum:
                        raise ValueError('actual command bound exceeded')
                    phase['warmup_sends' if time < start else 'measurement_sends' if time < end else 'drain_sends'] += 1
                    if not start <= time < end:
                        evidence.issue('offered_command_outside_window', f'{sid}/{mid}', invalid=False)
                else:
                    counts[{2: 'ack_v', 3: 'ack_x', 4: 'ack_s'}[kind]] += 1
                    if kind == 3 and not success:
                        counts['ack_failed'] += 1
                    if legacy_pending:
                        previous = legacy_slots[mid % 512]
                        if previous and previous[:2] == [sid, mid]:
                            previous[2] |= {2: 1, 3: 2, 4: 4}[kind]
                    duplicate = db.execute('SELECT success,cam FROM ack WHERE sid=? AND mid=? AND kind=?', (sid, mid, kind)).fetchone()
                    if duplicate:
                        counts['duplicate_ack_rows'] += 1
                        if duplicate != (success, ack_cam):
                            evidence.issue('conflicting_ack_phase', f'{sid}/{mid}/{kind}')
                    else:
                        db.execute('INSERT INTO ack VALUES(?,?,?,?,?,?)', (sid, mid, kind, time, success, ack_cam))
                    if ack_cam:
                        counts['acks_with_raw_cam'] += 1
                        expected_phase = {2: 1 << 20, 3: 1 << 19, 4: 1 << 18}[kind]
                        if ack_cam & (7 << 18) != expected_phase or (ack_cam >> 23) & 3 != 2:
                            evidence.issue('wrong_ack_cam_phase_or_action', f'{sid}/{mid}/{kind}')
            except sqlite3.IntegrityError:
                evidence.issue('duplicate_peer_phase', f'{sid}/{mid}/{kind}')
        for line, row in rows(directory / 'trace.csv', TRACE, maximum * 12):
            generation, operation, peer, sid, mid, stage, time, cif, bit, status, simulated = row
            counts['trace_rows'] += 1
            if (not generation or not operation or not peer or not sid or not mid or stage > 4
                    or cif > 7 or bit > 31 or status > 5 or simulated > 1):
                evidence.issue('invalid_trace_value', line)
                continue
            if stage in (0, 1, 4) and (cif or bit):
                evidence.issue('whole_command_field_tag', line)
            if stage <= 2 and status != 0 or stage >= 3 and status == 0:
                evidence.issue('inconsistent_stage_status', line)
            if simulated:
                evidence.issue('simulated_benchmark_command', f'{sid}/{mid}', invalid=False)
            try:
                db.execute('INSERT INTO trace VALUES(?,?,?,?,?,?,?,?,?,?,?,?)',
                           (sid, mid, stage, cif, bit, generation, operation, peer, time, status, simulated, line))
            except sqlite3.IntegrityError:
                evidence.issue('duplicate_trace_stage', f'{sid}/{mid}/{stage}/{cif}:{bit}')
        for key in ('trace_rows', 'peer_rows', 'normal_sent', 'burst_sent', 'ack_v', 'ack_x', 'ack_s', 'ack_failed'):
            if counts[key] != numbers[key]:
                evidence.issue('summary_raw_count_mismatch', f'{key}: {counts[key]} != {numbers[key]}')
        for generated, rows_key in [('trace_generated', 'trace_rows'), ('peer_generated', 'peer_rows')]:
            if generated in summary and integer(summary[generated]) != counts[rows_key]:
                evidence.issue('generated_raw_count_mismatch', generated)
        for table in ('trace', 'ack'):
            orphan = db.execute(f'SELECT count(*) FROM {table} t WHERE NOT EXISTS (SELECT 1 FROM command c WHERE c.sid=t.sid AND c.mid=t.mid)').fetchone()[0]
            if orphan:
                evidence.issue('unmatched_' + table + '_key', orphan)
        if legacy_pending:
            if eviction_counts['reconstructed'] != numbers['pending_overwrite']:
                evidence.issue('legacy_pending_counter_mismatch')
            for sid, mid, overwritten_at, old_phases in db.execute('SELECT sid,mid,time,phases FROM eviction'):
                stage_count, accepted_count = db.execute('SELECT count(*),sum(stage>=1) FROM trace WHERE sid=? AND mid=?', (sid, mid)).fetchone()
                if accepted_count:
                    eviction_counts['accepted_correlation_evicted'] += 1
                    evidence.issue('legacy_live_correlation_evicted', f'{sid}/{mid}')
                elif stage_count == 1:
                    eviction_counts['pre_admission_evictions'] += 1
                else:
                    eviction_counts['unresolved_evictions'] += 1
                    evidence.issue('legacy_unresolved_correlation_eviction', f'{sid}/{mid}')
        elif numbers['pending_overwrite']:
            evidence.issue('stateless_capture_reports_pending_overwrite')
        for sid, mid, sent, kind in db.execute('SELECT sid,mid,sent,kind FROM command ORDER BY sent'):
            key = f'{sid}/{mid}'
            events = db.execute('SELECT stage,cif,bit,generation,operation,peer,time,status,simulated FROM trace WHERE sid=? AND mid=? ORDER BY seq', (sid, mid)).fetchall()
            acks = {row[0]: row[1:] for row in db.execute('SELECT kind,time,success,cam FROM ack WHERE sid=? AND mid=?', (sid, mid))}
            counts['commands'] += 1
            identities = {event[3:6] for event in events}
            if len(identities) > 1:
                evidence.issue('inconsistent_command_identity', key)
            if len({event[8] for event in events if event[0] >= 3}) > 1:
                evidence.issue('inconsistent_simulation_outcome', key)
            stages = collections.defaultdict(list)
            for event in events:
                stages[event[0]].append(event)
            for previous, current in zip(events, events[1:]):
                if current[6] < previous[6]:
                    evidence.issue('misordered_trace_timestamp', key)
            for stage in (0, 1, 4):
                if len(stages[stage]) > 1:
                    evidence.issue('duplicate_command_stage', f'{key}/{stage}')
            if events and len(stages[0]) != 1:
                evidence.issue('missing_received_stage', key)
            if (stages[2] or stages[3] or stages[4]) and len(stages[1]) != 1:
                evidence.issue('missing_validated_stage', key)
            if stages[3] and not stages[2]:
                evidence.issue('completion_without_dispatch', key)
            if events:
                sequence = [event[0] for event in events]
                if sequence[0] != 0 or (1 in sequence and sequence.index(1) != 1) or (4 in sequence and sequence[-1] != 4):
                    evidence.issue('misordered_stage_sequence', key)
                for position, event in enumerate(events):
                    if event[0] == 3 and not any(prior[0] == 2 and prior[1:3] == event[1:3] for prior in events[:position]):
                        evidence.issue('completion_row_before_dispatch', key)
            dispatch = {(e[1], e[2]): e for e in stages[2]}
            done = {(e[1], e[2]): e for e in stages[3]}
            for field, event in done.items():
                if field not in dispatch or event[6] < dispatch[field][6]:
                    evidence.issue('unpaired_device_completion', f'{key}/{field}')
            if writable == 1 and (set(dispatch) | set(done)) - {(0, 21)}:
                evidence.issue('wrong_benchmark_field', key)
            if len(dispatch) > writable or len(done) > writable:
                evidence.issue('excess_field_events', key)
            for stage in range(5):
                if not stages[stage]:
                    counts['commands_missing_stage_' + str(stage)] += 1
            for ack_kind in (2, 3, 4):
                if ack_kind not in acks:
                    counts['commands_missing_ack_' + str(ack_kind)] += 1
            if not events:
                counts['unobserved_commands'] += 1
            elif not stages[1]:
                counts['pre_admission_rejections_or_incomplete'] += 1
            terminal = stages[4][0] if stages[4] else None
            successful = bool(terminal and terminal[7] == 1 and not terminal[8])
            if successful and (len(dispatch) != writable or len(done) != writable):
                evidence.issue('successful_terminal_missing_backend_events', key)
            if 3 in acks and acks[3][1] and not successful:
                evidence.issue('successful_ack_without_successful_terminal', key)
            if terminal and not successful:
                counts['terminal_failed_or_simulated'] += 1
            rejected = bool(not stages[1] and 2 in acks and acks[2][2] and (acks[2][2] & (1 << 16) or not acks[2][2] & (1 << 10)))
            if rejected:
                counts['terminal_admission_rejections'] += 1
            elif not terminal:
                counts['incomplete_commands'] += 1
            if not (3 in acks and acks[3][1]):
                counts['execution_unconfirmed_commands'] += 1
            # peer.success is ControllerObserver execution evidence; V/S false
            # is not a failed validation/state observation.
            if 3 in acks and not acks[3][1]:
                counts['peer_rejected_or_failed_commands'] += 1
            for ack_kind, (time, success, ack_cam) in acks.items():
                if ack_kind == 2 and ack_cam and (ack_cam & (1 << 16) or not ack_cam & (1 << 10)):
                    counts['validation_rejected_or_partial_commands'] += 1
                if ack_kind == 4 and ack_cam and not ack_cam & (1 << 10):
                    counts['state_not_known_commands'] += 1
                if ack_kind == 3 and ack_cam and success and (not ack_cam & (1 << 10) or ack_cam & (1 << 11)):
                    evidence.issue('ack_success_flag_contradiction', key)
                if time < sent:
                    evidence.issue('ack_before_send', f'{key}/{ack_kind}')
                if time >= end:
                    phase['drain_ack_rows'] += 1
            if not start <= sent < end:
                continue
            if stages[0]:
                rx = stages[0][0][6]
                if rx < sent:
                    evidence.issue('receive_before_send', key)
                else:
                    db.execute('INSERT INTO metric VALUES(?,?)', ('send_to_receive', rx - sent))
                if stages[1]:
                    db.execute('INSERT INTO metric VALUES(?,?)', ('validation', stages[1][0][6] - rx))
                    counts['validation_over_1ms'] += stages[1][0][6] - rx > 1_000_000
                if successful and dispatch and done:
                    first = min(e[6] for e in dispatch.values())
                    last = max(e[6] for e in done.values())
                    recorded = terminal[6]
                    if not rx <= stages[1][0][6] <= first <= last <= recorded:
                        evidence.issue('invalid_pipeline_order', key)
                    else:
                        counts['successful_paired_commands'] += 1
                        counts['recorded_over_2ms'] += recorded - rx > 2_000_000
                        for name, value in [('receive_to_recorded', recorded-rx), ('predispatch', first-rx),
                                            ('backend_span', last-first), ('completion_consumption', recorded-last)]:
                            db.execute('INSERT INTO metric VALUES(?,?)', (name, value))
                        if recorded >= end:
                            phase['drain_recorded_commands'] += 1
                if 3 in acks and acks[3][1] and acks[3][0] >= sent:
                    db.execute('INSERT INTO metric VALUES(?,?)', ('send_to_ack_x', acks[3][0]-sent))
        for key in ('terminal_admission_rejections', 'incomplete_commands', 'execution_unconfirmed_commands', 'terminal_failed_or_simulated',
                    'peer_rejected_or_failed_commands', 'validation_rejected_or_partial_commands', 'state_not_known_commands', 'commands_missing_ack_2', 'commands_missing_ack_4'):
            if counts[key]:
                evidence.issue(key, counts[key], invalid=False)
        db.execute('CREATE INDEX metric_order ON metric(name,value)')
        percentiles = {name: percentile(db, name) for name in metrics}
        for name, limit in [('validation', 1_000_000), ('receive_to_recorded', 2_000_000)]:
            value = percentiles[name]['p99_ns']
            if value is None or value > limit:
                evidence.issue('latency_target_failed', f'{name}: {value} > {limit}', invalid=False)
        data = summary.get('data')
        if not isinstance(data, list) or len(data) != 4:
            evidence.issue('missing_stream_metrics')
        else:
            if {integer(stream['sid']) for stream in data} != {1, 2, 3, 4}:
                evidence.issue('invalid_data_stream_identity')
            for stream in data:
                for required in ('peer_measured_samples', 'peer_expected_measured_samples',
                                 'peer_measured_missing_samples', 'peer_measured_overlap_samples'):
                    if required not in stream:
                        evidence.issue('missing_measured_data_evidence', f'SID {stream.get("sid")} {required}')
                if all(key in stream for key in ('peer_measured_samples', 'peer_expected_measured_samples', 'peer_measured_missing_samples')):
                    covered, expected, missing = (integer(stream[key]) for key in ('peer_measured_samples', 'peer_expected_measured_samples', 'peer_measured_missing_samples'))
                    if covered + missing != expected or expected != duration * integer(summary['sample_rate']) // 1_000_000_000:
                        evidence.issue('inconsistent_measured_sample_coverage', stream.get('sid'))
                for key in ('skipped_samples_measured', 'skipped_packets_measured',
                            'peer_invalid', 'peer_measured_missing_samples', 'peer_measured_overlap_samples'):
                    if key in stream and integer(stream[key]):
                        # Overload is permitted to drop explicitly; corrupted data is never permitted.
                        if summary['mode'] == 'normal' or key in ('peer_measured_overlap_samples', 'peer_invalid'):
                            evidence.issue('data_target_failed', f'SID {stream["sid"]} {key}={stream[key]}', invalid=False)
        db.close()
    return {'schema': 1, 'mode': summary['mode'], 'measurement_start_ns': start,
            'measurement_end_ns': end, 'duration_seconds': duration / 1e9,
            'minimum_duration_seconds': minimum, 'qualification_duration_met': duration >= 1_800_000_000_000,
            'late_evidence_definition': 'rows after measurement_end are drain evidence; schema supplies no per-command deadline, so they are not assumed transaction timeouts', 'expected_normal_commands': expected_normal,
            'expected_burst_commands': expected_burst, 'expected_burst_count': expected_bursts,
            'counts': dict(counts), 'phases': dict(phase), 'latency': percentiles,
            'peer_capture_policy': summary.get('peer_capture_policy', 'legacy_pending_512'),
            'legacy_pending_evictions': dict(eviction_counts),
            'source_diagnostics': summary.get('source_diagnostics'),
            'percentile_method': 'exact nearest rank from paired raw timestamps',
            'latency_population': 'commands offered inside measurement window; terminal/drain evidence retained; incomplete and unsuccessful commands counted separately',
            'framework_bytes': numbers['framework_bytes'], 'budget_rows': summary.get('budget_rows'),
            'allocations': {key: summary.get(key) for key in ('critical_cpp_allocations', 'critical_c_allocations', 'c_allocation_coverage_available')},
            'data': summary.get('data'),
            'total_phase_diagnostics': 'total send failures/gaps/overlap include warmup and drain; measured coverage and measured skip fields determine loss acceptance',
            'deployment_qualified': False,
            'scope': 'local software evidence only; no independent-peer or GPS qualification'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_dir', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--minimum-duration-seconds', type=float, default=1800)
    parser.add_argument('--max-commands', type=int, default=10_000_000)
    args = parser.parse_args()
    evidence = Evidence()
    result = {'schema': 1}
    try:
        if not math.isfinite(args.minimum_duration_seconds) or args.minimum_duration_seconds < 0 or not 1 <= args.max_commands <= 10_000_000:
            raise ValueError('invalid analyzer bounds')
        result.update(analyze(args.run_dir, args.minimum_duration_seconds, args.max_commands, evidence))
    except (OSError, ValueError, KeyError, TypeError, InvalidOperation, sqlite3.Error, csv.Error) as error:
        evidence.issue('analysis_input_error', error)
    code = 2 if evidence.invalid else 1 if evidence.failures else 0
    result.update({'measurement_valid': not bool(evidence.invalid),
                   'acceptance_passed': code == 0, 'exit_code': code,
                   'invalid_reasons': dict(evidence.invalid),
                   'failed_requirements': dict(evidence.failures),
                   'examples': evidence.examples})
    output = args.output or args.run_dir / 'analysis.json'
    try:
        # An incomplete JSON write must never replace an existing good report.
        with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', dir=output.parent,
                                         prefix=output.name+'.', delete=False) as target:
            temporary = Path(target.name)
            json.dump(result, target, indent=2, allow_nan=False)
            target.write('\n')
        temporary.replace(output)
    except OSError as error:
        print(f'INVALID: cannot write analysis report: {error}')
        return 2
    label = 'INVALID' if code == 2 else 'MEASURED FAIL' if code == 1 else 'PASS'
    print(f'{label}: {result.get("counts", {}).get("commands", 0)} offered commands; '
          f'{sum(evidence.invalid.values())} integrity issues; '
          f'{sum(evidence.failures.values())} failed requirements; {output}')
    return code


if __name__ == '__main__':
    raise SystemExit(main())
