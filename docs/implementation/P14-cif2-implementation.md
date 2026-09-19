# P14 CIF2 implementation

Candidate frozen after developer verification. Independent and integrated gates are recorded separately. All29 named CIF2 fields, bits31 through3, have bounded Current-only field codecs. This does not implement general CIF7, CIF3, command recall device behavior or P14/M5 in full.

Authority: P14 registry inventory and readiness note; ANSI/VITA-49.2-2017 (R2024) §§8.2,9.8, Table9.1-1; coordinator-approved explicit-rule precedence for Country Code. Printed page references below refer to the supplied local normative PDF.

| Fields / CIF2 bits | Representation | Authority |
|---|---|---|
|Bind31|One word; bit0 association code, all other bits reserved|§9.8.1 p200|
|CitedSID30, SiblingSID29, ParentSID28, ChildSID27|One uint32 SID each; plural relationship names do not imply lists|§9.8.2 pp200–202|
|CitedMessageId26, ControlleeId25, ControllerId23|One uint32 each|§§9.8.3–4 pp202–203|
|ControlleeUUID24, ControllerUUID22|Four ordered uint32 words, native-arena owned|§9.8 Rule4; §8.2, including nonzero Rule8.2.6-1 p97|
|InformationSource21, TrackId20|One uint32 linkage each|§§9.8.5–6 p204|
|CountryCode19|CountryCodeValue: code bits10..0, ISO selector15; other bits reserved|Rules9.8.7-1/2/3 pp204–205|
|OperatorId18|Generic16, high16 reserved|§9.8.7 Rule5 p205|
|PlatformClass17, PlatformInstance16, PlatformDisplay15|Full uint32 linkages, no invented high-half padding|§9.8.8 pp205–206|
|EmsDeviceClass14|Class code11..0, receiver12, exciter13, organization15..14; high16 reserved, organization3 reserved|§9.8.9 Rules1–5 p206|
|EmsDeviceType13, EmsDeviceInstance12|Full uint32 linkages|Table9.8.9-1 p206|
|ModulationClass11, ModulationType10|Generic16|§9.8.9 Rules6–7 p207|
|FunctionId9, ModeId8, EventId7, FunctionPriority6|Generic16|§§9.8.10.1–4 pp207–208|
|CommunicationPriority5|Full uint32 priority code; no invented Generic16 narrowing|§9.8.10.5 pp207–208|
|RFFootprint4, RFFootprintRange3|Full uint32 KML linkages, not distances|§9.8.11 p208; field locations follow CIF matrix/appendix|

Country Code Figure9.8.7-1 draws twelve code bits and labels the selector misleadingly. Three consecutive explicit rules say eleven code bits and selector1=ISO/0=user. The approved implementation follows those rules: reserved mask `0xffff7800`, code mask `0x7ff`. Independent audit agreed with this precedence. ISO assignment validity and user-enumeration applicability require their external/class data; this codec does not claim an ISO lookup. RFFootprintRange uses CIF2/3 from the CIF matrix and appendix rather than the conflicting local table's2/5, already assigned CommunicationPriority. Neither choice is silently presented as a corrected publication.

The public field tags use the names above. Ordinary identifiers reuse uint32 storage. CountryCodeValue `{uint16 code; bool iso3166;}` and EmsDeviceClassValue `{uint16 class_code; uint8 organization; bool exciter, receiver;}` append variant indices22/23; all earlier indices remain unchanged. UuidValue `{array<uint32,4> words;}` uses existing private NativeSlice index8 and copies16 native bytes per UUID into the snapshot arena. `set<ControlleeUUID/ControllerUUID>`, copied `get<Field>() const&`, `FieldView::uuid()` and explicit `materialize_into` use the existing ownership/compaction/rollback contracts. No UUID pointer, shared allocation or encoded wire image is stored in SemanticValue.

Typed UUID construction/replacement/materialization rejects all-zero words per §8.2. Structural decoding preserves an all-zero16-byte value so an application can inspect and diagnose it; it does not turn raw receipt into semantic validity. No unsupported UUID generation/version restriction or global-uniqueness proof is invented. Native measurement checks the same semantic nonzero requirement.

Selectors contain no UUID value body; diagnostics carry one32-bit diagnostic word per selected UUID, not sixteen bytes. Wire and native value paths continue through the existing shared traversal with four work units per UUID. New fields remain Current-only. Default16 selected-field parsing remains explicit: registering29 fields does not silently enlarge the default per-packet bound.

## Semantic and profile boundaries

Body IDs do not replace prologue SID, MID, Controller/Controllee identities or transport routing. Bind encoding does not execute a hardware binding or dynamic route change. All non-baseline fields remain unsupported device operations on the four-field IQ profile.

CitedMessageId's contextual Command rules (§9.8.4-1/2/3) require CitedSID, a query/recall purpose and no other CIF options; they also forbid replacing the prologue MID. This batch implements the individual field representations, not a generic query-recall operation or complete contextual Command validator. It does not infer how selector-only query layout supplies a cited value. Controller attribution constraints (§9.8.3), class identifier assignments, ISO databases, priority ordering and KML interpretation likewise are not established by raw codec support. These scope limits remain explicit rather than advertising complete field-driven device semantics.

## Bounds and developer evidence

Actual arm64 sizes remain SemanticValue16, FieldEntry328, scalar snapshot/builder5312, native8192 snapshot/builder13512 and runtime StateSnapshot136. UUID contributes16 actual native bytes per live value inside caller-selected capacity; a32-byte arena stores two UUIDs. Repeated replacement compacts and reuses that capacity. No baseline runtime arena/budget increase occurs.

`p14_cif2_identifiers` passes direct C++23 no-exception/no-RTTI and ASan/UBSan:27 literal scalar values, full-width linkages, Country selector/code boundaries and every reserved bit, Generic16 padding, EMS reserved organization, semantic bound rejection, truncation/unchanged short output, selectors/diagnostics and prologue isolation. `p14_cif2_uuid` passes ASan/UBSan: two independent literal UUID wire copies, caller mutation/snapshot copy isolation,100 replacements, exact arena bounds/reclamation, raw-ref transplant rejection, materialization, nonzero semantic checks with raw zero preservation, all truncations and work limits. Prior CIF1 developer regression also passes. Independent no-allocation/literal coverage and aggregate Release/sanitizer checks remain separate gates.

Frozen source SHA-256:

- `include/vita/fields/cif2.hpp`: `113e028afeab07abc040dad5bd03c6ec1af76f3f1895ed44ef8af1babc43cf27`
- `include/vita/fields/types.hpp`: `a7fa5214dd502bdfe3674a3a88b920339272b04d2d1b103b778b8eedbe0a009c`
- `include/vita/fields/structured.hpp`: `b01efeac3a12a6d48ac2d8ac9751a5f647361118ff5b1011a09b629b2796d624`
- `include/vita/codec/scalar.hpp`: `fbf8136d83e176121f10e03acac095c1a4e400b23a2356c714b9134bc1501efe`
- `include/vita/codec/layout.hpp`: `051c1236fb6f0c11c82c762ac332fcfdc216989923ff4a61e94309153082b230`
- `include/vita/codec/structured.hpp`: `2bd85ed0dba1095cbc615ca617ecb772d3cbbce7f06c1db8c18f124c0bc50b38`
- `include/vita/codec/packet.hpp`: `1649d033f41fa1195a8b56b43e7f10ffd802ca7a9e1acdd8fbe51a2597fef008`
- `tests/unit/P14/cif2_identifiers.cpp`: `dd14756a44f8999b2d69e2809522fb947eb32f2f2904760f3725c57718ca4868`
- `tests/unit/P14/cif2_uuid.cpp`: `5254e6746b3dcbb1e4f71a89236529c7061095c721366503380721ca0406eda1`
- `tests/unit/P14/CMakeLists.txt`: `8b5cf113053ab9fadfd796f5c3e5ac8580107a32d42ae39f4e02f8235284ce55`
