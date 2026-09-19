# P04 independent verification

Date: 2026-09-18. Verifier: V-P04, independent from I-P04.

**Verdict: PASS for the local P04 completion, execution, physical admission, and current-type budget gate.** Linux qualification and complete-runtime resource qualification remain pending.

Baseline Git revision `8435ab71d8004e9014a37a63d0f4576ea533252b`; frozen uncommitted source manifest SHA-256 `1d7b3f3d38a5d50a3b2250ca7eb29ca47fe1460ceb435dd4f6bb2a1cb7c6d12c` (sorted `sha256  path\n` records below).

## Requirements, scenarios, and tests

Architecture §§5–5.2 and P04 task card are the oracle. No wire interpretation IDs apply. Tests exercise actual production APIs, with independent expected state transitions:

| Test | Evidence |
|---|---|
| p04_verify_tickets | **S12:** claimed writer remains unconsumable before finish; exactly one coherent result after publication. **S13:** generation 7 producer cannot publish into generation 8 operation. Eight racing producers/consumers yield one publication/consumption; 1,000 concurrent reserve/publish/consume cycles preserve operation/payload correlation. Abandoned token reports failure; abandoned claimed writer stays writing; max-generation slot retires; late token survives arena frontend destruction. |
| p04_verify_admission_executor | Atomic multi-resource rejection; failed admission changes no earlier counters; cancellation credit survives ordinary saturation; retention-credit transfer; bounded executor rejects full queue; completion scan drains independently of full queue; close drains pending work; same-domain and nested ancestor waits reject. |
| p04_verify_quiescence_budget | **S16:** consuming synthetic abandonment never returns active TX storage; explicit matching quiescence returns it. Stale copied proof cannot release rearmed generation. Budget reservation totals 64 MiB, category overflow preserves charges, headroom transfer preserves total, object count multiplication rejects overflow, reference projection includes concrete retention/completion/plan storage. |
| p04_verify_coupled_admission | Real plan arena occupied while logical credits available: reject and rollback. Real completion arena occupied: reject, release previously acquired byte slot and logical credits. Successful operation owns actual bytes/ticket; advertised backing overcapacity and insufficient requested byte credits reject. |

These scenario mappings use the supplied S12/S13/S16 event ordering; the old Python checker remains specification arithmetic evidence only.

## Memory-order and lifetime review

Inspected producer/consumer code against every step of §5.1:

- Reservation is serialized among allocators; acquire observes free, operation metadata is initialized before release-store reserved.
- Producer CAS compares one indivisible generation/state tag and uses acquire-release success; loser touches no payload.
- Winner writes payload before release-store ready. Consumer acquire-CAS claims reading before copying operation/result; only one consumer wins.
- Consumer resets payload before release-store free(next generation); generation maximum retires rather than wraps.
- Publisher/token/writer own shared lifetime handles. A claimed stalled writer cannot be consumed or reused. Quiescence storage has a separate lifetime and generation from transaction outcome.

TSan exercises actual concurrent publication/consumption and contention, but does not prove every weak-memory execution. Queue-independent scan/drain and steady-state reservation contain no allocation calls by inspection; setup allocates shared control state. Full process-wide allocation qualification remains later integration.

## Commands and results

Apple clang 21/libc++, Darwin 27 arm64, CMake 4.4.3; exceptions/RTTI disabled.

```sh
cmake --preset dev
cmake --build --preset dev --target p04_verify_tickets p04_verify_admission_executor p04_verify_quiescence_budget p04_verify_coupled_admission
ctest --preset dev -R '^p04_verify' --output-on-failure
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan --target p04_verify_tickets p04_verify_admission_executor p04_verify_quiescence_budget p04_verify_coupled_admission
ctest --preset asan-ubsan -R '^p04_' --output-on-failure
cmake --preset tsan
cmake --build --preset tsan --target p04_verify_tickets p04_verify_admission_executor p04_verify_quiescence_budget p04_verify_coupled_admission
ctest --preset tsan -R '^p04_verify' --output-on-failure
```

Independent suite passes 4/4 in all modes; ASan also passes developer test (5/5). After adding reference projection assertions, affected budget test was rebuilt and passed in all three modes, with the full four-test Debug suite repeated. Raw ignored logs: build/{dev,asan-ubsan,tsan}/Testing/Temporary/LastTest.log (latest sanitizer logs contain the one-test budget rerun).

## Review findings and bounds

Pre-freeze reviews found rearmable quiescence proof without generation, nested ancestor executor wait omission, and logical-only admission counters. Implementer repaired them; independent regressions cover each.

Current actual-type projection is 36,834,200 bytes on this ABI, with unimplemented transaction/revision/history categories still reserved. This is not complete-runtime feasibility. P06 must request all command resources before effects and hold appropriate credits/storage for their actual lifetime; P05 must integrate completion/transport ownership; later components must update reference projection. Callback Task contexts are borrowed and must outlive invocation; no implicit application-object ownership is promised. Quarantine intentionally remains retained without independent proof and can outlive external guard handles.

Integration revision: same shared-workspace source manifest, no commit created. Relevant future changes require affected gate reruns.

## Frozen source manifest

```text
1a088395deb9683f29ff74c4dda5e0513a1aab743ff9c4f40d355a39458385d1  CMakeLists.txt
ed9510455f242e53a3f0f9316890fdea1de5a4f8cd7f9dfeb41e28bed9d77277  include/vita/core/bytes.hpp
c7090fadba63f24786de0de12c2cda91c0069124db1b7b934949ee88a5c530eb  include/vita/core/capacity_policy.hpp
c76159658b171bdfd1b30eec57297f27e44f2f619721dac358780faefc23ecca  include/vita/core/error.hpp
9add4a941546a64ae088f55985554e74a886221c3501e23d94a1b088b91a2eaf  include/vita/core/fixed_vector.hpp
7d0a8c6896e0eb4d76d233b021cdd34731282ced6882a580e907c58a09636ab9  include/vita/core/version.hpp
e46a7af35ff9a7035569295c0f7d7d8e4ab586c09d489b53b65fa644cadae31d  include/vita/memory/envelope.hpp
9d932caebbe321a6c83b641692677333d0049e8a728e98a0a794b2e634a7e32c  include/vita/memory/memory.hpp
7ca679bb74e765d39696dc94b5da0e7d87e54446d12d2a54a0ab7697484407a8  include/vita/memory/pool.hpp
0f9d62f9db98fb525c43bfaf9c0a9577057d66a70c195ac3efbc3b4c7b45a54c  include/vita/runtime/completion/ticket.hpp
0ba38961e3b7e7a690ef839708d86f56fe4c61ed88354a7919e8f0adb63597f7  include/vita/runtime/execution/admission.hpp
ed33dbd7a3917f296f568aff824e2c2b9c0f086550bb58bacd185a6b498e345a  include/vita/runtime/execution/arena.hpp
8b5279aaf8b40a1a7295b5bf0f7d86b1f847de03bbdce5e644dd268cc0a8e7d7  include/vita/runtime/execution/budget.hpp
354e99cc3c6f17906f5c24bb72409d9da0dff6362373cd5eb783108891631fcf  include/vita/runtime/execution/executor.hpp
8c3cc067511769f582d615978d79e3daa7c3c346bde5ef2cc449631b22887eec  include/vita/runtime/execution/operation.hpp
459c63e5238e57def227560026786a4c250676bb2a245f49d3d1e54b6647df63  tests/unit/P04/CMakeLists.txt
2ea78f236d73e52ca5bb756d1ca4a0c2efa850bab35ab99b65a356658055e90a  tests/unit/P04/runtime.cpp
8eee1976e79bbf5bf7510f8e66d63cd928426a379fe9b8e3120a0950923263eb  tests/verification/P04/CMakeLists.txt
8a31087eeb079fa28d686beff717ce90560d7cfee671c40d6c76e085f1bf7ed5  tests/verification/P04/admission_executor_contract.cpp
4dfa605aed1384edb504f7ca7cd819f923131f07d44876792af0ff06bec06689  tests/verification/P04/coupled_admission_contract.cpp
7b91c2ad58583654ecae055978ff7fc340da416750cbb30e18d557ad629f378d  tests/verification/P04/quiescence_budget_contract.cpp
69137fa15f9f0f6c158f87ba58ec34db245275ec2731d0bf22442f03d3b07c3a  tests/verification/P04/ticket_contract.cpp
```

## Additive P05 accessor revalidation

The coordinator authorized `CompletionToken::active/is_reserved` and publisher `is_reserved` accessors for P05 submission preflight. Independent ticket tests now check empty, reserved, and writing observations. Production state transitions are unchanged. Rebuilt and passed `p04_verify_tickets` in dev, ASan/UBSan, and TSan after the change. All other source-manifest entries remain as above. Updated files:

```text
e7dc8211935e6c102c60ff9145dfe2686c1b13d4ebe9ec517ca92c849b5c5bbb  include/vita/runtime/completion/ticket.hpp
840588a9c4ccf068dce1a0b159153f50560b43ca9ad5fb4decef7982f1ff82ff  tests/verification/P04/ticket_contract.cpp
```

## P05 isolation integration revalidation

Added separate `Resource::data_queue` counters so Data does not consume ordinary/cancellation reserves. Independent admission regression fills Data credits and verifies ordinary/cancellation counters remain zero. All four independent P04 tests pass after rebuild; actual available-type projection becomes 36,834,216 bytes due to two extra size counters.

Updated source/test hashes (other production entries unchanged):

```text
4076824209c3e1d9cacc21a99313b5cd88bf326ded79a22be7b4df8c8c3871da  include/vita/runtime/execution/admission.hpp
b966366438d2c97326e4ca3cc35a9383474ed46301f2381e974e62639a5a20bd  tests/verification/P04/admission_executor_contract.cpp
```

P05 integration ASan/UBSan rerun also passed the affected independent tests after these additions; combined filter and evidence are recorded in P05-verification.md.
