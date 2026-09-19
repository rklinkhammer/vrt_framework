# P01 independent verification

Date: 2026-09-18. Verifier: V-P01, independent from I-P01.

**Verdict: PASS for the local P01 semantic/layout gate**, after correction of a selector-validation defect. Baseline Git revision `8435ab71d8004e9014a37a63d0f4576ea533252b`; candidate is the frozen uncommitted manifest below, SHA-256 `de9609698a3924b345c60eb528cfcc9ef4328216a4d0d599fe5767f0a300ac53` (sorted `sha256  path\n` records).

## Requirements and independent oracle

Architecture §§3–3.1, P01 task card, protocol appendix baseline descriptor/selector distinction, and W9 atomic attribute scenario. CIF7 interpretation I2 applies to future wire encoding; this package tests semantic layout, not wire conformance. No protocol behavior was inferred from round trips or encoder output.

- `p01_verify_core`: move-only bounded objects, exact lifetime balance, failed insertion preserves contents, zero-capacity container, and const borrowed bytes.
- `p01_verify_semantics`: distinct semantic wrapper defaults; typed units; immutable snapshots; failed missing/duplicate attribute edits preserve state and generation; selector-only attributes contribute indicator words but no values; diagnostic Sample Rate contributes four bytes rather than eight; independent expected body sizes 12/32/8; sorted field offsets 4/8/16; index exhaustion; bounded packet failure; unknown layout; invalid selectors; checked overflow; arena failure preserves existing native values.
- `p01_verify_allocations`: 1,000 edits/snapshots/measurements/index builds with zero counted ordinary heap allocations; stale cache rejects after edit while previous frozen snapshot remains valid.

Public constructors cannot combine arbitrary body kind with incompatible subtype; named wrappers select defaults and the generic typed wrapper has a compatibility constraint. Source inspection confirms semantic state contains native values and no external buffer leases, transport state, or serialized packet storage. Variable-value arena is native bounded storage; standard variable descriptors remain future work.

## Failure and repair

Initial independent semantic test returned 27: `QueryPacket::select(FieldId{0,7})` accepted a CIF7-enable indicator as a field; CIF0 bit1 similarly accepted. This violates the distinction between presence indicators and actual field identities. Implementer added selector validation rejecting CIF0 bits 0–7 and change-indicator 31 before mutation. Independent regression retained in semantic_contract.cpp. Repaired candidate passes.

Earlier pre-freeze review found aliases with inappropriate default subtypes; implementer replaced them with distinct constrained typed wrappers. Cached measurement validation now includes layout signature as well as generation/size; equivalent independent layouts may legitimately reuse pure sizing information.

## Commands and results

Apple clang 21/libc++, Darwin 27 arm64, CMake 4.4.3. RTTI and exceptions disabled.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev -R 'p00|p01|p03|architecture_fixtures' --output-on-failure
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan --target p01_verify_core p01_verify_semantics p01_verify_allocations
ctest --preset asan-ubsan -R '^p01_' --output-on-failure
```

Integrated Debug passes 15/15 tests, including all P00/P01/P03 tests and 190 specification checks; ASan/UBSan P01 passes 4/4. Raw local logs: build/{dev,asan-ubsan}/Testing/Temporary/LastTest.log (ignored and replaceable by later runs). P01 has single-owner builders and no concurrency primitive; no TSan concurrency claim is appropriate. The P03 TSan gate independently exercises shared ownership.

Allocation instrumentation counts ordinary new/new[] on tested paths, supported by inspection of fixed native containers; it is not process-wide allocator qualification. No binary encoder/decoder, full standard field coverage, transport, or deployment qualification is claimed. Clause-level exact wire vectors belong to P02. Existing local sizes: FieldEntry 328, layout 48, snapshot capacity 16=5312, capacity 4=1376 bytes; P04 must include combined instantiated transaction/revision/storage costs rather than assuming one snapshot represents a whole operation.

Integration revision: same shared workspace and root CMake manifest below; no commit created. Future relevant edits invalidate affected evidence.

## Frozen source manifest

```text
1a088395deb9683f29ff74c4dda5e0513a1aab743ff9c4f40d355a39458385d1  CMakeLists.txt
150fa936fd99a1bc6cefdca27a5625a997efe75e6f9b2a4804367b03232d7000  include/vita/codec/layout.hpp
ed9510455f242e53a3f0f9316890fdea1de5a4f8cd7f9dfeb41e28bed9d77277  include/vita/core/bytes.hpp
c7090fadba63f24786de0de12c2cda91c0069124db1b7b934949ee88a5c530eb  include/vita/core/capacity_policy.hpp
c76159658b171bdfd1b30eec57297f27e44f2f619721dac358780faefc23ecca  include/vita/core/error.hpp
9add4a941546a64ae088f55985554e74a886221c3501e23d94a1b088b91a2eaf  include/vita/core/fixed_vector.hpp
7d0a8c6896e0eb4d76d233b021cdd34731282ced6882a580e907c58a09636ab9  include/vita/core/version.hpp
606b0db7ee2f0a0cc5f776e8846143f2dd865abf8c9f24cb86a7ee63c311c655  include/vita/fields/arena.hpp
be20b065970cc2d188112ca22dc4922f0850d07a6afd88e6f1c030ed13c5fe2d  include/vita/fields/packet.hpp
2f02a01694f87107b6f69cd06289b6a5cf4715e9d821dd871138d69fd7f6eb65  include/vita/fields/types.hpp
4778e4e2d2c4d1728322893f054dae2b4bac8ebbdc3fb0536134d5b5984b3d7e  tests/unit/P01/CMakeLists.txt
37421c398634d0f2bd76184273952784f71f7c66b6fedea5c449804d1b7348df  tests/unit/P01/semantics.cpp
a4dd076adc2e41112f75cbf63dca82b48c4a4c2389a4e7f9cdc8f6ba87474014  tests/verification/P01/CMakeLists.txt
6e4130ce039af40ad4e251c9e2ff46d662edd0e310133ccd00b9ded161d8fbbe  tests/verification/P01/allocation_contract.cpp
252c68906ae0efd16dc9b65fef9b080d6404e9b61ba78c34349cf6300e12c28a  tests/verification/P01/core_contract.cpp
3bcb5e3e1e489926f9530eef0b4b61551be1de3f1e3f7c1b5cecc7402920a409  tests/verification/P01/semantic_contract.cpp
```
