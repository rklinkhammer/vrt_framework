# M4 integration and local measurement report

Status: **functional software integration PASS; P13 continues as receiver-focused performance characterization, not a hard local no-drop gate**. Production IQ generation runs on a separate machine, and hardware will be chosen to meet application requirements. Existing measurements remain evidence; receiver capacity and deployment performance are not yet established.

The [accepted receiver performance-model direction](P13-receiver-performance-model.md) supersedes the earlier P13 qualification interpretation used in the historical sections below. Their recorded target failures are preserved, but do not block framework software integration. Current localhost tests combine generation and native-peer reception; they do not isolate the complete framework receiver path or establish receiver-induced loss. Receiver-only measurements and a validated performance model remain outstanding.

## Subsequent 256-pair payload comparison

Both requested fixed-payload modes are implemented and independently verified. The generated/copy/prefilled60-second comparison observed68/35/33skipped packets per stream respectively, with all6064commands completed per run, passing p99latency and zero critical C/C++ allocations. Prefilled mode initialized all8192fixed payload blocks and recorded zero production payload writes/copies. The framework already used fixed pools; no per-packet allocation/deletion path was substituted. Final aggregate tests:142/142 optimized and139/139 ASan/UBSan. See [payload comparison and exact evidence](P13-payload-comparison.md).

These single-trial diagnostics retain256pairs and do not close normal no-drop or sustained qualification. The candidate and measurements below predate this additive payload-mode change and remain historical evidence.

## Retention/capture revised candidate

The original sustained run exposed three concrete defects: quadratic retained-byte placement, live peer correlation eviction, and premature source faults on transient resource pressure. These were repaired and independently verified; the original evidence below is preserved as history. The [remediation verification](P13-remediation-verification.md) records 135/135 ASan/UBSan and seven targeted TSan checks. The final combined optimized build passes **139/139 tests**, including the additional developer peer test: [test log](artifacts/M4-remediation/LastTest.log).

The final 266-file [source manifest](artifacts/M4-remediation/source.sha256) has SHA-256 `cec82bb9749f278dc1094a273e5425939b983a3496218bfed7597bb43953a5a8`; all source and binary hashes matched after measurement. New results apply to this candidate, not the original manifest below.

| Revised 60-second diagnostic | Normal | 120% overload |
|---|---:|---:|
| Offered / fully completed commands | 6,064 / 6,064 | 7,264 / 7,264 |
| p99 validation | 0.019417 ms | 0.018875 ms |
| p99 receive-to-recorded | 1.326834 ms | 0.196875 ms |
| Capture integrity issues | 0 | 0 |
| Sources running at measurement end | 4 / 4 | 4 / 4 |
| Analyzer result with explicit 60-second minimum | MEASURED FAIL: Data loss | PASS |

Normal missed 22,528 samples (88 packets) per stream out of 60,000,000 samples: **0.0375467%**. Local skip counters and peer coverage reconcile. This is an improvement over the original run, but zero loss is still required. Overload peer coverage misses 6,945,664 samples per stream (9.646756%); loss is explicitly observed and bounded, and a passing overload result does not assert zero loss. The independent audit documents a 128-sample difference between clipped peer coverage and whole-packet skip counters, plus a 256-sample snapshot-boundary deficit; no per-Data-packet forensic reconstruction is claimed. Burst tails include individual completions above 2 ms while the required p99 remains below 2 ms.

Both revised runs record zero critical C/C++ allocations, no trace/peer-ring overflow, no pending correlation eviction and no invalid peer packets. Actual framework charge is **55,305,480 / 67,108,864 bytes**, leaving 11,803,384 bytes below the ceiling. Added index storage is fully charged. Stream diagnostics show running through the measurement window and explicitly stopped at its end, with no fault transition.

[Revised normal analysis](../../artifacts/P13/revised-normal-60s/analysis.json) and [revised overload analysis](../../artifacts/P13/revised-overload-60s/analysis.json) retain exact commands, raw timing/peer observations, statuses, budgets and binary/source manifests beside the reports. These windows span retained-history occupancy but **do not replace the required 1800-second normal run**. A further long run was not used to relabel an already failing short diagnostic as accepted.

P13 remains open for no-drop normal-load qualification and a new 1800-second run on the revised candidate once the remaining pacing/scheduling losses are resolved. The artifacts establish obsolete-interval skips; they do not prove that all remaining loss is solely an OS or hardware problem. Designated deployment host/peer/clock inputs remain missing. P14/P15 were not started.


## Original candidate software evidence

P12 supplies the optional compiled POSIX UDP adapter, static peer/lane provisioning, bounded service and ownership accounting. P13 supplies actual monotonic-stage tracing, an inline four-field virtual-register backend, bounded raw capture, allocation probes, budget reporting and repeatable measurement tools. Implementer and independent verifier responsibilities and findings are retained in [P12 verification](P12-verification.md) and [P13 verification](P13-verification.md).

The final optimized aggregate passes **134/134 tests**, including public-header isolation, examples, independent contracts, allocation probes, benchmark smoke and bounded malformed-input replay. The recorded gate is [LastTest.log](artifacts/M4/LastTest.log). Independent sanitizer evidence includes 130/130 ASan/UBSan checks before the fuzz addendum, seven targeted P13 TSan checks, and the separately passing 100,000-mutation ASan/UBSan replay. The optimized allocation probe exercises five C and four C++ allocation forms. These are distinct gates, not an assertion that sanitizer and allocator interposition were combined.

The frozen measurement source manifest covers 259 files and has SHA-256 `d3bdecc07fef2430b7fa652bc3882f37ac7ac49ac35135bddec44dbf6eddd496`. A [tracked copy](artifacts/M4/source.sha256) accompanies the test log. Raw measurement directories retain binary hashes, exact invocation, compiler build commands and host metadata. Source files remain uncommitted; the manifest identifies the measured candidate independently of the starting Git revision.

## Measurement scope

Apple M4 Max, macOS 27, Apple Clang 21/libc++, optimized C++23; IPv4 localhost, explicit lab identity, synthetic protocol-clock conditioning and actual host monotonic pacing. Normal load is four 1 MS/s IQ16 streams, 256 sample pairs per 1052-byte VRT packet, 100 ordinary commands/s plus 64-command bursts every 60 seconds. The requested normal window is 1800 seconds after five seconds of warmup, followed by a one-second drain. Overload raises IQ and ordinary Control rates to 120% without raising resource bounds.

No compiler, sanitizer, or artifact-analysis workload runs alongside the timed measurements. Localhost results do not establish an independent peer, qualified GPS/PPS binding or deployment NIC behavior. Missing deployment inputs are listed in [M4 inputs](M4-inputs.md). P14/P15 are outside this execution scope.

## Original sustained results

The normal run completed its 1800-second window; overload completed 60 seconds at 120% load. Both processes exited 0, but both frozen analyzers exited **2 (invalid complete capture)** because peer pending records were overwritten before all requested responses arrived. Independent audit distinguishes normal explicit rejection/no-Ack records from the 16 overload operations that completed after live correlation was evicted. The frozen counter conflates these cases. Process success is not measurement acceptance. No failed capture was discarded or relabeled a passing run.

| Observation | Normal, 1800 s | Overload, 60 s |
|---|---:|---:|
| Offered commands | 181,856 | 7,264 |
| Fully paired recorded commands | 178,523 | 6,689 |
| Receive-only / incomplete commands | 3,333 | 575 |
| Unconfirmed execution responses | 3,333 | 591 |
| Pending-record overwrites | 3,333 | 591 |
| Diagnostic p99 validation | 6.499 ms | 8.375 ms |
| Diagnostic p99 receive-to-recorded | 918.065 ms | 2,829.355 ms |
| Framework charged bytes | 55,272,696 | 55,272,696 |
| Observed critical C / C++ allocations | 0 / 0 | 0 / 0 |

Latency values describe the paired subset, with incomplete outcomes retained separately; they are not passing population qualification. Normal targets are 1 ms validation and 2 ms receive-to-recorded. Each normal Data stream missed 374,959,872 of 1,800,000,000 samples (20.831104%), exactly matching its reported skips. Overload SIDs1/2 additionally fail full-window sample reconciliation; therefore complete overload loss accounting is not established. Zero trace-ring overflow and successful file writes do not repair peer correlation loss.

The reserved budget sums to 67,108,864 bytes and actual framework charges leave 11,836,168 bytes below that ceiling. Three explicit 1 MiB stacks and capture storage are included. Native peer application storage and OS socket buffers are reported separately. `/usr/bin/time` reports normal maximum RSS of 25,034,752 bytes; RSS is a different measure from the complete admitted framework ledger and does not replace it.

[Normal analysis](../../artifacts/P13/normal/analysis.json), [overload analysis with explicit 60-second minimum](../../artifacts/P13/overload/analysis-60s.json), and [component totals](../../artifacts/P13/components/components.json) retain the local results. The default overload analysis also records the default 1800-second duration check; the explicit 60-second analysis removes only that mismatch and remains invalid. Raw CSVs, summaries, manifests, invocation, build commands, host metadata and process logs remain under `artifacts/P13/`; these large local artifacts are ignored by Git.

Component totals over 100,000 iterations: encode 2,088,708 ns, decode 8,025,083 ns, fill 256 IQ pairs 92,973,000 ns, lease acquire/return 4,714,417 ns, clock-plus-trace push/pop 1,870,833 ns. Gathered UDP roundtrip totals 115,659,292 ns over 10,000 iterations. These are local elapsed totals, not percentile or independent-peer results.

The 259 source entries and two measured binary hashes were rechecked unchanged after all timed runs. See the [independent measurement audit](P13-measurement-verification.md) for raw-record reconciliation and limitations. At this original checkpoint, required remediation was to repair and independently verify pending-response capture behavior, investigate incomplete/admission and sustained latency behavior, and close overload sample reconciliation before rerunning acceptance measurements. The current revised candidate section records the subsequent repairs and diagnostic evidence. Missing deployment inputs separately prevent deployment qualification.

## Completed remediation

The failed sustained run exposed quadratic retained-byte placement cost, not merely host jitter. A bounded sorted extent index, corrected peer capture handling, and Runtime transient-backpressure behavior passed independent verification. Original measurements describe the pre-remediation manifest and are not reused as measurements of the revised source.

The [corrected legacy reanalysis](../../artifacts/P13/legacy-reanalysis/normal.json) classifies the original normal run as a valid measured failure: all overwritten records were pre-admission rejection/incomplete cases. The [original overload reanalysis](../../artifacts/P13/legacy-reanalysis/overload.json) remains invalid with 16 accepted-operation correlation evictions. Paired metrics and failed performance findings remain unchanged; the frozen reports are retained alongside this explicitly versioned reanalysis.

## Receiver replay validation checkpoint

The receiver-only UDP/checked-codec/ContextReceiver/checksum harness passes its independent gate and the full optimized 146-test suite. Ten repeated local replay captures at four 256-pair streams are valid, covering approximately 1–16 million aggregate IQ pairs/s. Fixed pools and explicit worker stacks account for 4,218,560 bytes; observed critical C/C++ allocations and adapter drops are zero. This is an initial receiver-component operating range, with source skips separately reported, not deployment capacity or full VitaRuntime qualification. See [results, scope and reproduction](P13-receiver-validation.md) and [independent audit](P13-receiver-verification.md). External sender measurements, 1,024-pair support/MTU verification and broader model validation remain outstanding.
