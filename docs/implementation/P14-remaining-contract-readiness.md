# P14 remaining contract readiness

Continuation update (2026-09-19): D-P14-1/2/3 and the general raw-code-only policy are now accepted in [P14 decisions](P14-decisions.md). Pending-approval statements below describe the original audit checkpoint. No engineering conversion dialect or ambiguous wire layout is thereby selected.

Read-only audit, 2026-09-19. Owned change: this note only; no production/test edits or builds. Reviewed architecture §§3/3.1, sample conversion and extension security contracts; protocol appendix §§2.4,3.2–3.4/I9; P14 task card; current field arena/builder/shared traversal and sample APIs; supplied ANSI/VITA49.2-2017(R2024) PDF §§6.1.1–6.1.2,6.4,7.2,8.6,9.3,9.4.1,9.6,9.12,9.13. Existing [field inventory](P14-cif123-readiness.md) and [independent sample readiness](P14-sample-readiness.md) remain complementary references. Page numbers below are printed pages.

The next implementation can cover general CIF7 structure, bounded native nested records, a locally registered extension interface, and generic exact sample access without selecting a new application protocol. Caller-explicit conversion/packing policies and class metadata are normal required parameters, not reasons to stop all work. Engineering-unit ambiguities and externally referenced numerical conformance have narrower limits below. Accepted D-P14-1/2 raw-only Beam Width/Barometric coverage remains in force; do not reopen it. D-P14-3 (general raw-only continuation for Probability/Spectrum conversions) is pending user response; this note neither accepts that policy nor starts affected implementation.

## CIF7: implementable contract

All thirteen attribute bits are31..19, descending order inside each selected field, before advancing to the next field (§9.12/Table2 pp219–220). CIF7 itself precedes value bodies. Omitted CIF7 implies Current; explicitly present CIF7 need not include Current. Do not confuse Table9.12-1's erroneous1/7 label with the existing CIF0 bit7 enable/CIF7 matrix. Current implementation supports only a subset and restricts native structures to Current; lifting those restrictions requires a shared traversal gate.

| Bits | Attribute | Extent/type rule |
|---|---|---|
| 31 | Current/Standard | Field's ordinary representation. |
| 30,29,28 | Mean, Median, Standard Deviation | Same field size; numeric meaning per Rules3–5. Same-size representation does not justify silently converting an integer field to float or inventing a structured statistic. |
| 27,26,25,24 | Maximum, Minimum, Precision, Accuracy | Same field size/type; precision/accuracy describe +/- uncertainty. |
| 23,22,21 | First, Second, Third Derivative | Same field size; dimensions per second, second squared/cubed. A rate attribute may need signed values even where ordinary Current has a nonnegative device range. |
| 20 | Probability | Exactly1word regardless of controlled field size. Raw value7..0, function15..8, upper16 reserved zero. Functions0 uniform,1 normal,2..255 user-defined. Percentage conversion caveat below. |
| 19 | Belief | Exactly1word, raw value7..0, upper24 reserved zero; exact confidence ratio `raw/255`, or percent `100*raw/255`. |

Proposed descriptor contract: separate `attribute_shape(id, attr, context)` from `validate_attribute_semantics(...)` and device capability. The shape returns ordinary-field extent policy or the one-word probability/belief shape. Statistical/derivative encoding can preserve exact storage without claiming every mathematical operation has meaning for an ID, OUI, timestamp bundle, or list. Do not apply a Current-value physical bound indiscriminately to derivatives/statistical attributes. Record applicable semantic support per descriptor/attribute in the coverage matrix; unsupported device use still produces diagnostics after checked structure.

Add small native `ProbabilityCode {u8 value,u8 function}` and `BeliefCode {u8 value}` types (exact spellings are routine choices). For same-shape structured attributes, typed setters take the attribute explicitly and deep-copy each supplied value into the candidate snapshot arena. `with_attributes(mask, supplied)` remains an all-fields transaction: validate all newly required values/shapes, capacity and generation before committing; no partial arena mutation or missing sibling values. Removing attributes compacts native ownership without invalidating earlier frozen snapshots. Explicitly reject raw/copied native slice insertion.

A field's independent variable attributes may have different list lengths. The extent provider must inspect each attribute's own header/value, rather than multiplying Current's length by attribute population count. Cached layout signatures include attribute mask and every dynamic attribute shape. Selector-only query/cancel bodies remain zero bytes, and diagnostic bodies one word per diagnostic selection; never invoke ordinary structured extent logic for either.

No statistics engine is required to encode/decode supplied statistics. Documentation Rule9.12-1 p220 places the sampling interval/population definition in class documentation. Likewise uniform-distribution endpoints are class inputs (Rule9 p221); user-defined probability functions require local interpretation. Those are caller/profile metadata, not missing framework architecture.

**Probability conversion caveat:** rendered p221 confirms Rule9.12-7 says LSB `1/255 %` while also specifying `FF` as100%. These conflict by a factor100. Belief Rule13 explicitly says1/255 **of100%** and is consistent. An exact raw-code API is unambiguous and can proceed. Recommend no implicit engineering percentage for Probability until the coordinator records whether to preserve raw-only or explicitly use endpoint-normalized `100*raw/255`; do not call the latter an official correction. Render inspected at `/tmp/vrt-p221-attrs.png`; licensed page is not committed.

## Nested CIF1 structures: shared traversal and ownership

Implement Index List and3D Vector Structure first, then Sector/Step-Scan and Array of CIFs; Spectrum is fixed13words and can be independent except its raw-only coefficient interpretation. Exact IDs/extents/subfields are in the preceding inventory. There is no need for a new arena lifetime model: use the approved snapshot-owned optional bounded native arena, private relative offsets, checked typed borrows, deep-copy freeze, and capacity0 specialization. No pointers to caller lists or RX leases belong in semantic snapshots.

Proposed shared context extends existing layout inputs with enclosing TSI/TSF, `depth`, remaining work/record/field budgets, and the explicitly selected I9 variant capability. Use one non-owning record visitor/cursor whose native and wire providers return the same validated shape: total bytes, header words, record words/count, selected subfield/CIF masks, and work units. Native provider calculates from typed records; wire provider validates declared size against bounded input before traversal. A record descriptor may recurse through the same field/attribute order core. Do not build a second encoder-only nested walker.

Bounds/check ordering:

1. Validate available header, reserved selectors, legal widths and checked total-size arithmetic before using declared counts to loop or advance.
2. Validate per-field exact formula and each record's selected layout. Optional native lists may vary across records only if their computed encoded record lengths still equal declared `record_words`.
3. Charge all visited attribute scalars, record headers and nested values against shared work; charge nested fields/records/depth before descending. Stop with `resource_limit` on legal but excessive input; `unsupported_layout` on unknown extent; malformed only for invalid structure.
4. Preserve atomic decode's no-application-callback barrier. Public nested views borrow the enclosing PacketView/RxEnvelope; a consumer needing survival must retain the RX owner or materialize a semantic snapshot explicitly.

Use appendix limits (depth4,4096 work,256 records/field,1024 index/association entries,128 selected fields, configured arena capacity) throughout recursion, not fresh limits per child. Each Array-of-CIFs record begins with its index; CIF0/CIF1/CIF2/CIF3/CIF7 masks describe the common record shape. Already accepted I9 requires five CIF words after3basewords, HeaderSize7 and total `8+R*N`; peer agreement gates semantic use. It must not be relabeled universal standard interoperability.

Sector timing uses specific definitions, not the erroneous one-word summary: dwell/time3/time4 are2-word femtoseconds; start time follows enclosing TSI/TSF. The explicit context dependency is a normal API parameter. Counts, record selector shape and resulting timestamp-dependent sizes must participate in signatures. Unknown CIF4/5/6 content has no skip-by-guess rule.

Spectrum alpha remains raw32 unless an encoding convention is supplied (§9.6.1.8 p176); window IDs and type descriptors do not require an FFT implementation or external window-coefficient computation. No new framework decision is needed to carry those typed codes, and no unsupported DSP capability should be advertised.

## Registered extension interface

The architecture/appendix already defines the policy. A bounded setup-only `ExtensionRegistry<N>` can register immutable `ExtensionClass` descriptors keyed by full Class ID plus packet family and checked prologue-option contract. Reject duplicate keys and mutation after freeze. Retain setup owner lifetime or require it explicitly to outlive the frozen registry; hot dispatch must neither allocate nor load code from packet data.

Suggested descriptor members are bounded payload limits, allowed header/prologue bits, declared Control/Ack CAM custom masks, a pure checked payload validator, optional measure/encode operations over caller-owned native state/output, and a separately invoked semantic dispatcher. Validator input is a bounded payload view plus envelope, class context and shared work budget; validator must not execute hardware/application actions. Returned metadata/index capacity is caller supplied. Framework first checks envelope/source/class/admission permissions; only after whole-payload validation and admission may an authorized dispatcher run. An unknown extension stays bounded opaque bytes and cannot execute commands.

Sections6.4 pp80–81 and7.2 p87 permit custom payloads; resemblance to standard CIF bodies is a recommendation, not a mandatory CIF parser. Section8.6 pp122–123 retains the common Command prologue but permits registered custom indicator/payload structures. Explicit Permissions8.6-2/3 allow CAM bits7..1; the preceding introductory “lower8 bits” prose must not be used to repurpose bit0. Keep standard timing/request/identity validation unless the actual standard expressly permits custom meaning. Placing an extension dispatcher directly inside structural `decode_packet` would violate architecture §3; expose a validated result for the framework dispatch phase instead.

Concrete vendor emission needs a caller-owned OUI/class assignment, payload layout and semantic documentation. Those inputs are unavailable for a real vendor class, but not necessary to implement/test the registry using explicitly named isolated fixtures. No default vendor codec or invented class/OUI is authorized. A no-Class-ID extension can remain opaque unless the static binding supplies an unambiguous documented class; do not match it to a wildcard registered vendor key.

## General sample access and conversion

The existing `SampleView<T>`/`pack_iq` paths cover only the three simple complex formats. Do not broaden their implicit assumptions. Add an independent descriptor-driven path with explicit `PackingSpec`, `PayloadExtent` and caller-owned output:

- `PackingSpec`: Data Item numeric format/width, packing-field width, fractional/exponent width where applicable, real/Cartesian/polar type, component/channel repetition, vector size, repeat count, channel/event tag widths and packing mode.
- `PayloadExtent`: bounded byte/segment view, exact meaningful item/structure count or sufficient explicit pad information, and required frame/trailer/class context. Missing metadata that admits multiple sample counts returns an insufficient-input error; never consume padding as invented samples.
- `PackedItemView::at(index)` returns exact raw data bits and separate tag values plus checked semantic coordinates (component/channel/repetition/vector/time). No reinterpret-cast native span and no mandatory sample walk just to expose opaque payload bytes.
- `measure_pack(...)` and `pack_items(..., MutableBytes)` validate full dimensions/capacity before writes. Reject unsupported overlap rather than risking partial source overwrite. Segmented input can use a bounded bit cursor; conversion output remains explicit caller storage.
- Numerical `convert(..., ConversionPolicy, destination)` is separate from bit-preserving access. Policy explicitly selects rounding, overflow/error versus saturation, nonfinite handling, permitted precision loss and VRT equivalent-code selection. Exact/raw mode has no silent narrowing. Preserve polar components by default; Cartesian transformation is a distinct requested operation.

These are routine contract choices consistent with architecture; no universal lossy policy is needed. Baseline ties-even/saturation/nonfinite rejection remains the IQ provider policy, not a new normative requirement for all generic formats.

Normative checks from §§6.1.1.1–5 pp65–73 and6.1.2 p74:

- Item/packing widths1..64; tags0..15/0..7; item left-justified, channel tag rightmost and event tag immediately left. Spare packing bits sit between item and tags. Tags apply separately to complex components; equal component tags are not a mandatory rejection rule.
- Link-efficient items can span words; processing-efficient layout has right-side word gaps. The latter zeros are recommended (“should”), so preserve/ignore or caller-explicit strict validation rather than declaring every nonzero gap malformed.
- Preserve component/repetition/vector ordering and integral packing structures. No complex sample/structure split across packets. Frame state needs a separate bounded assembler if requested; ordinary sample access need not allocate/assemble an entire frame.
- Signed/unsigned normalized fixed point widths1..64 require exact integer math; phase uses pi/2pi scaling, not ordinary amplitude normalization. Non-normalized fraction count is explicit. Time-domain and spectral format eligibility differ.
- VRT mantissa/exponent representation is specified locally (Rules9–12 pp71–72 and AppendixD p323); it is not IEEE exponent/bias. Preserve mantissa/exponent when target precision cannot represent all bits.
- IEEE formats16/32/64 preserve signed zero, infinities, subnormals and NaN bits in raw access. No baseline nonfinite ban on this raw path. Complete numerical conversion conformance invokes IEEE754 by Rule13 p72; its full semantics are not reproduced in the supplied VITA PDF.

An authoritative IEEE754 specification/reference and independent rounding/NaN/subnormal vectors are required before claiming complete IEEE conversion qualification. No such source was identified in repository documentation for this audit. This limits that claim; it does not block raw half/single/double bit access, fixed/VRT conversion work, or an explicitly limited numerical API. Physical unit calibration, tag meanings, polar application meaning and sample-frame association are caller/class inputs; hardware performance qualification is separate.

## Suggested next gates

Freeze CIF7 attribute-shape/native-type additions first; independently test all13 masks, Current omission, one-word Probability/Belief on large structures, transactional edits, and raw invalid physical values. Gate nested traversal next with malformed lengths, selector-only and diagnostic cases, nested work exhaustion and offset signatures. Test registry duplicate/freeze/unknown/validator-before-dispatch and owner lifetime separately. Sample gates should distinguish exact packing golden vectors from lossy conversion policy tests; use all extrema, odd widths, tags/repetition, ambiguous padding, fragmented/unaligned bytes and caller-storage exhaustion. Re-run affected P02/P09 and fuzz gates for shared traversal changes.

No P15 device/SDK or external peer is needed for these software contracts. Remaining actual input limitations are explicit: Probability percentage interpretation (raw storage is safe), existing I9 semantic peer agreement, a coefficient convention for numerical Spectrum alpha, real extension deployments, and complete IEEE numerical qualification. None should be hidden behind a claim of full M5 interoperability.

## Sector header-count clarification

Independent implementation-stage source review distinguishes the physical three-word header from the encoded HeaderSize byte. Sector/Step-Scan has no optional global header, so §9.3.1 pp130–131 and §9.6.2.1 p180 require encoded HeaderSize0. Earlier shorthand H3 referred to the physical extent and must not be copied into that byte. Pointing Vector and I9 have their own explicit overrides; neither override applies to Sector. See [independent structure verification](P14-cif1-structures-verification.md).
