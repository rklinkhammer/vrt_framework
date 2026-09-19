# P01 implementation evidence

Date: 2026-09-18. Status: candidate pending independent V-P01. Scope: architecture §3/3.1 and P01 task card. Interpretations: I2 (CIF7 enable at CIF0 bit7); no new wire interpretation selected.

## Public contracts

`core/error.hpp` supplies `Result<T>` and structured `Error` with stage/effect/field/byte context. `core/bytes.hpp` supplies const/mutable byte spans. `FixedVector<T,N>` owns up to N optional native objects, supports nonthrowing construction, move-only objects and checked insertion, and makes no allocations. Access via `operator[]` requires a valid index, like `std::array`. `push_back(T)` takes ownership of its argument, including on failure; `emplace_back` first checks capacity. No pointer returned from a vector is promised stable through movement of the vector.

`fields/types.hpp` defines strongly typed Q20 Hertz (raw fractional precision retained), identities and typed selectors for Reference Point, Sample Rate, State/Event and Payload Format. Attributes have exact CIF7 bit identities; baseline semantic extents support Current and Sample Rate Minimum/Maximum. Remaining general descriptors/attribute semantics belong to P14, explicitly return unsupported layout now, and are not guessed.

`fields/packet.hpp` supplies distinct named packet wrappers with checked class/action configuration, bounded sorted field entries, immutable copied snapshots and monotonic layout generations. Query/cancel store no semantic values; diagnostic results remain uint32. Attribute edits validate all selected fields and required values on a bounded copy before publishing; failure preserves generation and prior state. Existing snapshots never observe later edits. `fields/arena.hpp` is a separate bounded native-word semantic arena with checked slice access and atomic failed append. It is not transport/wire storage; variable standard-field descriptors consuming it remain P14.

`codec/layout.hpp` defines the shared descriptor traversal for measure/encode/decode/index consumers. Offset accounting includes CIF words and field body, excludes the family prologue. It visits selected fields by ascending CIF/descending bit and attributes by descending bit. Selectors contribute no value bytes; diagnostics contribute 32-bit words, independently of controlled field width. Checked arithmetic, unsupported extents and bounded offset-index exhaustion fail explicitly. Cached measurements carry generation, layout signature and byte count; validation rejects stale layout edits. Equivalent independent layouts may share a measurement because it contains only sizing, never value pointers.

## Tests and sizes

Apple clang21/libc++ arm64, CMake4.4.3. Executed:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev -R p01 --output-on-failure
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan --target p01_semantics
ctest --preset asan-ubsan -R '^p01_semantics$' --output-on-failure
cmake --build --preset dev --target check-docs
git diff --check
```

Developer test passed 1/1 in Debug and ASan/UBSan. Specification checker passed190 checks (not implementation evidence). Cases cover attribute failure atomicity, snapshot isolation, generations, sizing, index capacity, unknown extent, arithmetic overflow, query/diagnostic distinctions, command actions, semantic arena exhaustion and move-only bounded elements. Logs: `build/{dev,asan-ubsan}/Testing/Temporary/LastTest.log` (ignored local artifacts).

Local sizeof evidence: FieldEntry328 bytes; LayoutContext48; snapshot capacity16=5312; capacity4=1376. A four-field native snapshot fits its individual 8KiB envelope; combined runtime transaction/planning storage is not yet measured. Builders temporarily copy bounded state for atomic edits. Production APIs contain no heap allocation, transport lease or wire buffer. This package is single-owner semantic editing; no concurrent builder mutation contract or concurrency claim.

## Limits and handoff

No binary packet encode/decode is implemented by P01. P02 will consume traversal. No M5 generic-variable wire support is claimed. Header-local templates are standard-library-only; all tests compile without exceptions/RTTI. Independent oracle, allocation instrumentation and approval belong to V-P01. No changes to top-level build were made for P01; coordinator registers the package CMake subdirectory.

Independent V-P01 found CIF0 presence bits admitted as selectors. The production selector check now rejects CIF0 bits0–7 and change bit31 without mutation. Independent failing regression is retained by V-P01; developer regression also covers these three cases.

## Candidate source hashes

| Path | SHA-256 |
|---|---|
| `include/vita/core/error.hpp` | `c76159658b171bdfd1b30eec57297f27e44f2f619721dac358780faefc23ecca` |
| `include/vita/core/bytes.hpp` | `ed9510455f242e53a3f0f9316890fdea1de5a4f8cd7f9dfeb41e28bed9d77277` |
| `include/vita/core/fixed_vector.hpp` | `9add4a941546a64ae088f55985554e74a886221c3501e23d94a1b088b91a2eaf` |
| `include/vita/fields/types.hpp` | `2f02a01694f87107b6f69cd06289b6a5cf4715e9d821dd871138d69fd7f6eb65` |
| `include/vita/fields/arena.hpp` | `606b0db7ee2f0a0cc5f776e8846143f2dd865abf8c9f24cb86a7ee63c311c655` |
| `include/vita/fields/packet.hpp` | `be20b065970cc2d188112ca22dc4922f0850d07a6afd88e6f1c030ed13c5fe2d` |
| `include/vita/codec/layout.hpp` | `150fa936fd99a1bc6cefdca27a5625a997efe75e6f9b2a4804367b03232d7000` |
| `tests/unit/P01/CMakeLists.txt` | `4778e4e2d2c4d1728322893f054dae2b4bac8ebbdc3fb0536134d5b5984b3d7e` |
| `tests/unit/P01/semantics.cpp` | `37421c398634d0f2bd76184273952784f71f7c66b6fedea5c449804d1b7348df` |
