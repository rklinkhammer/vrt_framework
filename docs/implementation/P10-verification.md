# P10 independent verification

Status: **PASS for the local deterministic P10 package gate.** P11 lifecycle/recovery and final M3 integration remain separate gates.

## Candidate and scope

Independent verifier owns only tests and this report. Production changes were made by the implementers after reproducible findings. Baseline Git revision is `8435ab71d8004e9014a37a63d0f4576ea533252b`; the candidate is an uncommitted workspace, identified by the complete source manifest below. Local evidence uses macOS arm64, Apple clang 21/libc++, C++23 without exceptions or RTTI. Linux GCC 14/Clang 19 qualification remains unavailable and unclaimed.

The frozen manifest covers every public header, P10 developer/independent tests, examples, CMakeLists and presets. Manifest SHA-256: `cbe60d52687c47b7479df94d383bab04d24d55b24d616c5a27e26f4837e66773`.

## Independent oracles

| Executable | Evidence |
|---|---|
| `p10_verify_source` | Literal independent 16-phase IQ16/IQ32/IEEE float32 values; integer ties-even/saturation; finite float/subnormal/signed-zero behavior; nonfinite and overflow rejection before pair mutation; sample coverage and ordinal bounds; default float output under changed rounding modes. |
| `p10_verify_credit` | Real leases, completion tickets and Loopback: supplied completion credit must belong to the same AdmissionPool and contain exactly one completion credit; synchronous rejection retains ownership; accepted work does not double charge. |
| `p10_verify_packetization` | Complete IQ pairs, rate-dependent packet size, IPv4/IPv6 overhead, trailer overhead and MTU boundary literals. |
| `p10_verify_pacing` | First-sample due time; repeated progress does not duplicate Data; bounded catch-up advances ordinal/phase exactly; stop/resume waits for a fresh Context-covered boundary; one-second stall remains bounded. |
| `p10_verify_mapping` | Protocol remapping preserves monotonic sample progress; tiny nonoverlapping negative correction succeeds; actual overlap and equality to the last occupied sample fault; numeric query truth remains available. |
| `p10_verify_ownership` | Foreign runtime/stream transaction handles cannot observe, wait, cancel or release another owner's work; invalid stream registration and unsafe provider aliases leave budget and registration usable. |
| `p10_verify_allocation` | Too-small runtime/lab limits reject before oversized allocation; failed stream registration is transactional; safe aliases counted once; 100 operational Data progress calls allocate nothing after setup. |
| `p10_verify_clock_scenarios` | Exact S5 and S11 through public Runtime, Controller, Engine, Manager and real Loopback, described below. |
| `p10_verify_rate` | Real timed 1 MHz to 2 MHz effect after coarse progress: old-rate ordinal advance, exact packet boundary, literal source phase and next interval use the frozen applicable rate. |
| `p10_verify_provider` | Missing samples, nonfinite input and raw payload corruption cannot emit Data; stopped provider replacement preserves phase; actual IPv6 float/trailer payload sizes and distinct trailer class. |
| `p10_verify_callback` | Callback blocking wait/progress/run_for reject with would_deadlock; asynchronous query submission works; V/X/S observations remain distinct. |
| `p10_verify_reference` | Default sixteen-stream reference pools and actual native startup ledger; seventeenth stream rejection preserves charges; all sixteen streams emit. |
| `p10_verify_wait` | Separate caller wait budget and original deadline; early requested-evidence return; zero-budget checked poll; No-Ack and NACK-only silence never imply confirmation. |

The source oracle is literal data derived independently from exact axis values and high-precision radicals for the 16-point waveform, not a call to the production cosine table or encoder. Public examples compile and run without application protocol loops, manual Acks or buffer-return logic. The combined example records callback evidence and prints after runtime progress, keeping output outside the callback.

## Scenario closure

S5 is exercised in `p10_verify_clock_scenarios`: qualified protocol time 1000 at monotonic zero steps to 2000 at monotonic 10 ms. A genuinely lost query retains its 50 ms monotonic deadline, is not timed out one nanosecond early, and times out at exactly 50 ms. Separately admitted valid unarmed timed rate work receives validation, is revalidated after the mapping step, produces execution failure without confirmation, and leaves the queried rate at 1 MHz. `p10_verify_mapping` additionally proves the same large forward mapping step does not skip 1000 seconds of samples.

S11 is exercised in the same executable: the clock reaches its faulted state after holdover expiry; Data start rejects, a mode-zero query returns known metadata, and a lost query still reaches its exact monotonic deadline with no Data emitted. These are production composition tests, extending the earlier P08 foundation-only evidence.

The overlap rule derives from the accepted requirement to avoid false metadata association: compare against accepted packet sample coverage and published Context events. A negative correction is not intrinsically a reset. Equality to a previously occupied last sample is unsafe; a corrected next sample strictly beyond it remains usable. P11 must verify coordinated fresh-SID recovery; P10 does not claim recovery or graceful shutdown.

## Repaired findings and prerequisite regression scope

Independent and coordinator review found and reproduced incorrect Command identifier-enable bits, foreign-handle aliasing, loss of intermediate phase callbacks, mutable-cursor rate/highwater calculations, inclusive endpoint overlap, protocol-step-driven sample skipping, stale Context coverage on resume, periodic catch-up ordering, unsafe Data/control provider aliasing, nontransactional setup admission and an untyped wait success. The frozen candidate repairs these cases, and the executable regressions above preserve them.

P10 changes existing P03 provider accounting, P04 admission ownership, P05 accepted completion-credit transfer, P06 AckS/pending-time inspection, P07 retained-terminal inspection and P08 mapping reanchor. The affected P03–P09 tests are rebuilt and rerun alongside P10. Historical package manifests remain historical evidence; this report records the integrated replacement candidate without rewriting prior approvals.

The default reference ledger is **47,831,664 / 67,108,864 bytes**, including **30,998,528 raw pool bytes**. This is checked native storage plus explicit ownership allowances, not RSS, allocator qualification, throughput or latency measurement. Operational allocation instrumentation covers the exercised paths rather than arbitrary application callbacks.

## Commands and results

```sh
cmake --preset dev
cmake --build --preset dev -j 4
ctest --preset dev -R '^(p0[3-9]|p10)_' --output-on-failure
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan -j 4
ctest --preset asan-ubsan -R '^(p0[3-9]|p10)_' --output-on-failure
```

Debug: **76/76 passed**. ASan/UBSan: **76/76 passed**. TSan: **24/24 passed**, including all twenty P10 developer/independent/example targets and four existing pool, ticket, late-callback guard and revision concurrency targets. Public standalone-header compilation: **47/47 headers passed** through the CTest below. All 74 manifest entries were checked unchanged after the gate.

```sh
cmake --preset tsan
cmake --build --preset tsan -j 4
ctest --preset tsan -R '^(p10_|p03_verify_pool|p04_verify_ticket|p07_verify_guard_race|p09_verify_revisions)' --output-on-failure
ctest --preset dev -R '^public_headers_standalone$' --output-on-failure
```

The runtime is serialized; the TSan run supplements its functional checks with the actual concurrent foundation paths. No Linux, real hardware, network transport, latency, throughput or VITA conformance qualification is inferred. The documentation fixture checker is separate arithmetic/specification evidence and is not counted as an implementation test here.

## Frozen manifest

```text
5eec853850675492ba5a5690dc3b371b31d9b44f99ff37c531373c4d0f725737  CMakeLists.txt
e2e705349919ce3574b3797ee2af737df11b1449aa42b22a101943e2fbcd3489  CMakePresets.json
8e3e43982920cf7b42f935950b85e760e5ec1dbb1afc00998273709b716fe9f5  examples/CMakeLists.txt
4a63cfea2086e9543814d0cc565acacf683368c4cdadf52f2156b5969578b086  examples/combined.cpp
8c1aa7f302d8221c7679d02980a2bc6eae1aa973f9935e4be8768dde8cf5164b  examples/controllee.cpp
e99bf34c050c3d2e7648bb09a5d37c3a5ea1829ea87565497b542ce4c02e4a81  examples/controller.cpp
1c8d2be01b8c8714fa74d88fd79d20c64dfe424fbaad3ede2ba6acda62e4b5fe  include/vita/adapters/loopback/loopback.hpp
150fa936fd99a1bc6cefdca27a5625a997efe75e6f9b2a4804367b03232d7000  include/vita/codec/layout.hpp
0b7291cb2658cfe8783133f20f454a4933f0f92da55f424a5d3eae4c8b08553e  include/vita/codec/packet.hpp
9f84b53e42b28ee21303541141a0bb4dafa63101cb1f2c12ae145b4b3c9d5e13  include/vita/codec/prologue.hpp
3db37f96f7f61603aaa6f909959172adaa3c7a583d1fb5f3da021ffbf3f80298  include/vita/codec/samples.hpp
cc2e6266f8802128e1ab4ead5e391ce22f5a342ca54edb63fed39b219253ba0a  include/vita/codec/wire.hpp
ed9510455f242e53a3f0f9316890fdea1de5a4f8cd7f9dfeb41e28bed9d77277  include/vita/core/bytes.hpp
c7090fadba63f24786de0de12c2cda91c0069124db1b7b934949ee88a5c530eb  include/vita/core/capacity_policy.hpp
f0a625379793e9e95fc616e6248899df09fbc401ecbdcfa673c9d8688915ba52  include/vita/core/error.hpp
9add4a941546a64ae088f55985554e74a886221c3501e23d94a1b088b91a2eaf  include/vita/core/fixed_vector.hpp
7d0a8c6896e0eb4d76d233b021cdd34731282ced6882a580e907c58a09636ab9  include/vita/core/version.hpp
606b0db7ee2f0a0cc5f776e8846143f2dd865abf8c9f24cb86a7ee63c311c655  include/vita/fields/arena.hpp
be20b065970cc2d188112ca22dc4922f0850d07a6afd88e6f1c030ed13c5fe2d  include/vita/fields/packet.hpp
2f02a01694f87107b6f69cd06289b6a5cf4715e9d821dd871138d69fd7f6eb65  include/vita/fields/types.hpp
5ede1e74572f0bea379019596176dac7cd31e98beec7b2c09690669a2511eeb1  include/vita/memory/envelope.hpp
9d932caebbe321a6c83b641692677333d0049e8a728e98a0a794b2e634a7e32c  include/vita/memory/memory.hpp
62bc0de8e852809af10007d0ac4e2eb944207d5efc611135273fecb433e6cdb7  include/vita/memory/pool.hpp
587c16a9dab2e2d1b6ac4e17178656bcb5a2f2453dbabd7d729d087e14106197  include/vita/profiles/iq/lab.hpp
613adb3676968a633fefbd3c35f1e5df7faebdfc920b06863c5c94b188a64079  include/vita/profiles/iq/source.hpp
be79298527d02902e84874af72e33d92a117a5b76e2378e18b6a80592087b3a0  include/vita/runtime/budget/loopback.hpp
e7dc8211935e6c102c60ff9145dfe2686c1b13d4ebe9ec517ca92c849b5c5bbb  include/vita/runtime/completion/ticket.hpp
3c1d1469e8fde9cbef9812dc0f1f9b1b01df5e7c491b714a94db39002acb3999  include/vita/runtime/context/budget.hpp
3ede591ebdee7c55531ba249ae85159b47a8e4196892f172b64b024089e207ba  include/vita/runtime/context/publisher.hpp
c042b1460c46401007660ef941a5574535bdd419d699134f5afe1b37f2b8b6a7  include/vita/runtime/context/receiver.hpp
e68c39b0b74b64d5991d8e4b405e81b63405c9e31417d10c629e2dc918897f27  include/vita/runtime/context/revisions.hpp
66cc03f6fb3ab2716c967fa00aaaf343a9170376e6ec56ac1b6b40675fd45be7  include/vita/runtime/execution/admission.hpp
ed33dbd7a3917f296f568aff824e2c2b9c0f086550bb58bacd185a6b498e345a  include/vita/runtime/execution/arena.hpp
8b5279aaf8b40a1a7295b5bf0f7d86b1f847de03bbdce5e644dd268cc0a8e7d7  include/vita/runtime/execution/budget.hpp
354e99cc3c6f17906f5c24bb72409d9da0dff6362373cd5eb783108891631fcf  include/vita/runtime/execution/executor.hpp
8c3cc067511769f582d615978d79e3daa7c3c346bde5ef2cc449631b22887eec  include/vita/runtime/execution/operation.hpp
2485ecec51bad7114e5e70b6a9bb672557d43a6fdf90fe167fe56a1a948b68bf  include/vita/runtime/public/config.hpp
34b1646eccf45f8c7ef3ce34dcf8c38c396a12b012b1c2e10f649249b409e88c  include/vita/runtime/public/runtime.hpp
7772e2d134d9c844f2f0d7dc6497505fb7b6a114158772854c8cc41722da6ad0  include/vita/runtime/state/contracts.hpp
b984fd6a79f6a3b4d3bfd18cc7d3095e63720039207c118013faa442cc77f249  include/vita/runtime/stream/counters.hpp
34c4859e891a6c3a07b2fb16e756c276e7a1c26955a92c692f39ecc20c2e813a  include/vita/runtime/stream/routing.hpp
fa0be1dd3b89c2fdf27cc0d4cf584295e18ac5c9ce1a28821020af7d8c2983e6  include/vita/runtime/timing/clock.hpp
3fe07bad5ee65a63690be0b1d9f99dcc22930eb57cbcc618a9fba81b8980d08e  include/vita/runtime/timing/sample_timeline.hpp
74dc772739b2c188f20bb01d22d45e45833248566fc2043ad3cfaf0d007ff201  include/vita/runtime/timing/scheduling.hpp
3015ec14a5d16ee5dd68d86fdf793d3bb016802e83a842c989160d9ee8dfa285  include/vita/runtime/timing/time.hpp
73b72854d96cf4beddea5b7d94d38ec39f3366325522a55e43ae00b7bf5f6c53  include/vita/runtime/transaction/backend.hpp
4ec208ef16dccb2233a7ec0269eff73dc641d1b63927209ac432fccedf284dda  include/vita/runtime/transaction/cam.hpp
bb9a8f750564195ea2ca16e7eb75d57e41dd26e22a5d4fb48cf48b57dc04f383  include/vita/runtime/transaction/cancellation.hpp
f18ce64ba96a6b17ba87b0543207d0bb421314a4e733e789172603b14a4fd9c6  include/vita/runtime/transaction/controller.hpp
7e90a5b40fffdb04a16a48ee1264e140bcd3e5933585a047e4dd82f38f532086  include/vita/runtime/transaction/engine.hpp
0074e0e681070bda2c948faee55a1d60a1625475fce50f4ba51ff321cc1a7a61  include/vita/runtime/transaction/manager.hpp
4bdf0c2e86e5d677ee5df70a874ea5df8e4c9e29f02004772dc8040c1cf3acae  include/vita/runtime/transaction/outcomes.hpp
74dc1667459ae77663603622b87ed19ea8732e56ff46956344e121eae021da36  include/vita/runtime/transaction/retention.hpp
2cb87d350078a8466bcab315c3e0432fec9f80b5db7881e6a95e4cb54b0131d1  tests/unit/P10/CMakeLists.txt
79f2d9cbc529f8e03f33dd283ed7610d5259f63898022e1a8eadc9929d981e9d  tests/unit/P10/budget.cpp
f3a573e8f1ead7ebf56e6aa9b84399911f564421f238dc806e22fd1fe561f461  tests/unit/P10/lab.cpp
d48c14e69d72bb47a330391744fd67debdcc06f789ad1301dd57d66174a3d91d  tests/unit/P10/runtime.cpp
3edda084d1979f5231ef79a0ba617d2449eafe8d28e5a1e24988186d34c88872  tests/unit/P10/source.cpp
309875a560894bc240396f7f4961efa90ec6671ac3e11122b86865c8df879ae2  tests/verification/P10/CMakeLists.txt
cf4e13668c43340d0f5aedfb4b2fe42e80d090b825d08bb509d54d07de5a13f9  tests/verification/P10/allocation_contract.cpp
4cf4470235d68f54684b4ede9a47119cd664fc86ba3b70ce6ed8a34ebb825a5b  tests/verification/P10/callback_contract.cpp
f44b474c209b72ddcebcf98ea9678b28f549811aee5b85e7c12c12cbccfb281b  tests/verification/P10/canonical_oracle.hpp
08cec085813cb330b2906f32030672ceceabb7f329c55e034f38a546ce8da387  tests/verification/P10/clock_scenarios.cpp
9f36f9b8b2caa22e46fa0af80fa7f433c46b1130c62cf4b985ce113d711eb53f  tests/verification/P10/credit_contract.cpp
b540afa84370bd02a9677817236fdd9684b34af437047184522d3e6abed163e3  tests/verification/P10/mapping_contract.cpp
d78cef33f0fccaa3115ba6d405c3dab80b650111fc30522bd49b986f38c6cb94  tests/verification/P10/ownership_contract.cpp
9e81466fe3da7e599fc3faf5bad126a2aba70d68f689eacbd7c24626b224eee9  tests/verification/P10/pacing_contract.cpp
7c6d0cac58a0755f9eccd1801b0be02fe955550ddfb0628469db09cfaf409d2e  tests/verification/P10/packetization_contract.cpp
4c40856cbeb495a7fde114dd3f5059164152d165f5761f2aa5a514d0b53178cb  tests/verification/P10/provider_contract.cpp
3417b80e5e260b3cebba8cf84855ef1b4335ee4d1fd9ea1ccc3fb5fb691776b9  tests/verification/P10/rate_contract.cpp
510aa0a212bc20c877ad9b3a1716c5a758271f69157515423140764fafc2cc83  tests/verification/P10/reference_contract.cpp
b264057d66c0aac4ac93746632ac9e76150ae50c1435ed87c56f580b26a56042  tests/verification/P10/runtime_fixture.hpp
ec0b738d60cee11cbe978c8427a359401256f0dd7380e0571f970aa8f6955446  tests/verification/P10/source_contract.cpp
aff603dfc46d885f6dce7446ff45194d30b2de20431900adf9204f8042c6d285  tests/verification/P10/wait_contract.cpp
```

## P11 integration addendum

The subsequent independently verified P11 candidate adds setup-owned recovery banks and lifecycle/routing accounting. Its full affected Debug/ASan gate reruns all P10 regressions and examples; the reference executable now measures **51,668,752 / 67,108,864 bytes**, including unchanged **30,998,528 raw pool bytes**, for sixteen streams. The original P10 manifest and 47,831,664-byte measurement above remain historical evidence for that earlier candidate. See [P11 verification](P11-verification.md) for the replacement source manifest, exact commands, lifetime/recovery tests and qualification limits.
