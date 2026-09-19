# P13 remediation independent verification

Status: functional/sanitizer and legacy classification remediation gate PASS. This gate authorizes diagnostic measurement, not P13 performance acceptance.

The78-file `/tmp/p13-remediation-verifier-manifest.txt` has SHA-256 `6f7c5bbd761b4da077d5f80035c9835ce08a79963410a19cbfe5316e017a9739`. It identifies the checked Runtime, retention index, stateless capture helper, analyzer and independent tests. The original failed measurements remain unchanged and are documented in [P13-measurement-verification.md](P13-measurement-verification.md).

| Independent regression | Verified behavior |
|---|---|
| `p13_verify_retention_index` | Exact-capacity original/cancellation extents, transactional exhaustion, alternating expired holes reused, immutable neighboring AckS payloads, duplicate replay, byte-credit return, and reference-scale3,000 retained commands. Placement visits at most existing extents and totals4,498,500 probes for3,000 sequential admissions; no elapsed-time assertion substitutes for the complexity bound. Exact30-second retention remains unchanged. |
| `p13_verify_pressure` | Exhausted Context control pool preserves running below10ms and resumes known-metadata Data after release; exact10ms blockage enters context_unavailable. Header-pool exhaustion is retryable. Source callback returning capacity_exhausted still faults. A high-rate blocked stream buffers64 packets, then remains bounded by other physical capacity until the10ms terminal gate. P09's direct publisher test separately exercises the65th held submission limit. |
| `p13_verify_late_capture` | An old issued MID1 remains capturable when next issued MID is514 or100,000. Duplicate observations remain recordable; truncation, wrong source/SID/OUI/controller, unissued MID and cancellation traffic reject. No fixed pending-slot state exists to overwrite. |
| `duplicate_capture_contract.py` | An identical appended Ack is explicitly counted/coalesced without corrupting a valid capture; conflicting same-phase flags are INVALID. |
| `analyzer_contract.py` | Paired p50/p99/max remain equal to the independent oracle, and eight prior malformed/corrupted artifact cases still reject. |

`cmake --preset udp-asan-ubsan`, a complete build, and full CTest passed **135/135**; logs are `/tmp/p13-remediation-asan-build.log` and `/tmp/p13-remediation-asan-tests.log`. All earlier P07–P12 retention, cancellation, lifecycle, timing, ownership and transport regressions were included. The coordinator subsequently added a developer-only peer helper test registration; the independent helper target was already included in this gate.

Targeted UDP TSan passed **7/7**: the three new tests, concurrent trace publication, SPSC ring, P07 late-result guard race and P11 retained late capability. Raw analyzer mutation tests run independently in Python. No performance run occurred concurrently with these checks.

The ordered extent index increases reference retention storage by32,784 bytes. This is bounded setup storage and must appear in the next measured composition's ledger; the earlier55,272,696-byte measurement is historical, not a current-size claim. Allocation, identity, byte contents and30-second retention semantics are preserved by the tests above. The algorithm still has bounded linear scans/shifts; the gate does not claim constant-time admission.

## Legacy classification closure

**PASS.** A separate streaming reconstruction of the512-slot legacy peer table from raw send/Ack order, joined to independently collected accepted trace identities, agrees with the corrected analyzer:

- Normal:3,333 reconstructed evictions, **zero accepted operations evicted**;2,786 explicit validation rejections and547 no-Ack receive-only cases. This is a valid **failed measurement** after correct classification, not evidence of lost executed completion. Its p99 recording remains918,064,708 ns and its Data/command targets still fail.
- Overload:591 reconstructed evictions, including **16 accepted operations** and575 pre-admission cases. The16 accepted correlations make it INVALID even with the correct60-second duration requirement. Its diagnostic successful-subset p99 remains2,829,354,834 ns.

The updated analyzer wrote only `/tmp/p13-remediation-normal-analysis.json` and `/tmp/p13-remediation-overload-analysis.json`; the original frozen analyses and raw captures were preserved. Both independent classifications match exactly, and the revised analyzer leaves all paired latency statistics unchanged. The remediation gate is now **PASS for isolated diagnostic measurement readiness**. Actual performance acceptance remains unachieved pending new runs.

## Revised isolated60-second diagnostics

**Independent audit: normal is a valid failed diagnostic;120% overload passes its bounded-overload diagnostic criteria. P13 remains open.** Both revised runs have complete raw command evidence, no pending-record overwrite, no trace/capture corruption and zero instrumented critical C/C++ allocations. The266-file source manifest SHA-256 is `cec82bb9749f278dc1094a273e5425939b983a3496218bfed7597bb43953a5a8`; every source and both binaries match their recorded manifests. These are new artifacts under `artifacts/P13/revised-normal-60s` and `revised-overload-60s`; original failed runs remain preserved.

| Independent observation | Revised normal | Revised overload |
|---|---:|---:|
| Measured duration | 60s | 60s |
| Ordinary sends plus burst | 6,000 +64 | 7,200 +64 |
| Received/validated/recorded successful commands | 6,064 each | 7,264 each |
| Raw trace rows | 30,320 | 36,320 |
| Raw AckV/AckX/AckS rows | 6,064 each | 7,264 each |
| p99 validation | 19,417ns | 18,875ns |
| p99 receive-to-recorded | 1,326,834ns | 196,875ns |
| Maximum receive-to-recorded | 3,253,125ns | 3,416,375ns |
| Instrumented critical C / C++ allocations | 0 /0 | 0 /0 |

The independent paired oracle finds no integrity errors, missing commands or stage mismatches. Both meet the p99 command targets in this60-second diagnostic; maxima above2ms do not contradict a p99 target. Source/peer fatal errors, bad peer evidence, malformed Data, overlap, pending overwrite, trace overflow and capture I/O errors are zero. Both report five backpressure events, which remain visible rather than being reclassified as fatal errors.

Normal still drops88 complete packets,22,528 samples, per stream. Each stream accounts exactly for60,000,000 expected samples:59,977,472 measured peer samples plus22,528 missing, a **0.0375467%** deficit. Measured accepted packets234,287 ×256 equal measured peer samples exactly. This violates the selected no-drop normal target even though command latency improved. A60-second run that already fails that target is not evidence warranting a sustained PASS; no revised1,800-second qualification is claimed.

Overload has254,118 measured accepted packets per stream and27,131 skipped packets (6,945,536 skipped samples). The peer's timestamp-clipped measurement covers65,054,336 of72,000,000 expected samples, leaving6,945,664 missing (**9.646756%**). These figures differ from packet-counter deltas at the measurement boundaries: accepted packets ×256 =65,054,208,128 fewer than clipped peer coverage; accepted samples plus skipped samples =71,999,744,256 fewer than the ideal interval. The packet-counter snapshots occur on host progress boundaries, whereas peer coverage clips sample ordinals to the exact protocol interval. At1.2MS/s, the5-second warm-up boundary is ordinal6,000,000, halfway through a256-sample packet, explaining why128-sample clipping is possible. The remaining256-sample accounting discrepancy is one packet and within the explicit boundary tolerance. The artifacts do not include a per-Data-packet trace to independently attribute each endpoint; this bounded discrepancy must not be presented as exact per-packet forensic proof. It is materially different from the tens of millions of unaccounted samples in the original overload run.

All accepted total Data packet counts match peer totals in both revised runs, with no reported overlap or malformed samples. Overload permits observable bounded loss; its PASS does **not** mean no packet drops and does not satisfy the normal no-drop requirement. The source control path continued completing every offered command under120% load.

Actual accounted framework storage is **55,305,480 bytes**, exactly the historical55,272,696 plus32,784-byte retention-index increase. Category charges sum to that value; reservations remain67,108,864, leaving **11,803,384 bytes** headroom. The process logs separately report24,936,448-byte maximum resident set size; RSS is not substituted for the explicit ownership/capacity ledger. The allocation scope remains critical Runtime/native-peer threads, with setup/writer and kernel-buffer exclusions disclosed.

Independent commands were `python3 tests/verification/P13/verify_run.py artifacts/P13/revised-normal-60s --output /tmp/p13-revised-normal-independent.json` and the corresponding overload command. Peer phases were independently streamed and counted; the production analyzer's valid/failed versus valid/overload-PASS classifications agree with this audit. Neither60-second result is a revised30-minute acceptance result or a Linux/independent-peer/GPS deployment qualification.

| Revised artifact | SHA-256 |
|---|---|
| normal summary | `84cf42f5e86a0c205516a707e6e1210aca0050e0db351df692ce67228fea6618` |
| normal trace | `1b7d4d70a0ddfa0183419268be74503ffaa412240125ef309a1a658a79a9831f` |
| normal peer | `11d586a76a2f5bcf25c59b64ae4fe5b69f4d2032b1b4ca0dafe5aedcc7132e24` |
| overload summary | `8c455cadd1e03ec8855fe38a9adabbab0c1ccac0852f919919a4e2e45dd4a3e1` |
| overload trace | `2a308cd7d44209de88c53c886ee8b157797faf902e12c90bcb219be471790e1f` |
| overload peer | `1e9e0ccdb95de6f723e452b729386393cdce89f4547917b3bfc84f6a74c9b95d` |
