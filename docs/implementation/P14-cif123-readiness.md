# P14 CIF1/CIF2/CIF3 readiness

Read-only inventory, 2026-09-19. No production files, tests, or builds were changed. Scope is the remaining standard fields in protocol appendix §3.2, implementation-plan P14/M5, and ANSI/VITA 49.2-2017 (R2024), local `AV49DOT2-2017-R2024.pdf`. Page references below are **printed** pages (PDF physical page = printed + 16). IDs are exact `{CIF number, bit number}`, not proposed C++ enumerator spellings. `sN/uN Qf` means an N-bit signed/unsigned native fixed-point integer with f fractional bits; retain exact raw precision rather than using floating point as storage.

Most fixed fields can proceed without a new architecture decision. **Complete Beam Width and Barometric Pressure semantic conversions cannot be selected consistently from the supplied standard.** Their conflicting rules and decision options are recorded below. Spectrum's Weighting Factor also lacks a numeric encoding definition in the reviewed field clause; do not infer IEEE float or a Q format. The existing I9 Array-of-CIFs interpretation remains explicitly peer-dependent. These limitations must stay visible; this note is an implementation proposal, not a coverage or interoperability claim.

Continuation update: the user accepted D-P14-1/2 raw-code-only coverage on 2026-09-19. Beam Width is included in the current fixed CIF1 batch; Barometric Pressure can use raw codes in its later batch. Exclusions and proposed alternatives below record the original readiness assessment, not a reopening of those accepted decisions. The subsequent Probability/general conversion policy is tracked separately in [P14 decisions](P14-decisions.md).

## Proposed independent batches

1. CIF1 signal scalars, simple identifiers/bitfields and fixed status values, excluding Beam Width. Use existing exact scalar machinery and independent literal/reserved-bit vectors.
2. CIF2 identifiers plus CIF3 Network ID. UUID is a fixed structured native value; use owned arena storage if needed to preserve the compact semantic variant. Plural SID labels are **single SID words**, not lists.
3. CIF3 femtosecond scalars and unambiguous environmental fields, excluding Barometric Pressure. Age/Shelf Life and Timestamp Details follow as context-dependent/fixed structured values.
4. CIF1 Index List and 3D Vector Structure using the approved snapshot-owned bounded arena and shared native/wire extent traversal. Sector/Step-Scan needs prologue-dependent record extents. Spectrum can expose checked structure only until its unresolved coefficient encoding is settled. Array of CIFs follows existing I9 and nested traversal limits.

Codec support does not make these fields writable on the four-field IQ device. Baseline profile permissions and historical Context/AckS semantics remain unchanged. Query/cancel selectors and one-word warning/error diagnostics must not inherit ordinary value extents. Reserved-bit errors are structural; valid wire representations with disallowed physical values remain distinguishable semantic errors.

## CIF1 fixed fields

| ID | Field | Words | Exact representation / checks | Source |
|---|---|---:|---|---|
| 1/31 | Phase Offset | 1 | Lower16 s16 Q7 radians; upper16 reserved zero. | §9.5.8, p157 |
| 1/30 | Polarization | 1 | Upper tilt and lower ellipticity, each s16 Q7 radians. Reference orientation needs class documentation. | §9.4.8, pp145–146 |
| 1/29 | 3D Pointing Vector | 1 | Lower azimuth u16 Q7 degrees (0..511.9921875); upper elevation s16 Q7 degrees (-90..90). Do not silently cap azimuth at360. | §9.4.1.1, p134 |
| 1/27 | Spatial Scan Type | 1 | Generic16 ID: lower16 value, upper16 zero; application enumeration. | §9.4.11, p148; §9.8 |
| 1/26 | Spatial Reference Type | 1 | User spatial ID31..16; reserved15..4; reference3..2 (unspecified/ECEF/platform/array); beam1..0 shown in figure. Preserve class interpretation, do not substitute Generic16. | §9.4.12 Rules2–3/Fig1, p148 |
| 1/25 | Beam Width | 1 | Upper horizontal/lower vertical16-bit quantities; signedness/full-range conflict below. | §9.4.2, p137 |
| 1/24 | Range | 1 | u32 Q6 metres, exact max `(2^32-1)/64`; no reserved bits. Meaning of distance is class-defined. | §9.4.10, pp147–148 |
| 1/20 | Eb/No and BER | 1 | Upper Eb/No, lower BER, each s16 Q7 dB; `0x7fff` unused sentinel. Normal max `0x7ffe/128`; BER cannot exceed0dB except sentinel. | §9.5.17, pp163–164 |
| 1/19 | Threshold | 1 | Upper stage2/lower stage1 s16 Q7, dB or dBm. Single stage2 unused marker differs by mode (0dB or -256dBm); window stage2 > stage1. Mode is documented, never guessed from raw bits. | §9.5.13, p162 |
| 1/18 | Compression Point | 1 | Lower16 s16 Q7 dBm, upper16 reserved; input-referred1dB compression. | §9.5.2/Fig1, p151 |
| 1/17 | Second/Third-order Intercept | 1 | Upper2IIP/lower3IIP s16 Q7 dBm; `0x7fff` sentinel when unused/no distortion. | §9.5.6, pp154–156 |
| 1/16 | SNR/Noise Figure | 1 | Upper SNR s16 Q7 dB (`0x7fff` unused); lower NF s16 Q7 dB, nonnegative, zero unused. Preserve distinct sentinel meanings. | §9.5.7, p156 |
| 1/15 | Auxiliary Frequency | 2 | s64 Q20 Hz, most-significant word first; RF/IF frequency format. | §9.5.14, pp162–163 |
| 1/14 | Auxiliary Gain | 1 | Two signed16 Q7 gain stages, same order/rules as Gain. | §9.5.15, p163; §9.5.3 |
| 1/13 | Auxiliary Bandwidth | 2 | s64 Q20 Hz, nonnegative semantic bandwidth; retain negative structural raw value. | §9.5.16, p163; §9.5.10 |
| 1/6; 1/5 | Discrete I/O32; I/O64 | 1; 2 | Native unsigned bitset32/64; user-defined bit meanings; no inferred hardware action. | §9.11, p218 |
| 1/4 | Health Status | 1 | Lower16 ID, upper16 reserved; profile-defined health enumeration. | §9.10.2, pp212–213 |
| 1/3 | V49 Spec Compliance | 1 | u32 codes1=49.0,2=49.1,3=49A,4=49.2; other codes not assigned here. | §9.10.3, p213 |
| 1/2 | Version/Build | 1 | Year31..25 =2000+u7; day24..16 =1..366; revision15..10 u6; user9..0 u10. Do not invent leap-year validity beyond the stated day range. | §9.10.4/Table1, p214 |
| 1/1 | Buffer Size/Status | **2** | Word1 u32 capacity bytes; word2 reserved31..16, level15..8, status7..0. Empty level0; fullness encoding is hardware-documented (not universally255). | Fig9.10.7-1 p215; Rules1–3 p216; summary anomaly below |

## CIF2 identifiers (all fixed)

`G16` means a lower16 unsigned identifier and upper16 reserved zero (§9.8 Rule2). `U32` means one opaque full word where no narrower representation is specified; this does not invent identifier assignments. All names/IDs are from matrix Table9.1-1 and the local appendix.

| IDs | Fields | Words each | Native constraints / clause |
|---|---|---:|---|
| 2/31 | Bind | 1 | Bit0 association/disassociation; other bits reserved in Fig9.8.1-1; §9.8.1 p200. Binding is scoped to the same structure/array index. |
| 2/30,29,28,27 | Cited, Sibling, Parent, Child SID | 1 | U32 SID each, **not variable arrays**; Table9.8.2-1 p200, §§9.8.2.1–3 pp201–202. Cited SID changes described entity, not packet routing identity. |
| 2/26 | Cited Message ID | 1 | U32; §9.8.4 p203. Does not replace prologue MID. Semantic query combinations require the clause's restrictions. |
| 2/25,23 | Controllee ID, Controller ID | 1 | U32; §9.8.3 pp202–203. Does not override prologue addresses. |
| 2/24,22 | Controllee UUID, Controller UUID | 4 | Native UUID128 in §8.2 order; §9.8 Rule4 and Table9.8.3-1. No borrowed wire pointer in immutable snapshot. |
| 2/21,20 | Information Source, Track ID | 1 | U32 linkage; §9.8.5/Table1 and §9.8.6 Rule1, p204. Information-source assignments are external/class-defined. |
| 2/19 | Country Code | 1 | Lower11 code; bit15 selects ISO numeric (1) or user-defined (0); bits31..16 reserved. Rules specify lower11 code bits, but Fig9.8.7-1 draws12; see anomaly note before freezing the low reserved mask. §9.8.7 pp204–205. ISO assignment validation is a separate supplied-table concern. |
| 2/18 | Operator | 1 | G16, §9.8.7 Rule5 p205. |
| 2/17,16,15 | Platform Class, Instance, Display | 1 | U32 opaque linkage; Table9.8.8-1/Rules1–3 pp205–206 specify word counts/meaning but do not select G16 or upper-half padding. Do not invent it. |
| 2/14 | EMS Device Class | 1 | G16: organization15..14=coalition/known/unknown (3 reserved); exciter13, receiver12; class11..0. §9.8.9 Rules1–5 p206. |
| 2/13,12 | EMS Device Type, Instance | 1 | U32 opaque linkage; Table9.8.9-1 p206. No narrower wire type prescribed in the reviewed clause. |
| 2/11,10 | Modulation Class, Type | 1 | G16, §9.8.9 Rules6–7 p207. |
| 2/9,8,7,6 | Function, Mode, Event, Function Priority | 1 | G16, §§9.8.10.1–4 pp207–208. Priority ordering belongs to class documentation. |
| 2/5 | Communication Priority | 1 | U32 opaque priority code; Table9.8.10-1 and §9.8.10.5 pp207–208 do not specify G16. |
| 2/4,3 | RF Footprint, RF Footprint Range | 1 | Generic32 links to KML; **not numeric distance**. §9.8.11 p208. Matrix/appendix put Range at2/3; local table incorrectly repeats2/5 (Communication Priority). |

No demanded CIF2 wire layout depends on obtaining ISO/KML/VICTORY databases. Those inputs are necessary only for particular assignment validation, display or device behavior; a bounded typed identifier codec can preserve the values without claiming those interpretations.

## CIF3 temporal/environmental fields

| IDs | Fields | Words each | Native representation / checks / source |
|---|---|---:|---|
| 3/30 | Timestamp Skew | 2 | s64 femtoseconds (not prologue picoseconds), full signed range. §9.7 Rules1–2 p189; §9.7.3.2 p193. |
| 3/27,26,25,24,23,22,21,20 | Rise, Fall, Offset, Pulse Width, Period, Duration, Dwell, Jitter | 2 | s64 femtoseconds, MSW first. §§9.7.1.1–8 pp191–192 and definitions pp188–189. Durations/jitter have physical nonnegative semantics; Offset/Skew are signed differences. Never reject negative raw wire merely because a device cannot apply it. Jitter applicability rules depend on the other selected temporal fields. |
| 3/17,16 | Age, Shelf Life | `T = (TSI!=0 ? 1 : 0) + (TSF!=0 ? 2 : 0)` | Tagged unsigned timestamp components selected by **enclosing** prologue TSI/TSF; do not reinterpret every fractional type as ps. §9.7.2 pp192–193; Table9.7-1 lists1/2/3, not0. A selected field with both codes0 should be diagnosed as unavailable representation, not silently omitted; freeze that validation rule explicitly. |
| 3/31 | Timestamp Details | 2 | Word1 user31..24, reserved23..19, G18, TSE17..16, LSH15..14, LSP13..12, source11..9, E8, signed8 POSIX offset7..0; word2 unsigned epoch seconds. §9.7.3.4 pp194–198. TSE0 unspecified/1UTC/2GPS/3POSIX; LSH/LSP/source codes and epoch pairs must follow Tables1–6 and Rules14–23. No clock qualification inferred from mere field presence. |
| 3/7,6 | Air, Sea/Ground Temperature | 1 | Lower s16 Q6 Celsius, upper16 zero; semantic minimum-273.15°C, maximum511.984375°C. §9.9.2 Rules1–2/Observation1 pp210–211. |
| 3/5 | Humidity | 1 | Lower u16 rational `raw*100/65535` percent; upper16 zero; preserve exact raw ratio. §9.9.2 Rule3 p211. |
| 3/4 | Barometric Pressure | 1 | Low17 bits, high15 reserved; physical scaling unresolved below. §9.9.2 Rule4 p211. |
| 3/3 | Sea/Swell State | 1 | G16: sea4..0 and swell9..5 codes per Table9.9.1-1 (0..9); user15..10. §9.9.1 p210. |
| 3/2 | Tropospheric State | 1 | G16 application enumeration; §9.9.1 Rule4 p210. |
| 3/1 | Network ID | 1 | Generic32; §9.8.12 p209. |

Timestamp Details is fixed structure, Age/Shelf Life are context-dependent extents, and the remaining rows are fixed scalars/bitfields. Source-time conversion/qualification still requires explicit profile and clock inputs. No third-party time standard is needed to preserve these wire fields; implementing actual IEEE1588/IRIG synchronization is outside this codec batch.

## CIF1 variable/structured values

| ID | Field | Extent and native contract | Source |
|---|---|---|---|
| 1/28 | 3D Vector Structure | `H + R*N` words; H3 or4 (optional global Index/Reference/Beam), R1 or2. Header contains total size, H:u8/R:u12/N:u12, selector word. Selector31 adds Index/Reference/Beam;30 requires vector; other selector bits reserved. Index upper16, reference3..2, beam1..0; middle12 reserved. Global index0; per-record reference overrides global. Copy bounded native records into snapshot-owned arena. | §§9.3.1,9.4.1.3–5 pp130,135–137 |
| 1/7 | Index List | `2 + ceil(N*entry_bytes/4)` words. Second word entry-code31..28 =1/2/4 for8/16/32bits, reserved27..20, N19..0. Entries packed MSB first; final unused bytes zero. Preserve selected width and order. Zero N is representable though warning is recommended; do not invent a hard malformed rule. | §9.3.2 pp131–132 |
| 1/10 | Spectrum | Fixed13 words: type1, window1, transform-count1, window-count1, resolution2, span2, averages1, weighting1, F1/F2 indices2, window-delta1. Type fields: spectrum7..0, averaging15..8, delta-kind19..16, high12reserved. Resolution/span s64Q20Hz; indices signed32; counts unsigned32. Delta u32 ns/sample-count or s32Q12 percent (-524288..100). Weighting encoding unresolved; see below. | §§9.6.1.1–10 pp165–179 |
| 1/9 | Sector/Step-Scan | `3 + R*N` words; H3, selector19..0 reserved. Required31 sector-u32 (1word),30 F1(s64Q20Hz,2). Optional29F2(2),28bandwidth(2),27step(2),26pointcount(1),25gain(1),24threshold(1),23dwell(2),22start(T),21time3(2),20time4(2). Compute R from actual selectors/prologue; don't trust declared R alone. Specific timing rules override erroneous summary1word entries. | §§9.6.2.1–14 pp180–187 |
| 1/11 | Array of CIFs | Existing **I9** interpretation: mandatory3words + five CIF words, HeaderSize7, zero optional-selector word, N records beginning with u32 index and then selected fields/attributes. Total `8 + R*N`; shared recursive extent resolver; equal R for each record. No inferred padding or four-CIF alternate dialect. | §9.13.1 pp222–223; protocol interpretation I9 |

Spectrum subfield constraints include spectrum codes0..4 or128..255, averaging bits0..5 only (smoothing cannot stand alone), delta-kind0..3, table-defined window IDs plus class-defined IDs>=100. Meaningful/default/not-used control values must retain the distinctions in §§9.6.1–9.6.1.10. Table9.6.1.2-1 includes window names/coefficients; using these identifiers needs no external paper. Actually generating every window may require the cited Harris paper/equations and is not implied by codec coverage.

All variable rows use approved owned-arena typed-copy APIs, never public unchecked arena-relative refs. One shared traversal must validate native encode and bounded wire decode extents, per-record layout, nested attributes, reserved bits, arithmetic and total packet bounds. Apply appendix §3.3 limits:256 records per field,1024 index/association entries,128 selected fields, depth4,4096 visited units, and configured arena bytes. Preserve `resource_limit` versus malformed distinction. General traversal/CIF7 support is a separately gated prerequisite, not a reason to claim all structures already implemented.

## Conflicts and required decisions

**B1 — Beam Width semantic encoding, verified visually on printed p137 / PDF page153.** Rule9.4.2-2 says “range of 0 to 360 degrees, inclusive.” Rule9.4.2-3 says “two’s-complement format in the lower 16 bits” and places the radix at bit7. Figure9.4.2-2 contains upper horizontal/lower vertical16-bit halves. Signed16Q7 cannot represent360; maximum is255.9921875, and raw `0xb400` means-152 signed versus360 unsigned. Neither changing signedness nor silently clipping to255.992 fulfills both rules. Recommended decision: retain unsupported semantic conversion until an authoritative erratum or explicitly selected peer/class interpretation is supplied. Options to approve explicitly are unsigned16Q7 for both halves (preserves stated angular range, overrides signedness text), or a signed-Q7 restricted subset (cannot claim full0..360 support). Raw extent alone is unambiguous; do not advertise full semantic support from it.

**B2 — Barometric physical scaling, verified visually on printed p211 / PDF page227.** Rule9.9.2-4 simultaneously specifies bits16..0, an LSB of1/131071Pa, `FFFF` representing131071Pa, and upper15 bits reserved. These statements cannot describe one linear encoding: `65535/131071` is approximately0.5Pa, not131071Pa;17-bit full scale would be `1ffff`, not `ffff`. Recommended decision: defer Pa conversion until authoritative correction or explicit peer interpretation. A plausible corrected17-bit integer-Pascal interpretation uses1Pa/LSB and `1ffff`=131071Pa; this changes two assertions and is **not selected here**. Literal fractional scaling and a16-bit full-scale ratio are competing interpretations, neither consistent with all the rule. The structural low17/high15 distinction can be preserved without claiming a pressure conversion.

**B3 — Spectrum Weighting Factor encoding is missing in the reviewed clause.** §9.6.1.8/Fig1 on p176 specifies a32-bit field representing alpha, with default0, but supplies no radix, signedness, float encoding, or scaling. §9.6.1.7 Rule3 says class documentation defines how averages and weighting are used. Do not choose float32/Q31/Q32 by analogy. Preserve the raw coefficient word in structural work; complete native alpha conversion needs a supplied class convention or authoritative clarification. This is a missing representation input, not an arena architecture issue.

**Existing I9 remains unchanged.** Five CIFs after3basewords versus HeaderSize7 is already recorded/accepted as an explicit interpretation. No unqualified peer interoperability claim until peer agreement.

Other anomalies have more specific resolving evidence and do not require inventing a dialect:

- Buffer Size summary Table9.10-1 p211 says1word; detailed Figure9.10.7-1 p215 and field rules p216 define capacity plus status,2words.
- Sector table p181 gives1word for all four timing subfields; specific rules9.6.2.10-2/.12-1/.13-1 p186 mandate64-bit Fractional Time; StartTime Rule9.6.2.11-1 uses prologue TSI/TSF. Use those explicit definitions.
- Temporal summary Table9.7-1 p190 misplaces Jitter at3/21 and omits Dwell's bit; authoritative CIF matrix p126 and appendix select Dwell3/21, Jitter3/20.
- RF Footprint Range Table9.8.11-1 p208 says2/5, colliding with Communication Priority; CIF matrix and appendix select2/3.
- Spatial Reference Rule9.4.12-1 repeats Spatial Scan prose, but Rules2–3 and its figure specify the distinct32-bit layout. Range's rounded maximum differs by one LSB from its explicit u32Q6 representation; do not manufacture an extra representable endpoint.
- Country Code Rules9.8.7-1/2/3 p204 repeatedly specify lower11 code bits; Figure9.8.7-1 p205 draws code11..0 and reserved14..12. Prefer the explicit repeated eleven-bit rules (code10..0, reserved14..11), but record and independently approve that rule-over-figure choice before advertising strict padding validation. Bit15 also has a misleading “User Defined” drawing label; Rules2–3 explicitly say1=ISO,0=user.
- Generic identifier Rule9.8-3 p199 refers to bits20..31 of a “24 Bit Identifier.” None of the above fixed layouts needs that unspecified generic24 type. Do not use it to infer narrower IDs where only one-word linkage is defined.

The local source provides the necessary packet layouts for safe batches; it does not provide application identifier databases, hardware health/discrete-I/O meanings, full KML behavior, profile priority policies or qualified clock inputs. Keep those interfaces typed and explicit. Stop before implementing the affected semantic conversions above, while preserving the already-independent CIF0 structured work and its verification.

## Sector header-count clarification

Independent implementation-stage source review distinguishes the physical three-word header from the encoded HeaderSize byte. Sector/Step-Scan has no optional global header, so §9.3.1 pp130–131 and §9.6.2.1 p180 require encoded HeaderSize0. Earlier shorthand H3 referred to the physical extent and must not be copied into that byte. Pointing Vector and I9 have their own explicit overrides; neither override applies to Sector. See [independent structure verification](P14-cif1-structures-verification.md).
