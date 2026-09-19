# P13 independent measurement audit

**Disposition: qualification not achieved.** The 30-minute normal run and 60-second overload run completed, but both overwrite pending controller correlation records before terminal cleanup. Neither is certified as a valid complete qualification capture by the frozen analyzer. Independently recoverable command traces also exceed the latency targets, and Data delivery falls below the configured rates. These findings do not invalidate the earlier functional harness/sanitizer gate; they prevent a P13/M4 acceptance claim.

No production files were changed in this audit. Raw CSV rows were independently parsed using `tests/verification/P13/verify_run.py`, without importing the production analyzer. Nearest-rank percentiles were recomputed from matching full-identity command rows. Additional streaming checks reconciled peer phases, actual measurement windows, charged budget rows and source/binary hashes.

## Provenance and integrity

The captured host is an Apple M4 Max, 16 CPUs, 64 GiB RAM, macOS 27.0 build26A428, Apple Clang21/libc++. Captured Git HEAD is `dd1597e0a439bf180f41711b2725b44fd81fe0bd`; the uncommitted candidate is identified by the source manifest, not Git HEAD alone. All259 current source entries match `normal/source.sha256`, whose SHA-256 is `d3bdecc07fef2430b7fa652bc3882f37ac7ac49ac35135bddec44dbf6eddd496`. Both recorded binaries also match:

- Runner: `b46307d4ec6dcfb9f01ba892bf73a8d91599eb271ac90708b704e15fb1e8c387`.
- Allocation interposition library: `a969b7d8c171c1a665c1c8817cdec10e2dd5355c67b476c17375347bb17a30c1`.

Actual monotonic measurement intervals are exactly1,800 seconds and60 seconds, after5 seconds warm-up. The native peer and Runtime use localhost IPv4, three socket lanes, MTU1500, checked no-fragment behavior, observed262,144-byte send/receive socket buffers, and kernel-copy sendmsg/recvmsg. Affinity is not pinned. Protocol timestamps use explicitly injected GPS-epoch conditioning; this is not physical GPS/PPS qualification, Linux qualification, 10GbE testing or independent-peer interoperability evidence.

Both captures have zero trace-ring overflow, zero peer-ring overflow, zero capture I/O error, and generated-row counts equal written/raw row counts. This does **not** resolve pending-command correlation overwrite: the peer's bounded512-slot observation store reused entries before all requested evidence arrived. Normal reports3,333 overwrites; overload reports591. Subsequent classification identifies2,786 explicit validation rejections and547 commands with no Ack among normal's3,333 receive-only operations. Thus normal does not prove that an executed completion was miscorrelated: rejected operations left pending tracking behind. Overload has16 successful local completions without captured X/S after slot eviction, a concrete delayed-evidence correlation loss. The frozen analyzer cannot distinguish these lifecycle cases sufficiently to certify a complete capture. The overload analyzer's default1,800-second duration requirement is a separate mismatch: the coordinator reran it with60 seconds and it remains INVALID due to overwrite.

## Raw command reconciliation

| Observation | Normal | 120% overload |
|---|---:|---:|
| Actual ordinary sends | 180,000 | 7,200 |
| Burst sends | 1,856 (29 ×64) | 64 |
| Total offered and received | 181,856 | 7,264 |
| Admitted/terminal successful paired traces | 178,523 | 6,689 |
| Receive-only, no validated/recorded stage | 3,333 | 575 |
| Raw trace rows | 895,948 | 34,020 |
| Raw peer rows | 720,211 | 27,392 |
| AckV rows | 181,309 | 6,782 |
| AckX / AckS rows | 178,523 /178,523 | 6,673 /6,673 |
| Pending observation overwrites | 3,333 | 591 |
| Peer bad-evidence counter | 0 | 80 |

Offered traffic meets100/s and120/s exactly; burst counts agree with the requested periods. Every offered command has a received trace. Trace arithmetic is exact: one receive-only row per incomplete command, five stages per paired successful command. In overload,16 locally recorded operations lack captured X/S evidence; local completion must not be presented as remote confirmation. Receive-only commands cannot be assigned fabricated validation/completion times or silently removed from the offered denominator.

The peer's `success=false` for V/S rows is intentional observer semantics: neither phase confirms execution. It is not independently an execution failure. Raw Ack CAM and phase must be considered. No captured peer row lies after the measurement end; the available artifacts supply no per-command timeout deadline, so missing evidence cannot be labeled a proven transaction timeout.

## Independently paired latency

These statistics describe only the recorded successful subset. The incomplete population and correlation losses above prevent population-wide qualification; the observed values are nevertheless sufficient to demonstrate that the selected targets were not met.

| Paired interval | Normal p99 | Overload p99 |
|---|---:|---:|
| Received → validated | 6,498,750 ns | 8,375,125 ns |
| Received → recorded | 918,064,708 ns | 2,829,354,834 ns |
| Received → first dispatch | 918,056,958 ns | 2,829,345,125 ns |
| First dispatch → last device done | 458 ns | 416 ns |
| Last device done → recorded | 11,042 ns | 11,042 ns |

Normal maxima are10,439,375 ns validation and1,328,564,209 ns receive-to-recorded. Overload maxima are8,798,583 ns and4,921,996,250 ns. Normal p99 exceeds the1 ms validation and2 ms recording targets. Most observed tail latency is before dispatch; this locates the interval but does not, by itself, establish a production root cause. No independently calculated percentile was subtracted from another percentile.

## Data load, boundedness and allocation

Normal configures four1MS/s IQ16 streams with256 pairs/packet. Each stream accounts for exactly1,800,000,000 expected samples:1,425,040,128 measured received samples plus374,959,872 skipped/missing samples. The missing fraction is **20.831104%** per stream. Each accepted measured packet count is5,566,563; total native peer packet counts equal accepted totals, with no reported malformed or overlapping Data. The loss is already visible in source skip accounting, rather than inferred only from modulo16 Packet Count.

Overload configures four1.2MS/s streams,72,000,000 expected samples each. SIDs3/4 reconcile46,083,584 received plus25,916,416 skipped samples, a35.995022% deficit. SIDs1/2 each receive23,030,144 samples, leaving48,969,856 missing (68.013689%). Their accepted-plus-skip accounting is only36,892,032 and39,913,856 samples respectively: **the source accounting does not reconcile the full configured interval for those streams**. The supplied metrics cannot attribute that remaining deficit. Bounded queues and absence of allocation growth do not establish satisfactory overload progress or complete loss accounting.

Both runs charge55,272,696 framework bytes; independently summing category rows matches the reported total. Reservations sum exactly67,108,864 bytes, and each charge fits its reservation. Headroom is11,836,168 bytes. The ledger includes491,840-byte trace storage,3 MiB explicitly reserved worker stacks, pool metadata and runtime storage. Kernel buffers and the15,912-byte native peer application structure are separately disclosed. This is accounted capacity, not an independently measured process RSS high-water mark.

Both actual critical-thread C and C++ allocation counters remain zero, with Apple C interposition coverage reported available. Earlier deliberate injection tests proved detection of each advertised form. This claim covers instrumented Runtime/native-peer critical sections; it excludes setup and the CSV writer and does not claim every process allocation source is interposed. Runtime/peer fatal error counters remain zero, and normal adapter totals show all23,529,011 accepted sends locally completed without socket errors. Those boundedness observations do not erase incomplete operations or establish successful delivery.

## Component measurements

`components/components.json` records aggregate elapsed times, not percentile latency. For100,000 iterations: checked encode2,088,708 ns; decode8,025,083 ns; source256-pair filling92,973,000 ns; lease acquire/return4,714,417 ns; clock-plus-ring push/pop1,870,833 ns. Gathered native UDP round trips take115,659,292 ns for10,000 iterations. Checksum is223,370,021 and instrumented C/C++ allocations are zero. These results preserve the operation counts and overhead measurement; they cannot be subtracted from command p99 or treated as physical device performance.

## Artifact hashes

| Artifact | SHA-256 |
|---|---|
| normal/summary.json | `f9593fa55d641b8e3ffc97de9d742e50f45135b6a1472808710f9cf2c1d5f215` |
| normal/trace.csv | `a0679b513b982e9d1a7a655edd044c889ed439929e043961065bd6f26668a9a8` |
| normal/peer.csv | `2c5ee2856b8bc19dc74860ff72d6e6b0f188baaf4b639c4358d1e530fccdcfc8` |
| overload/summary.json | `e9dc93bf07f07bc03b07d9fbdcb1688cb028bf8ac5aaa793c5427fe1d42c97be` |
| overload/trace.csv | `32643a97098b2a9c438aa307e9ed71a3a70a6fcec5399ae88864318ead2f083f` |
| overload/peer.csv | `020e40078790c10cacedf746193cda37f088ab9e3050639eb08023717b7c3676` |
| components/components.json | `48b1263a0e42b79eb18e1865c80b9fb8b7a1405276e973ee857c56962e277d88` |

Independent recomputation outputs are `/tmp/p13-normal-independent.json` and `/tmp/p13-overload-independent.json`. Commands used: `python3 tests/verification/P13/verify_run.py artifacts/P13/normal --output /tmp/p13-normal-independent.json` and the corresponding overload command. Source/binary manifests and raw artifact hashes were checked directly with Python hashlib; CSV phase counts were separately streamed with csv.DictReader. Existing functional/sanitizer evidence remains in [P13-verification.md](P13-verification.md).
