# P14 CIF0 structured implementation

Candidate independently verified and integrated:157/157 optimized checks and154/154 ASan/UBSan checks passed. The verifier report and M5 integration report retain the gate evidence. This batch implements the remaining six CIF0 layouts only. It does not complete P14/M5, general CIF7, I9 arrays, or later CIF1/2/3 work.

## Scope and authority

Authority is architecture §3, the implementation-plan P14 batch contract, protocol appendix §3.2–3.3 and the supplied ANSI/VITA-49.2-2017 (R2024). Printed Figures 9.4.5-1 and 9.13.2-1 were rendered and inspected to verify embedded timestamp/OUI and association count bit locations.

| Field | CIF0 | Wire words | Authority | Developer evidence |
|---|---:|---:|---|---|
| Formatted GPS | 14 | 11 | §9.4.5, pp141–144 | p14_structures, p14_structured_wire |
| Formatted INS | 13 | 11 | §9.4.6, p144 | p14_structures |
| ECEF Ephemeris | 12 | 13 | §9.4.3, pp138–140 | p14_structures |
| Relative Ephemeris | 11 | 13 | §9.4.9 Rule1, p146 | p14_structures |
| GPS ASCII | 9 | 2 + declared ASCII words | §9.4.7, pp144–145 | both new developer tests |
| Context Association Lists | 8 | 2 + all SID/tag entries | §9.13.2, pp224–227 | both new developer tests |

Relative Ephemeris follows the explicit Rule9.4.9-1 reference to the thirteen-word ECEF format; the inconsistent eleven-word illustration is documented in P14-next-readiness.md. This is a local explicit-rule interpretation, not a claim to have corrected the publication. Speed uses the signed32 Q16 representation; the impossible larger numerical range in an observation does not add representation bits. I9 is unchanged and outside this batch.

Only Current attributes are implemented for these structures. Fixed field numerics preserve exact signed scales and unknown `0x7fffffff` values as `optional<int32_t>`. Embedded fix timestamps retain their own TSI/TSF and all three timestamp words; undefined codes require all-ones corresponding words. GPS/INS and ECEF/Relative fix timestamps do not inherit the enclosing packet timestamp. Location semantic ranges are checked on typed construction/materialization; structurally readable out-of-range location values remain inspectable as wire views.

## Ownership and public API

`PacketSnapshot`/`PacketBuilder` gain final `NativeBytes=0`; `TypedPacket` has the same final parameter. Existing scalar aliases remain capacity zero. `NativeContextPacket<NativeBytes=8192,N=16>`, `NativeControlPacket`, and `NativeStateAck` provide explicit bounded native storage. `set<Field>(typed_input)` and `replace<Field>(typed_input)` validate and synchronously copy caller data into a candidate arena. Replacement/removal compacts live values and commits one generation change; any validation/capacity failure preserves the old state.

`freeze()` deep-copies native storage. Snapshots outlive builders and caller inputs. `get<Field>() const&` returns copied fixed native structures or immutable borrowed ASCII/list views; rvalue access is deleted. List accessors copy host or network integers rather than reinterpret byte storage as live uint32 objects. The compact NativeSlice alternative is privately constructible; ordinary `set_value` and supplied attribute values reject slices. Internal resolution additionally requires the exact value reference to belong to the receiving snapshot, rejecting copied/foreign-owner slice resolution.

`FieldView` exposes `geolocation()`, `ephemeris()`, `gps_ascii()`, `associations()` and `materialize_into(builder, validator={})`. Materialization rejects non-Current attributes instead of silently reinterpreting them. Fixed values and list/ASCII payloads are explicitly copied into the native owner. A structural wire view has no implicit retained Rx lease.

GPS ASCII typed setters require an explicit synchronous `SentenceValidator` callback. The callback establishes the locally configured complete-sentence grammar; it is not stored or called during later encoding. It must inspect without retaining/mutating its borrowed input and must not rely on side effects for admission. An impossible text length/native capacity fails before walking bytes or invoking it. Structural decoding checks ASCII bytes, count, zero padding and extents, and does not claim a universal GPS sentence grammar. No NMEA implementation, manufacturer identity or deployment-specific geospatial reference is invented.

## Shared traversal and limits

`walk_field_layout` is the single CIF/attribute ordering, offset and work-accounting core. Native and wire providers supply field extents; selectors remain zero-value bodies, diagnostics four-byte bodies. Native measurement/encoding/indexing and wire decoding share that policy. The decoder no longer builds placeholder native values just to learn wire offsets. Whole-packet validation completes before user visitor callbacks.

Default `DecodeLimits` has1024 association entries including tags and4096 work units. Fixed structures charge11/13 units; ASCII charges2 plus padded ASCII bytes in both native and wire paths; lists charge2 plus all entries. Declared full extents are checked against packet bytes before work exhaustion is classified. Custom decode limits affect structural views; typed association materialization remains bounded at1024 entries. Native setters may own a legal value too large for default traversal work; measure/encode then return `resource_limit` consistently. No hidden heap fallback is present.

Layout signatures use explicit shape only: field identity, fixed extent, ASCII character count, individual association list counts and tag presence. Native object padding and numeric contents are not hashed. Equal-total-size list repartitioning invalidates a cached signature. Generation checks remain required.

The existing16 selected-field default is unchanged;128-field generic materialization remains later P14 work. General structured CIF7 attribute sizes are not inferred from base layouts.

## Resource accounting

Actual arm64 Clang/libc++ size probe:

| Type | Bytes |
|---|---:|
| SemanticValue | 16 |
| FieldEntry | 328 |
| scalar PacketSnapshot / ContextPacket builder | 5312 each |
| NativeBytes8192 snapshot / builder | 13512 each |
| GeolocationValue | 80 |
| EphemerisValue | 96 |
| existing runtime StateSnapshot | 136 |

Capacity-zero storage is an empty `no_unique_address` specialization; existing baseline scalar/runtime sizes are preserved. The native8192 owner adds8192 data bytes plus8 used-size bytes. It is an explicit caller-selected generic object, not silently inserted into baseline runtime slots or the existing64MiB ledger. Callers allocating it must also budget bounded candidate copies/compaction scratch: nested edits can retain two additional13512-byte candidate objects and8200-byte arena scratch (8192 data bytes plus8 used-size bytes); association materialization has a separate4096-byte host-integer scratch array. These are logical object sizes, not a measured compiler stack frame or performance claim. No encoded packet images are cached in the native arena.

## Developer validation

`p14_structures` passes direct C++23 no-exception/no-RTTI compilation and ASan/UBSan. It covers all six native fields, wire roundtrip, caller mutation, snapshot copy, old snapshot preservation, foreign/raw slice rejection, rvalue API constraints through verifier scope, repeated replacement/compaction, capacity rollback, required sentence validator and impossible-capacity preflight, semantic ranges, shape signatures and padded ASCII work boundaries. `p14_structured_wire` passes ASan/UBSan with raw malformed extent/reserved/padding cases, tagged-list truncation, configurable resource limits, undefined timestamp words and raw-versus-semantic location rejection. Both test every truncated packet prefix/short-output preservation through the main structured test. Existing P01 semantics and P14 scalar developer regressions pass direct compilation/run.

Independent literal vectors, no-allocation evidence, mutation/fuzz evidence and the full optimized/sanitizer regression are separate gates, not implied by these developer checks. Further batches stop before the separately identified unresolved BeamWidth/BarometricPressure requirements; this candidate makes no decision for those fields.

## Frozen source SHA-256

- `include/vita/fields/types.hpp`: `272b818c0017baed6f838c076701be3dcf966d512ed8a82691613fc1d149989f`
- `include/vita/fields/arena.hpp`: `62c2f3f45f79d2c77cc24e1263207200d954f985917cd142b9e17fb8d660bc8e`
- `include/vita/fields/packet.hpp`: `a7f9f678da189ec19210e6f4fd335cf515cbedc501cb52fdad2ba3e8c8e58046`
- `include/vita/fields/structured.hpp`: `24c222cbccce8a474139836d03f42b63dd5740f81c9e71e16c30476d93d71f96`
- `include/vita/codec/layout.hpp`: `3a993334114bfed039fbb0a5603cee821671006cea278dcffec888dc17f59c11`
- `include/vita/codec/packet.hpp`: `a5b3659fa335616c73256ab4469acaec0a6205efd867782cba0820981c9fe607`
- `include/vita/codec/scalar.hpp`: `9d0f46016303d89f8736b2fc40564fb38348a716f23dda4248fa392a2776315b`
- `include/vita/codec/structured.hpp`: `5fea29314c51830bcffd06ba69b5d3a767e87d51f1354215360ef4788d521e1f`
- `tests/unit/P14/structures.cpp`: `019c9bcfb07b0865e5e666c3ecd9e2df703f9ec1f7412ed495fe3f7cb46daf3d`
- `tests/unit/P14/structured_wire.cpp`: `c20a3ff343875ef05abdd699834261c6fb7db1517b818917d5b857e1bcf00c1c`
- `tests/unit/P14/CMakeLists.txt`: `90595bbda59c3c906466c476a3c63b15ec6a69e3a5ecc656f908f5c086de9842`
