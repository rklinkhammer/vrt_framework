# P13 independent verification

Status: frozen harness readiness PASS; sustained performance and overload evidence pending; bounded malformed-packet replay PASS. Deployment qualification is not claimed.

## Harness validity gate before sustained measurement

- Four IQ16 streams at1MS/s each,256 complete pairs/packet,1500-byte IP MTU. Report configured and observed stream/packet/sample rates. The actual monotonic host clock drives measurement; injected protocol conditioning remains explicitly a lab binding.
- A separate inline virtual-register Controllee path with four fixed-size fields, no device I/O, no deliberate delay and no requested execution timestamp. Preserve runtime semantic decode, admission, dispatch, ticket publication/consumption and result recording. Generator boundary waits are not silently substituted for this backend.
- Ordinary command traffic actually offered at100/s, plus periodic64-command bursts under load (four model instances respect per-Controllee active limits). Count offered, sent, received, rejected, admitted, completed, missing and trace-overflow separately. Do not self-throttle because the public Controller observation store retains256 records for30s; an explicit native traffic generator may supply unique MIDs without weakening receiver retention.
- `t_rx` is actual complete-datagram/envelope handoff before semantic decode. `t_validated` follows validation/admission; `t_dispatch` precedes backend update; `t_device_done` marks ready publication; `t_recorded` follows strand incorporation. Every completed row carries the full correlation identity. Validate ordering and preserve incomplete/rejected/overflow records without inventing timestamps.
- Independently recompute nearest-rank percentiles and maxima from paired raw per-command rows. Acceptance uses validated-minus-rx and recorded-minus-rx; additional backend/pre-dispatch/consumption intervals derive from the same row. Do not subtract separately calculated percentiles or include Ack delivery in the local recording target.
- Instrument ordinary/aligned allocation after setup, actual object/arena/stack accounting and socket-buffer exclusions. Trace capacity exhaustion must remain bounded and visible. Reconcile the selected completed composition with64MiB, including headroom transfers.
- Per-stream loss methodology must go beyond modulo16 Packet Count: sample timestamps/ordinals and source attempted/emitted/skipped accounting identify gaps, duplication/reordering and framework versus receiver/kernel losses.
- Exercise120% offered overload with a reproducible load definition, finite queues and observable drops/rejections; ordinary/cancellation/completion capacity remains isolated. Stress100MS/s is separate and cannot replace the normal workload.
- Malformed-input fuzzing and failure injection are executable reproducible checks; keep them out of the sustained timing interval.

## Measurement and qualification boundary

Gate short-run trace integrity and counts before spending30minutes on sustained output. During a performance run, avoid unrelated compiler/test workloads. Record actual duration, warm-up, CPU/OS/compiler/library, affinity policy, packet sizes, socket buffer requests and observed values, copy path, clock source, profiling overhead, raw artifacts and command lines.

The local development host is macOS, not the selected Linux qualification platform with10GbE and an independent peer. A30-minute localhost result remains local measurement evidence. Missing authorized production OUI, calibrated GPS/PPS deployment binding, external peer/NIC and capture inputs block their applicable deployment claims; they do not justify fabricated measurements or skipping software harness verification.

Final source manifests, commands, raw-artifact hashes, recomputed statistics and separate software/deployment dispositions will be recorded after candidate freeze.

## Frozen harness gate

**PASS for sustained measurement readiness; P13 performance acceptance remains pending.** Independent verification ran on macOS arm64 with Apple Clang 21/libc++. The baseline revision is `8435ab71d8004e9014a37a63d0f4576ea533252b`; implementation is an uncommitted candidate. The 73-file source manifest `/tmp/p13-verifier-harness-manifest.txt` has SHA-256 `d061d21eb66eecce441ada7a6b344d518946223d3a11b18e92c532980b51901a`. It includes public headers, compiled adapter, benchmark sources/analyzer, independent P13 tests, and CMake configuration.

| Independent test | Evidence |
|---|---|
| `p13_verify_trace` | Four real fixed-field model updates, inline publication before incorporation, complete stage identity/order, backend-thread timestamp publication, no validated trace or backend call after admission rejection. |
| `p13_verify_runtime_trace` | Public virtual-register endpoint executes without PPS, Data start or generator-boundary progression; five actual monotonic stages; shared trace-owner accounting and transactional inconsistent-size rejection. |
| `p13_verify_ingress` | Actual UDP malformed semantic body reaches pre-decode timestamp hook but never semantic callback. |
| `p13_verify_ring` | Bounded overflow without overwrite, 10,000 cross-thread payload/timestamp transfers, and failed CSV write exposes I/O error without incrementing written rows. |
| `p13_verify_allocation_probe` | Deliberately injects all five advertised C allocation forms and four C++ ordinary/aligned forms while critical; each increments its counter. Noncritical allocation does not. Links the separate Mach-O interposition image. |
| `p13_verify_oracle` | Synthetic independent paired percentile arithmetic and capture/identity/order/coverage corruption checks; valid slow results remain measurement failures. |
| `analyzer_contract.py` | Production analyzer agrees exactly with independent p50/p99/max for all five paired intervals and rejects eight corrupted copies of a real capture. |

Commands and outcomes:

- `cmake --preset udp-release` and build the five independent P13 executable targets; `ctest --test-dir build/udp-release -R '^p13_verify_' --output-on-failure`: **6/6 PASS**. Release production uses optimization; test assertions remain enabled.
- `cmake --preset udp-asan-ubsan`; `cmake --build --preset udp-asan-ubsan`; `ctest --test-dir build/udp-asan-ubsan --output-on-failure`: **130/130 PASS**, including public-header isolation and all affected earlier packages/examples. Log: `/tmp/p13-asan-gate.log`.
- `cmake --preset udp-tsan`; build `p13_verify_trace p13_verify_runtime_trace p13_verify_ingress p13_verify_ring p11_verify_late p04_verify_tickets`; targeted CTest including the Python oracle: **7/7 PASS**.
- `python3 tests/verification/P13/analyzer_contract.py artifacts/P13/final-smoke`: **PASS** for exact paired metrics and eight corruption cases.

Sanitizer presets exclude the benchmark allocation interposition library. Thus sanitizer functional evidence and optimized Release allocation-detection evidence are separate; no claim assumes compatibility between two allocator interposition mechanisms. Linux C-allocation coverage remains unavailable.

The final short capture contains 164 offered/received/admitted/recorded successful commands and 820 stage rows, with exact configured ordinary traffic and reconciled four-stream sample coverage. Independent p99 validation is **35,917 ns** and p99 receive-to-recorded is **1,954,125 ns**. The capture is internally valid and meets those short-run latency targets, but measured Data skips fail the no-drop target. Its one-second duration is not sustained qualification. The measured composition charges **55,272,696 bytes**, below 64 MiB; setup/writer/application exclusions and socket buffers remain explicit in the artifacts.

Closed verification findings: rejected factory cleanup was repaired in P12; P13 C allocation detection requires a separate shared image; mixed simulated/real outcomes within one command now invalidate capture; writer failure/generated-row mismatch invalidate capture. The analyzer preserves valid performance failures instead of converting them to missing-evidence claims.

## Bounded malformed-packet replay addendum

**PASS.** Independently verified all 12 binary seed hashes against `fuzz/corpus/SHA256SUMS`, reviewed the target's bounds and callback invariants, and ran the corrected target with ASan/UBSan:

```
cmake --preset udp-asan-ubsan
cmake --build --preset udp-asan-ubsan --target vita_fuzz_replay
build/udp-asan-ubsan/fuzz/vita_fuzz_replay fuzz/corpus 100000 0x564954413439
ctest --test-dir build/udp-asan-ubsan -R '^malformed_packet_mutation_replay$' --output-on-failure
```

The registered test passes in 1.32 seconds. The direct run reports `seeds=12 mutations=100000 final_rng=3072068718813708468 checksum=16816288533938920164 cap=65536`. The 18-file fuzz/CMake manifest `/tmp/p13-fuzz-verifier-manifest.txt` has SHA-256 `8bff3a88aca91b5a54d0e850e2dd2b968a188c6b75843f21982a760931539f59`.

Seeds include Data, execute, query, cancellation, Ack, CIF7 selectors, unsupported Context, truncated UUID, short/oversized framing, all-indicator and empty cases. Six deterministic mutation families cover truncation, bit changes, independent declared lengths, fresh random datagrams, appended tails up to the explicit 65,536-byte input bound, and complete-word changes. Each input runs both correlated and uncorrelated decoding. The target checks decode/visit agreement, no callbacks on rejected packets, nonempty field-byte bounds, checked semantic access, and immediate callback-error propagation. This is finite mutation evidence, not proof of exhaustive field/layout coverage.

The first run found a **target false invariant**: selector views intentionally have empty/null byte spans, while the target demanded that every span's pointer lie inside the input. Independently isolating the query, cancellation and CIF7 seeds with zero mutations reproduced that failure. The coordinator corrected only the target to bound nonempty spans; no production parser change was needed.

Optional coverage-guided compilation was attempted with `clang++ -std=c++23 -fno-exceptions -fno-rtti -Iinclude -fsanitize=fuzzer,address,undefined -g fuzz/libfuzzer.cpp -o /tmp/p13-packet-fuzzer`. Linking failed because this Apple Clang installation lacks `libclang_rt.fuzzer_osx.a`. No libFuzzer run or coverage count is claimed. The deterministic ASan/UBSan replay is available and passed.

## Final remediation and measurement status

See [independent remediation verification](P13-remediation-verification.md) and [M4 integration report](M4-integration.md) for the current candidate. Retention placement, stateless peer capture and transient Runtime pressure repairs passed independent checks. Final aggregate Release:139/139; independent ASan/UBSan:135/135 and targeted TSan:7/7. Revised 60-second normal and overload captures have complete Control outcomes and passing p99 latency. Normal no-drop remains failed; no revised 1800-second or deployment qualification is claimed. Earlier manifests and captures above are historical evidence.
