# P14 CIF1 fixed-field implementation

Candidate frozen after developer checks; independent V-P14 and coordinator integration gates are separate. Scope is21 fixed fields only. No CIF1 variable structures, general CIF7 attributes, CIF2/CIF3 additions or hardware qualification are included.

Authority: P14 implementation plan, protocol registry inventory, P14-cif123-readiness.md and accepted D-P14-1 in P14-decisions.md. Clause/page references are to the supplied ANSI/VITA-49.2-2017 (R2024), printed pages. Field identity does not change the four-field IQ profile's supported controls.

| CIF1 bit | Field/type | Words | Source |
|---:|---|---:|---|
|31|PhaseOffset / RadiansQ7|1|§9.5.8 p157|
|30|Polarization / PolarizationAngles|1|§9.4.8 pp145–146|
|29|PointingVector3D / PointingAngles|1|§9.4.1.1 p134|
|27|SpatialScanType / uint32 lower16|1|§9.4.11 p148|
|26|SpatialReferenceType / SpatialReferenceValue|1|§9.4.12 pp148–149|
|25|BeamWidth / BeamWidthCode|1|§9.4.2 p137; D-P14-1|
|24|Range / MetresQ6|1|§9.4.10 pp147–148|
|20|EbNoBer / EbNoBerValue|1|§9.5.17 pp163–164|
|19|Threshold / ThresholdValue|1|§9.5.13 p162|
|18|CompressionPoint / DecibelsQ7|1|§9.5.2 p151|
|17|InterceptPoints / InterceptPointsValue|1|§9.5.6 pp154–156|
|16|SnrNoiseFigure / SnrNoiseFigureValue|1|§9.5.7 p156|
|15|AuxiliaryFrequency / Hertz|2|§9.5.14 pp162–163|
|14|AuxiliaryGain / GainStages|1|§9.5.15 p163|
|13|AuxiliaryBandwidth / Hertz|2|§9.5.16 p163|
|6|DiscreteIO32 / uint32|1|§9.11 p218|
|5|DiscreteIO64 / Unsigned64Bits|2|§9.11 p218|
|4|HealthStatus / uint32 lower16|1|§9.10.2 pp212–213|
|3|V49SpecCompliance / uint32|1|§9.10.3 p213|
|2|VersionBuild / VersionBuildValue|1|§9.10.4 p214|
|1|BufferSize / BufferSizeValue|2|§9.10.7 pp215–216|

All added descriptors are Current-only. Existing descriptor indices0–8 and baseline descriptors are preserved. New exact native alternatives occupy indices9–21 and use no heap or arena. `fields/cif1.hpp` contains the value types; `fields/types.hpp` registers fields and semantic validation; shared scalar read/write and packet reserved-bit checks are extended. The approved structured arena/traversal implementation is unchanged.

BeamWidthCode exposes `horizontal_code` and `vertical_code` as two unsigned16 storage codes. Codes such as `0x8000`, `0xb400` and `0xffff` round-trip unchanged. No degree conversion, clipping, inferred signedness or full physical-value interoperability is claimed. This is the accepted interpretation-limited coverage, not a correction to the contradictory standard.

ThresholdValue retains exact stage1/stage2 signedQ7 values without choosing relative/absolute or single/window semantics. The separate `validate_threshold(value, ThresholdMode)` helper requires caller-supplied single_db, single_dbm, window_db or window_dbm and checks the clause's unused-stage2 marker or strict window ordering. The generic builder does not invent this missing class context. Polarization orientation, scan/health identifiers, I/O meanings, buffer fullness/status interpretation and range reference remain class/hardware-defined.

Semantic validation enforces elevation±90degrees while retaining the full unsigned azimuth code range (not silently capped at360); BER at most0dB except unused; nonnegative Noise Figure with zero represented as unused; nonnegative auxiliary bandwidth; compliance codes1–4; and build day1–366 plus explicit bit capacities. Known nullable values cannot equal their wire-unused sentinel. Structural decoding preserves readable semantic-invalid values for diagnostics/materialization rejection. Reserved padding and reserved SpatialReference beamcode3 are rejected before callbacks. Buffer Size follows the detailed two-word figure/rules despite its inconsistent summary-table count.

`OptionalQ7` explicitly stores signed16 plus a known flag, offers nullopt/int16/optional construction, bool/has_value, optional-style dereference, value_or and checked get(). It preserves the difference between a known sentinel-shaped number (rejected by semantic admission) and unknown. This replaced std::optional pairs because the tested libc++ variant grew to24bytes despite8-byte alternatives; an isolated alignas8 experiment did not fix that ABI interaction. Explicit compact state restores16-byte SemanticValue without padding tricks or altered wire values.

Actual arm64 size probe: SemanticValue16, FieldEntry328, scalar snapshot/builder5312 each, native8192 snapshot/builder13512 each, runtime StateSnapshot136. Baseline runtime storage and default16 selected-field bound are unchanged. The21 fields are supported in bounded packets; a default decoder does not silently promise simultaneous materialization beyond its selected-field capacity. Generic128-field capacity remains later work.

Developer evidence: `p14_cif1_fixed` passes direct C++23 no-exception/no-RTTI and ASan/UBSan. It includes21 independent literal expected words, signed/code/sentinel boundaries, all truncated prefixes and unchanged short-output buffers, semantic rejection versus raw structural preservation, reserved padding, mixed CIF0/CIF1 order, selector-only commands, and four-byte diagnostics for two-word normal values. Existing structured developer tests also pass direct compilation/run. Independent golden/no-allocation and full-suite optimized/sanitizer checks remain separately recorded; these checks do not imply P14/M5 completion.

Frozen source SHA-256:

- `include/vita/fields/cif1.hpp`: `9dec442873f27e422251eb5ed8a708bbbc1351defc539493178a16f7d091a253`
- `include/vita/fields/types.hpp`: `f80735aaf4aea2c86db4777722ceeb351a664c15093ae377f7cea6522dee536b`
- `include/vita/codec/scalar.hpp`: `87d741c8a03e8e2daf06fae0431e351f87e9f36044b847d3afe5cfc9c92a0264`
- `include/vita/codec/packet.hpp`: `9cf7dbecf3b0527a496685b7bbbbfb95de7648271450b3bace66b0f1b23504a2`
- `tests/unit/P14/cif1_fixed.cpp`: `b5420d2bb2b07821ceae6529c145773edda2e386a34b392111fcf2aabf7f6590`
- `tests/unit/P14/CMakeLists.txt`: `3d7b40dd2caf2b4002a5166d21a181c2125767176964e7f4d3a5a13cd52c1881`

Integration fixture correction: the provisional aggregate Release run exposed P01's old unknown-selector fixture `{1,31}`, which is now the implemented PhaseOffset field. Updated only that selector to still-unsupported `{3,0}`; the assertion that measuring an unknown layout fails is preserved. The corrected P01 semantics test passes isolated direct C++23 compilation/run. No production change was required and the production candidate hashes remain frozen. `tests/unit/P01/semantics.cpp` SHA-256: `dd44e9af9cebf3f6bc7c9e01cf9c3395c36df1df415947fc3a88e4c0a19eb559`.
