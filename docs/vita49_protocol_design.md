# VITA 49.2 protocol design and verification appendix

Companion to [the framework architecture](vita49_framework_architecture.md), dated 2026-09-17. The original milestone tables record design commitments, not evidence by themselves; current implementation evidence and accepted scope revisions are linked below. Normative references are to the supplied ANSI/VITA 49.2-2017 (R2024), printed page numbering. Software/profile selections are identified as such. The [existing review](architecture_prompt_review.md) remains historical rationale.


Current M5 scope (D-M5-1, user accepted2026-09-19): the IQ operational profile and advertised bounded codecs define software acceptance. Array-of-CIFs is excluded from production support; its separate I9 structural utility is optional. Earlier milestone assignments below are design history where superseded. Implementation evidence and limitations are in the [coverage matrix](implementation/P14-coverage.md) and [operational scope](implementation/M5-operational-scope.md). No universal standard-field support is claimed.

## 1. Information Class documentation

The following completes the software-selected portions of Sections 4 and 10 documentation. `PROFILE_OUI`, `SID`, `CONTROLLER_ID`, `CONTROLLEE_ID`, and `CLOCK_BINDING` are required deployment parameters, validated at startup. They are intentionally not fabricated numerical assignments. The resulting deployment instance is not ready for interoperability sign-off until these parameters and the interpretation register are agreed with its peer.

| Required item | IQ Generator v1 definition |
|---|---|
| Name and code | `VRT Framework IQ Generator v1`, Information Class code `0x0001` under `PROFILE_OUI` |
| Purpose | Synthetic/application-provided complex baseband IQ and the Context/controls needed to interpret and configure it |
| Data stream | `iq.data`: one type-0x1 time-domain Signal Data stream per generator |
| Context stream | `iq.context`: one type-0x4 stream paired to Data |
| Command relationship | `iq.command`: type-0x6 Control/Ack/cancellation relationship paired to Data; separate logical sender counters in each direction |
| Classes | `0x0001` IQ16, `0x0002` IQ32, `0x0003` IQ float32, `0x0010` Context, `0x0020` Command |
| Associations | Data/Context/Command share configured SID; no Context Association Lists in baseline; each generator is a separate Information Stream |
| Reference point | Logical generator sample output immediately before packetization; Reference Point Identifier equals SID |
| Control point | Configured SID plus explicit 32-bit Controllee ID; Controller identified by explicit 32-bit Controller ID |
| Identifier authority | Static deployment registry; uniqueness checked in local peer/binding namespace; no discovery or UUID generation in baseline |
| Time binding | GPS/PPS-conditioned time expected; deployed TSI chosen from UTC, GPS, or documented Other, fixed for the association; TSF real-time picoseconds; no session-relative simulated production default |
| Timing precision | Sample-time interpretation; absolute accuracy is supplied by qualified clock/device binding, not inferred from picosecond units |
| Stream rate | Default 1 MS/s; integer 1–100,000,000 samples/s subject to admission; rate changes at packet boundaries |
| Data publication | Wall-clock paced, normally 256 complete pairs/packet, reduced for MTU or 1 s packetization target |
| Context publication | Full snapshot before initial/changed Data and every elapsed active second; changes stamped with effective sample time |
| Command publication | Event-driven; 100 accepted commands/s/runtime reference admission budget, not a wire limit |
| Source | `0.5*(cos(phase)+j*sin(phase))`, positive frequency sample_rate/16, initial phase zero; application provider replaceable before start |
| Actual deployment values | Mandatory configuration; absent values block external emission, not isolated parser fixture tests |

Section 10.1 allows at most one Data Stream in an Information Stream, and Sections 10.1.2–10.1.3 govern pairing. Type-0x0/SID-less decoding and standalone data-only bindings remain generic capabilities, not an additional paired stream in this Information Class. A standalone binding must supply its own class/profile document and static format mapping; it is not advertised as this paired Information Stream. Likewise, no concrete extension class is emitted by default. Register its separately documented payload before enabling it (Sections 6.4, 7.2, 8.6, 10.2.5.3–10.2.5.4).

## 2. Packet Class option tables

Untimed Control mode 0 does not relax the Data timestamp options below. Baseline Data requires a qualified locked clock at start and only the documented bounded holdover thereafter. Required-metadata recovery follows architecture §7.1, not an implicit same-SID reset.

### 2.1 IQ Data classes

These choices address Table 10.2.5.1-1, Sections 5–6, and Section 9.13.3.

| Option | IQ16 / `0x0001` | IQ32 / `0x0002` | IQ float32 / `0x0003` |
|---|---|---|---|
| Name | IQ16 Time Data | IQ32 Time Data | IQ Float32 Time Data |
| Purpose | Normalized complex source samples | Same, higher integer precision | Same, floating representation |
| Packet type | 0x1, SID included | Same | Same |
| Spectral indicator | Time Data | Same | Same |
| Class ID | Present: configured OUI, Information Class 0x0001, corresponding Packet Class | Same | Same |
| Class pad count | Zero | Zero | Zero |
| Integer timestamp | Present, `CLOCK_BINDING` UTC/GPS/Other | Same | Same |
| Fraction timestamp | Present, real-time picoseconds | Same | Same |
| Packet timestamp | First sample time at reference point | Same | Same |
| Packet Count | Coordinated modulo-16 owner for SID/type | Same | Same |
| Header reserved indicators | Zero | Zero | Zero |
| Packing method | Processing-efficient | Same | Same |
| Real/complex | Complex Cartesian | Same | Same |
| Data Item Format | Signed normalized fixed-point, code 00000 | Same | IEEE single, code 01110 |
| Item packing width | 16 bits | 32 bits | 32 bits |
| Data item width | 16 bits | 32 bits | 32 bits |
| Item fraction size field | 0 (normalized representation) | 0 | 0 |
| Sample ordering | I0,Q0,I1,Q1,...; I precedes Q in wire order | Same | Same |
| Scale | signed value / 2^15 | signed value / 2^31 | normalized float units |
| Event/channel tags | Absent, widths zero | Same | Same |
| Component/channel repetition | None, repeat count 1 encoded as 0 | Same | Same |
| Vector size | 1, encoded as 0 | Same | Same |
| Payload padding | None for complete pairs | Same | Same |
| Trailer | Absent; all trailer indicators and associated Context count unused | Same | Same |
| Baseline VRT prologue | 7 words / 28 bytes | Same | Same |
| Payload sample count | 1–256, further bounded by MTU/pacing | Same | Same |
| VRT packet words | 7+N | 7+2N | 7+2N |
| IPv4 MTU1500 maximum N | 256 default; class maximum 256 | 180 | 180 |
| Frame boundaries | No spectral frame semantics | Same | Same |

Data Packet Payload Format words are `200003cf 00000000` (IQ16), `200007df 00000000` (IQ32), and `2e0007df 00000000` (float32). Sizes/counts use value-minus-one where specified; normalized integer fractional scaling is not encoded as a nonzero Data Item Fraction Size (Rules 9.13.3-9, -12 through -15). DPF meanings come from Tables 9.13.3-1 through -4. Generic codecs accept other legal packing; these classes do not silently change format during a running stream.

### 2.2 Context class `0x0010`

Name: `IQ Generator Context`. Purpose: identify reference point, sample format/rate, data validity, and time calibration for the paired stream. Options follow Tables 10.2.5.2-1 and 9.10.8-1; the template's optional SID entry is overridden by the actual Context packet definition.

| Option | Choice |
|---|---|
| Type / SID / Class ID | 0x4; SID mandatory; Class ID present, OUI/Information Class as above |
| Timestamp | Same TSI/TSF as paired Data; TSM=0, precise event/sample time |
| CIF words | CIF0 only in emitted baseline; other defined CIFs parseable generically |
| Change indicator | Used: set on initial/change snapshots; clear on unchanged periodic refresh |
| Trailer / pad count | No trailer; pad count zero |
| Inclusion | Every full snapshot includes Reference Point, Sample Rate, State/Event, DPF when known |
| Full snapshot size | 14 words with those fields and the 7-word prologue |
| Update delay | Submission attempted at the effective boundary; reference maximum 10 ms, then fault publication/stop Data rather than claim a missed guarantee |
| Unknown required format/rate | Stop affected Data; Valid Data=false if reportable; application-initiated `recover_stream` with fresh paired SID, peer readiness, and known full snapshot required before restart (architecture §7.1) |
| Association lists | Absent; pairing is through SID |
| Other fields | Not published by this generator; unsupported semantics are not fabricated |

| Field / selector | Meaning and range | Persistence / trigger |
|---|---|---|
| Reference Point, CIF0/30 | Configured SID of logical generator output, 32-bit | Persistent; initial/configuration change and refresh |
| Sample Rate, CIF0/21 | Positive integer 1–100,000,000 Hz subject to admission; 64-bit signed fixed-point with 20 fractional bits | Persistent; effective rate change and refresh (§9.5.12) |
| State/Event, CIF0/16 | See bit policy below | Per indicator, never entire-word blanket persistence |
| DPF, CIF0/15 | Exactly one of the three class formats above | Persistent; pre-start format configuration and refresh (§9.13.3) |

State/Event subfield choices are exhaustive for this class:

| Enable / indicator | Baseline policy |
|---|---|
| 31 / 19 Calibrated Time | Used persistently; 1 only when externally calibrated within declared uncertainty; 0 during acquisition/holdover loss policy |
| 30 / 18 Valid Data | Used persistently; 1 when sample production and required metadata are known valid; 0 on unknown required state or generator fault |
| 29 / 17 Reference Lock | Unused for software generator: PPS availability is not a claim that signal-affecting PLLs are locked |
| 28 / 16 AGC/MGC | Unused |
| 27 / 15 Detected Signal | Unused |
| 26 / 14 Spectral Inversion | Unused |
| 25 / 13 Over-range | Unused for baseline sinusoid; custom source requiring it needs documented provider/class behavior |
| 24 / 12 Sample Loss | Event use only when a discontinuity can be associated with resumed Data at the precise timestamp; never persist the event across refreshes |
| 23..20 / 11..8 | Reserved, zero |
| 7..0 user fields | Unused, zero |

Unused enable bits cannot serve as an unknown/reset signal for previously asserted persistent indicators (Rule 9.10.8-13). The runtime uses explicit validity and lifecycle policy instead. Periodic snapshots do not repeat old sample-loss events.

### 2.3 Command class `0x0020`

Name: `IQ Generator Command`. Purpose: change Sample Rate, query supported state, validate/simulate, and cancel pending supported controls. Options follow Sections 8.2–8.5 and Table 10.2.5.5-1 with interpretation corrections below.

| Option | Choice |
|---|---|
| Type / SID / Class ID | 0x6; SID mandatory; normal Class ID present and same class in Acks |
| Header A/L | A distinguishes Control/Ack; L distinguishes cancellation and its Ack; reserved header bit zero |
| Controller / Controllee | Both explicit 32-bit IDs; CE=CR=1, IE=IR=0; generic runtime also supports UUID/omission on another documented binding |
| Timestamp | Immediate mode may omit both timestamp fields; timed mode uses association's TSI and real-time TSF; return timestamp only when a meaningful subtype-specific time is known |
| Precision/windows | Device binding supplies D_e/D_l, A_e/A_l, lead time, uncertainty; tests use main-document reference parameters |
| Action modes | 0 query/no action; 1 isolated dry run; 2 execute; 3 reserved |
| Partial / warning / error | All eight P/W/Er combinations parsed and governed by §4; Controller default P=1,W=0,Er=0 |
| Acks | Separate V/X/S packets as requested; Controller default requests V+X+S and both diagnostic details, NACK=0 |
| Query emission | Mode 0, ReqS=1, no control values; baseline query API emits only ReqS |
| Post-action state emission | Request X together with S (interpretation I8); dry-run state is hypothetical |
| Cancellation | Supported per field, mode 2, P=1, ReqV=0; default ReqX=1, ReqS=0; original IDs/MID and subset selectors, no values |
| Command change indicator | Not emitted; not used for deduplication |
| CIF7/arrays in generator behavior | No writable attribute/array semantics; structurally known unsupported selection receives applicable diagnostics |
| Warning/error details | 32-bit field diagnostics; no semantic-value encoding in V/X bodies |
| Control field | Sample Rate CIF0/21, same numeric representation/range as Context |
| Query fields | Reference Point, Sample Rate, State/Event, DPF; return only selected known fields |
| Other setters | Unsupported; no invented tuner, gain, start/stop, or waveform remote fields |
| No-timestamp baseline sizes | Query/cancel 7 words without Class ID, 9 with Class ID; rate control adds 2 value words |
| Normal timestamped rate control | 14 words with Class ID and both 32-bit IDs |

Sample Rate modification policy: exact in-range integers execute without adjustment. A positive fractional Hz request within the range is a recoverable precision warning: ties-to-even rounding to integer is permitted only with W=1 and only if the rounded result remains in range and passes admission. Out-of-range, negative, unknown-format, resource, and device failures are errors with no automatic corrective modification. Er=1 therefore does not make those executable in this profile. Generic device classes may define recoverable error adjustments explicitly; they cannot leave them implicit.

Wire diagnostics (Table 8.4.1.2.1-1): bit31 not executed, bit30 device failure, bit29 unsupported field, bit28 range, bit27 precision, bit26 invalid value, bit25 timing. Baseline adds documented user error bits: 1 resource exhausted, 2 dependency blocked, 3 state indeterminate, 4 identity/message conflict. User bits 5–12 unused; reserved bits remain zero. A condition is not duplicated as both warning and error. Cancellation uses its specific not-cancelled/device-failure meanings and keeps parameter bits 28–19 zero (§8.4.1.2.1, §8.5). State indeterminacy stays explicit locally even when no suitable wire value exists.

### 2.4 Extension registration contract

`ExtensionClass` provides full class identity, allowed prologue/header options, payload bounds, layout validator, optional encoder, semantic dispatch callbacks, and declared CAM extensions. Registration is configuration-time, immutable while streams run, and rejects duplicate class keys. Unknown extension payloads are opaque bounded bytes; unknown extension Command semantics cannot trigger device callbacks. Do not validate arbitrary vendor content with standard CIF assumptions. This architecture defines no vendor field or borrowed OUI. A real extension deployment must supply location/type/meaning for every payload field and custom bit (Sections 8.6, 10.2.5.3–5).

## 3. Coverage matrices and bounds

### 3.1 Packet family wire coverage

`M1` means planned envelope support in milestone M1; `M2` adds transactions; `M5` closes the advertised bounded codec scope under D-M5-1, excluding Array-of-CIFs. All are encode/decode/validate commitments, not passing tests today.

| Code | Envelope encode/decode | Payload / dispatch | Generator use |
|---|---|---|---|
| 0x0 Signal Data without SID | M1, legal time/spectral options | Opaque or class-known sample views | Only separately documented standalone binding |
| 0x1 Signal Data with SID | M1 | Segmented payload/trailer views | Time-domain IQ output |
| 0x2 Extension Data without SID | M1 | Registered codec or bounded opaque | None by default |
| 0x3 Extension Data with SID | M1 | Registered codec or bounded opaque | None by default |
| 0x4 Context | M1 envelope/baseline; M5 remaining fields | Context traversal/cache | Full snapshots and change events |
| 0x5 Extension Context | M1 envelope | Registered layout, optional generic CIF use | None by default |
| 0x6 Command | M1 envelope; M2 V/X/S/cancel | Mode-dependent selector/value/diagnostic layouts | Rate control/query/cancel |
| 0x7 Extension Command | M1 envelope; M2 common transactions | Registered semantics required for execution | None by default |
| 0x8–0xF | Reject | Reserved packet types | Never emit |

### 3.2 Standard field registry scope

All named standard fields in Table 9.1-1 are assigned a wire-codec milestone below. Baseline generator semantics are limited to the four fields in §2. Reserved bits have no guessed codec. Named rows cover fixed and structured layouts; a row's wire codec includes its field-specific reserved bits and size validation, not merely a byte skip. This is the required implementation coverage inventory; field implementations must add clause-specific fixtures before changing their status to tested.

| CIF | Bit(s) and field | Wire milestone | Generator control / query / publication |
|---|---|---|---|
| 0 | 31 Change indicator; 7,3,2,1 CIF enables | M1 structural | Change indicator in Context; never a value callback |
| 0 | 30 Reference Point Identifier | M1 | Read/query/publish only |
| 0 | 29 Bandwidth; 28 IF Reference Frequency; 27 RF Reference Frequency; 26 RF Reference Frequency Offset; 25 IF Band Offset | M5 | Unsupported setters; no publication |
| 0 | 24 Reference Level; 23 Gain; 22 Over-range Count | M5 | Unsupported |
| 0 | 21 Sample Rate | M1 | Set/query/publish |
| 0 | 20 Timestamp Adjustment; 19 Timestamp Calibration Time; 18 Temperature; 17 Device Identifier | M5 | Unsupported |
| 0 | 16 State/Event; 15 DPF | M1 | Query/publish only |
| 0 | 14 Formatted GPS; 13 Formatted INS; 12 ECEF Ephemeris; 11 Relative Ephemeris; 10 Ephemeris Reference ID; 9 GPS ASCII; 8 Context Association Lists | M5 | Unsupported |
| 1 | 31 Phase Offset; 30 Polarization; 29 3D Pointing Vector; 28 3D Pointing Vector Structure; 27 Spatial Scan Type; 26 Spatial Reference Type; 25 Beam Width; 24 Range | M5 | Unsupported |
| 1 | 20 Eb/No BER; 19 Threshold; 18 Compression Point; 17 Second/Third-order Intercept Points; 16 SNR/Noise Figure; 15 Auxiliary Frequency; 14 Auxiliary Gain; 13 Auxiliary Bandwidth | M5 | Unsupported |
| 1 | 11 Array of CIFs | Excluded from production/M5; optional separate I9 structural inspection only | Unsupported |
| 1 | 10 Spectrum; 9 Sector Scan/Step; 7 Index List | M5 bounded generic codecs | Unsupported |
| 1 | 6 Discrete I/O 32; 5 Discrete I/O 64; 4 Health Status; 3 V49 Spec Compliance; 2 Version/Build; 1 Buffer Size | M5 | Unsupported |
| 2 | 31 Bind; 30 Cited SID; 29 Sibling SIDs; 28 Parent SIDs; 27 Child SIDs; 26 Cited Message ID | M5 | Unsupported |
| 2 | 25 Controllee ID; 24 Controllee UUID; 23 Controller ID; 22 Controller UUID; 21 Information Source; 20 Track ID; 19 Country Code; 18 Operator | M5 | Unsupported as CIF controls; prologue identities are M1 |
| 2 | 17 Platform Class; 16 Platform Instance; 15 Platform Display; 14 EMS Device Class; 13 EMS Device Type; 12 EMS Device Instance | M5 | Unsupported |
| 2 | 11 Modulation Class; 10 Modulation Type; 9 Function ID; 8 Mode ID; 7 Event ID; 6 Function Priority; 5 Communication Priority; 4 RF Footprint; 3 RF Footprint Range | M5 | Unsupported |
| 3 | 31 Timestamp Details; 30 Timestamp Skew; 27 Rise Time; 26 Fall Time; 25 Offset Time; 24 Pulse Width; 23 Period; 22 Duration; 21 Dwell; 20 Jitter | M5 | Unsupported |
| 3 | 17 Age; 16 Shelf Life; 7 Air Temperature; 6 Sea/Ground Temperature; 5 Humidity; 4 Barometric Pressure; 3 Sea/Swell State; 2 Tropospheric State; 1 Network ID | M5 | Unsupported |
| 7 | 31 current; 30 average; 29 median; 28 standard deviation; 27 max; 26 min; 25 precision; 24 accuracy; 23 first derivative; 22 second; 21 third; 20 probability; 19 belief | M5 general; current/min/max scalar fixture M1 | No generator attribute setters/publication |
| 4,5,6 | No standard field layouts selected in this version | Unsupported layout when interpretation requires unknown fields | None |

The profile may structurally reject unsupported M5 layouts before M5 is delivered; the release matrix must advertise that limitation. It must not call an M1/M2 release complete standard-field support. The supported M5 registry retains the explicit resource limits below; it does not promise every wire-maximum array can be materialized in the reference arena.

### 3.3 Bounds and semantic distinction

| Dimension | Generic codec default | Reference generator binding |
|---|---|---|
| VRT total bytes | At most 65,535 words, also bounded by supplied span and configured parser budget | UDP MTU budget; loopback max 8 KiB |
| CIF selector storage | All 8 words addressable; known layouts 0,1,2,3,7 | CIF0 emitted |
| Selected value fields | 128 total including nested materialization budget | Four query/publish, one writable |
| Attributes | All 13 named bits supported where descriptor/class supplies defined extent; scalar current/min/max first | No attribute payloads emitted |
| Array records | 256 per field | No array semantics |
| Index entries / association entries | 1,024 per field across lists; enforce narrower wire subfield limits | None emitted |
| Nested structures | Depth 4, 4,096 aggregate visited scalar/record units | No recursion |
| Semantic arena | 8 KiB per ordinary control transaction in reference config | Small fixed values normally inline in semantic arena |
| Opaque extensions | Packet-bound byte views, no mandatory materialization | Explicit local registration |
| RX fragments | 16 physical fragments total | Contiguous UDP RX produces logical views |

Size validation is checked before work-budget traversal. Work budget exhaustion returns `resource_limit`, not `malformed` if the wire could be legal. For Array-of-Records, validate `total_words = 3 + application_header_words + record_words*record_count` against its actual field definition (§9.3.1); Array of CIFs has a recorded interpretation exception (I9). For Index List, validate element-width code, count, exact padded size, and final zero padding (§9.3.2). Attribute size does not always equal a field's scalar size; probability/belief and structured variants require the specific descriptor (§9.12). Unsupported attribute/field semantics are distinct from an unknown structural layout.

### 3.4 Optimized samples versus general wire access

| Representation | Opaque wire access | Conversion / packing | Optimized generator |
|---|---|---|---|
| Complex signed normalized IQ16 | M1 | M1 scalar; optional SIMD after equivalence tests | Yes |
| Complex signed normalized IQ32 | M1 | M1 scalar | Yes |
| Complex IEEE float32 | M1 | M1 scalar, bit-preserving endian handling | Yes |
| Real int16/int32/float32 | M1 | M5 generic scalar conversion | No baseline generation |
| Other fixed-point widths, signed/unsigned and non-normalized | M1 bounded bytes | M5 reference bit packing into caller storage | No |
| VRT exponent formats, IEEE half/double | M1 bounded bytes | M5 descriptor-based conversions; no native-span promise | No |
| Complex polar, tags, repetition, larger vectors | M1 bounded bytes | M5 general accessors/packing with explicit parameters | No |
| Spectral Data and frame boundaries | M1 envelope/trailer | M5 generic sample metadata; no FFT or spectral source | No |
| Vendor extension payload | M1 bounded bytes | Registered class codec only | No |

Conversion output is caller-provided storage. Generic accessors handle scaling, signedness, component order, tag widths, repetitions, vector size, and padding independently of transport segmentation. A native typed view is never obtained merely by reinterpreting a byte pointer. Frames/trailers and pad count are validated even if samples are not materialized (§§5.1.3, 5.1.6, 6.1.1, 9.13.3).

## 4. Factorized CAM truth tables

The tables compose into a complete decision function: classify structure/action, apply P/W/Er to field outcomes and dependencies, apply timing, compute subtype results, apply request bits and NACK suppression, then add requested details. Their Cartesian product is the state-machine test domain; listing millions of repeated rows would obscure independent rules. Every valid combination has a result under these factors, while reserved combinations fail the first stage. The companion JSON contains representative fixtures, and the checker verifies their bit/size arithmetic without claiming a protocol implementation.

### 4.1 Action and layout

| A1 A0 | Action | Body after CIF selectors | Effect and response basis |
|---|---|---|---|
| 00 | No action / query | No control values | Validate selectors; S observes current state. If V/X are requested, V validates query support, X reports no control action under this profile's documented empty-action interpretation |
| 01 | Dry run | Selected control values | Isolated model state only; V/X and S describe hypothetical results, retain action=01 in Acks |
| 10 | Execute | Selected control values | Apply eligible plan; S observes post-action state |
| 11 | Reserved | Reject before device effects | No generic reserved-action reply invented |

Cancellation additionally requires header L=1, action=10, P=1, ReqV=0 and selectors without values; when any acknowledgement is requested, X is required, S optional (§8.5). Query/cancel selector-only forms are not normal Context value layouts. For malformed/untrusted packets, no response is guaranteed even if arbitrary bytes resemble request bits.

Cancellation retry policy (D-P07-1, accepted 2026-09-18): allow one immutable cancellation request meaning per original transaction identity for its active and retained lifetime. Ordinary Control and cancellation have separate records under the original transaction key, distinguished by header L. The first admitted cancellation may select any valid subset. Identical retries attach to its pending result or replay its original cancellation outcomes and state observation, with new outgoing Packet Count only. A different cancellation meaning is rejected locally by the Controller; the Controllee performs no new cancellation effects and reports a bounded identity/message-conflict diagnostic when safely permitted. Retain cancellation correlation and results together with the original transaction, extending retention to at least 30 seconds after the latest terminal outcome and until outstanding references are released. This deliberately prohibits later different-subset cancellation attempts under the same original Message ID; serialization alone cannot distinguish delayed acknowledgements.

### 4.2 P/W/Er eligibility: all eight combinations

Define clean field `C`; warning field `Wf` with a documented valid adjusted value; error field `Ef` with a documented valid adjusted value; and unresolvable field `U`. An enabled warning/error permission does not repair an unresolvable field. Cross-field dependency checks can turn a nominally eligible field into ineligible. Multiple conditions require all applicable permissions.

| P W Er | Clean fields | Recoverable warning | Recoverable error | If any field remains ineligible |
|---|---|---|---|---|
| 0 0 0 | Eligible | Ineligible | Ineligible | Execute none |
| 0 0 1 | Eligible | Ineligible | Eligible with documented modification | Execute none |
| 0 1 0 | Eligible | Eligible with documented modification | Ineligible | Execute none |
| 0 1 1 | Eligible | Eligible with modification | Eligible with modification | Execute none |
| 1 0 0 | Eligible | Ineligible | Ineligible | Execute eligible independent subset |
| 1 0 1 | Eligible | Ineligible | Eligible with modification | Execute eligible independent subset |
| 1 1 0 | Eligible | Eligible with modification | Ineligible | Execute eligible independent subset |
| 1 1 1 | Eligible | Eligible with modification | Eligible with modification | Execute eligible independent subset |

Basis: Table 8.3.1.2-1. `U` never executes. If all fields are ineligible, no effects occur. Diagnostics still record adjusted warnings/errors even when execution succeeds; warnings can therefore coexist with full execution. With P=0, prevalidation is all-or-none admission, not a promise of rollback after an unpredictable hardware failure. Reference Controller defaults to P=1,W=0,Er=0. Runtime failure continues only independent allowed fields and never executes a dependent field against an unknown prerequisite.

### 4.3 Request mask and NACK suppression

For the table below, request tuples are V,X,S. Start with the listed candidate packets, then apply the suppression predicate to V and X independently using their phase's accumulated warnings/errors. S is never suppressed by NACK. Execute-mode S-only requests encounter interpretation I8; our Controller emits X+S and our profile receiver rejects missing-X post-action state requests as a profile constraint rather than labeling a reserved wire bit.

| ReqV ReqX ReqS | Candidate responses in order |
|---|---|
| 0 0 0 | None |
| 0 0 1 | S |
| 0 1 0 | X |
| 0 1 1 | X, then S |
| 1 0 0 | V |
| 1 0 1 | V, then S |
| 1 1 0 | V, then X |
| 1 1 1 | V, then X, then S |

| NACK | Warning present | Error present | Requested V/X response |
|---|---|---|---|
| 0 | 0 | 0 | Send |
| 0 | 1 | 0 | Send |
| 0 | 0 | 1 | Send |
| 0 | 1 | 1 | Send |
| 1 | 0 | 0 | Suppress |
| 1 | 1 | 0 | Send |
| 1 | 0 | 1 | Send |
| 1 | 1 | 1 | Send |

Basis: §§8.3.1.4–8.3.1.6, §8.4.1.1. An execution-time failure can produce X even after a clean V was suppressed. Lack of detail requests does not suppress the whole response or clear its summary warning/error flags (interpretation I1 for V). No-Ack and clean NACK-only outcomes stay unconfirmed at the Controller. A timeout is unknown remote outcome, not proof of failure or success.

### 4.4 Diagnostic detail and subtype fields

| ReqW ReqEr | Detailed bodies in V/X when corresponding conditions exist |
|---|---|
| 0 0 | No diagnostic indicator/value body; summary flags still report conditions |
| 0 1 | Error indicators then 32-bit error fields |
| 1 0 | Warning indicators then 32-bit warning fields |
| 1 1 | Warning indicators, error indicators, warning fields, error fields |

Absent condition groups are omitted. Each diagnostic field is one 32-bit word, not the value length of Sample Rate or another selected control. Descriptor selection follows the original field identity. A receiver uses the correlated original request to know requested detail groups; ambiguous unsolicited diagnostic bodies are not guessed (I11). No zero-valued filler diagnostics are emitted by this profile. S has a state-value body and AckW=AckEr=0.

| Subtype / outcome | AckP | SchX | Timestamp |
|---|---:|---:|---|
| V: all requested fields eligible at allowed time | 0 | 1 | Scheduled effective time |
| V: some or none eligible | 1 | 0 | Known planned time if meaningful; otherwise omitted |
| X: all requested fields executed within allowed policy/time | 0 | 1 | Actual final effective time |
| X: subset or none executed | 1 | 0 | Actual final effective time if any; otherwise omit |
| S: all selected values returned | 0 | 1 | State observation time |
| S: some selected values returned | 1 | 1 | State observation time |
| S: none returned | 1 | 0 | Observation time if taken |
| Cancellation X: all selected controls cancelled | 0 | 1 | Actual cancellation time |
| Cancellation X: some cancelled | 1 | 1 | Actual cancellation time |
| Cancellation X: none cancelled | 1 | 0 | Cancellation attempt time if known |

Ordinary V/X SchX follows Table 8.4.1-1's all-fields meaning; cancellation explicitly uses any-field success under Rules 8.5-13/-14. AckP describes predicted coverage in V and actual coverage in X; it is not synonymous with Ctrl-P. Empty no-action V validates selectors as a query; empty-action X reports AckP=0,SchX=0. Dry-run bits use hypothetical counterparts per §8.4.1.4; action bits identify the simulation. Original command header L=0, cancellation response L=1, and only one of AckV/AckX/AckS is set per Ack. Mirror request CAM bits31–21; reserved output bits zero.

If every field was executed but one violated timing, preserve completion locally, set AckT=7 and SchX=0; AckP=1 under the interpretation that not all requested controls executed as timed. Do not label that an unqualified success. Multi-field effective-time aggregation is I10. These policies must be tested against the intended peer before asserting interoperability.

### 4.5 Timing modes and status

Let T be requested reference time. D_e/D_l are device early/late bounds and A_e/A_l are application bounds. All are nonnegative and configured; the application bounds must be at least their respective device bounds for the reference binding. Check the entire uncertainty interval of an effect, not just its estimated center.

| Ctrl timing | Allowed effective interval | Policy |
|---|---|---|
| 000 | Unconstrained by T | Next eligible boundary |
| 001 | [T-D_e, T+D_l] | Device precision |
| 010 | [T-D_e, T+A_l] | Device window plus allowed late interval |
| 011 | [T-A_e, T+D_l] | Device window plus allowed early interval |
| 100 | [T-A_e, T+A_l] | Application early/late interval |
| 101,110,111 | Reserved in Control | Reject |

With no Control timestamp, emit Ctrl timing 000 and Ack timing 000. Successful timed handling reports the matching mode; inability to meet timing for some controls reports Ack timing 111. Ack modes 101/110 are reserved; 111 is a status, not a legal Control request. Basis: Tables 8.3.1.7-1 and 8.4.1.5-1. The mode-3 reference text contains an apparent late-window wording error; this design follows its name, figure, and early-window intent rather than enabling a second late window (I12).

### 4.6 Controller observations and ordering

Send V after structural/semantic/timing validation and admission; do not wait for the scheduled execution. Send X after terminal execution/cancellation results, not merely enqueueing writes. For ordinary requests, this profile does not send an early scheduled-only X even though some reference wording discusses scheduling. S follows requested V/X and reports a coherent observation, with hypothetical post-state for dry runs. No-Ack has no remote completion event. A late response after local timeout is exposed as late evidence without silently rewriting the timeout event. A cancellation timeout cannot cancel the original device action by itself.

Invalid/reserved fixtures include action11; Control timing101–111; nonzero standard reserved CAM bits; Ack with zero/multiple subtype bits; cancellation with P=0, ReqV=1, wrong action, values, or S without X; malformed selector/value mismatch; illegal ID lengths; unknown class; undefined ordered-field layout. Profile-unsupported and structurally malformed are separate errors. Permitted omission of identifiers or Class ID in the generic codec is not malformed solely because the generator's normal binding requires them.

## 5. Specification interpretation register

Each entry is a chosen project implementation basis with unresolved peer/authority confirmation, not an official correction. Do not add automatic on-wire dialect guessing. A compatibility variant requires explicit peer configuration, distinct fixtures, and recorded behavior.

| ID | Reference issue | Selected behavior and interoperability action |
|---|---|---|
| I1 | Table 8.4.1-1 p110 forbids AckV AckEr; Rules 8.4.1.2-6..9 pp112–113 describe V/X summary errors | Follow detailed diagnostic rules: V may indicate errors. Confirm peer before external conformance claim |
| I2 | Table 9.12-1 p219 says Attributes at 1/7; Table 9.1-1 and Rule 9.1-7 place enable at CIF0/7 | Use CIF0/7; fixture verifies word placement |
| I3 | Context/Command class templates pp238/242 offer SID omission; Command template calls type Context | Actual packet structures govern: SID mandatory for standard Context/Command and Command type0x6 |
| I4 | Command template says change indicator inapplicable; Permission 9.1.1-1 permits it | Generator emits it only in Context; generic parser accepts permitted Control use; never deduplicate with it |
| I5 | §8.4.1.3-5/-6 p116 name SchX for query partial/full, conflicting with table AckP and §8.4.1.4-3/-4 | AckS AckP indicates missing selected state; SchX means at least one query value returned, following table and dedicated SchX rules |
| I6 | Dry-run observation p116 mentions action00 in Ack; Rule 8.4.1-2 requires bits31–21 match | Preserve action01 in dry-run Acks; use request correlation to retain simulation semantics |
| I7 | Dry run prohibits live effects; §8.4.2-2 includes mode1 in post-action state wording | Dry-run S reports isolated predicted post-state, action01; never changes live state |
| I8 | §8.3.1.5 permits any request combination; Rule 8.3.1.5-7 requires X with post-action S | Controller emits X+S for modes1/2; profile receiver rejects execute-mode S without X as unsupported profile combination; generic codec retains it structurally |
| I9 | Array of CIFs §9.13.1 pp222–223 mandates five CIFs plus 3-word header but HeaderSize=7 conflicts with generic extra-header-size formula | The optional explicit I9 utility uses exactly five CIF words after three base words, requires encoded HeaderSize=7, validates total as 8+record_words*count; no two invented padding words. Optional structural utility only under D-M5-1; production Array support excluded, no native/emission/semantic integration. Preserve peer-dependent interpretation for future optional work; reject unknown alternate layouts |
| I10 | §8.4.1.5 defines an actual execution timestamp but not aggregation of distinct per-field effective times | X timestamp is final actual effect time; retain per-field timing issues and revisions. If no effect happened, omit actual-effect timestamp instead of inventing one. Peer agreement required for multi-effect hardware |
| I11 | Summary W/Er flags remain set even when detail omitted, while diagnostic body indicator presence depends on original detail requests | Decode with correlated request's detail mask. If unavailable and group identity is ambiguous, return opaque diagnostic body plus `requires_request_context`; never infer value bodies from flags alone |
| I12 | Mode3 text Table 8.3.1.7-1 refers to late window despite early-mode name/figure | Use early application interval plus device late tolerance; confirm with peer and include boundary tests |

Printed pages 110, 116, 222, and 223 were rendered and inspected to verify the AckV/AckS/dry-run and Array of CIFs issues are present in the pages, not solely text-extraction artifacts. I9 is a conservative explicit dialect selection, not a claim to have resolved the standard for all peers. Any future Array interoperability claim requires independent implementation evidence or authoritative clarification. Under D-M5-1, Array is excluded and this condition does not block M5 acceptance.

## 6. Representative fixtures

Machine-readable vectors are in [architecture fixtures](vita49_architecture_fixtures.json). They are design fixtures with independent expected constants, not captured interoperable traffic. Minimal wire fixtures intentionally omit optional Class ID/timestamps and use configured test-only identifiers SID=1, Controllee=2, Controller=3. They exercise the generic codec; they do not replace the normal generator class options. No example appropriates a production OUI.

| ID | Input / scenario | Expected behavior |
|---|---|---|
| W1 | Query Sample Rate: `60000007 00000001 a0040000 00000001 00000002 00000003 00200000` | Seven words, no Sample Rate value body |
| W2 | Execute 1 MHz with partial allowed, X/details requested: `60000009 00000001 a90b0000 00000002 00000002 00000003 00200000 000000f4 24000000` | Nine words, value=1,000,000 Hz, execution candidate |
| W3 | Cancel Sample Rate: `61000007 00000001 a9080000 00000002 00000002 00000003 00200000` | L=1, original MID=2, selector-only, X requested |
| W4 | Successful X for W2: `64000006 00000001 a9080400 00000002 00000002 00000003` | A=1, X only, AckP=0, SchX=1, no diagnostics |
| W5 | Two IQ16 pairs: `10000004 00000001 40000000 3b21187e` | Generic type1, no timestamp/class; payload directly accessible |
| W6 | W2 truncated by last word without correcting size | Short packet, no callbacks |
| W7 | Query W1 with surplus value words and corrected header size | Selector-only layout rejects surplus body |
| W8 | Reserved type8 packet | Invalid packet type |
| W9 | CIF7 change adds min/max but builder lacks required values | Edit fails atomically; original snapshot/size unchanged |
| W10 | Index List says 4 words but declares more entries than fit | Structural length error before next field |
| W11 | Valid total array span but inconsistent record count/product | Structural inconsistency, no guessed next offset |
| W12 | Diagnostic AckEr set, details not requested | No forced CIF/value read; request context determines omission |

W5 uses the canonical rounded 16-entry oscillator; exact bytes validate packing/rounding, not an arbitrary platform `sin` result. Additional scheduled fixtures use an injected clock and explicitly configured windows, avoiding any wall-clock test sleeps.

| State fixture | Expected outcome |
|---|---|
| P=1,W=0,Er=0; clean A, warning B, unrecoverable C | Execute A only; B/C not executed; per-field diagnostics; X partial |
| P=0 on same set | Execute none; no rollback needed because no effects began |
| P=1,W=1; adjusted precision B | Execute B's accepted integer value; warning retained; S returns adjusted value |
| Clean NACK-only V/X | No V/X; Controller unconfirmed |
| Clean V then device failure under NACK-only | V suppressed; X reports failure |
| ReqS plus NACK=1 | S still sent when valid request combination |
| Dry run V/X/S | Predicted outcomes and state, no live writes/revisions/Context change |
| Cancel before commit | Cancel selected pending fields; cancellation X L=1; original transaction records cancelled fields |
| Cancel after commit | No rollback, not-cancelled diagnostic; correct cancellation AckP/SchX |
| Partial cancel | Only pending subset cancelled; cancellation X AckP=1,SchX=1 |
| Clock step before arming | Revalidate new mapping or reject; monotonic timeout unchanged |
| Context submission fails after device success | Hold/drop/stop Data under gate; X still reports device result |
| Unknown rate after partial register failure | Mark unknown, stop Data, S incomplete; no stale rate presented as confirmed |
| Two effect times | Two revisions; Context before each affected Data interval; X final actual effect time with per-field timing results |
| Stuck completion during shutdown | Fault/quarantine, never return memory still reachable by device |

## 7. Verification record and limits

The delivered checker verifies wire-fixture word counts/header types, CAM bit masks, selector-only versus Sample Rate body lengths, 20-bit fixed-point rate encoding, three payload-format constants, all eight P/W/Er eligibility rows, request-mask/NACK/detail factor tables, timing uncertainty intervals, and packet/pool arithmetic including the complete projected 64 MiB arena partition and its agreement with the architecture table. It also checks that sixteen machine-readable state scenarios include setup, events, and expected outcomes; it does not execute a transaction engine. Run `python3 docs/fixtures/check_architecture_fixtures.py` from the repository root. It checks architecture fixture consistency independently of any future framework implementation. These checks cannot validate every clause of the standard or substitute for a peer implementation.

At the architecture baseline, planned evidence included public API compilation, field/attribute codecs, fuzz/sanitizer and race tests, peer captures and performance/timing. Current implementation evidence is recorded in the M5 integration report; D-M5-1 excludes Array production support, while peer/deployment qualification remains separate. The deliverable is the requested architecture and implementation plan; deployment identifiers, timing qualification, and interpretation agreement remain explicitly tracked inputs.

Review closure, 2026-09-18: scenarios S11–S16 specify clock-loss mode-0 behavior, completion publication and stale generations, fresh-SID recovery, same-SID rejection, and synthetic failure without quiescence. The checker verifies scenario structure and budget arithmetic; memory ordering, runtime recovery, and actual object-size feasibility still require implementation tests.
