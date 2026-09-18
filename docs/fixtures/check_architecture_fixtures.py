#!/usr/bin/env python3
"""Check architecture constants and factored fixtures, not a VITA implementation."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
f = json.loads((ROOT / 'vita49_architecture_fixtures.json').read_text())
checks = 0

def check(condition, message):
    global checks
    if not condition:
        raise AssertionError(message)
    checks += 1

for v in f['wire']:
    words = [int(w, 16) for w in v['words'].split()]
    check((words[0] & 0xffff) == v['declared_words'], v['id'] + ': declared count')
    check((words[0] >> 28) == v['type'], v['id'] + ': type')
    if v['valid']:
        check(len(words) == v['declared_words'], v['id'] + ': actual size')
    if 'cam' in v:
        c = words[2]
        for name, bit, width in [('partial',27,1),('action',23,2),('nack',22,1),
                                 ('v',20,1),('x',19,1),('s',18,1),('warn',17,1),
                                 ('error',16,1),('ackp',11,1),('schx',10,1)]:
            if name in v['cam']:
                check(((c >> bit) & ((1 << width)-1)) == v['cam'][name], v['id']+': '+name)
        check(((words[0] >> 24) & 1) == v.get('cancel',0), v['id']+': L')
        check(((words[0] >> 26) & 1) == v.get('ack',0), v['id']+': A')
    if v['id'] == 'W1':
        check(len(words) == 7 and words[-1] == 0x00200000, 'selector-only query')
    if v['id'] == 'W2':
        raw = (words[-2] << 32) | words[-1]
        check(raw == 1_000_000 * 2**20, 'sample rate fixed point')
    if v['id'] == 'W3':
        check(len(words) == 7 and words[3] == 2, 'cancellation selectors and MID')
    if v['id'] == 'W4':
        check(len(words) == 6 and ((words[2] >> 18) & 7) == 2, 'AckX has no detail body')
    if v['id'] == 'W5':
        check(words[2:] == [0x40000000, 0x3b21187e], 'canonical IQ16 packing')

for v in f['formats']:
    word0 = (1 << 29) | (v['item_code'] << 24) | ((v['bits']-1) << 6) | (v['bits']-1)
    check(f'{word0:08x}' == v['words'].split()[0], v['name']+': DPF word0')
    check(v['words'].split()[1] == '00000000', v['name']+': repeat/vector minus one')

# Independent truth-table expectations for C (clean), W (recoverable warning),
# E (recoverable error), U (unresolvable). Actual dependency/timing tests belong
# to the future transaction engine.
for row in f['partial_modes']:
    p,w,e = map(int,row['bits'])
    allowed = ['C'] + (['W'] if w else []) + (['E'] if e else [])
    results = allowed if p or len(allowed) == 3 else []
    check(results == row['clean_warning_error'], 'PWE '+row['bits'])
    results_with_u = allowed if p else []
    check(results_with_u == row['with_unresolvable'], 'PWE with U '+row['bits'])

for row in f['request_masks']:
    candidates = [name for name,bit in zip(['V','X','S'],row['bits']) if bit == '1']
    check(candidates == row['candidate_order'], 'request mask '+row['bits'])
    for nack in (0,1):
        for warnings in (0,1):
            for errors in (0,1):
                actual = [x for x in candidates if x == 'S' or not nack or warnings or errors]
                expected = candidates if not nack or warnings or errors else [x for x in candidates if x=='S']
                check(actual == expected, 'NACK factor '+row['bits'])

for row in f['diagnostic_masks']:
    rw,re = map(int,row['bits'])
    groups = (['warning_indicators'] if rw else []) + (['error_indicators'] if re else [])
    groups += (['warning_values'] if rw else []) + (['error_values'] if re else [])
    check(groups == row['groups_if_both_conditions_present'], 'diagnostic mask '+row['bits'])

for row in f['timing_cases']:
    mode = row['mode']
    early = row['app_early_ns'] if mode in (3,4) else row['device_early_ns']
    late = row['app_late_ns'] if mode in (2,4) else row['device_late_ns']
    lo,hi = row['effect_interval_ns']
    accepted = lo >= row['request_ns']-early and hi <= row['request_ns']+late
    check(accepted == row['allowed'], row['id']+': effect uncertainty interval')

for row in f['state_scenarios']:
    check(bool(row['initial']) and bool(row['events']) and bool(row['expected']),
          row['id']+': state fixture has setup, events, expected results')

pool_bytes = sum(x['block_bytes']*x['blocks'] for x in f['pools'])
check(pool_bytes == 30_998_528, 'pool byte arithmetic')
budget = f['arena_budget']
check(sum(x['reserved_bytes'] for x in budget['categories']) == budget['cap_bytes'],
      'complete projected arena fits 64 MiB')
check(budget['categories'][0]['reserved_bytes'] == pool_bytes, 'arena raw blocks match pools')
check(all(x['reserved_bytes'] > 0 for x in budget['categories']), 'positive category reservations')
check(budget['cap_bytes'] == 64 * 1024 * 1024, 'arena cap units')
# Ensure the reviewed human-readable table and machine fixture cannot drift.
import re
architecture = (ROOT / 'vita49_framework_architecture.md').read_text()
table = architecture.split('| Arena category |')[1].split('The non-headroom')[0]
documented = [(name.strip(), int(size.replace(',', '')))
              for name, size in re.findall(r'^\| ([^|]+) \| ([\d,]+) \|', table, re.M)]
check(documented == [(x['category'],x['reserved_bytes']) for x in budget['categories']],
      'documented arena partition matches fixture')

check(4*1_000_000/256 == 15_625, 'normal aggregate packet rate')
check(15_625*(28+256*4) == 16_437_500, 'normal VRT byte rate')
check(100_000_000/256*(28+256*4) == 410_937_500, 'stress VRT byte rate')
check((1472-28)//8 == 180, 'IPv4 IQ32 MTU samples')
check((1452-28)//8 == 178, 'IPv6 IQ32 MTU samples')
print(f'PASS: {checks} architecture fixture checks; no framework implementation tested.')
