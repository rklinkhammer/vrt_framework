# P14 nonrecursive CIF1 structures — independent verification

Verdict: **PASS** for the four nonrecursive CIF1 structures on the repaired frozen source. Full ASan/UBSan integration passed195/195; coordinator optimized Release passed198/198 with69 standalone public headers. This is bounded codec/native-storage evidence, not a claim of full generic semantic execution or peer qualification.

The independent source is the supplied ANSI/VITA 49.2-2017 (R2024), printed pages 130–137 and 165–187. Expected wire words were assembled independently, rather than produced with the encoder under test.

## Source decisions and expected boundaries

| Field | Source | Independent expectations |
|---|---|---|
| Index List, CIF1/7 | §9.3.2, pp131–132 | Two-word header; entry width codes 1/2/4; 20-bit count; MSB-first packed entries; zero trailing padding. Zero entries are representable despite a recommendation discouraging them. Wire count and configured traversal limit are separate. |
| 3D Pointing Vector structure, CIF1/28 | §9.4.1.3–5, pp135–137 | Three-word header, or four with global reference; mandatory selector30; optional per-record selector31; consistent record size/count/total size. Global index is zero; reference and beam remain explicit raw values, with beam3 reserved. Record zero values retain inheritance meaning rather than being silently replaced by global values. |
| Spectrum, CIF1/10 | §9.6.1, pp165–178 | Thirteen words. Spectrum codes5–127 and window codes44–99 are reserved; averaging uses six low bits. Smoothing alone is a semantic restriction. Resolution/span are signed Q20 representations with nonnegative Current semantics. Delta codes0/1/2/3 mean unused/percentage/samples/time; percentage uses signed Q12 and a maximum100 percent. Alpha remains raw32 under the accepted policy. No class-dependent transform/sample-rate equation is invented. |
| Sector/Step-Scan, CIF1/9 | §9.6.2, pp180–187 | Three physical header words, encoded optional-header count0; required sector and F1; shared selector/layout for all records. Dwell, Time3 and Time4 are each two words under specific Rules9.6.2.10-2,9.6.2.12-1,9.6.2.13-1, overriding the summary table's one-word entries. Only Start Time derives its integer/fractional layout from enclosing TSI/TSF. Scan execution/default interpretation is outside raw codec scope. |

Reserved representations must fail checked structural parsing before semantic callbacks. Physically invalid but structurally representable values must remain inspectable on raw decode, with Current native admission applying the documented semantic rules. NonCurrent attributes require representation validation without inventing physical derivative ranges.

## Executable coverage

Literal wire vectors cover all four fields, malformed header/count/selector equations, reserved bits, truncation at every byte, and shared work/record/index limits. Sector tests cover all timestamp format combinations, explicit binding, equal-size code changes and stale measurements. CIF7 tests cover independently sized sibling attributes and one-word Probability/Belief shapes without base storage or timestamp prerequisites.

Native tests check deep copies, immutable prior snapshots, checked record access, materialization, transactional failures and arena exhaustion. The gate records actual object/storage sizes, instruments bounded operations for ordinary and aligned C++ allocations, and preserves baseline profile isolation. No generalized sector execution, spectrum processing, recursive arrays, hardware qualification or external peer qualification is claimed.

Final source and verifier hashes were captured before the integrated gate and rechecked unchanged afterward.

## Finding resolved before candidate freeze

Independent review caught the proposed Sector HeaderSize=3 encoding. Section9.3.1 p130 defines this byte as the optional application-header count, zero when absent; Sector §9.6.2.1 p180 explicitly has no global header and applies the template. Its physical header is still three words. Pointing has a specific Rule9.4.1.3-1 override to3/4; Sector does not. Coordinator confirmed the correction without a new dialect decision. Independent Sector literals now require encoded0 and reject encoded3. The owner repaired the encoding; both independent literals and final integrated gates pass.

## Native view provenance defect and repair

After the first frozen aggregate passed193/193 under ASan/UBSan, independent public-API review found that `NativeRecordView<SectorRecord>(Bytes)` could copy arbitrary bytes into records containing `std::optional`. A minimal all-FF input triggered UBSan's invalid-bool diagnostic. The [reproducer](artifacts/P14-cif1-structures/native-view-reproducer.cpp) and [failure log](artifacts/P14-cif1-structures/native-view-ubsan-failure.log) are retained; the initial aggregate did not exercise this adversarial public constructor and is explicitly provisional.

The owner made byte construction private, restricted the record template to the two supported native record types and retained only trusted internal native-arena reader friends. Public default/copy construction remains. The independent native test now statically rejects public Bytes construction for both types and verifies empty-view errors and successful copied snapshot views. Internal detail helpers have an explicit trusted native-object-byte precondition and are not wire parsing APIs. The repaired targeted optimized native test and final integrated rerun both pass.

The first direct verifier compilation also required correcting an assert macro around a braced initializer and an ambiguous expected/optional comparison. The profile oracle initially expected an admitted diagnostic transaction; existing structured Current handling instead rejects with `unsupported_capability` before admission. The corrected assertion verifies that exact error plus unchanged state and zero backend operations. These were verifier corrections, not production fixes or weakened acceptance of the fields.

## Final frozen evidence

- Five independently authored optimized contracts pass: `p14_verify_cif1_structures_wire`, `_native`, `_time`, `_bounds`, `_profile`.
- Full ASan/UBSan: **195/195**,55.63s, including both new generic-capacity fuzz entry paths and100,000 deterministic mutations. [Configure](artifacts/P14-final-local/configure-asan-ubsan.log), [build](artifacts/P14-final-local/build-asan-ubsan.log), [tests](artifacts/P14-final-local/test-asan-ubsan.log).
- Coordinator Release: **198/198**,33.92s,69 standalone public headers; [test log](artifacts/P14-final-local/test-udp-release.log). Both public examples and prior reference-budget regressions remain in the full suite.
- [Final22-file verifier/source manifest](artifacts/P14-final-local/verifier-source.sha256) includes the repaired native provenance code and additive I9 layout-resolver integration. Every entry rechecked unchanged after the gate. Initial source/test manifests and193-test sanitizer logs remain separately named provisional in [earlier artifacts](artifacts/P14-cif1-structures).

Bounds are demonstrated at1024/1025 Index entries and256/257 records, with explicit wire limits admitting the larger cases. A4095-record Pointing field accepts work4098 and rejects4097; two1024-entry attributes require shared2052 work. Native construction keeps its configured1024/256 limits. Proper-prefix mutation loops exercise full checked callback barriers at every byte of the main literals and maximal default list/record fixtures. These deterministic checks complement the generic malformed-packet campaign; they are not coverage-guided fuzzing or proof of every possible malformed input.

The [measured sizes](artifacts/P14-final-local/sizes.txt) preserve SemanticValue16,LayoutContext48,FieldView32,scalar snapshot5312,native8192 snapshot13512 and AttributeInput120 bytes. SectorRecord is160 bytes and the256-record materialization scratch is40960 bytes. The implementation report separately measures coexisting materializer/setter stack frames; no assertion of zero stack growth or ordinary runtime use is made. The independent native operations observe zero ordinary/aligned C++ allocations and positive probes prove both hooks work. C malloc/realloc and all-process heap interception are not claimed.

Profile isolation uses actual Engine and ReceiverHistory: generic structured Current controls return unsupported capability before backend admission, with zero starts/writes and unchanged known sample rate; unsupported Context metadata is rejected. Parsing these fields does not enable a scanner, spectrum processor, new IQ-generator controls or publication behavior.

## Promoted component integration

The separately verified DPF/segmented headers and I9 structural header/tests match their frozen draft copies. The live layout header matches the independently verified additive descriptor-resolver shadow; default traversal remains unchanged. Final integration includes the portable IEEE contract, DPF matrix and additional segmented tests, and the independently authored I9 structural mutation oracle. I9's independent source/oracle verdict belongs to its [separate report](../../drafts/P14-array-cif/verification/report.md); this report verifies copy/integration provenance, not a duplicate independent recursive review.

The generic packet fuzz helper now invokes both16/64 and128/1664 capacities with and without diagnostic request context, checks complete validation before callbacks, and checks fresh parsing after callback failure. Handwritten17-field selector/value seeds demonstrate actual access to the wider path. Its65536-byte input cap and4096 work limit are explicit; it does not exercise I9 recursion through the ordinary descriptor registry. The separate I9 oracle covers nested length/mask/work/depth mutations and remains structural-only, with semantic/native/emission and external peer inputs outside this gate.
