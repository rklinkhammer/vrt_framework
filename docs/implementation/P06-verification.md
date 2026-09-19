# P06 independent verification

Status: **PASS for the frozen local functional gate**, 2026-09-18. P07 prerequisites released. This is not Linux qualification or external VITA interoperability/conformance certification.

## Candidate and environment

Baseline revision `8435ab71d8004e9014a37a63d0f4576ea533252b` with the uncommitted source/test manifest below; aggregate manifest SHA-256 `b9db445bc53e40986b8ca12a065ce794fece0ee55b6eedcdf892bcc70993f9fd`. Every listed hash was checked unchanged after verification. Environment: Apple arm64, clang 21/libc++, C++23, exceptions and RTTI disabled by the test helper. Linux GCC14/Clang19 jobs remain unexecuted locally.

## Independent evidence

| Test | Evidence |
|---|---|
| `p06_verify_cam` | 2,048 raw action/PWE/request/detail/NACK tuples; reserved action3 and specific I8 profile restriction; 12,288 legal quality/resolvability cases, independent permission and phase predicates; ties-even adjustment/range checks. |
| `p06_verify_factors` | 6,144 combinations through production checked decode, Engine execution, response encoding and checked decode. Independently expected phase order, NACK suppression, summary/detail separation, ordinary AckP/SchX, action bits, live/hypothetical state and writes, and credit retirement. |
| `p06_verify_scenarios` | S1/S2, P=0 known rejection and unexpected runtime failure, P=1 dependent blocking with independent continuation, cycle rejection with rollback, whole-plan admission ordering despite recycled lower slots, stale ticket-generation cleanup, unknown required-state faulting and late completion rejection. |
| `p06_verify_async` | Synthetic failure leaves external payload untouched; stale generation cannot write recycled result; eight competing producers publish one coherent result; capability pins storage after frontend destruction while abandonment wins publication. |
| `p06_verify_admission` | Each logical resource ceiling rejects before effects; physical revision exhaustion rejects and rolls credits back; committed revision owns transferred revision/Context credits after Engine release until revision retirement. |
| `p06_verify_timing` | No-boundary rejection survives revalidation; clock fault, mapping mismatch and committed boundary prevent execution; no invented planned/effect timestamp on invalid schedule; no-effect timing7 remains encoded; an effect one picosecond beyond the exact window remains effective but reports timing failure. |
| `p06_verify_query` | I5 AckS all/some/no known values, empty query X flags, query observes current state while a future plan waits, and a later immediate plan can run before that future plan. This is not JSON S5. |
| `p06_verify_controller` | Raw literal responses distinguish local send, validation, simulation, partial execution and real completion; timeout is retained separately from late execution evidence; wrong Message ID rejects. |
| `p06_verify_outcomes` | Literal timestamp prefix checks for explicit UTC/GPS/Other, missing epoch rejection, invalid fraction/seconds overflow, no-effect timing failure, separate outgoing count and range rejection. |
| `p06_verify_allocation` | 1,000 repeated admission/execution/VXS encoding/revision-retirement cycles allocate no ordinary or aligned C++ heap storage after setup. |

Expected CAM outcomes are independently encoded appendix predicates, not calls to production decision helpers. S1/S2 also pass the reviewed developer `p06_loopback_transaction` integration: leased commands and responses cross checked Loopback routes, retained control state passes through the virtual backend, and leases reclaim exactly once. Developer tests are supporting evidence and are distinguished from the ten independent executables.

Interpretation mapping: I1 summary/detail behavior in factors; I5 query completeness in query; I6/I7 action-preserving dry run and isolated state in factors/scenarios/controller; I8 raw profile combinations in CAM; I10 omission of unknown planned/actual times and actual-effect timing failure in timing/outcomes, with aggregation additionally reviewed in production traversal; I11 correlated diagnostic decode in factors and the prior independent P02 gate; I12 early-window arithmetic remains covered by the independent P08 scheduling gate, with full generator/clock integration deferred to M3. JSON S5 and S11 remain open for integrated monotonic deadlines, clock steps and Data-start gating; no foundation-only test closes those scenarios.

## Findings closed before approval

- Profile receiver I8 restriction now applies specifically to execute action2; Controller generation still requests X+S for actions1/2.
- AckS reports selected known current state even when corresponding writes were ineligible.
- Completion cleanup uses exact ownership generation; releasing an old transaction cannot discard another transaction's recycled READY result.
- Future timed work does not hold whole-plan execution ownership before it is due; equal-ready plans preserve admission order.
- Timing eligibility survives revalidation, respects lost clocks/committed boundaries, and never invents a planned timestamp for a rejected schedule.
- Physical revision reservation precedes effects, and immutable revision ownership retains its logical credits beyond transaction release.
- Controller evidence retains timeout and distinguishes late confirmation from success before deadline; explicit epochs and outgoing counters are required by response production.
- Generic Ack timing parsing was corrected according to original-Control context; separate P02/P05 regression evidence is recorded in those reports.

## Commands and results

Configured `dev` and `asan-ubsan`, then built these targets in each preset:

```text
p06_engine p06_outcomes p06_loopback_transaction
p06_verify_cam p06_verify_async p06_verify_factors p06_verify_scenarios
p06_verify_controller p06_verify_timing p06_verify_admission
p06_verify_outcomes p06_verify_allocation p06_verify_query
```

`ctest --preset dev -R '^p06_' --output-on-failure`: **13/13 passed**.

`ctest --preset asan-ubsan -R '^p06_' --output-on-failure`: **13/13 passed**.

`cmake --preset tsan`; `cmake --build --preset tsan --target p06_verify_async`; `ctest --preset tsan -R '^p06_verify_async$' --output-on-failure`: **1/1 passed**. TSan scope is the concurrent result-publication path; the Engine strand and virtual backend are intentionally serialized. No claim that all runtime APIs permit concurrent entry.

P07 supplies cancellation, canonical duplicate retention and full correlation. P09 supplies production physical revision storage; the independent fake sink here tests its frozen admission/ownership contract. P10 must charge complete native object storage and shared-owner overhead in its configuration ledger; this report does not claim the final 64 MiB configuration is already instantiated. P11 recovery remains outside the current package. Broad platform and hardware qualification remains deferred as specified by the plan.

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
c76159658b171bdfd1b30eec57297f27e44f2f619721dac358780faefc23ecca  include/vita/core/error.hpp
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
a4795ce7367e01ccc9e8bbbe2932b4de495724b46dc9bc0bc9f152db51d0f0d5  include/vita/runtime/transaction/backend.hpp
4ec208ef16dccb2233a7ec0269eff73dc641d1b63927209ac432fccedf284dda  include/vita/runtime/transaction/cam.hpp
4b74ec5eb9fd96fd49bd24089c44379d084a71f269d3cc6f7fd9dccc502e75bb  include/vita/runtime/transaction/engine.hpp
19a7bc51e19cc95f7ed38cb8c0ae2acb6573ec04fd2ee925c894a0c8d152bdbc  include/vita/runtime/transaction/outcomes.hpp
42c4cab3a1432934eb994e65e9e60a8a40ab9f735a95b7b1d42972166a151675  tests/integration/P06/CMakeLists.txt
98e0b8b978eddbf834bab328bf5e7092a3a519d321253b63d6618900ea6980eb  tests/integration/P06/chain.cpp
ca8438c0229199006718c82b2351a344508c8237f56ecd607c3fe19f310fbd55  tests/unit/P06/CMakeLists.txt
abc9e6b5ac5506ac4a411ffa49ce755cf3796b28f1320a22ef291cfdb2693dfc  tests/unit/P06/engine.cpp
df2debb691ed3b2dc07e7cb8cde6a01e27813baff32290e654300781a12b5105  tests/unit/P06/outcomes.cpp
ca672000ab4e5cbd071c9b2022ec31b8e13b81f0f665ccc6bf52611e1d5fbe94  tests/verification/P06/CMakeLists.txt
cd4bc0b1ab8f0897cef11bf7d3573ecc7d9f582904081dc9a756c12a3d9ac97d  tests/verification/P06/admission_contract.cpp
610274429324f0e25485ee6922f8c48b10d46b5a67d0d4681bc4ece0030221e5  tests/verification/P06/allocation_contract.cpp
b24b8d18d6730628499b2a9eb36d7b865993a834043b5666f7755378fc904de7  tests/verification/P06/async_contract.cpp
bd1a710c87fa8b9198fe4d6954b3be22b33e3c147b3b5d69f17df6e9ec24c27e  tests/verification/P06/cam_contract.cpp
1d5ead3adb109717888e445053d93eeee6ef2f81ee3405c6fbf99cf4263d32ad  tests/verification/P06/controller_contract.cpp
f68b23b7914de2061f2cc22280dcdce6358003e599938d198d438a1be7d168f7  tests/verification/P06/engine_factors.cpp
e540eaadac2b2ee0897bad3b7961516af7441773c2cedc573d92484bd3fc58a1  tests/verification/P06/outcome_contract.cpp
0beeca0c1652e6516e7e003a34a325a7df1c914df61d954674cd38365b4c2643  tests/verification/P06/query_contract.cpp
71b2a70a4cdda82b5da4678477c1b2ba0b739dea8f323d1045a9911537f9084a  tests/verification/P06/scenario_contract.cpp
d664abe6041dd1d02ed8018986227c2076d8495612aab72d77133e020d50c888  tests/verification/P06/sink.hpp
a327ab1c4cfffe75ce76589559b36ff4ca621345e7c3238579f777892a0d2f15  tests/verification/P06/timing_contract.cpp
```

## Authorized P07 integration revalidation

P07 adds cancellation-aware outcomes, controller correlation, backend disarm and generation-scoped contradiction/lifetime guards to the P06 engine surface. All 13 P06 executables rebuilt and passed within the 24/24 P06/P07 Debug and ASan/UBSan runs. The P06 async publication test also passed TSan alongside the new P07 race test. The original P06 manifest above remains historical; current additive-source manifest `949a73727248ae46bc791e1bf0191327ebf89a5f5f65fb0ae53997f5dd54f970` and the complete evidence are in [P07 verification](P07-verification.md).
