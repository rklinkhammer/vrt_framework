# V-P14 CIF3 temporal/environmental verification

Status: PASS for the frozen19-field CIF3 temporal/environmental batch and affected regressions.

Scope:19 fields, including nine signed64 femtosecond fields, Timestamp Details, envelope-bound Age/Shelf Life and seven environmental/network fields. The supplied PDF pp189–198 and209–211 was independently extracted; epoch Tables9.7.3.4-4/5 and Time Source Table6 on pp196–197 were rendered and inspected.

Age/Shelf Life use enclosing TSI/TSF (§9.7.2 p192), not the femtosecond representation (§9.7 Rules1/2 p189). Both-zero ordinary values have no supported representation, whereas selectors and diagnostic words retain their independent layouts. Equal-width timestamp code changes must still invalidate cached layout identity and cannot reinterpret existing values. Raw fractional values are preserved without assuming every format is a subsecond picosecond count.

Timestamp Details epoch checks: UTC permits TSE0/any,1/0,3/0; GPS permits0/any,1/315964811,2/0,3/315964800; mixed UTC/GPS requires TSE0. These constants are epoch locations, not live leap-second offsets. Source codes0..7 are all defined, with6/7 class-defined. LSH0 permits LSP0 or2. E0 offset and TSE0 epoch are undefined rather than structurally zero-required. Applicability scope and clock qualification are distinct from readable field layout.

Barometric Pressure remains raw17-bit code only under the accepted policy. No Pascal conversion is implied. Sea/Swell uses two5-bit codes0..9 and6 user bits; tropospheric/network identifiers require no inferred application database. Generic parsing does not enable these fields on the four-field IQ profile.

## Independent executable coverage

- `p14_verify_cif3_scalars`: all nine signed femtosecond fields with literal negative raw values, correct signed Offset/Skew versus nonnegative duration semantics, maximum signed values, all truncation prefixes and baseline IQ rejection. Seven environmental/network literal vectors include absolute-zero representable boundary, full humidity/pressure raw codes, Sea/Swell/user maxima and full Network ID. Reserved bits and invalid native values are rejected. All19 selector and diagnostic extents remain valid without timestamps.
- `p14_verify_cif3_duration`: every TSI/TSF combination, exact1/2/3-word Age/Shelf payloads, both-zero ordinary rejection, independent indexed offsets, full64-bit fractional preservation, materialization and literal encoding, all truncations, both TSI and TSF envelope mismatch with output unchanged, transactional failed rebind, immutable earlier snapshots and equal-size/equal-generation different-code layout-measure rejection. Public forged FieldView byte bounds/code checks are covered independently. Tagged unbound values may be stored but cannot be measured until explicitly bound; materialization does not silently configure the destination.
- `p14_verify_cif3_details`: independent Cartesian epoch-table oracle across nine scope masks, all four epoch codes and five epochs; partial scope distinction; all16 leap handling/prediction pairs; all eight time sources; documented user bits/source; fractional-only applicability; raw E0 offset and TSE0 epoch preservation; intrinsic-invalid raw readability with typed rejection; reserved-bit callback barrier.
- `p14_verify_cif3_allocation`:1,000 cycles of owned duration insertion, clone, measurement, encoding, decoding, materialization, removal/replacement and old-snapshot checks. Zero ordinary/aligned C++ allocations inside the measured loop; positive probes verify both instrumentation hooks. This excludes arbitrary C allocators/process-wide heap behavior.

Independent review found that the initial applicability helper could conflate fractional-only timestamps with no timestamps because it inspected TSI alone. The owner added explicit observed TSF scope before freeze; the regression now verifies fractional-only scopes are applicable without inventing UTC/GPS epoch restrictions.

The direct optimized four-test gate passes. Two duration coverage refinements (TSI code changes and public forged views) were bundled after that initial direct run; the final sanitizer and coordinator Release gates below identify the final source. The initial complete sanitizer run passed167/167 but its duration object preceded the final refinement; it is retained as provisional evidence rather than substituted for the final rebuild.

Actual [sizes](artifacts/P14-cif3/sizes.txt) remain SemanticValue16, LayoutContext48, FieldView32, scalar snapshot5312, native8192 snapshot13512 and runtime StateSnapshot136. StateDurationValue occupies16 bytes of explicitly chosen native arena capacity. [Production hashes](artifacts/P14-cif3/source.sha256) and [verifier hashes](artifacts/P14-cif3/verifier.sha256) identify the frozen working tree.

## Final gate

Final rebuilt ASan/UBSan suite: **167/167 PASS**,24.31 seconds. Coordinator optimized Release suite: **170/170 PASS**, including60 standalone public headers. All eight production and five verifier hashes match after final execution. Durable [sanitizer results](artifacts/P14-cif3/test-asan-ubsan.log), [initial build](artifacts/P14-cif3/build-asan-ubsan.log), [final duration rebuild](artifacts/P14-cif3/rebuild-duration-asan-ubsan.log) and [Release results](artifacts/P14-cif3/test-release.log) retain the evidence. Commands use `cmake --preset udp-asan-ubsan`, `cmake --build --preset udp-asan-ubsan -j4`, a final `--target p14_verify_cif3_duration` rebuild, and `ctest --preset udp-asan-ubsan --output-on-failure`.

The full suites include prior registry, baseline protocol/profile, runtime and reference-budget regressions. Evidence is local macOS functional/sanitizer validation, not complete P14/M5 conformance, independent-peer interoperability, Linux qualification or P15/M6 hardware evidence. Stream-global Timestamp Details consistency and truth of source/clock calibration remain deployment semantics beyond this stateless codec.
