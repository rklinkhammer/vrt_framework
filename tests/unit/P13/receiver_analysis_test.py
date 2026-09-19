#!/usr/bin/env python3
"""Independent literal fixtures for receiver-analysis evidence semantics."""
import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ANALYZER = Path(__file__).resolve().parents[3] / 'bench' / 'analyze_receiver.py'
HEADER = 'sid,ordinal,rx_ns,validated_ns,app_ns,consumed_ns,phase,status,known,bytes,checksum'.split(',')
# FNV1a64 of the literal sixteen IQ16 wire pairs repeated sixteen times;
# deliberately exceeds SQLite's signed INTEGER range.
CHECKSUM = 11748044937395394853
START, END = 1_000_000_000, 2_000_000_000


def fixture_rows():
    return [
        [1, 0, 900_000_000, 900_000_010, START+10, START+20, 0, 0, 1, 1024, CHECKSUM],
        [1, 256, 1_100_000_000, 1_100_000_005, 1_100_000_020, 1_100_000_030, 1, 0, 1, 1024, CHECKSUM],
        [1, 512, 1_200_000_000, 1_200_000_007, 0, 0, 1, 1, 0, 0, 0],
        [1, 768, 1_900_000_000, 1_900_000_010, 2_100_000_000, 2_100_000_010, 1, 0, 1, 1024, CHECKSUM],
        [1, 1024, 2_200_000_000, 0, 0, 0, 2, 2, 0, 1024, 0],
    ]


class ReceiverAnalysis(unittest.TestCase):
    def execute(self, rows=None, summary_update=None, arguments=(), sender=False):
        rows = fixture_rows() if rows is None else rows
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            matrix = [[0]*4 for _ in range(3)]
            for row in rows:
                matrix[row[6]][row[7]] += 1
            summary = dict(schema=1, kind='receiver', measurement_start_ns=START,
                           measurement_end_ns=END, association_generation=1, epoch_seconds=1000,
                           sids=[1], samples_per_packet=256, payload_bytes=1024,
                           sample_rate=1_000_000, checksum_algorithm='fnv1a64',
                           rows=len(rows), generated=len(rows), capture_overflow=0,
                           capture_io_error=False, phase_status_counts=matrix)
            summary.update(dict(pool_raw_bytes=10, pool_provider_bytes=10,
                                receiver_object_bytes=10, receiver_auxiliary_bytes=10,
                                capture_bytes=10, writer_stack_bytes=10, receiver_stack_bytes=10,
                                receiver_loop_state_bytes=10, accounted_bytes=80,
                                c_allocation_coverage_available=True))
            summary.update(summary_update or {})
            (root/'summary.json').write_text(json.dumps(summary))
            with (root/'receiver.csv').open('w', newline='') as out:
                csv.writer(out).writerows([HEADER, *rows])
            command = [sys.executable, str(ANALYZER), str(root), *arguments]
            if sender:
                source = root/'sender'
                source.mkdir()
                (source/'summary.json').write_text(json.dumps(dict(
                    measurement_start_ns=100, measurement_end_ns=200,
                    offered_packets=100, accepted_packets=80, skipped_packets=20)))
                command += ['--sender-dir', str(source)]
            run = subprocess.run(command, capture_output=True, text=True)
            self.assertIn(run.returncode, (0, 2), run.stderr)
            return run.returncode, json.loads((root/'receiver_analysis.json').read_text())

    def test_exact_pairs_and_nondelivery(self):
        code, result = self.execute()
        self.assertEqual(code, 0)
        self.assertTrue(result['characterization_valid'])
        self.assertFalse(result['performance_thresholds_applied'])
        self.assertFalse(result['sender']['available'])
        self.assertEqual(result['counts']['metadata_expired_measurement_packets'], 1)
        self.assertEqual(result['latency']['rx_to_validated'],
                         dict(samples=2, p50_ns=5, p90_ns=10, p99_ns=10, max_ns=10))
        self.assertEqual(result['latency']['rx_to_consumed']['p99_ns'], 200_000_010)
        self.assertEqual(result['throughput']['consumption_window']['packets_per_second'], 2)
        self.assertEqual(result['throughput']['measurement_cohort_delivered']['complex_samples_per_second'], 512)
        self.assertEqual(result['counts']['measurement_cohort_consumed_in_drain'], 1)

    def test_real_duplicate_and_reordering_are_observations(self):
        rows = fixture_rows()
        duplicate = rows[1].copy()
        duplicate[2:6] = [1_500_000_000, 1_500_000_005, 1_500_000_020, 1_500_000_030]
        rows.append(duplicate)
        code, result = self.execute(rows)
        self.assertEqual(code, 0)
        self.assertEqual(result['per_stream']['1']['duplicate_delivered_ordinal_events'], 1)
        self.assertEqual(result['per_stream']['1']['overlap_delivered_cohort_samples'], 256)
        self.assertGreaterEqual(result['per_stream']['1']['ordinal_regressions_in_rx_order'], 1)

    def test_duplicate_identical_capture_event_is_invalid(self):
        rows = fixture_rows()
        rows.append(rows[1].copy())
        code, result = self.execute(rows)
        self.assertEqual(code, 2)
        self.assertIn('duplicate_full_packet_key', result['invalid_reasons'])

    def test_reversed_stage_and_bad_checksum(self):
        rows = fixture_rows()
        rows[1][3] = rows[1][2]-1
        self.assertEqual(self.execute(rows)[0], 2)
        rows = fixture_rows()
        rows[1][-1] ^= 1
        self.assertEqual(self.execute(rows)[0], 2)

    def test_known_and_expiry_contracts(self):
        rows = fixture_rows()
        rows[1][8] = 0
        self.assertEqual(self.execute(rows)[0], 2)
        rows = fixture_rows()
        rows[2][4] = rows[2][3]+1
        self.assertEqual(self.execute(rows)[0], 2)

    def test_phase_and_capture_accounting(self):
        rows = fixture_rows()
        rows[1][6] = 2
        self.assertEqual(self.execute(rows)[0], 2)
        for update in ({'capture_overflow': 1}, {'capture_io_error': True}, {'generated': 6}, {'rows': 4}):
            self.assertEqual(self.execute(summary_update=update)[0], 2)

    def test_consumer_rejection_is_observed_processing(self):
        rows = fixture_rows()
        rows[1][7] = 2
        rows[1][-1] ^= 1
        code, result = self.execute(rows)
        self.assertEqual(code, 0)
        self.assertEqual(result['counts']['consumer_checked_reject_measurement_packets'], 1)
        self.assertEqual(result['consumer_rejected_latency']['rx_to_consumed']['max_ns'], 30)
        self.assertEqual(result['latency']['rx_to_consumed']['samples'], 1)

    def test_budget_and_consumer_counters(self):
        self.assertEqual(self.execute(summary_update={'accounted_bytes': 81})[0], 2)
        self.assertEqual(self.execute(summary_update={'receiver_stack_bytes': 0})[0], 2)
        self.assertEqual(self.execute(summary_update={'streams': [{'sid': 1, 'delivered': 99}]})[0], 2)

    def test_bounds_do_not_truncate(self):
        self.assertEqual(self.execute(arguments=['--max-rows', '1'])[0], 2)
        rows = []
        for index in range(5000):
            rx = START + 100 * index
            rows.append([1, index * 256, rx, rx+1, rx+2, rx+3, 1, 0, 1, 1024, CHECKSUM])
        code, result = self.execute(rows, arguments=['--max-disk-mib', '1'])
        self.assertEqual(code, 2)
        self.assertIn('analysis_input_error', result['invalid_reasons'])

    def test_sender_clock_is_separate(self):
        code, result = self.execute(sender=True)
        self.assertEqual(code, 0)
        self.assertEqual(result['sender']['sender_window_seconds'], 0.0000001)
        self.assertEqual(result['latency']['rx_to_validated']['p99_ns'], 10)
        self.assertNotIn('one_way_latency', result)


if __name__ == '__main__':
    unittest.main()
