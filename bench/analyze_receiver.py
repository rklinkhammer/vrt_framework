#!/usr/bin/env python3
"""Analyze receiver-local characterization, not historical Control acceptance gates.

The raw CSV streams into a temporary disk-backed SQLite database (4 MiB cache).
Command/row bounds and duplicate packet keys are explicit; no truncation is
allowed. Exit 0 means valid characterization, including measured nondelivery;
exit 2 means unusable receiver evidence. An unavailable sender is not a local
receiver evidence error. No cross-host timestamp subtraction is performed.
"""
import argparse
import collections
import csv
from decimal import Decimal, InvalidOperation
import json
from pathlib import Path
import sqlite3
import struct
import tempfile

HEADER = ['sid', 'ordinal', 'rx_ns', 'validated_ns', 'app_ns', 'consumed_ns',
          'phase', 'status', 'known', 'bytes', 'checksum']
PHASES = ('warmup', 'measurement', 'drain')
STATUSES = ('delivered', 'metadata_expired', 'checked_reject', 'pending_overflow')
# Independent literal IQ16 oracle, nearest/ties-even at amplitude 0.5.
IQ16 = ((16384, 0), (15137, 6270), (11585, 11585), (6270, 15137),
        (0, 16384), (-6270, 15137), (-11585, 11585), (-15137, 6270),
        (-16384, 0), (-15137, -6270), (-11585, -11585), (-6270, -15137),
        (0, -16384), (6270, -15137), (11585, -11585), (15137, -6270))
U64 = (1 << 64) - 1
I64 = (1 << 63) - 1


def integer(value, maximum=U64):
    if isinstance(value, bool):
        raise ValueError('boolean is not a counter')
    if isinstance(value, str):
        if not value.isascii() or not value.isdecimal():
            raise ValueError('unsigned decimal integer required')
        value = int(value)
    if not isinstance(value, int) or not 0 <= value <= maximum:
        raise ValueError('counter outside supported bounds')
    return value


def load_summary(path):
    if path.stat().st_size > 1_048_576:
        raise ValueError('summary exceeds 1 MiB metadata bound')
    with path.open(encoding='utf-8') as source:
        value = json.load(source)
    if not isinstance(value, dict):
        raise ValueError('summary must be an object')
    return value


def canonical_checksum(ordinal, samples):
    value = 14695981039346656037
    for index in range(samples):
        for byte in struct.pack('>hh', *IQ16[(ordinal + index) % 16]):
            value = ((value ^ byte) * 1099511628211) & U64
    return value


class Issues:
    def __init__(self):
        self.counts = collections.Counter()
        self.examples = []

    def add(self, code, detail=''):
        self.counts[code] += 1
        if len(self.examples) < 32:
            self.examples.append({'code': code, 'detail': str(detail)[:256]})


def quantiles(db, name):
    count = db.execute('SELECT count(*) FROM metric WHERE name=?', (name,)).fetchone()[0]
    result = {'samples': count}
    for label, numerator in [('p50_ns', 50), ('p90_ns', 90), ('p99_ns', 99), ('max_ns', 100)]:
        if not count:
            result[label] = None
        else:
            rank = (count * numerator + 99) // 100 - 1
            result[label] = db.execute(
                'SELECT value FROM metric WHERE name=? ORDER BY value LIMIT 1 OFFSET ?',
                (name, rank)).fetchone()[0]
    return result


def sender_evidence(directory):
    if directory is None:
        return {'available': False, 'reason': 'sender evidence not supplied',
                'attribution': 'no sender shortfall or unexplained loss assigned to receiver'}
    try:
        summary = load_summary(directory / 'summary.json')
        # Sender times only define the sender's own duration. Never compare them
        # with receiver timestamps, even when both files came from localhost.
        start = summary.get('measurement_start_ns')
        end = summary.get('measurement_end_ns')
        duration = None
        if start is not None and end is not None:
            start, end = integer(start, I64), integer(end, I64)
            if end <= start:
                raise ValueError('sender measurement window is invalid')
            duration = (end - start) / 1e9
        return {'available': True, 'sender_window_seconds': duration,
                'reported_summary': summary,
                'clock_comparison': 'none; no cross-host clock subtraction',
                'attribution': 'offered/accepted/skipped are sender-scoped; residual loss location remains unknown'}
    except (OSError, ValueError, TypeError) as error:
        return {'available': False, 'reason': str(error),
                'attribution': 'sender unavailable; local receiver service metrics remain independently usable'}


def analyze(directory, sender_dir, maximum, issues, disk_mib=4096):
    summary = load_summary(directory / 'summary.json')
    if summary.get('schema') != 1 or summary.get('kind') != 'receiver':
        raise ValueError('expected receiver summary schema 1')
    start = integer(summary['measurement_start_ns'], I64)
    end = integer(summary['measurement_end_ns'], I64)
    if not start or end <= start or summary.get('startup_timeout'):
        raise ValueError('receiver has no qualified measurement window')
    duration_ns = end - start
    if 'duration_seconds' in summary:
        declared = Decimal(str(summary['duration_seconds'])) * 1_000_000_000
        if not declared.is_finite() or abs(declared - duration_ns) > 1:
            raise ValueError('duration disagrees with receiver window')
    sids = summary['sids']
    if not isinstance(sids, list) or not 1 <= len(sids) <= 16:
        raise ValueError('expected 1..16 configured SIDs')
    sids = tuple(integer(sid, (1 << 32)-1) for sid in sids)
    if 0 in sids or len(set(sids)) != len(sids):
        raise ValueError('duplicate/zero configured SID')
    budget_keys = ('pool_raw_bytes', 'pool_provider_bytes', 'receiver_object_bytes',
                   'receiver_auxiliary_bytes', 'capture_bytes', 'writer_stack_bytes',
                   'receiver_stack_bytes', 'receiver_loop_state_bytes')
    budget_total = sum(integer(summary[key]) for key in budget_keys)
    if budget_total != integer(summary['accounted_bytes']):
        issues.add('receiver_budget_sum_mismatch')
    if not summary['receiver_stack_bytes'] or not summary['writer_stack_bytes']:
        issues.add('missing_explicit_thread_stack_budget')
    if not isinstance(summary.get('c_allocation_coverage_available'), bool):
        issues.add('missing_c_allocation_coverage_declaration')
    generation = integer(summary['association_generation'])
    epoch = integer(summary['epoch_seconds'])
    if not generation:
        raise ValueError('association generation must be nonzero')
    samples = integer(summary['samples_per_packet'], 4096)
    payload_bytes = integer(summary['payload_bytes'], 16384)
    if not samples or payload_bytes != samples * 4 or summary.get('checksum_algorithm') != 'fnv1a64':
        raise ValueError('unsupported IQ16 packet/checksum shape')
    declared_rows = integer(summary['rows'], maximum)
    generated = integer(summary['generated'], maximum)
    if integer(summary['capture_overflow']):
        issues.add('capture_overflow', summary['capture_overflow'])
    if summary.get('capture_io_error') is not False:
        issues.add('capture_io_error_or_missing_false')
    if generated != declared_rows:
        issues.add('generated_written_count_mismatch')
    expected_matrix = summary['phase_status_counts']
    if not isinstance(expected_matrix, list) or len(expected_matrix) != 3 or any(not isinstance(row, list) or len(row) != 4 for row in expected_matrix):
        raise ValueError('phase_status_counts must be a 3x4 matrix')
    expected_matrix = [[integer(value, maximum) for value in row] for row in expected_matrix]
    matrix = [[0] * 4 for _ in range(3)]
    stream_counts = {sid: collections.Counter() for sid in sids}
    counts = collections.Counter()
    checksum = [canonical_checksum(phase, samples) for phase in range(16)]
    metric_names = ('rx_to_validated', 'validated_to_app', 'app_to_consumed', 'rx_to_consumed')
    duration_seconds = duration_ns / 1e9
    with tempfile.TemporaryDirectory(prefix='vita-receiver-analysis-') as temporary:
        db = sqlite3.connect(str(Path(temporary) / 'evidence.sqlite'))
        db.execute('PRAGMA max_page_count=' + str(disk_mib * 256))
        db.executescript('''PRAGMA cache_size=-4096; PRAGMA temp_store=FILE;
          PRAGMA journal_mode=OFF; PRAGMA synchronous=OFF;
          CREATE TABLE packet(sid INTEGER,ordinal TEXT,rx INTEGER,phase INTEGER,status INTEGER,
              PRIMARY KEY(sid,ordinal,rx));
          CREATE TABLE metric(name TEXT,value INTEGER);
          CREATE INDEX metric_order ON metric(name,value);
          CREATE INDEX packet_rx ON packet(sid,rx);
          CREATE INDEX packet_ordinal ON packet(sid,phase,status,length(ordinal),ordinal);''')
        csv.field_size_limit(128)
        with (directory / 'receiver.csv').open(newline='', encoding='utf-8') as source:
            reader = csv.reader(source)
            if next(reader, None) != HEADER:
                raise ValueError('incorrect receiver CSV header')
            for line, raw in enumerate(reader, 2):
                counts['rows'] += 1
                if counts['rows'] > maximum:
                    raise ValueError('configured raw row bound exceeded')
                if len(raw) != len(HEADER):
                    raise ValueError(f'line {line}: wrong column count')
                sid, ordinal, rx, validated, app, consumed, phase, status, known, length, digest = map(integer, raw)
                if (sid not in stream_counts or phase > 2 or status > 3 or known > 1 or
                        any(time > I64 for time in (rx, validated, app, consumed)) or not rx):
                    issues.add('invalid_row_value', line)
                    continue
                if ordinal > U64 - samples:
                    issues.add('sample_ordinal_overflow', line)
                try:
                    db.execute('INSERT INTO packet VALUES(?,?,?,?,?)', (sid, str(ordinal), rx, phase, status))
                except sqlite3.IntegrityError:
                    issues.add('duplicate_full_packet_key', f'{generation}/{epoch}/{sid}/{ordinal}/{rx}')
                matrix[phase][status] += 1
                stats = stream_counts[sid]
                stats[f'{PHASES[phase]}_{STATUSES[status]}'] += 1
                actual_phase = 0 if rx < start else 1 if rx < end else 2
                if phase != actual_phase:
                    issues.add('phase_does_not_match_rx_cohort', line)
                if validated and validated < rx:
                    issues.add('validation_before_receive', line)
                delivered = status == 0
                if delivered:
                    if length != payload_bytes:
                        issues.add('unexpected_payload_bytes', line)
                    if not known or not validated or not app or not consumed or not rx <= validated <= app <= consumed:
                        issues.add('invalid_delivered_stage_pair', line)
                        continue
                    if digest != checksum[ordinal % 16]:
                        issues.add('canonical_iq16_checksum_mismatch', line)
                    if phase == 1:
                        counts['measurement_cohort_delivered_packets'] += 1
                        counts['measurement_cohort_delivered_payload_bytes'] += length
                        counts['measurement_cohort_delivered_samples'] += samples
                        if consumed >= end:
                            counts['measurement_cohort_consumed_in_drain'] += 1
                        for name, value in zip(metric_names, (validated-rx, app-validated, consumed-app, consumed-rx)):
                            db.execute('INSERT INTO metric VALUES(?,?)', (name, value))
                    if start <= consumed < end:
                        counts['consumption_window_packets'] += 1
                        counts['consumption_window_payload_bytes'] += length
                        counts['consumption_window_samples'] += samples
                        stats['consumption_window_packets'] += 1
                    else:
                        stats['consumed_outside_measurement_window'] += 1
                else:
                    # Missing delivery stages represent an observed nondelivery,
                    # not zero-latency samples and not a hidden subset omission.
                    if status == 2 and (app or consumed):
                        if not known or length != payload_bytes or not validated or not app or not consumed or not rx <= validated <= app <= consumed:
                            issues.add('invalid_consumer_rejection_stages', line)
                        elif phase == 1:
                            counts['consumer_checked_reject_measurement_packets'] += 1
                            for name, value in zip(metric_names, (validated-rx, app-validated, consumed-app, consumed-rx)):
                                db.execute('INSERT INTO metric VALUES(?,?)', ('consumer_rejected_' + name, value))
                    else:
                        if app or consumed or digest or known:
                            issues.add('nondelivery_contains_application_evidence', line)
                        if length not in (0, payload_bytes):
                            issues.add('unexpected_nondelivery_extent', line)
                    if status in (1, 3) and not validated:
                        issues.add('missing_required_validation_outcome', line)
                    if phase == 1:
                        counts['measurement_cohort_nondelivered_packets'] += 1
                        counts[STATUSES[status] + '_measurement_packets'] += 1
                if phase == 1 and validated and validated >= rx:
                    db.execute('INSERT INTO metric VALUES(?,?)', ('rx_to_validation_outcome_all_statuses', validated-rx))
        if counts['rows'] != declared_rows:
            issues.add('raw_summary_row_mismatch', f'{counts["rows"]} != {declared_rows}')
        if matrix != expected_matrix:
            issues.add('raw_phase_status_count_mismatch')
        if sum(map(sum, expected_matrix)) != declared_rows:
            issues.add('summary_phase_status_sum_mismatch')
        if not counts['rows']:
            issues.add('empty_receiver_capture')
        # Repeated/reordered ordinals are network observations, not corrupt CSV.
        # Compute union coverage only over observed delivered cohort intervals;
        # gaps inside that span have no automatic sender/receiver attribution.
        for sid in sids:
            stats = stream_counts[sid]
            prior = None
            for ordinal_text, in db.execute('SELECT ordinal FROM packet WHERE sid=? ORDER BY rx', (sid,)):
                ordinal = int(ordinal_text)
                if prior is not None and ordinal < prior:
                    stats['ordinal_regressions_in_rx_order'] += 1
                prior = ordinal
            end_ordinal = None
            last_start = None
            for ordinal_text, in db.execute('SELECT ordinal FROM packet WHERE sid=? AND phase=1 AND status=0 ORDER BY length(ordinal),ordinal', (sid,)):
                ordinal = int(ordinal_text)
                if last_start == ordinal:
                    stats['duplicate_delivered_ordinal_events'] += 1
                last_start = ordinal
                if end_ordinal is None:
                    stats['unique_delivered_cohort_samples'] += samples
                elif ordinal >= end_ordinal:
                    stats['unattributed_gap_samples_inside_observed_span'] += ordinal - end_ordinal
                    stats['unique_delivered_cohort_samples'] += samples
                else:
                    overlap = min(end_ordinal, ordinal + samples) - ordinal
                    stats['overlap_delivered_cohort_samples'] += overlap
                    stats['unique_delivered_cohort_samples'] += samples - overlap
                end_ordinal = max(end_ordinal or 0, ordinal + samples)
        for stream in summary.get('streams', []):
            sid = integer(stream['sid'])
            if sid not in stream_counts:
                issues.add('unknown_summary_stream', sid)
                continue
            observed = stream_counts[sid]
            total = sum(observed[p + '_delivered'] for p in PHASES)
            checks = {'delivered': total, 'known': total,
                      'measured_packets': observed['measurement_delivered'],
                      'measured_samples': observed['measurement_delivered'] * samples,
                      'pending_overflow': sum(observed[p + '_pending_overflow'] for p in PHASES),
                      'metadata_drops': sum(observed[p + '_metadata_expired'] for p in PHASES)}
            for key, expected in checks.items():
                if key in stream and integer(stream[key]) != expected:
                    issues.add('consumer_summary_mismatch', f'{sid}/{key}')
        latency = {name: quantiles(db, name) for name in metric_names}
        rejected_latency = {name: quantiles(db, 'consumer_rejected_' + name) for name in metric_names}
        validation_outcomes = quantiles(db, 'rx_to_validation_outcome_all_statuses')
        db.close()
    throughput = {}
    for prefix in ('measurement_cohort_delivered', 'consumption_window'):
        throughput[prefix] = {
            'packets_per_second': counts[prefix + '_packets'] / duration_seconds,
            'payload_bytes_per_second': counts[prefix + '_payload_bytes'] / duration_seconds,
            'complex_samples_per_second': counts[prefix + '_samples'] / duration_seconds}
    return {'schema': 1, 'kind': 'receiver_characterization',
            'association_generation': generation, 'epoch_seconds': epoch,
            'measurement_start_ns': start, 'measurement_end_ns': end,
            'duration_seconds': duration_seconds, 'counts': dict(counts),
            'phase_status_counts': matrix, 'phase_names': PHASES, 'status_names': STATUSES,
            'per_stream': {str(sid): dict(value) for sid, value in stream_counts.items()},
            'samples_per_packet': samples, 'payload_bytes': payload_bytes,
            'latency': latency, 'consumer_rejected_latency': rejected_latency, 'validation_outcomes_all_statuses': validation_outcomes,
            'percentile_method': 'exact nearest rank of paired receiver-local timestamps',
            'latency_population': 'delivered measurement-ingress cohort; drain completions retained; nondelivery counts remain explicit',
            'throughput': throughput,
            'throughput_scope': 'cohort rate includes later drain completion; consumption-window rate includes only completion timestamps inside the actual receiver window',
            'consumed_stage_scope': 'application checksum/consumption completed; not an inferred final provider lease return',
            'sender': sender_evidence(sender_dir),
            'receiver_reported_metrics': {key: value for key, value in summary.items()
                                          if key not in ('phase_status_counts',)},
            'capacity_or_loss_attribution': 'duplicates, reordering and gaps are observations, not capture corruption or attributed receiver loss; source offered/accepted/skips remain separate',
            'performance_thresholds_applied': False,
            'deployment_model_validated': False,
            'bounds': {'maximum_rows': maximum, 'maximum_sqlite_bytes': disk_mib * 1024 * 1024, 'sqlite_cache_bytes': 4 * 1024 * 1024,
                       'temporary_storage': 'disk-backed keys, paired intervals and exact sort; O(rows), hard row bound enforced'}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('receiver_dir', type=Path)
    parser.add_argument('--sender-dir', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--max-disk-mib', type=int, default=4096)
    parser.add_argument('--max-rows', type=int, default=10_000_000)
    args = parser.parse_args()
    issues = Issues()
    result = {'schema': 1, 'kind': 'receiver_characterization', 'performance_thresholds_applied': False}
    try:
        if not 1 <= args.max_rows <= 100_000_000:
            raise ValueError('max-rows must be 1..100000000')
        if not 1 <= args.max_disk_mib <= 65536:
            raise ValueError('max-disk-mib must be 1..65536')
        result.update(analyze(args.receiver_dir, args.sender_dir, args.max_rows, issues, args.max_disk_mib))
    except (OSError, ValueError, KeyError, TypeError, InvalidOperation, sqlite3.Error, csv.Error) as error:
        issues.add('analysis_input_error', error)
    code = 2 if issues.counts else 0
    result.update({'characterization_valid': code == 0, 'exit_code': code,
                   'invalid_reasons': dict(issues.counts), 'examples': issues.examples})
    output = args.output or args.receiver_dir / 'receiver_analysis.json'
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', dir=output.parent,
                                         prefix=output.name+'.', delete=False) as target:
            temporary = Path(target.name)
            json.dump(result, target, indent=2, allow_nan=False)
            target.write('\n')
        temporary.replace(output)
    except OSError as error:
        if temporary:
            temporary.unlink(missing_ok=True)
        print(f'INVALID: report write failed: {error}')
        return 2
    print(f'{"VALID CHARACTERIZATION" if code == 0 else "INVALID EVIDENCE"}: '
          f'{result.get("counts", {}).get("rows", 0)} receiver rows; '
          f'{sum(issues.counts.values())} integrity issues; {output}')
    return code


if __name__ == '__main__':
    raise SystemExit(main())
