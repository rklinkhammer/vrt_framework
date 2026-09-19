# P03 independent verification

Date: 2026-09-18. Verifier: V-P03, independent from I-P03.

**Verdict: PASS for the local P03 storage/retention API gate.** Actual transport acceptance/completion and conversion integration remain P05/P02 obligations; this report does not approve those future paths.

Baseline Git revision `8435ab71d8004e9014a37a63d0f4576ea533252b`; frozen uncommitted manifest SHA-256 `86242f424db50a0b74f89450abfac3cc74c3254feb8013089c76c635b0868edf` (sorted `sha256  path\n` records below). The integrated root CMake registration is included.

## Requirements and oracle

Architecture §4 and P03 task card; no protocol interpretation IDs apply. Independent instrumentation observes exact return counts per backing block, quota counters, byte preservation, allocation counts, and public lifetime semantics. Expected counts derive from explicit ownership transitions, not provider internal reference counts.

| Test | Independent observations | Result |
|---|---|---|
| p03_verify_pool | Move-only lease; bounds/alignment/domain errors; idempotent reset; two threads each acquire/release 1000 times with 2000 successful returns; lease outlives pool and external frontend; opaque device refuses CPU access; configuration overflow | Pass |
| p03_verify_envelope | Two independent IQ handles share one backing block across two fragments; header/trailer return independently; each physical block returns once; quota rollback and shared quota identity; three-region limit preserves rejected lease; partial acquisition rollback ; 16 fragments accepted, 17th rejected; immutable RX bytes | Pass |
| p03_verify_edges | Reclamation hook can query/acquire without deadlock and cannot reacquire its returning block; retained handle outlives envelope, pool, quota frontends and application owner; zero counted heap allocations in acquire/add/retain/move; total TX size overflow rejects before ownership transfer | Pass |

No conversion API is available in P03; unchanged raw backing bytes are tested, but conversion immutability must be tested in the consuming codec package. TxStorage movement/rejected append are tested; they are not proof of asynchronous transport semantics. Lifetime-backed device metadata is modeled without dereferencing opaque memory, not real DMA qualification.

## Commands and results

Apple clang 21/libc++, Darwin 27 arm64, CMake 4.4.3, exceptions/RTTI disabled.

```sh
cmake --preset dev
cmake --build --preset dev --target p03_verify_pool p03_verify_envelope p03_verify_edges
ctest --preset dev -R '^p03_verify' --output-on-failure
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan --target p03_verify_pool p03_verify_envelope p03_verify_edges
ctest --preset asan-ubsan -R '^p03_' --output-on-failure
cmake --preset tsan
cmake --build --preset tsan --target p03_verify_pool p03_verify_envelope p03_verify_edges
ctest --preset tsan -R '^p03_verify' --output-on-failure
```

Three independent tests pass in all modes; ASan run also passed the developer test (4/4). Final integrated Debug run is recorded separately by coordinator. Raw ignored logs: build/{dev,asan-ubsan,tsan}/Testing/Temporary/LastTest.log; subsequent package runs can replace them. TSan exercised real pool contention with no report, but does not establish all possible interleavings. Reference counts are protected by provider mutex; quota counts use atomics. Setup allocations remain allowed and allocation-failure behavior is documented by implementer. New-count instrumentation covers ordinary allocation operators on tested paths, not every system allocator API.

## Review findings and remaining qualification

Two pre-freeze review issues were repaired by implementer and retained as independent regressions: callback under provider mutex could deadlock; TX byte-size summation could overflow. Current callbacks run unlocked while the returning block remains unavailable; append validates aggregate size before moving ownership.

RetainedRx is 944 bytes on this ABI, so 1024 handles require 966656 bytes, 704512 beyond the architecture's original 262144-byte estimate. P04 must charge that transfer and control-block overhead; no complete 64 MiB fit is claimed here. External allocation extent is caller-guaranteed through BufferSpec lifetime and declaration. Linux/device qualification remains pending.

## Frozen source manifest

```text
1a088395deb9683f29ff74c4dda5e0513a1aab743ff9c4f40d355a39458385d1  CMakeLists.txt
ed9510455f242e53a3f0f9316890fdea1de5a4f8cd7f9dfeb41e28bed9d77277  include/vita/core/bytes.hpp
c76159658b171bdfd1b30eec57297f27e44f2f619721dac358780faefc23ecca  include/vita/core/error.hpp
e46a7af35ff9a7035569295c0f7d7d8e4ab586c09d489b53b65fa644cadae31d  include/vita/memory/envelope.hpp
9d932caebbe321a6c83b641692677333d0049e8a728e98a0a794b2e634a7e32c  include/vita/memory/memory.hpp
7ca679bb74e765d39696dc94b5da0e7d87e54446d12d2a54a0ab7697484407a8  include/vita/memory/pool.hpp
abc7b358a1c85ce62efff473b77394514064c14844850a8e149e0f8a952314b9  tests/unit/P03/CMakeLists.txt
98e78f134433d6b9301773a228ae265286bf6e2000bcc741fe46cb507bf4c454  tests/unit/P03/memory.cpp
d2cf8e1c2892b5d559fc146037c3dc61b444fbd98416553220a23a7693fec5d6  tests/verification/P03/CMakeLists.txt
a6335cf62a24d5f8861772e050e3aae8a4474d36b1f2e65798fe5849918bbe1a  tests/verification/P03/edge_contract.cpp
759f21ed66e3aa45671b63e4778fbf89f7810d34646830f53f2788ebcc0e6e59  tests/verification/P03/envelope_contract.cpp
8cc35953a19ef364fe539094de37532424b02da723cd5cf2ebb92e1d4fe0f3d9  tests/verification/P03/pool_contract.cpp
```

## P05 isolation integration revalidation

Added provider-identity comparison for validating distinct receive pools. Independent test checks same and empty provider identities; rebuilt `p03_verify_pool` and passed locally. Lifetime/state machinery is unchanged.

Updated source/test hashes (other production entries unchanged):

```text
863f73a4a74b6db81285f7440f0ad7cd72eadbb38942c41fe6be13c8f37a6d85  include/vita/memory/pool.hpp
adc183201354bf76d567e7e11a540f0b82eda36ab3a6d875f0aa23d6e7bc28dd  tests/verification/P03/pool_contract.cpp
```

P05 integration ASan/UBSan rerun also passed the affected independent tests after these additions; combined filter and evidence are recorded in P05-verification.md.

## P09 retained-callback integration addendum

P09 adds `RetainedRx::retain` so an application can optionally retain a callback backed by the receiver's waiting lease; `BufferLease` adds the corresponding private sharing friendship. The final source hashes are `5ede1e74572f0bea379019596176dac7cd31e98beec7b2c09690669a2511eeb1` (envelope) and `820197487e6fd856a0a9388516fd26da594bc3b722ac59e4793df4f959533b89` (pool). Historical manifests above remain historical evidence.

All four P03 tests passed Debug and ASan/UBSan in the P09 39-test regression gate; the three independent P03 ownership tests also passed TSan. New independent `p09_verify_receiver` verifies optional application retain from both immediate and waiting-backed callbacks, quota failure without mandatory-copy failure, release of waiting credits, and retained data surviving receiver detach. Exact commands and the full final source/test manifest are in [P09-verification.md](P09-verification.md).
