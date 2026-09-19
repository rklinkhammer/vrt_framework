# P11 independent verification

Status: **PASS for the local deterministic P11 package gate.** Final M3 aggregate integration is recorded separately by the coordinator.

Independent ownership: tests/verification/P11 and this report. Main implementer owns Runtime/lifecycle/banks/routes; contracts implementer owns explicit backend/Engine/Manager quiescence and retained-association queries. The P10 frozen approval remains recorded separately.

## Required independent cases

| Area | Required oracle |
|---|---|
| S14 fresh association | Begin SID1 with unknown required metadata; confirmed state plus fresh SID4 and explicit peer readiness; old effects quiesce; new qualified full Context precedes Data; old outcomes remain under old keys and old history does not apply. |
| S15 rejection | Same-SID recovery rejects and leaves Data stopped; missing readiness, unknown/invalid confirmed state and failed backend reinitialization also cannot restart Data. |
| Physical safety | Zero callback holders and synthetic completion do not imply backend or transport quiescence; writing tickets cannot be stolen; no reset until every explicit safety predicate passes. |
| Generation banks | Repeated safe recovery can reuse preallocated banks; held old capabilities/retained transactions pin banks and yield bounded capacity failure; no hot allocation fallback. |
| Old generation | Late old completion cannot mutate new numeric state or emit new-generation effects; old exact retry replays retained old results; a new command on an old identity cannot execute. |
| Independent stream | Recovering/stopping one stream does not close the shared transport or unrelated command admission. |
| Application lifetime | Retained payload and immutable metadata outlive detach, recovery and Runtime destruction; no manual forced return. |
| Graceful shutdown | Original monotonic deadline expires at exactly 2s; distinguish completed logical work from outstanding physical I/O; timeout quarantines rather than returns storage. |
| Fresh publication | New SID counter domain and current full Context before first resumed Data; rejected Context keeps gate closed. |
| Wait semantics | Preserve P10 typed evidence/budget/deadline behavior, early terminal return, No-Ack/NACK silence and same-domain deadlock rejection. |
| Budget | Actual second-bank/routes/lifecycle storage charged before allocation; failed registration/recovery leaves accounting consistent; no allocation during recovery. |

S14/S15 require the public Runtime composition, supplemented by direct Engine/Manager and explicit backend test doubles. Passing only a boolean state helper is insufficient. The exact frozen manifest and local results appear below. Linux/deployment/performance qualification remains a separate gate.

## Independent regressions

- `p11_verify_engine`: missing physical evidence; held callback capability after logical release; unavailable backend proof; invalid reset rollback; preserved generations; synthetic completion while backend still pending.
- `p11_verify_manager`: retained original outcomes; closed exact replay; changed/new old-key requests and new cancellation meaning reject; generation-qualified reset; stale lifecycle request cannot close replacement admission.
- `p11_verify_recovery`: same SID, missing readiness, unknown and malformed state; explicit pending/failed/successful device reinitialization with initially unavailable backend proof.
- `p11_verify_scenarios`: production unknown-effect S14/S15, fresh SID4 and known full Context-associated Data, historical original unknown outcome remains unconfirmed.
- `p11_verify_banks`: six safe preallocated bank reuses and a held old Controller transaction pin surviving 31s until explicit release.
- `p11_verify_continuity`: absolute sample ordinal and literal oscillator phase preserved across recovery at an 11-sample packet boundary.
- `p11_verify_shutdown`: exact 2s graceful deadline, immediate quarantine, held failed transport completion, unaffected stream progress, explicit physical proof updates live outstanding accounting without rewriting delivered completion.
- `p11_verify_leases`: retained payload and immutable metadata survive fresh association and Runtime destruction.
- `p11_verify_allocation`: ordinary and aligned allocations instrumented around successful recovery, pinned-bank rejection and shutdown; no post-setup allocation.
- `p11_verify_late`: old asynchronous backend result capability survives bank switch and Runtime destruction; contradictory late execution loses publication and cannot mutate new known state.
- `p11_verify_timed`: unstarted timed original command quiesces without backend execution; resulting wire AckX has AckT7, per-field timing diagnostic and no invented actual timestamp.

Preliminary review found a stale-generation Manager quiesce request mutating admission before validation, recovery resetting oscillator ordinal/phase, reinitialization unreachable until preexisting quiescence, and stale outstanding-I/O lifecycle status after explicit proof. Owners repaired these; the independent cases preserve regressions. The frozen gate reruns these repairs and affected prerequisite packages together.

## Frozen candidate and reproducibility

The candidate is an uncommitted workspace above baseline Git revision `8435ab71d8004e9014a37a63d0f4576ea533252b`. The independent manifest includes all 47 public headers, P11 developer/independent tests, examples and build/preset configuration: 68 files, manifest SHA-256 `dc23b49d0abe40ce4a35f03dc59bb6c41ca66336a1016fb9744fdda48ad6c71b`. Production sources belong to the main and contracts implementers; verifier changes are limited to tests/reports.

```sh
cmake --preset dev
cmake --build --preset dev -j 4
ctest --preset dev -R '^(p0[3-9]|p1[01])_' --output-on-failure
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan -j 4
ctest --preset asan-ubsan -R '^(p0[3-9]|p1[01])_' --output-on-failure
cmake --preset tsan
cmake --build --preset tsan -j 4
ctest --preset tsan -R '^(p11_|p03_verify_pool|p04_verify_ticket|p07_verify_guard_race|p09_verify_revisions|p10_verify_callback|p10_verify_wait)' --output-on-failure
ctest --preset dev -R '^public_headers_standalone$' --output-on-failure
build/dev/tests/verification/P10/p10_verify_reference
```

Debug: **89/89 passed**. ASan/UBSan: **89/89 passed**. Standalone compilation: **47/47 public headers passed**. TSan: **19/19 passed**, including all P11 tests plus actual concurrent pool/ticket/old-callback/revision foundation tests and callback/wait regressions. All 68 manifest files were checked unchanged after the gate. The affected gate includes prior P03–P10 ownership, credit, codec integration, transaction, Context, timing and public-example regressions, plus thirteen P11 developer/independent targets. Earlier package reports retain their original historical manifests.

Independent startup measurement reports `reference raw=30998528 charged=51668752 streams=16`. The final two-bank composition therefore accounts for **51,668,752 / 67,108,864 bytes** on this native ABI. The P10 report's 47,831,664-byte figure remains correct for its earlier single-bank candidate; P11 adds charged setup-owned recovery banks and lifecycle/routing storage. This is a native storage ledger plus declared ownership allowances, not process RSS or allocator/throughput qualification. Operational allocation checks instrument both ordinary and aligned allocation and cover successful recovery, bounded rejection and shutdown.

Local environment: macOS arm64, Apple clang 21/libc++, C++23 with exceptions and RTTI disabled. Linux GCC14/Clang19, real hardware, POSIX UDP, latency/throughput and VITA conformance qualification remain unexecuted and unclaimed. Explicit lab proof models a backend/transport confirmation; it does not establish physical safety for an external hardware adapter. The 32-association lifetime capacity is a documented bounded implementation limit, with fresh-SID exhaustion rejection rather than unbounded storage or same-SID reuse.

## Final manifest

```text
5eec853850675492ba5a5690dc3b371b31d9b44f99ff37c531373c4d0f725737  CMakeLists.txt
e2e705349919ce3574b3797ee2af737df11b1449aa42b22a101943e2fbcd3489  CMakePresets.json
8e3e43982920cf7b42f935950b85e760e5ec1dbb1afc00998273709b716fe9f5  examples/CMakeLists.txt
4a63cfea2086e9543814d0cc565acacf683368c4cdadf52f2156b5969578b086  examples/combined.cpp
8c1aa7f302d8221c7679d02980a2bc6eae1aa973f9935e4be8768dde8cf5164b  examples/controllee.cpp
e99bf34c050c3d2e7648bb09a5d37c3a5ea1829ea87565497b542ce4c02e4a81  examples/controller.cpp
b6337ac6f83a390c3c91174f11afbb2a8761e0878277f813fb1e8257f35d08f6  include/vita/adapters/loopback/loopback.hpp
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
ec35b88f55ceb5035c74428d1b98474cb10e9b37685bf94b5c688e0adcfaf238  include/vita/runtime/public/config.hpp
374e630d2aa1d07231e46cbe8bad1d598d04f17af07c11d895b94fb08a182100  include/vita/runtime/public/runtime.hpp
7772e2d134d9c844f2f0d7dc6497505fb7b6a114158772854c8cc41722da6ad0  include/vita/runtime/state/contracts.hpp
f8c9deabec3ebc4e5be0aa128f80855b5714523090d6f7d1011bd500261ad85f  include/vita/runtime/stream/counters.hpp
c2cf518ca8d271a8f12a722fce0df1bceb510561f90a6100413670b154383a32  include/vita/runtime/stream/routing.hpp
fa0be1dd3b89c2fdf27cc0d4cf584295e18ac5c9ce1a28821020af7d8c2983e6  include/vita/runtime/timing/clock.hpp
3fe07bad5ee65a63690be0b1d9f99dcc22930eb57cbcc618a9fba81b8980d08e  include/vita/runtime/timing/sample_timeline.hpp
74dc772739b2c188f20bb01d22d45e45833248566fc2043ad3cfaf0d007ff201  include/vita/runtime/timing/scheduling.hpp
3015ec14a5d16ee5dd68d86fdf793d3bb016802e83a842c989160d9ee8dfa285  include/vita/runtime/timing/time.hpp
c387bba553de811c0923c3536409be18e70d7ce884b1b6e60c8203060b088bdd  include/vita/runtime/transaction/backend.hpp
4ec208ef16dccb2233a7ec0269eff73dc641d1b63927209ac432fccedf284dda  include/vita/runtime/transaction/cam.hpp
bb9a8f750564195ea2ca16e7eb75d57e41dd26e22a5d4fb48cf48b57dc04f383  include/vita/runtime/transaction/cancellation.hpp
f983c5bcce955f63215a6e8d721386a57e4c9f95ced57ba7487bebf95a7026a9  include/vita/runtime/transaction/controller.hpp
206bd77105ffc197334d685b8ed076c41c470feffe8acbea7ff0872e551ede4b  include/vita/runtime/transaction/engine.hpp
1d33f2cc8aa5efe33461c9235d22e432c63d7b4142a1f31b166fb270ebe76787  include/vita/runtime/transaction/manager.hpp
4bdf0c2e86e5d677ee5df70a874ea5df8e4c9e29f02004772dc8040c1cf3acae  include/vita/runtime/transaction/outcomes.hpp
831b6a40cbbd864057094db440ef33951e06f2344b8f9c16c739fa5fbf97f12e  include/vita/runtime/transaction/retention.hpp
47aa9e2c025a0513775d5135360887508aba4bcbdba5e0ec9526e62c68b17ed5  tests/unit/P11/CMakeLists.txt
851d1702a5192697c6a44128fe2e801c331a399fcef451bf841044e4f4f45e9a  tests/unit/P11/runtime.cpp
31f7214cdb2f2519ca778ca76e0eb6126506cd7d3ca95538a210d21e37db1dee  tests/unit/P11/transactions.cpp
2f5dbc86fb89070b9fec0134e9b80f359f593efc8e7cba4b3aadd825b1479662  tests/verification/P11/CMakeLists.txt
15c85d442f1d48ce7b7fb2076d9829f1aa97ee9110769f3c790b93a047e7a585  tests/verification/P11/allocation_contract.cpp
31e0c9ff33340392fdd88c885aa90e238cfe892bb1d721a43d12a673133726d6  tests/verification/P11/banks_contract.cpp
76f1109830610e89eb1b68d437e773d410820c1392c4eb20f22bc45377ba5f09  tests/verification/P11/continuity_contract.cpp
53d1b59bd621ea68710a3b6da9a21d35b5930fafa43c9560c1e9b400bcd056fe  tests/verification/P11/engine_contract.cpp
515a5109cfb42748e8d7068a558b441e32e72ddec0699be0555d18dacbb76fbe  tests/verification/P11/late_contract.cpp
053eef91ca0a5d1a90b6dae2593ef7d3e8dd56a81ebbde32688ffd0e719d4692  tests/verification/P11/lease_contract.cpp
c5c40f971c7d1657dab5452251299a16357504e1e9fafb3ff385d3e59f199743  tests/verification/P11/manager_contract.cpp
a9b4bf566c179212c76d06b1a9a9440f305c1cdaf3fcb829ccb828523f627d5b  tests/verification/P11/recovery_contract.cpp
312b2cc9fd8f58c70e77f176ee7caa5a0f02f85833454ece3242e4aafda5514a  tests/verification/P11/scenarios.cpp
0f2d3438917b3d84391ff11fb57c8f23b5c8ce73d31823a7b5ab85552336154c  tests/verification/P11/shutdown_contract.cpp
ae7a46a9991bfd0759e94fc8f57d6a824a547fe7a98c2c89da10f34a9c2613e8  tests/verification/P11/timed_contract.cpp
```
