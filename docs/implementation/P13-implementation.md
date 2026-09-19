# P13 implementation and measurement record

Status: independent functional verification passed; sustained measurements executed but qualification not passed. See [M4 results](M4-integration.md) and [independent measurement audit](P13-measurement-verification.md). Local software measurements are not deployment qualification. The supplied fixture OUI is `0xabcdef`, with an explicitly injected GPS epoch and actual `steady_clock` elapsed pacing; there is no claim of a physical PPS/GPS source or independent VITA peer.

## Runtime and traffic composition

One Runtime owns the reference external pools, shared 4096-entry/8MiB duplicate store, globally bounded admission, UDP adapter and sixteen configured stream slots (two generation banks each). Four IQ sources emit 1MS/s IQ16, 256 complete sample pairs per 1052-byte VRT packet, no trailer, at IPv4 MTU1500. Five generic virtual-register Controllees provide one steady endpoint and four burst endpoints; seven inactive IQ slots retain the full sixteen-stream physical capacity. The generic backend has four native fields, validates the whole request and updates its native model before publishing completion.

A separate bounded native-socket traffic-generator peer offers 100 new commands/s to SID101 and an additional 64-command burst every60 seconds to SIDs102–105 (16 each). It avoids treating the public Controller's256 retained records as a sustainable100/s history. Each request changes exactly one Sample Rate model register, uses CAM `0xa91f0000`, short Controller/Controllee IDs2/3, explicit fixture Class ID, and occupies44 bytes. It requests V/X/S and diagnostic detail, permits partial execution, and uses immediate execute with no rounding permission. MIDs are monotonically unique over the bounded maximum run; requests are never automatically retried after accepted native send. EAGAIN before native acceptance retains the same offered command.

The default overload mode raises both IQ rate and steady command rate to120% (four1.2MS/s streams,120commands/s), preserving the same bounded resources and burst. A separate100MS/s stress qualification is not claimed by this runner.

## Timing, scheduling and capture

The pre-decode route hook captures receive time after checked envelope/source/route selection but before semantic body decoding. Engine trace stages capture full validation/admission, dispatch, actual inline device completion publication, and incorporation into the complete transaction record. Full keys include association generation, operation, authorized peer, SID and MID. Completion timestamp publication follows the existing release/acquire result contract. The backend thread never invokes the trace sink.

Inline model results are drained under the existing bounded `Transactions*4+1` service-turn limit, including READY results with no pending device work. This avoids adding one host cycle for every inline field. The ordinary queued IQ backend path is unchanged. Every adapter cycle still limits Data attempts to64 and combined Control/cancellation attempts to32, with separate physical cancellation reserves.

The framework trace ring and native-peer observation ring each hold4096 entries. A separate writer thread drains them to CSV using setup-provided64KiB FILE buffers. Ring overflow, failed writes/flush/close, generated-versus-written row mismatches, and incoherent stage keys invalidate a capture. Raw Ack CAM accompanies the execution-confirmation flag; V/S receipt alone does not imply execution success. The peer continues receiving during a fixed1s drain after the offer window, and unresolved commands remain failures in analysis.

Data loss uses exact timestamp-derived sample ordinals at the fixed configured rate. Unique received sample intervals are clipped to the measurement window, excluding warmup gaps while retaining total counters for diagnosis. Duplicate/overlapping samples and malformed packets are reported separately. Framework skips and native receive coverage are different observations; successful kernel-copy completion does not imply peer delivery.

## Memory and allocation scope

The runner records every final BudgetLedger category's reserved and actual charged bytes. Three explicit1MiB pthread stacks and the complete shared Capture object are charged to the Runtime; the shared trace owner is charged once across all model banks. The peer's separate fixed application object and actual getsockopt socket buffers are reported separately. Kernel buffers, executable mappings, platform thread bookkeeping and libc implementation storage are not mislabeled framework external pool storage. Reference raw pools remain30,998,528bytes. Actual candidate framework charge is about55.24MB, below67,108,864bytes; the frozen run summary is authoritative.

C++ ordinary/aligned new instrumentation is complemented on Apple by a separately linked Mach-O interposition library for malloc, calloc, realloc, posix_memalign and aligned_alloc. Separating the image is necessary to observe executable C allocation calls. A deliberate five-operation probe verifies detection. Both Runtime and native-peer progress threads are marked critical; setup and CSV writer allocation are excluded and explicitly identified. The summary reports whether C coverage is available; non-Apple zero counts are not universal no-allocation evidence. Recovery/setup are covered by earlier package tests, not inferred from this benchmark.

## Running

Build an optimized runner with `cmake --preset udp-release` and `cmake --build --preset udp-release --target vita_benchmark`. `p13_runner_smoke` is a short functional CTest and does not enforce performance thresholds. Long measurement is opt-in:

```
python3 bench/capture_host.py --output artifacts/P13/normal/host.json
build/udp-release/bench/vita_benchmark --mode normal --duration-seconds 1800 --warmup-seconds 5 --burst-every-seconds 60 --output-dir artifacts/P13/normal
python3 bench/analyze.py artifacts/P13/normal
build/udp-release/bench/vita_benchmark --mode overload --duration-seconds 60 --warmup-seconds 5 --burst-every-seconds 30 --output-dir artifacts/P13/overload
build/udp-release/bench/vita_benchmark --mode components --output-dir artifacts/P13/components
```

Component measurements separate checked codec encode/decode, direct256-pair source filling, external lease acquire/final return, gathered native UDP send/receive, and clock-plus-trace-ring overhead. They are total elapsed times over explicitly reported iteration counts, not percentile subtraction or hardware-device performance.

Preliminary short runs completed all264 commands with1320 stage rows and no observed critical C/C++ allocation. They also exposed local packet skips and burst latency above the acceptance target. Those findings are retained as performance failures, not hidden by excluding burst commands. Only frozen sustained artifacts will determine the local acceptance result. Linux execution, physical clock conditioning, configured production identity, independent-peer interoperability and100MS/s stress qualification remain distinct unavailable/unperformed deployment evidence.

## Frozen implementer source manifest

```text
a0cc7c85818159450afaacc595673f2cacba00fbda2d3399a11ad98dfa37e0f0  include/vita/runtime/public/runtime.hpp
d8893f4faf929d5ec5d20e59f78a12653169213b202bb38fab1fd9b08f177199  include/vita/runtime/public/config.hpp
2cad32559a39b2cb544e1dc3b79236a691b225ae98d359ddff30c98d928c1eb3  include/vita/runtime/stream/routing.hpp
634fb5490a07ab465c393197a5664cf278a174d54f5d559cb728062c91832840  include/vita/adapters/posix_udp/udp.hpp
e3725c5102f0a7ae0f056fb93fd679acd35dab144c890acbfab62df827088ffc  include/vita/adapters/loopback/loopback.hpp
4ee834c7a75fdc85deef779328ed2bec84ac85a5cad7524a953691e59dd02790  bench/main.cpp
398e4433e82e2abae74b49086eff3fe46b0aaf03584d509b52b5fa2955e85c83  bench/capture.hpp
b643807d55a049d398775854247e449fb24ec3d304d4b45dcc3b4d95aa71290d  bench/allocation.hpp
250e0678af3d37d1aa0c2f01d7d9d684b77df6305f03360dd6933aa7469812f2  bench/allocation.cpp
f4997db1c994d304a71a3d0f886ac3d6972a594422ed465fd7c66a0ac7553489  bench/CMakeLists.txt
0740b63603c753a396978828915ee87ac9bf22788dbeeb570f33779179b234b7  tests/unit/P13/CMakeLists.txt
587f2509900526423f9905104c0c94df75a142cb5f9a90d703262232e853208e  tests/unit/P13/allocation_probe.cpp
```

## Revised candidate checkpoint

The [retention](P13-retention-remediation.md), [capture](P13-capture-remediation.md) and [Runtime pressure](P13-runtime-remediation.md) fixes supersede the original candidate details where noted. Peer capture now logs checked responses without a fixed pending-slot table and the analyzer performs bounded offline correlation, retaining duplicate/conflict distinctions. The final charge is55,305,480bytes. [Revised measurements](M4-integration.md) pass latency and overload checks but normal Data still has measured skips; P13 qualification remains open.
