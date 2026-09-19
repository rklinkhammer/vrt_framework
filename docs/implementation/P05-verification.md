# P05 independent verification and M1 chain evidence

Date: 2026-09-18. Verifier: V-P05, independent from I-P05.

**Verdict: PASS for local P05 deterministic transport/routing and the tested M1 ownership chain.** Coordinator owns aggregate milestone integration. Real network transport and deployment interoperability remain unqualified.

Baseline Git revision `8435ab71d8004e9014a37a63d0f4576ea533252b`; frozen uncommitted source manifest SHA-256 `8074e6e84a0ee7661cae938c040f43090891aaf525bd2e8449481307bb3c21d4` (sorted `sha256  path\n` records below).

## Requirements and independent observations

Architecture §§4–5, P05 task card, and M1 gate. Routing/counter expectations derive from explicit sender/SID/type/identity keys. Release counts come from independent external-provider callbacks. Logical packet equality compares receiver-observed bytes from separate runs with fresh counter domains, not encoder round trips.

| Test | Evidence |
|---|---|
| p05_verify_chain | Production scalar packing and checked packet framing write externally leased storage; contiguous and segmented TX pass through actual Loopback copy/route/checked-decode; callback reads samples and retains IQ; TX returns after completion, RX stays until final retained handle reset; handle outlives transport and pool frontend; two paths deliver identical logical bytes and exactly one release per allocation. |
| p05_verify_routes | Paired SID 42 independently routes Data/Context/Command; typed short/UUID identities remain distinct; old peer generation rejects; SID-less duplicate route rejects; frozen registry rejects edits; sender/SID/type counters remain independent and wrap modulo 16; stale encoded count does not consume count. |
| p05_verify_faults | Synchronous rejection returns all submission ownership with no completion publication/count change; retryable returned submission succeeds; loss produces local success without receive/delivery claim; duplicate receive has one completion/count; explicit progress order reorders packets; nested other-token progress returns would_deadlock; failed completion retains quarantined TX until explicit proof; stale proof rejects; filling Data slots still permits ordinary Control and cancellation, which complete before queued Data. |

The M1 chain uses the production codec, actual provider leases, actual transport slots, checked receive parsing, and public retention API. Segmented test deliberately splits the encoded fixture to isolate transport equivalence; this is not a claim of zero-copy generator production. The planned additive prologue encoder enables P10 to avoid IQ staging. Loopback advertises a copy into receive pools explicitly. Default no-trailer Signal Data is exercised here; later profile variants require their own integration cases.

Three physically distinct RX providers and disjoint Data/ordinary/cancellation slots prevent Data saturation from taking control/cancel backing. Separate logical data_queue credits supplement those physical reservations. Registration occurs before traffic; transport progress is caller-serialized. Threaded transport behavior is not claimed.

## Commands and results

Apple clang 21/libc++, Darwin 27 arm64, CMake 4.4.3; no exceptions/RTTI:

```sh
cmake --preset dev
cmake --build --preset dev --target p05_verify_chain p05_verify_routes p05_verify_faults
ctest --preset dev -R '^p05_verify' --output-on-failure
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan --target p05_verify_chain p05_verify_routes p05_verify_faults p03_verify_pool p04_verify_admission_executor p04_verify_quiescence_budget p04_verify_coupled_admission
ctest --preset asan-ubsan -R '^p05_|^p03_verify_pool$|^p04_verify' --output-on-failure
```

Independent Debug suite passes 3/3. Combined ASan/UBSan suite passes 11/11, including P05 developer tests, developer M1 chain, independent P05 tests, and affected P03/P04 regression gates after provider identity/data_queue additions. Logs: build/{dev,asan-ubsan}/Testing/Temporary/LastTest.log (ignored, replaceable). No TSan claim is added for the explicitly serialized adapter; P03/P04 concurrent primitives have their own TSan evidence.

## Reviews and remaining obligations

Before freeze, coordinator/implementer closed nested progress reentrancy and physical/logical control/cancellation isolation gaps; independent regressions cover both. P04 empty/reserved token preflight and P03 provider-identity helpers received separate verifier revalidation.

No runtime class discovery, external OUI assignment, peer agreement, timed device execution, or application lifecycle is implemented by P05. Extension payload validators must be explicitly registered. Completion success remains local ownership/progress evidence, never remote delivery. Retained RX remains immutable while owned by application; forced lifetime revocation is not used. No new protocol interpretations were introduced.

Integration revision: same shared-workspace manifest, no commit created. Future relevant edits require affected regressions.

## Frozen source manifest

```text
1a088395deb9683f29ff74c4dda5e0513a1aab743ff9c4f40d355a39458385d1  CMakeLists.txt
b0c9ce305ad571e1ce950aeae0c65682a1c07d66e2d7c6de04b110c8101348ab  include/vita/adapters/loopback/loopback.hpp
150fa936fd99a1bc6cefdca27a5625a997efe75e6f9b2a4804367b03232d7000  include/vita/codec/layout.hpp
22c63c5b5e70f892317be156eb3fc6abb2ca41bc1d84464d5e4befa8db8edf4f  include/vita/codec/packet.hpp
3db37f96f7f61603aaa6f909959172adaa3c7a583d1fb5f3da021ffbf3f80298  include/vita/codec/samples.hpp
b7e4020d643a2065ddfb784dfaf244e0998ca313dce1690ff4df89fdaa2223b3  include/vita/codec/wire.hpp
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
e7dc8211935e6c102c60ff9145dfe2686c1b13d4ebe9ec517ca92c849b5c5bbb  include/vita/runtime/completion/ticket.hpp
4076824209c3e1d9cacc21a99313b5cd88bf326ded79a22be7b4df8c8c3871da  include/vita/runtime/execution/admission.hpp
ed33dbd7a3917f296f568aff824e2c2b9c0f086550bb58bacd185a6b498e345a  include/vita/runtime/execution/arena.hpp
8b5279aaf8b40a1a7295b5bf0f7d86b1f847de03bbdce5e644dd268cc0a8e7d7  include/vita/runtime/execution/budget.hpp
354e99cc3c6f17906f5c24bb72409d9da0dff6362373cd5eb783108891631fcf  include/vita/runtime/execution/executor.hpp
8c3cc067511769f582d615978d79e3daa7c3c346bde5ef2cc449631b22887eec  include/vita/runtime/execution/operation.hpp
b984fd6a79f6a3b4d3bfd18cc7d3095e63720039207c118013faa442cc77f249  include/vita/runtime/stream/counters.hpp
34c4859e891a6c3a07b2fb16e756c276e7a1c26955a92c692f39ecc20c2e813a  include/vita/runtime/stream/routing.hpp
6067b2c9128eff54c0a8383117f32f8e206275c0697552a798eead612dba2f30  tests/integration/P05/CMakeLists.txt
355f4bb964ecce62591d3bf9df87905d395238f61650bcfd0c442d976749f821  tests/integration/P05/chain.cpp
42d3841cbca09e8a8b1d2ce430a5e5cf7a735bc069b017e8afa2acd5e95294dd  tests/unit/P05/CMakeLists.txt
882ba197345336fb94ccf31f5c191aa06f4b098ba1f304c7a68cbc3c61776d7c  tests/unit/P05/isolation.cpp
d795de6c2e3897b0baf1fe34d0edcb84fbacac4bdb7e3f27ba0a853d606eb8a7  tests/unit/P05/loopback.cpp
58aaf62bb91e993f1c709d4d8390402541c0f72ec65c0dd4c092792c0667f331  tests/unit/P05/support.hpp
01e06b7c9dfb5431c9e5ea94c3456a3409eb12f06186a1eba92dce167a6b2845  tests/verification/P05/CMakeLists.txt
2a1baeefa9441c82fff02da8e3fc3868ddab3bc13bb6e9d1ed7b11139e19698d  tests/verification/P05/faults_contract.cpp
32dd53bf292d0eefebf90efb680bee0f1e168462cf93bce0c48301d87abcb9df  tests/verification/P05/loopback_integration.cpp
cfda8f7d438fd36e57ca0a57462be9c944cf161fa72c02522c425e59eb2e7854  tests/verification/P05/routes_contract.cpp
```

## Source-capture correction and restored-wire regression

A concurrent additive-helper task briefly modified wire.hpp after the original P05 test run, and the first report captured that temporary hash. The implementation restored the original P02 wire.hpp (`b7e4020d...`) exactly. This report's manifest is corrected to the restored source. All six independent P02/P05 executables were rebuilt and passed 6/6 in both Debug and ASan/UBSan on the restored file. Only wire.hpp differed from the first manifest; no transport or verification source changed. The new prologue helper is isolated in prologue.hpp and requires separate verification. This corrected manifest and rerun supersede the earlier source identification.

## Additive budget composition verification

The independent `p05_verify_budget` test passes 1/1 in Debug and ASan/UBSan. It derives charges from actual `sizeof` values and quiescence metadata, charges two adapters with shared registries counted once, checks instance multiplication overflow, and forces the second registry charge to fail to prove atomic rollback. The reserved reference total remains 64 MiB. This is composition evidence for present types, not a completed M3 resource ledger.

Commands: `cmake --build --preset dev --target p05_verify_budget`; `ctest --preset dev -R '^p05_verify_budget$' --output-on-failure`; repeat with `asan-ubsan`.

Frozen helper SHA-256: `be79298527d02902e84874af72e33d92a117a5b76e2378e18b6a80592087b3a0` (`include/vita/runtime/budget/loopback.hpp`).

After the P02 I4/I11 correction (`packet.hpp` SHA-256 `0b7291cb2658cfe8783133f20f454a4933f0f92da55f424a5d3eae4c8b08553e`), all eight independent P02/P05 targets rebuilt and passed 8/8 in Debug and ASan/UBSan. This revalidates transport decode/routing/retention integration against the corrected codec.

The subsequent narrow P02 Ack timing correction (`wire.hpp` SHA-256 `cc2e6266f8802128e1ab4ead5e391ce22f5a342ca54edb63fed39b219253ba0a`) was regression-tested by rebuilding all eight independent P02/P05 targets: 8/8 passed in Debug and ASan/UBSan. The earlier frozen/restored wire evidence remains historical; this is the authorized current codec revision.
