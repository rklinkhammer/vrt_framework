# P14 implementation readiness: CIF0 scalar batch

Status: scalar implementation candidate frozen after developer tests; independent verification pending. Coordinator approved the shared contract and exclusive file assignment. This is one P14 batch and does not complete remaining CIF0, P14 or M5.

Authority: implementation plan P14 card/batch split; architecture semantic/wire separation and bounded storage; protocol appendix §3.2–3.3 and interpretation register; local ANSI/VITA-49.2-2017 (R2024), `/Users/rklinkhammer/Downloads/AV49DOT2-2017-R2024.pdf`. Normative text was extracted with the existing `.venv` PyMuPDF; rendered printed pages 152, 212 and 214 were inspected for gain, device identifier and temperature bit placement.

## Proposed batch

| CIF0 bit | Field | Words | Clause / printed page | Semantic storage |
|---|---|---:|---|---|
| 29 | Bandwidth | 2 | §9.5.1 / 150 | Hertz, signed Q20; nonnegative semantic value |
| 28 | IF Reference Frequency | 2 | §9.5.5 / 153–154 | Hertz, signed Q20 |
| 27 | RF Reference Frequency | 2 | §9.5.10 / 159–160 | Hertz, signed Q20 |
| 26 | RF Reference Frequency Offset | 2 | §9.5.11 / 160–161 | Hertz, signed Q20 |
| 25 | IF Band Offset | 2 | §9.5.4 / 152–153 | Hertz, signed Q20 |
| 24 | Reference Level | 1 | §9.5.9 / 158–159 | DecibelsQ7; low signed16, upper16 reserved zero |
| 23 | Gain | 1 | §9.5.3 / 151–152 | GainStages; stage1 low signed16 Q7, stage2 high signed16 Q7 |
| 22 | Over-range Count | 1 | §9.10.6 / 215 | uint32; nonpersistent per-packet observation |
| 20 | Timestamp Adjustment | 2 | §9.7, §9.7.3.1 / 189–193 | Femtoseconds, signed64; explicitly not timestamp picoseconds |
| 19 | Timestamp Calibration Time | 1 | Table9.7-1, §9.7.3.3 / 190,193 | uint32 seconds; epoch supplied by enclosing TSI |
| 18 | Temperature | 1 | §9.10.5 / 214 | CelsiusQ6; low signed16, upper16 reserved zero |
| 17 | Device Identifier | 2 | §9.10.1 / 212 | DeviceIdentifierValue; first word low24 OUI, second low16 code; remaining bits reserved zero |
| 10 | Ephemeris Reference Identifier | 1 | §9.4.4 / 140 | uint32 Stream ID |

The six remaining CIF0 layouts—Formatted GPS, Formatted INS, ECEF Ephemeris, Relative Ephemeris, GPS ASCII and Context Association Lists—remain explicitly unsupported. Their larger/variable values require separate bounded view/arena and traversal work. No CIF1/2/3 or general CIF7 implementation is included here. New fields initially support implicit or explicit Current only; existing baseline SampleRate current/min/max support is preserved. Later attribute coverage must provide its own per-field qualification.

## Shared contract and exclusive files

Proposed owner I-P14: `include/vita/fields/types.hpp`, `include/vita/codec/packet.hpp`, optionally a new small scalar codec helper; `tests/unit/P14/`; this report. Coordinator owns CMake/status/coverage integration; independent V-P14 owns verification vectors. No `layout.hpp`, arena or packet-builder edit is presently required.

Preserve SemanticValue's first three alternatives and indices (`uint32`, Hertz, PayloadFormat). Append at most 8-byte typed alternatives DecibelsQ7, GainStages, Femtoseconds, CelsiusQ6 and DeviceIdentifierValue; retain 16-byte SemanticValue size with a compile-time assertion and remeasure dependent runtime categories. Keep baseline_descriptors exactly four; add scalar_descriptors searched by descriptor(). Descriptor-driven scalar default/read/write/validation replaces current SampleRate/DPF-only branches. Selectors and diagnostics use the same descriptor registry and shared checked traversal. No scalar serialization staging or operational allocation is needed.

Wire decoding validates reserved bits and lengths, retaining representable signed raw payloads for semantic diagnostics just as existing negative SampleRate decoding does. Semantic construction checks Bandwidth nonnegative and Temperature at least the smallest representable value not below -273.15°C (-17481 Q6 units). Cross-field physical relationships require external stream context and are not guessed by scalar parsing. Device profile/private OUIs are representable without claiming registration. Timestamp Calibration Time remains epoch-neutral until interpreted using the enclosing TSI.

The raw parser's default16 selected-value bound remains explicit: all17 registered scalar/baseline fields cannot be materialized together by its current default builder. This batch need not change that capacity silently; fixtures must exercise supported bounded subsets and capacity failure. Protocol target128 fields remains a later P14 traversal/materialization obligation.

## Decision and input audit

No missing normative input or unresolved architecture decision blocks this scalar-only proposal. Current-only attributes and exact integer wrappers are bounded implementation choices, not new generator controls. The generator remains four-field baseline with SampleRate as its only writable standard field.

I9 is outside this batch. Its already-selected Array-of-CIFs dialect remains explicit and semantic peer use remains disabled until the agreed qualification condition is met. Independent-peer evidence/authoritative clarification needed for unqualified full interoperability is unavailable here; this work does not resolve or conceal that gate.

Over-range Count's §9.10.6-4 has inconsistent repeated “fine resolution” wording alongside explicit TSM bit values. This batch implements its unsigned scalar codec and records nonpersistence only; it does not introduce a new timestamp-pairing interpretation or generator publication behavior. Any later implementation of that semantic pairing must revisit the exact normative wording rather than infer it from a scalar test.

## Verification contract

One coverage row and clause-linked independent literal vectors per field; signed extrema and fractional LSBs; gain stage order; exact femtosecond units; reserved-bit negatives; semantic versus raw validation; selector-only and warning/error diagnostic bodies; Context/Control/state-Ack encode/decode; mixed-field wire order/offsets; short input/output transactional rejection; unsupported structures and unsupported attributes remain visible; no allocations during scalar operation. Run affected P01/P02/P06/P09 and budget/source regressions because SemanticValue and codec/packet.hpp are shared dependencies. No PASS or conformance claim is made by this readiness note.

## Scalar candidate implementation and developer evidence

Implemented the thirteen approved fields in `fields/types.hpp`, with centralized descriptor-driven scalar representation in new `codec/scalar.hpp`, consumed by `codec/packet.hpp`. No layout, arena, builder or runtime production file changed. The first three variant indices and sizeof(SemanticValue)==16 remain asserted. FieldDescriptor aggregate shape and baseline_descriptors remain unchanged. The helper uses exact integer bit representations with no floating-point conversion or heap-backed storage.

Developer gates passed using the system C++23 compiler with exceptions/RTTI disabled: `p14_scalars` and standalone `p14_public_usage`; `p14_scalars` also passed AddressSanitizer/UndefinedBehaviorSanitizer. Existing P02 multi-translation-unit codec regression passed by isolated direct compile/run. Scalar tests cover thirteen literal wire vectors, combined ordered layout, every truncated packet prefix, unchanged short-output buffers, reserved padding, semantic rejection versus raw signed-value preservation, extrema, Current attribute behavior, selectors, diagnostics, Control and StateAck. Coordinator owns the full optimized regression and independent verifier owns the separate sanitizer/cross-check gate. These developer checks are not an M5 claim.

Frozen source SHA-256:

| File | SHA-256 |
|---|---|
| include/vita/fields/types.hpp | ec1591dcee1a1a7225a3edab47adfa0ff752b5e0f6d4311af336db9aa96ff930 |
| include/vita/codec/packet.hpp | c7e4d1f5cbafc6f819a5907a5025b99c0e1c630028003718c4bd2520d5250993 |
| include/vita/codec/scalar.hpp | e76c7ad4099b5997b60713447af74d162455722cbd85d965911f4e45d55aa5d4 |
| tests/unit/P14/scalars.cpp | 265b59d74b2db0fc19835ef2d7f6dcad0003c2c70b51411ce544a86b34482f8b |
| tests/unit/P14/public_usage.cpp | ad992c5811722ca301193a23791e91de2b0368a8eb309069f24a961dbd2072e6 |
| tests/unit/P14/CMakeLists.txt | 17d5bddd115407aaa5a286f280a820288bbd61dd40a85b7353cf06682be16a47 |
