# P13 fixed-payload comparison

Status: implementation and independent verification PASS; all three requested comparative runs completed. Normal no-drop qualification remains unmet. This experiment retains 256 IQ16 pairs per packet and the existing 1500-byte MTU. It does not change the normal-profile size to1024pairs or replace sustained deployment qualification.

## Existing fixed-pool behavior

`ExternalPool::create` builds a fixed block table during setup; the lab provider allocates each role's backing storage before execution. `ExternalPool::acquire` leases an available block, and final `BufferLease::reset` returns that block for reuse. Neither operation allocates or deletes packet backing storage. Runtime holds the provider lifetime during operation; final provider teardown releases setup allocations. Optional return callbacks run before the block becomes available again. The benchmark uses the standard lab pools without a per-return allocator callback.

The reference payload pool has8192fixed2048-byte blocks. Headers, payloads, Control/cancellation and receive buffers have separate configured pools. Pool acquisition still performs a bounded block-table scan under a mutex; fixed storage does not eliminate bookkeeping or scheduling costs. This experiment does not change that allocator strategy.

## Comparison contract

All modes retain four1MS/s streams,100ordinaryControl commands/s, a64-command burst every30seconds, 256IQ16pairs per packet, actual monotonic pacing, dynamic timestamps/packet counts, the existing Context gates and gatheredUDPtransport. Each diagnostic uses5seconds warmup,60seconds measurement and the existing bounded drain. These are comparative diagnostics, not1800-second qualification runs.

- `generated`: run the canonical sixteen-sample tone provider for each packet.
- `precomputed-copy`: prepare canonical encoded1024-byte payload once, then copy it into each leased payload block.
- `prefilled-pool`: initialize every eligible payload block before execution, release all setup leases, then reuse the initialized bytes without per-packet payload writes or copies.

The prefilled provider is scoped to the benchmark's fixedIQ16/256-pair/aligned-phase workload. Normal ownership, complete-write declaration/validation and transport lifetime rules remain enforced. Headers remain dynamic. Provider state and setup scratch storage are reported explicitly; actual worker-stack bytes are not relabeled to hide source storage.

## Results

The final optimized suite passes **142/142 tests**; the independent full ASan/UBSan suite passes **139/139**. Independent tests protect prefilled payload pages read-only and successfully reuse them, check every initialized block and literal canonical IQ bytes, reject unsupported inputs, and compare public Runtime packet bytes/timestamps/ordinals across all three modes. See [independent verification](P13-payload-verification.md) and [implementation details](P13-payload-comparison-implementation.md).

Each mode ran once for60seconds, sequentially in the order below, on the same frozen optimized binary. No build, sanitizer or analysis jobs ran during measurement. All runs used the same peer-side1024-byte canonical-content check, which was added for this comparison; earlier runs without that check are historical context, not an identical experimental control.

| Mode | Skipped packets per stream | Peer missing samples per stream /60M | p99 validation | p99 receive-to-recorded |
|---|---:|---:|---:|---:|
| generated | 68 | 17,472 (0.029120%) | 0.018917ms | 1.293542ms |
| precomputed-copy | 35 | 8,960 (0.014933%) | 0.020708ms | 1.378625ms |
| prefilled-pool | 33 | 8,448 (0.014080%) | 0.019292ms | 1.288042ms |

Every stream within each run has the same listed counts. Generated whole-packet skip accounting is17,408samples; the additional64samples in clipped peer missing coverage are a measurement-window boundary discrepancy and are not labeled another complete skipped packet. The raw summary and independent audit retain that distinction.

All three captures are valid, and all6064offered commands per run have complete trace and V/X/S responses. All p99 Control targets pass, all peer payload checks pass, all four sources remain running through the measurement window, and no trace/peer-ring overflow is reported. The analyzer returns **1 (valid measured failure)** for every normal run because Data no-drop requirements remain unmet.

| Payload accounting, total warmup + measurement + drain | generated | precomputed-copy | prefilled-pool |
|---|---:|---:|---:|
| Provider calls | 1,015,328 | 1,015,472 | 1,015,488 |
| Payload bytes written during production | 1,039,695,872 | 1,039,843,328 | 0 |
| Payload-copy calls during production | 0 | 1,015,472 | 0 |
| Payload blocks initialized before execution | 0 | 0 | 8192 |
| Observed critical C / C++ allocations | 0 / 0 | 0 / 0 | 0 / 0 |

Zero initialized blocks in generated/copy mode refers to pool prefill; every mode prepares its canonical comparison template during setup. Prefilled mode leases all8192blocks together before initialization, avoiding repeatedly filling just one returned block. Setup lease storage accounts for262,144bytes and is released before timing. No per-packet backing allocation or deletion was introduced in any mode.

Framework ledger charge remains55,305,480bytes. The application-owned source provider occupies66,608bytes in every mode, reported separately from the peer object and actual3MiB worker stacks. Framework plus provider is conservatively checked against64MiB and totals55,372,088bytes. Prefill's maximum resident size is41,779,200bytes, versus about25MB for generated/copy: touching the entire fixed payload pool makes its pages resident; this is not evidence of per-packet allocation.

The comparison demonstrates functioning copy and no-payload-write reuse modes. The two fixed modes had fewer observed skips in these trials, but one sequential trial per mode is not statistical evidence for a stable speedup or exclusive cause. Remaining loss with no sample generation or payload copying rules out those operations as the sole prerequisite for loss in this benchmark. Pool acquisition scans, pacing, other Runtime work and host scheduling remain in the measured path; this experiment does not isolate them or change their policies.

## Reproduction and evidence

Build with `cmake --preset udp-release` and `cmake --build --preset udp-release`. Run each command separately, with no concurrent compilation or measurement workload:

```sh
build/udp-release/bench/vita_benchmark --mode normal --payload-mode generated --duration-seconds 60 --warmup-seconds 5 --burst-every-seconds 30 --output-dir artifacts/P13/payload-comparison/generated
build/udp-release/bench/vita_benchmark --mode normal --payload-mode precomputed-copy --duration-seconds 60 --warmup-seconds 5 --burst-every-seconds 30 --output-dir artifacts/P13/payload-comparison/precomputed-copy
build/udp-release/bench/vita_benchmark --mode normal --payload-mode prefilled-pool --duration-seconds 60 --warmup-seconds 5 --burst-every-seconds 30 --output-dir artifacts/P13/payload-comparison/prefilled-pool
```

Use a fresh output directory for subsequent trials to preserve this evidence. Analyze each run with `python3 bench/analyze.py RUN_DIRECTORY --minimum-duration-seconds 60`. The explicit minimum labels the60-second diagnostic and does not satisfy1800-second qualification. `components` currently supports only the generated mode; unsupported combinations and payload-mode names are rejected.

The270-file source manifest has SHA-256 `b400b6a6ce06a2eb14f69a8755d8ca15f54710d40c41d497e923b4cc3435b00a`. Source and both measured binary hashes match after all runs. [Tracked test log](artifacts/P13-payload/LastTest.log), [source manifest](artifacts/P13-payload/source.sha256), and [compact comparison](artifacts/P13-payload/comparison.json) accompany this report. Complete raw traces, peer records, summaries, command lines, host/build provenance, process resource logs and hashes remain under [the local artifact directory](../../artifacts/P13/payload-comparison).

