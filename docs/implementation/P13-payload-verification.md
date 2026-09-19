# P13 payload-mode comparison verification

Status: independent candidate gate PASS; three sequential captures independently audited. All remain valid failed normal-load measurements because Data drops persist.

Independent gates will compare the existing generated source, a precomputed256-pair copy source, and a pool initialized before streaming. Payload bytes must match the independent canonical IQ16 oracle, while ordinal/timestamp progression, complete pairs, packet framing, lease ownership and bounded resources remain unchanged. The prefilled mode must reject incompatible format/count/phase and must initialize every physical payload block before use.

No-hot-write evidence will use read-only protected caller-backed payload pages after initialization, followed by repeated production and lease reuse. Allocation evidence remains separate from payload-write evidence. All modes must retain the same256-pair load and clearly identify setup costs versus per-packet work; performance measurements remain sequential and free of concurrent verifier builds.

## Frozen candidate gate

**PASS for isolated comparison measurement readiness.** The eight-file verifier manifest `/tmp/p13-payload-verifier-manifest.txt` has SHA-256 `d7c137ea5fc980444cbb2ceaf6cd9a163e3aae7524b366c3199ae70e53d27600`. Production source hashes match the implementer freeze; no production repairs were made by the verifier.

`p13_verify_payload` independently checks all512 IQ16 scalar words in every256-pair packet against the pre-existing literal canonical oracle. Generated and precomputed-copy modes produce identical1024-byte payloads, preserve bytes beyond the window, and report the expected write/copy counts. Unsupported mode, format, count and phase reject. Initialization with an already-held pool block fails without claiming partial initialization and succeeds after that lease is released. All four distinct caller-backed blocks are checked after initialization.

The verifier then changes all payload pages to **read-only with mprotect**. Forty prefilled production calls span all four blocks over ten acquire/release cycles and pass without a protection fault; canonical bytes remain unchanged and producer write/copy counters remain zero. This proves the callback does not secretly memcpy or write payload bytes in the tested prefilled path, independently of its counters. Foreign buffers reject. The source's coverage bookkeeping is in the separate writable window object. A nonfinite float payload fails `complete_from_wire()` without marking coverage complete; valid externally written float bytes subsequently pass.

`p13_verify_payload_runtime` uses the public Runtime with each mode and eight consecutive256-pair packets. All three modes deliver the same complete canonical payload, known metadata and sample timestamps spaced256 microseconds apart. Absolute ordinal ends at2048, emitted sample count is2048, skipped count is zero, and source status remains running. This exercises the normal lease, framing, Context and receive paths rather than bypassing the Runtime for the comparison.

Commands:

```
cmake --preset udp-asan-ubsan
cmake --build --preset udp-asan-ubsan --target p13_verify_payload p13_verify_payload_runtime
ctest --test-dir build/udp-asan-ubsan -R '^p13_verify_payload' --output-on-failure
cmake --build --preset udp-asan-ubsan
ctest --test-dir build/udp-asan-ubsan --output-on-failure
```

The two new independent tests pass; the full affected ASan/UBSan gate passes **139/139**, including earlier sample conversion, no-allocation, ownership, lifecycle and timing checks. Logs: `/tmp/p13-payload-asan-build.log` and `/tmp/p13-payload-asan-tests.log`. No new shared-state concurrency mechanism was introduced, so another broad TSan run was not used as a substitute for the relevant protected-memory and byte-identity tests.

The comparison is deliberately limited to IQ16,256 pairs and phase0 modulo16. Prefill initializes the first1024 bytes of every registered payload block; unused block capacity is not part of the transmitted payload. The source object and pool must remain alive, with no competing producer mutating those blocks. Source callback state and temporary setup lease storage are application-owned costs reported separately from the framework ledger. Measured runs must verify zero critical C/C++ allocations, full peer payload comparison, all8192 reference blocks initialized in prefilled mode, and mode-specific producer write/copy counts. This gate alone makes no throughput or qualification claim.

## Independent audit of the three60-second runs

**All three captures are valid; all three fail only the normal no-drop Data target.** Every run offers6,000 ordinary commands plus one64-command burst; all6,064 commands have complete received/validated/dispatch/done/recorded traces and matching AckV/AckX/AckS observations. Independently recomputed paired statistics match the analyzer. No missing correlations, trace overflow, peer capture overflow, malformed payloads, overlapping samples or critical C/C++ allocations were reported. Every source remains running at measurement end and stops normally afterward.

All270 source files and both recorded binaries match the comparison manifest. Source-manifest SHA-256: `b400b6a6ce06a2eb14f69a8755d8ca15f54710d40c41d497e923b4cc3435b00a`. Binary-manifest SHA-256: `350540e8640542389eefb177ce98f7bcb0ade7148802455f5e8e6de73c69ed7e`. The runs are sequential single trials on the same local host, with256-pair packets and the same full1024-byte canonical peer comparison. They do not establish causation, statistical significance or sustained qualification.

| Independent result | Generated | Precomputed copy | Prefilled pool |
|---|---:|---:|---:|
| p99 validation | 18,917ns | 20,708ns | 19,292ns |
| p99 receive-to-recorded | 1,293,542ns | 1,378,625ns | 1,288,042ns |
| Skipped packets per stream | 68 | 35 | 33 |
| Source skipped samples per stream | 17,408 | 8,960 | 8,448 |
| Peer missing samples per stream | 17,472 | 8,960 | 8,448 |
| Total producer calls, including warm-up/drain | 1,015,328 | 1,015,472 | 1,015,488 |
| Operational payload bytes written | 1,039,695,872 | 1,039,843,328 | 0 |
| Operational payload copy calls | 0 | 1,015,472 | 0 |
| Prefilled physical blocks | 0 | 0 | 8,192 |

Producer calls exactly equal the sum of accepted total packets across four streams. Generated/copy write bytes equal calls ×1024; copy mode makes exactly one payload copy per call. Prefilled mode initializes every8,192 reference block before streaming and makes1,015,488 calls with zero payload writes or copies. This agrees with the independent read-only-page proof above, not merely self-reported counters. Prefilled still performs membership checks, coverage bookkeeping and normal Runtime validation; zero payload writes does not mean zero CPU work.

The generated run's64-sample difference between source skip and peer missing counters is a measurement-boundary effect consistent with their different definitions. Its234,306 accepted measured packets account for59,982,336 samples, while clipped peer coverage is59,982,528 (192 more). Accepted samples plus skipped samples are256 below60,000,000. At the5-second warm-up boundary, ordinal5,000,000 is64 samples into a256-sample packet, leaving192 samples inside the measured interval. Thus the observed192/256/64 differences are consistent with clipping and packet-counter snapshots. No per-Data-packet trace is available to independently assign each endpoint; the report preserves both counters rather than hiding the difference. Copy and prefilled counters reconcile exactly.

The copy and prefilled trials observed fewer missing samples than generated, while all still miss the no-drop target. The two-packet difference between copy and prefilled is particularly insufficient to infer a reliable throughput advantage from one trial. Their command latency tails are similar and all pass the specified p99 thresholds in these60-second measurements.

Every mode accounts for55,305,480 framework bytes plus66,608 application-owned source bytes: **55,372,088 combined**, leaving **11,736,776 bytes** below64 MiB. Category charges and the combined total were independently summed. Prefill additionally uses262,144 temporary setup bytes for simultaneous leases; that storage is released before measured operation. All modes retain the same actual thread-stack accounting and disclose source state separately. The payload initialization/copy experiments do not claim an allocation-free setup phase.

Independent recomputation files are `/tmp/p13-payload-{generated,precomputed-copy,prefilled-pool}-independent.json`; the existing independent raw oracle was imported without using the production analyzer. Peer CSV phase counts and producer/budget arithmetic were separately checked. All artifacts reside under `artifacts/P13/payload-comparison/`.

| Raw artifact | SHA-256 |
|---|---|
| generated summary | `6330c8ccbef3da9d44b1d6cec2cf9784b4684a7ecfb4991299d2c250c02140d5` |
| generated trace | `e704a6fd520271425f417deddab92b06659e28f3bc048c3bf30c64d3fd6aa7b2` |
| generated peer | `7b340483d02b762d2a8a5a0ba9494ca5ab206791e3d3236fd11de0b1b93aa5b4` |
| copy summary | `360f1169bc43ca46642c2b37d724d985d5910ad2b6d27fa4c0d8c0fd426eff38` |
| copy trace | `4b95fa9e9d228e995a2517b6852ab3639bc93d9a9d1dbd91516e3c70e5db260c` |
| copy peer | `6970a442d50170c2c3a27ad27aec6af5809e012b83b58ffab2c238a5bb7f3964` |
| prefilled summary | `41781ef177bf260f64d87dc8ffb5be4a075dbc0cb20ddf5d6c4efd98111c9294` |
| prefilled trace | `feae4423c004837693822a585207ad9e90e8aeed5d092770f21746d12e1305a6` |
| prefilled peer | `6b6163359afe715343ceea104f1d52d9b384a2d56ef5486ee1f8c5818578adcd` |
