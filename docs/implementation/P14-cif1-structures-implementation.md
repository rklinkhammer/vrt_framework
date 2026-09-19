# P14 nonrecursive CIF1 structures — implementation

Status: implementation candidate frozen; developer checks PASS. Independent verification and aggregate integration are separate gates.

Implemented PointingVectorStructure (CIF1/28), IndexList (1/7), Spectrum (1/10), and SectorStepScan (1/9). Authority is the supplied ANSI/VITA49.2-2017(R2024), §§9.3.1–2, 9.4.1.3–5, 9.6.1 and 9.6.2 (printed pp130–137 and165–187). Array-of-CIF/I9 remains a separate batch. No required external input or unresolved architecture decision blocks these four fields.

## Ownership and APIs

Typed IndexListInput, PointingVectorInput, SpectrumValue and SectorStepScanInput setters copy into the existing snapshot-owned native arena. PointingReference names index/reference/beam explicitly; SectorStartTime is a distinct absolute timestamp value with explicit inherited format. Each CIF7 base attribute may own a different list/record shape. Probability and Belief stay one word without native storage or timestamp binding. Getters borrow immutable storage and return checked copied native records; wire getters decode bytes without native aliasing. Existing atomic edit/workspace and raw-reference rejection contracts apply.

SemanticValue remains16B, LayoutContext48B, and default scalar snapshot5312B. An example16-field/8192-byte-arena native snapshot remains13512B. No runtime storage category changes. Native headers are Index8B, Pointing12B, Sector12B; Pointing record10B, Sector record160B, Spectrum56B. Transient AttributeInput remains120B. Materialization uses bounded temporary arrays: Index4096B, Pointing2560B, Sector40960B. Apple Clang -O2 -fstack-usage measured a41568B materialize_into frame and13712B nested setter frame for a16-field/8192-byte-arena builder; the materializer and nested setter frames coexist on the call stack. This generic opt-in path is not an ordinary runtime stack claim; these are example compiler/configuration measurements, not a universal stack upper bound. Caller-selected larger snapshots/workspaces require their own stack budget. The developer test main frame124752B includes several simultaneous fixtures.

## Wire, semantics and bounds

The shared traversal consumes per-field extent metadata from common native/wire shape helpers. Default limits are1024 Index entries,256 records and4096 shared work units. Wire callers may explicitly raise entry/record limits within encoded count/extent bounds; native setters remain bounded at1024/256. Checked wire total/header/record equations precede resource classification. Index work is2+entry count; record structures charge physical header and record words. Every selected attribute contributes to aggregate work. Layout signatures depend on shape and timestamp format, never native padding or field content.

Index widths are1/2/4 bytes, entries are MSB-first, unused tail bytes zero, and zero entries are representable. Pointing has required vector bit30, optional record reference31, explicit header codes3/4, global index zero and reserved beam code3 rejected. Raw zero reference/beam values preserve inheritance meaning rather than being silently resolved. Zero Pointing/Sector records are representable.

**Source correction:** the initial preparation incorrectly treated Sector HeaderSize as the physical three-word header. Generic §9.3.1 defines that code as optional application-specific words; Sector §9.6.2.1 declares no global section and imports the template. Therefore its wire HeaderSize code is0, while its physical header is3 words. Pointing has an explicit3/4 override and remains unchanged. Dwell/Time3/Time4 use two femtosecond words under their detailed rules, overriding the erroneous one-word summary. Time3/4 retain signed values; Dwell Current values reject negatives.

Sector selected StartTime requires explicit nonzero TSI/TSF representation and agrees with the enclosing bound layout/envelope. All16 combinations are covered;00 is unavailable for selected values. Native construction may precede enclosing layout binding, but measurement/encoding requires it. Mismatched rebind and prewrite validation fail transactionally. Probability-only Sector needs no binding. No fractional format or effective timestamp is invented.

Reserved bits/code validation is distinct from Current semantic validation. Structurally representable invalid Current values remain raw-readable; typed Current admission/materialization rejects them. NonCurrent base attributes preserve representation without applying Current physical bounds. Spectrum rejects reserved spectrum/window codes structurally; Current additionally rejects smoothing alone, negative resolution/span and percentage delta above100%. User window IDs100 and above are preserved. Alpha/weighting remains raw32 under the accepted raw-code policy. Class-specific window, averaging and scan/individual-step execution semantics are not claimed by this generic codec.

## Developer evidence

Isolated clang++ C++23 -Wall -Wextra -Werror builds/runs PASS for cif1_structures.cpp and cif1_structure_bounds.cpp; both repeat PASS under AddressSanitizer+UndefinedBehaviorSanitizer. Prior CIF7, CIF3 and CIF2 UUID developer checks also PASS in direct regression builds. Cases include literal wire words, ownership/copies, all timestamp formats, scalar/native CIF7 variations, exact and expanded bounds, truncation, reserved/code errors, invalid semantic materialization, generation/shape changes, failed edits and short output leaving output unchanged. Independent verifier owns its separate literal and source oracles. No conformance or deployment performance claim follows from these checks.

## Frozen source SHA-256

- `include/vita/fields/cif1_structured.hpp`: `a9c7bd4b723ea7a5561ec967bec85e0d1f4d2eae70ed176cc4d364c562ad7881`
- `include/vita/fields/structured.hpp`: `1e1cddb5751acefdc3b399e6571a124e9195a1d5895b2990fe365607f884f49b`
- `include/vita/fields/types.hpp`: `ea237902e2e84ea5d10af8fb7e1377d72158862f991375a443260ed2cc287502`
- `include/vita/fields/packet.hpp`: `3909b29af0a5f1075fbcc7a3f3baa5e0772a31e9ea4ee3bb9f847b4deb3cb780`
- `include/vita/codec/layout.hpp`: `0b00ed54e7bfcaabccbcb5cdcc0e4af929ab9c03b167b0b2ddf2d168c25f0e0b`
- `include/vita/codec/cif1_structured.hpp`: `13abc039152a14c17ee64824018dd1bf97bb0f44d07cd32200574069c2e41f7a`
- `include/vita/codec/structured.hpp`: `3562fb66f1095ee7a33a89ec3f42ba191f1ab60f20b21a39efb117ac8383c413`
- `include/vita/codec/packet.hpp`: `3c1bf82ac4e24b7366d300abb287494420350bf49c22bfb6e7a682f8e5cb522f`
- `tests/unit/P14/cif1_structures.cpp`: `3a152294a585e8be7fc33a066245b1b4942f576b2b2f3b2390f23882dfe29b54`
- `tests/unit/P14/cif1_structure_bounds.cpp`: `e8bfb3467f9e405542c2fcf9b1b99feaded386ff8a78e3a9b30f25773d14c87e`
- `tests/unit/P14/CMakeLists.txt`: `957464d34f1c7a5d5fc80cf2a1497b2012c39ed5791ab7402c977e797cc0453e`

## Corrective re-freeze: native view provenance

Independent verifier demonstrated UBSan invalid-bool behavior by constructing a public NativeRecordView<SectorRecord> from arbitrary all-FF bytes. The initial candidate was therefore not accepted. The Bytes constructor is now private, with only the two narrowly scoped internal arena parser friends. The template accepts only the intended trivially copyable PointingVectorRecord/SectorRecord types; safe empty/default and copy/move operations remain available. The supported public mint path is the typed snapshot-owned arena, whose values originate from valid typed objects. The public checked wire views decode integer members and construct optionals normally; they never cast/copy arbitrary wire storage into native optional-bearing records.

Audit also checked existing detail::native_object and native header readers: these are internal trusted-native-object helpers, not public checked byte decoders. Their explicit precondition now states that input originated from typed arena copies. This repair does not claim C++ detail symbols are inaccessible capabilities. Byte extent checks alone cannot validate arbitrary native bool/optional representations. IndexList's public byte reader copies uint32 or assembles wire integers and does not have that object-representation risk.

Revalidation: plain structures developer test PASS; ASan/UBSan bounds test PASS including nonconstructibility/default/copy assertions. No layout/storage-size changes. The earlier SHA list records the rejected initial candidate; the following hashes supersede only the corrected files. Independent revalidation remains a separate gate.

- `include/vita/fields/cif1_structured.hpp`: `70b7713a069b0b72a4385568b58ad6b09db686071db0b558c41cdb38678e5438`
- `include/vita/fields/structured.hpp`: `1ed9b635e1b89211023ca62bb937fefb701d95f0a4c87673de0a17bcb28a4782`
- `tests/unit/P14/cif1_structure_bounds.cpp`: `08b7e20347e09b3e2618c5520280d54eb03873c78eb193f92be4efbdcb76394d`
