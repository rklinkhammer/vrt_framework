# P07 independent verification

Status: **PASS for the frozen local functional gate**, 2026-09-18. P09 prerequisites released. The previously identified cancellation-attempt ambiguity is resolved by user acceptance of [D-P07-1](P07-decision.md); verification enforces that chosen profile, not an inferred extension of the standard.

## Candidate and results

Baseline revision `8435ab71d8004e9014a37a63d0f4576ea533252b` plus the uncommitted manifest below. Aggregate manifest SHA-256: `949a73727248ae46bc791e1bf0191327ebf89a5f5f65fb0ae53997f5dd54f970`. All source/test hashes were checked unchanged after the runs. Apple arm64 clang21/libc++, C++23, no exceptions/RTTI. No Linux target qualification, device timing qualification, or external interoperability/conformance claim.

- Debug: **24/24 P06/P07 tests passed**, comprising 13 P06 regressions, seven independent P07 executables and four P07 developer/integration executables.
- ASan/UBSan: the same **24/24 passed** after rebuilding corresponding targets.
- TSan: **3/3 passed** (`p07_verify_guard_race`, `p07_verify_controller`, `p06_verify_async`). The new guard test runs 100 actual races between old-capability failed publication/destruction and owner-thread new admission; the Controller test is serialized semantic coverage, not a concurrency claim.
- `public_headers_standalone`: **1/1 passed**, compiling all 39 public headers separately with C++23 and exceptions/RTTI disabled.

## Independent tests

| Test | Evidence |
|---|---|
| `p07_verify_retention` | One L=0 original and one immutable L=1 cancellation share a transaction entry; count-only retries attach/replay; changed subset/CAM/timestamp conflicts; original AckS remains unchanged; latest cancellation terminal extends both records to 30 seconds; references prevent expiry; exact boundary expiry, byte-before-count exhaustion, count exhaustion, no active eviction and stale-token rejection. |
| `p07_verify_keys` | Full binding/peer/session generation, SID presence/value, Controller/Controllee identifier presence/form, every UUID word and MID distinguish keys. Invalid wire/key mismatch rejects; local namespaces remain distinct in server retention. |
| `p07_verify_manager` | Production manager joins canonical admission to Engine effects. Exact S3 cancellation9ms before10ms commit, S4 cancellation11ms after10ms commit, S10 A executed/B reversible; original/cancellation AckP/SchX and L remain distinct. Full ordinary queue does not block cancellation; repeats replay immutable outcomes; changed meaning never produces new effects. Contradictory late completion after successful cancellation and release faults the affected state before a new admission, even if the last callback handle was destroyed before progress. |
| `p07_verify_controller` | Last MID allocation and wrap blocking; no restart by changing local generation alone; immutable cancellation retry cannot renew deadline; full-key/class/phase/field checks; ordinary and cancellation confirmations remain distinct; first ordinary/cancel AckS observations are separately preserved; delayed conflicting state rejects; late receive records actual deadline timeout without requiring a prior advance call; duplicates do not renew retention. |
| `p07_verify_guard_race` | 100 concurrent last-holder destruction / late executed-result publication races against owner admission. No cancelled result becomes execution success, a different field remains unchanged, matching affected state becomes unknown, and no newly queued write starts after the contradiction is observed. |
| `p07_verify_timed_cancel` | Reject too-short preparation lead, beyond-horizon requests, unqualified capabilities and excessive uncertainty with full reservation rollback. Missed windows, mapping changes and clock faults yield timing7/no-cancellation while original backend work remains able to complete. |
| `p07_verify_admission` | Cancellation queue/response-credit exhaustion and physical retention-byte exhaustion reject before disarm or cancellation effects, preserving pending backend work and rolling additional resources back. |

The developer `p07_loopback` integration was reviewed and rerun as supporting evidence for the leased transport route through the production manager and checked responses. It is not counted among independent test authorship. P06's full CAM/dry-run/ownership regression suite remains passing on the additive P07 engine/backend/outcomes changes.

## Closed findings and policy boundaries

The accepted D-P07-1 policy prevents delayed successful cancellation Acks from being assigned to a different cancellation meaning under the same original MID. Identical retries do not create new attempts or refresh deadlines/results; changed subsets/CAM/timestamps conflict. Retention is shared but response interpretation is L-separated. First cancellation after Engine release is answered from retained original identity rather than a stale Engine handle.

Review and independent regressions closed response ClassID and requested-phase/selected-field correlation gaps, late-receive timeout classification, and duplicate terminal-time renewal. Cancellation reservation precedes disarm. Timed cancellation applies preparation lead, horizon, uncertainty and mapping revalidation. The ResultGuard admission race was reproduced and repaired: a pending contradiction cannot be cleared by recycling the result slot after its last callback handle drops.

Outstanding callback capabilities pin result storage. A contradictory executed callback after successful disarm does not write a recycled result or pretend a timed effect occurred; the current association faults and affected state becomes unknown. Recovery/new association admission remains explicitly outside this P07 API until the P11 lifecycle path; changing only a supplied generation is not recovery.

S3/S4/S10 are covered through the production manager. **S5 and S11 remain pending full M3 integration**: the Controller test demonstrates the exact protocol clock step1000→2000 at monotonic10ms and timeout at50ms, while separate manager tests demonstrate unarmed timing revalidation. They do not yet combine the entire S5 path in one integrated scenario. S11 additionally requires the actual P10 Data-start gate. Neither foundation-only coverage nor the P07 gate closes those remaining scenarios.

## Reproduction

Configure each preset, build all `p06_*` and `p07_*` targets named by their package CMakeLists, then:

```text
ctest --preset dev -R '^p0[67]_' --output-on-failure
ctest --preset asan-ubsan -R '^p0[67]_' --output-on-failure
cmake --build --preset tsan --target p07_verify_guard_race p07_verify_controller p06_verify_async
ctest --preset tsan -R '^(p07_verify_guard_race|p07_verify_controller|p06_verify_async)$' --output-on-failure
ctest --preset dev -R '^public_headers_standalone$' --output-on-failure
```

The exact P07 targets are `p07_transactions`, `p07_timing`, `p07_controller`, `p07_loopback`, `p07_verify_retention`, `p07_verify_keys`, `p07_verify_manager`, `p07_verify_controller`, `p07_verify_guard_race`, `p07_verify_timed_cancel`, `p07_verify_admission`. P06 target list is recorded in its independent report.

## Frozen source/test manifest

```text
b0c9ce305ad571e1ce950aeae0c65682a1c07d66e2d7c6de04b110c8101348ab  include/vita/adapters/loopback/loopback.hpp
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
e46a7af35ff9a7035569295c0f7d7d8e4ab586c09d489b53b65fa644cadae31d  include/vita/memory/envelope.hpp
9d932caebbe321a6c83b641692677333d0049e8a728e98a0a794b2e634a7e32c  include/vita/memory/memory.hpp
863f73a4a74b6db81285f7440f0ad7cd72eadbb38942c41fe6be13c8f37a6d85  include/vita/memory/pool.hpp
be79298527d02902e84874af72e33d92a117a5b76e2378e18b6a80592087b3a0  include/vita/runtime/budget/loopback.hpp
e7dc8211935e6c102c60ff9145dfe2686c1b13d4ebe9ec517ca92c849b5c5bbb  include/vita/runtime/completion/ticket.hpp
4076824209c3e1d9cacc21a99313b5cd88bf326ded79a22be7b4df8c8c3871da  include/vita/runtime/execution/admission.hpp
ed33dbd7a3917f296f568aff824e2c2b9c0f086550bb58bacd185a6b498e345a  include/vita/runtime/execution/arena.hpp
8b5279aaf8b40a1a7295b5bf0f7d86b1f847de03bbdce5e644dd268cc0a8e7d7  include/vita/runtime/execution/budget.hpp
354e99cc3c6f17906f5c24bb72409d9da0dff6362373cd5eb783108891631fcf  include/vita/runtime/execution/executor.hpp
8c3cc067511769f582d615978d79e3daa7c3c346bde5ef2cc449631b22887eec  include/vita/runtime/execution/operation.hpp
7772e2d134d9c844f2f0d7dc6497505fb7b6a114158772854c8cc41722da6ad0  include/vita/runtime/state/contracts.hpp
b984fd6a79f6a3b4d3bfd18cc7d3095e63720039207c118013faa442cc77f249  include/vita/runtime/stream/counters.hpp
34c4859e891a6c3a07b2fb16e756c276e7a1c26955a92c692f39ecc20c2e813a  include/vita/runtime/stream/routing.hpp
fa0be1dd3b89c2fdf27cc0d4cf584295e18ac5c9ce1a28821020af7d8c2983e6  include/vita/runtime/timing/clock.hpp
0ecf817f6b233addaac4edb5d418b2ece1874965f766a3f474888863ce1b2805  include/vita/runtime/timing/sample_timeline.hpp
74dc772739b2c188f20bb01d22d45e45833248566fc2043ad3cfaf0d007ff201  include/vita/runtime/timing/scheduling.hpp
3015ec14a5d16ee5dd68d86fdf793d3bb016802e83a842c989160d9ee8dfa285  include/vita/runtime/timing/time.hpp
73b72854d96cf4beddea5b7d94d38ec39f3366325522a55e43ae00b7bf5f6c53  include/vita/runtime/transaction/backend.hpp
4ec208ef16dccb2233a7ec0269eff73dc641d1b63927209ac432fccedf284dda  include/vita/runtime/transaction/cam.hpp
bb9a8f750564195ea2ca16e7eb75d57e41dd26e22a5d4fb48cf48b57dc04f383  include/vita/runtime/transaction/cancellation.hpp
f18ce64ba96a6b17ba87b0543207d0bb421314a4e733e789172603b14a4fd9c6  include/vita/runtime/transaction/controller.hpp
c8af6facec09dc1cbf0c6713c85ffc128b5a007e4964e20bfb632c3fc4036508  include/vita/runtime/transaction/engine.hpp
c4d5be7b0391d36f7c8f3cc26db80e3e8a1af6086b3c5fe6d0b127d22c4b43b2  include/vita/runtime/transaction/manager.hpp
4bdf0c2e86e5d677ee5df70a874ea5df8e4c9e29f02004772dc8040c1cf3acae  include/vita/runtime/transaction/outcomes.hpp
f9f6580f2548f3084acba77f8f76fac8fa0bc07b1f419607ee805f42f3ff576d  include/vita/runtime/transaction/retention.hpp
e48cc4fe0891784e5e55a6d72d1475a89ff07b1aed5f2381ed3c2fb4f1fbf0a2  tests/integration/P07/CMakeLists.txt
9452465861a19e37aa795b3ec8df025ba6416d359d87e6539bc002cf9a30694a  tests/integration/P07/chain.cpp
247b70b11858ae277adae6010491f0f6232b339105a8e052aae2c348a3fe0100  tests/unit/P07/CMakeLists.txt
985ce272affe07c2d4334c800d449dbe58d4b83d8a0689445d127b2cf9d4d819  tests/unit/P07/controller.cpp
54903a72f05ee15f2714a8bdcba73d27d1723e6ad71273ac4cd6a88e708548db  tests/unit/P07/support.hpp
53288cfd46c272e53d598489ce4ec6597750ac098add587574f45720a8cc9868  tests/unit/P07/timing.cpp
4890277fc4d093994ed8cb6d4a932d3b7ab63ea8c96655375990d77dd2f2182e  tests/unit/P07/transactions.cpp
60902502976db79f2d59d68b0217d7989d7c5e2f667592e49a6a796f2d0a976e  tests/verification/P07/CMakeLists.txt
ab88349b541088f23576ca01fd3668510f25c0b365107ed457608e887b7abfdb  tests/verification/P07/admission_contract.cpp
a7aabe00cfccc0da117d980db3c8d5b96ff08b60956ad6573b0945823742c399  tests/verification/P07/controller_contract.cpp
80462e4e115332c3b199be21ccfa1c6d2c6a647d9bbadf132679ca6ea587a331  tests/verification/P07/guard_race.cpp
c30ab828de8c0175801b32a3fe00f69d1f31bc8c4145befc436eaacc287d71a0  tests/verification/P07/key_contract.cpp
a7c80dca90becfc9e2613592dbbacba1b6d823804de0f5ee4aeb45d7f87dd1b0  tests/verification/P07/manager_contract.cpp
ce31ea996eed883d3f73b08743923d235f4cbfddae632ec9e9079fba90e7f375  tests/verification/P07/packets.hpp
c8e2739629689c77b5b39ecca9169410fa60347c7050d3344b25652bcb188f51  tests/verification/P07/retention_contract.cpp
db53c3221fbcff67f45a7611183ab87097343034f7407b24803dca66379c4b6d  tests/verification/P07/timed_cancel.cpp
```
