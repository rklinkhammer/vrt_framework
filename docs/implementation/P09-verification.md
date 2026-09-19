# P09 independent verification

Verdict: **PASS** for the local deterministic P09 package gate. Independent verifier authored tests separately from the implementation owner. This releases P10; it does not qualify a deployment, prove full VITA conformance, or complete the M3 integrated gate.

## Candidate and results

Git baseline `8435ab71d8004e9014a37a63d0f4576ea533252b`, with the uncommitted candidate identified below. Full listed source/test manifest SHA-256: `41f2bd7839c4e8562136533a7d73409f56b6ed0703bbdd91be38ff7b06d5f50b`. Every listed file was rehashed after the final runs and remained unchanged. Host: macOS arm64, Apple clang 21/libc++, C++23, exceptions and RTTI disabled by test configuration.

- Debug: **39/39** P03/P06/P07/P09 tests passed, including eight independent P09 tests and three developer/integration P09 tests.
- ASan/UBSan: the same **39/39** passed.
- TSan: **6/6** targeted ownership/concurrency tests passed, including concurrent immutable revision final release versus collection, retained RX cloning, pool reclamation, and the P07 late-completion guard race.
- Standalone public headers: **43/43** compiled through the single `public_headers_standalone` CTest.
- Independent counting allocator: 1,000 production reserve/record, Context encoding/decoding, historical receive and retirement cycles caused no ordinary or aligned allocation after setup. Final physical occupancy and revision credits returned to one current revision.

The initial final-run revision test checked a moved argument's destructor-dependent accounting within the same full expression; that test was corrected to observe after the call expression completed. No production change was needed for that test correction. The final manifest includes the corrected test.

## Independent cases

| Target | Evidence |
|---|---|
| `p09_verify_history` | Effective-time ordering despite reverse arrival; immutable copied old metadata; equal-time conflicting values ambiguous until a later full observation; no backfill; exact 128-entry and 2-second limits; identical replay cannot renew age; SampleLoss event does not become persistent state; delta confidence; malformed semantic values reject before mutation. |
| `p09_verify_revisions` | Physical reservation exhaustion, unused reservation release, transferred credits retained with old handles, concurrent final handle release/collection, backing outliving store, foreign handle rejection, detached old reservation/handle cannot affect new association, transactional invalid initial state rejection; same-boundary coalescing refuses intervening Data/nonpersistent-event dependency. |
| `p09_verify_receiver` | Known callback delivery works with zero waiting quota; optional application retain may fail while borrowed delivery succeeds; exact 64-packet/10-ms waiting bound; 1-ns-before deadline delivery and exact deadline drop; application-retained payload/copied metadata survive detach; stale association rejection; unknown metadata opt-in requires fixed format and never fabricates known rate. |
| `p09_verify_publisher` | Real external leases; initial Context before Data; ordinary restart forces fresh observation; initial state at100 with start at200 publishes an observation at200; initial and periodic rejection timeout; exact64 held packets then fail closed; backward periodic publication rejected; old encoded24-byte Data plus its revision remain immutable after a newer revision and delayed release. |
| `p09_verify_effects` | Real checked Control→Engine→production EffectSink→RevisionStore→Publisher composition. S6: executed rate remains committed and AckX completes despite10-ms Context publication failure. S7: unknown effect marks rate unknown, fails execution coverage, omits rate from AckS and stops Data with invalid Context. S9: actual effects10/12ms create coherent intermediate/final Context and final AckX time12ms. P07 successful disarm followed by contradictory late execution faults state; invalid observation uses qualified observation time without inventing a new effective revision. |
| `p09_verify_allocation` | 1,000 production Context/history cycles under independent ordinary/aligned allocation counters, bounded physical retirement and logical credit accounting. |
| `p09_verify_context_wire` | Literal GPS/picosecond header and CIF word oracle; full known field presence; change/refresh behavior; SampleLoss suppression on refresh; unknown rate omitted and ValidData cleared; manually patched negative raw rate remains structurally decodable but cannot become usable Context metadata. |
| `p09_verify_budget` | Actual contained type sizes charged once for16 streams; history not counted twice inside receiver; multiplication overflow and late-category exhaustion leave all ledger rows unchanged. |

S8 is jointly covered by `p09_verify_publisher` (encoded leased packet and original revision) and `p09_verify_effects` (Engine-produced next revision). S5 and S11 remain full-system integration obligations for P10; earlier clock/controller component tests alone do not close them.

## Closed findings and boundaries

Independent/coordinator review closed foreign publication-handle acceptance, stale reservation faulting a recovered association, unchecked group acceptance, mandatory retain on immediate borrowed delivery, initial/restart observation freshness, periodic Context timeout, backward periodic timestamp overlap, and malformed semantic Context/initial state. The last defect was reproduced as independent history case21 and wire case16: generic codec preservation of a raw signed rate must not imply a valid metadata association. The owner repaired admission using existing descriptor validation before mutation, preserving generic codec behavior.

Backward time overlap uses existing project invariants: preserve known numeric query truth, stop a temporally ambiguous Data association, and use existing coordinated fresh-SID recovery. This is not a claim that VITA mandates fresh SID on every negative correction. P10/P11 must combine published Context and accepted Data interval highwaters, prove overlapping correction fails closed, and prove a nonoverlapping correction does not falsely fault.

The physical effect callback requires a matching store reservation and serialized framework composition. Publication callbacks must take owned Context bytes on acceptance and order them before affected Data; P10 must supply the actual transport binding. Store mutation remains serialized; immutable handles may be released concurrently. The component budget test proves actual category charges and rollback, not whole-M3 feasibility. Root owns the aggregate budget and milestone report.

No required P09 input was unavailable. Linux/GCC14/Clang19 qualification, external peers, GPS/PPS hardware and measured deployment throughput remain unexecuted platform evidence, not implied passes.

## Reproduction

```sh
cmake --preset dev
cmake --build --preset dev -j 4
ctest --preset dev -R '^p0[3679]_' --output-on-failure
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan -j 4
ctest --preset asan-ubsan -R '^p0[3679]_' --output-on-failure
cmake --preset tsan
cmake --build --preset tsan --target p09_verify_revisions p09_verify_receiver p03_verify_pool p03_verify_envelope p03_verify_edges p07_verify_guard_race -j 4
ctest --preset tsan -R '^(p09_verify_(revisions|receiver)|p03_verify_(pool|envelope|edges)|p07_verify_guard_race)$' --output-on-failure
ctest --preset dev -R '^public_headers_standalone$' --output-on-failure
```

## Frozen source/test manifest

```text
9e709e901df2554abd3d9ecd867b0117af790b7fb76df28221ad8a223b8e95f5  CMakeLists.txt
e2e705349919ce3574b3797ee2af737df11b1449aa42b22a101943e2fbcd3489  CMakePresets.json
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
5ede1e74572f0bea379019596176dac7cd31e98beec7b2c09690669a2511eeb1  include/vita/memory/envelope.hpp
9d932caebbe321a6c83b641692677333d0049e8a728e98a0a794b2e634a7e32c  include/vita/memory/memory.hpp
820197487e6fd856a0a9388516fd26da594bc3b722ac59e4793df4f959533b89  include/vita/memory/pool.hpp
be79298527d02902e84874af72e33d92a117a5b76e2378e18b6a80592087b3a0  include/vita/runtime/budget/loopback.hpp
e7dc8211935e6c102c60ff9145dfe2686c1b13d4ebe9ec517ca92c849b5c5bbb  include/vita/runtime/completion/ticket.hpp
3c1d1469e8fde9cbef9812dc0f1f9b1b01df5e7c491b714a94db39002acb3999  include/vita/runtime/context/budget.hpp
3ede591ebdee7c55531ba249ae85159b47a8e4196892f172b64b024089e207ba  include/vita/runtime/context/publisher.hpp
c042b1460c46401007660ef941a5574535bdd419d699134f5afe1b37f2b8b6a7  include/vita/runtime/context/receiver.hpp
e68c39b0b74b64d5991d8e4b405e81b63405c9e31417d10c629e2dc918897f27  include/vita/runtime/context/revisions.hpp
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
3a35fb8ee490611f6ae9317db41dd4d77e3f216abf32d385d216f8dd4d6519fd  tests/integration/P09/CMakeLists.txt
8ff20c35f645cd93d5fb066912b54b090e9ce71eb28de8d1b8cf4b56e4b17613  tests/integration/P09/effects.cpp
8a999cee1e629930f6d9d99fca1a43700e81ff6ef77a8b00c7a2b653c65df8f2  tests/unit/P09/CMakeLists.txt
d02c95211a53b6dd44e03746f66a5c12b8364e8d9b327e189967031c873380de  tests/unit/P09/context.cpp
8c44b694d1182c8063988b1bc583df7a36dfb697d49d5e21b630b3fe6b5dc1ef  tests/unit/P09/receiver.cpp
7bf17736e2c4016019f87d63743b9d33ba484dc39d61f2b397cf11e4fa846ff7  tests/verification/P09/CMakeLists.txt
31d6b59152b0f69b456c7b436c984a6ae527694726bed96bdfeb664ff925115d  tests/verification/P09/allocation_contract.cpp
351ef6e7088b3928236710218a72f945a524622f40bf953d76601717f5e1a86b  tests/verification/P09/budget_contract.cpp
ae133fd07c0cc63677c3cc563b33ea608f2aeb9c638828c41cba246d65a6c878  tests/verification/P09/context_wire.cpp
e82bfab718ac8f40f5032c4c9d677bc226cfb1bba543b697130b3c578e0cf71b  tests/verification/P09/effects_contract.cpp
53fb2d73bd7a518714a02853cb4a1afcb7602df49331980a6e4915d283276edf  tests/verification/P09/history_contract.cpp
5cd145b2f2c7c6fcbd022e2416b42f5b2542633ded70b3aa9893cf40678129b1  tests/verification/P09/publisher_contract.cpp
a96edecc5658472e19089bff818380835a57429cf1bf8403469e66a3bc384631  tests/verification/P09/receiver_contract.cpp
584ae0a1b00560eeabef8b29215af3cc418a3983d57bb21f9b085146e31547f0  tests/verification/P09/revision_contract.cpp
```
