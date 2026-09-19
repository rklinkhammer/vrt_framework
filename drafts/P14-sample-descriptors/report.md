# P14 DPF descriptor and fragmented sample access isolated implementation

Approved proposal implemented in this isolated draft; no live or frozen draft headers changed. Re-read supplied ANSI/VITA49.2 §§6.1.1.1–6.1.1.5 pp65–73, §6.1.2 p74 and §9.13.3 pp228–230 against the current raw, fixed/VRT and IEEE draft APIs. This batch joins existing checked representations; it does not make a class assignment, infer physical units, assemble multi-packet frames or select a conversion policy.

## Descriptor adapter

New isolated header `sample_descriptors.hpp`, namespace `vita::codec::samples`:

```
enum class SignalDomain { time, spectral, spectral_log_power };
struct Descriptor {
    PayloadFormat wire;
    general::PackingSpec packing;
    std::variant<numeric::NumericSpec, ieee::Format> number;
    SignalDomain domain;
};
Result<Descriptor> descriptor(PayloadFormat, SignalDomain);
```

SignalDomain is required, without a default or an unknown-to-time fallback. It is caller-supplied class/context knowledge: packet ND bits alone do not distinguish every representation. Descriptor is a privately constructed inspectable value, validated once at its factory. The implementation uses private construction and read-only accessors; caller mutation of derived fields is not possible. DPF absent from a packet is not an error at this adapter boundary when the caller supplies the exact class-documented PayloadFormat (Permission6.1.1.4-1). No class documentation is fabricated.

The high32-bit word maps exactly as follows; the lower word carries `(repeat_minus_one<<16)|vector_minus_one`:

| Bits | Mapping and validation |
|---|---|
|31|1 link-efficient,0 processing-efficient|
|30..29|0 real,1 Cartesian,2 polar,3 reserved/reject|
|28..24|Numeric code table below; reserved codes reject|
|23|Sample-component repetition flag|
|22..20 /19..16|Event/channel tag widths0..7 /0..15|
|15..12|Non-normalized fraction width; zero required otherwise|
|11..6 /5..0|Packing/item width minus1, hence1..64|

Numeric code0 maps signed normalized fixed;16 unsigned normalized fixed. Codes1..6 map signed VRT exponent width equal to code;17..22 map unsigned VRT exponent width code-16. Codes7/23 map signed/unsigned non-normalized fixed, respectively. Codes13/14/15 map IEEE binary16/32/64 and require exactly matching item widths. Codes8..12 and24..31 are reserved. Fixed/VRT specs retain the exact integer representation and use the already explicit conversion policies only when the caller requests conversion.

Validate item+event+channel widths <= packing width; fraction < item width and fraction0 except codes7/23; VRT exponent < item width; repeat1..65536; vector1..65535. The encoded vector65536 is semantically rejected under Rule6.1.1.3-13 despite the minus-one field's representational capacity. Flag23 selects component repetition only for complex samples; repeat>1 with flag0 selects channel repetition; repeat1 with flag0 selects none. Flag1 with repeat1 is rejected as inconsistent: Rules6.1.1.3-9/10 say1 means component repetition is not in use, whereas Rule9.13.3-5 sets the flag when in use. Raw DPF bits remain preservable outside this semantic adapter.

Use `general::measure(packing,0)` to share raw packing validation, including the current explicit unsupported processing-efficient packing-field width>32. This remains a supported-scope limitation, not a claim such descriptors are universally malformed. Actual payload creation measures the supplied nonzero or zero structure count against explicit limits; a valid descriptor alone is not a capacity admission.

## Domain and component interpretation

Time-domain rejects non-normalized fixed codes7/23 (Rule6.1.1.4-3). Spectral permits the general numeric formats subject to the constraints above. Spectral log-power requires real fixed integer/fraction items of1..16 bits, with0..N-1 fractional bits and dB interpretation (Rules17–19); IEEE, VRT and complex samples reject. Non-normalized codes7/23 directly provide F. Signed normalized fixed code0 is an equivalent signed integer/fraction representation with F=N-1, so it also meets these explicit numeric limits; unsigned normalized code16 has F=N and fails Rule19. Do not silently reinterpret a normalized code's DPF fraction0 as an integer-only format. This follows the representation definition rather than inventing a blanket non-normalized-code requirement absent from Rule18.

Expose `component_unit(descriptor, component)` returning `dimensionless`, `decibels`, `pi_multiple`, `two_pi_multiple`, or `radians`. The exact number remains separate from that unit. For polar phase component1, signed normalized/VRT uses pi_multiple, unsigned normalized/VRT uses two_pi_multiple, IEEE uses radians. Non-normalized polar phase has no explicit phase scale in the cited rules; its generic exact numeric access can remain available but `component_unit` must return an explicit `unspecified` tag rather than guess radians or pi. If an application needs engineering phase conversion for that combination, it must supply class-documented interpretation. This does not block lossless structural/numeric access or require a universal policy decision. Other polar amplitude and Cartesian/real generic values remain dimensionless absent class/reference-level metadata. No transcendental or physical calibration calculations are added.

## Exact extent and padding evidence

A per-packet binding supplies **structure count**, not a byte-derived guess:

```
struct PayloadBinding {
    size_t structures;
    PaddingEvidence padding;
};
enum class PadReporting { omitted_by_class, exact, allow_zero_when_implied };
struct PaddingEvidence {
    PadReporting policy;
    std::optional<uint8_t> class_id_pad_bits;
};
```

Explicit structure count may originate in authenticated class/context/frame bookkeeping; this layer does not infer it from arbitrary trailing bits or trust the numeric value solely because it fits. The payload must exactly match `general::measure`'s minimum whole-word extent. Last occupied bit is zero for no items, otherwise the shared bit offset of the last field plus packing width; trailing pad count is measured_extent_bits minus last occupied bit (0..31). Internal unused item bits and processing-word slack are not extra samples. Empty extent requires count0/pad0 consistently.

For exact reporting, present pad evidence must equal the calculated tail. For allow_zero_when_implied, a zero reported count is also accepted only when calculated pad < item width (Permission6.1.1-2); nonzero evidence still must match. For omitted_by_class, the optional reported value must be absent; the explicit structure count supplies the missing boundary evidence. No optional-zero conflation. This separates complete-class metadata from a guessed Class ID convention. A class-specific transport binding must check that the caller's reporting mode matches its documented class; this standalone helper does not authenticate that statement.

Do not reject nonzero unused bits by default: processing-word unused zeros are a recommendation, and raw access already preserves data independently of those bits. A future explicit strict-padding policy can be added separately. Trailer bytes, prologue bytes and unrelated packet bytes are excluded from the supplied payload extent.

## Bounded fragmented view without duplicated layout

New isolated header `segmented_samples.hpp`:

```
template<size_t MaxSegments=8> class SegmentedSamples {
    static Result<SegmentedSamples> create(std::span<const Bytes>,
        const Descriptor&, PayloadBinding, general::Limits);
    Result<general::Item> at(size_t) const;
    const general::Shape& shape() const;
    const Descriptor& descriptor() const;
};
```

The view copies at mostMaxSegments byte-span descriptors and checked cumulative ends into inline arrays; it borrows immutable bytes. Caller segment-array lifetime need not outlive the view, but every nonempty byte range must. Copy/move of the view preserves those immutable borrows; it acquires no ownership and returns no pointer to internal scratch. Empty segments are skipped; count limits apply to supplied descriptors before skipping so pathological empty input cannot bypass work limits. Reject nonempty null spans, cumulative size overflow, wrong extent, excessive segments and invalid descriptor/padding before any access.

Construction calls the existing `general::measure`, with its checked bit-offset bounds. Item lookup obtains **the existing `general::detail::bit_offset`** from the frozen raw draft; it does not reimplement link/processing grouping. For each data/tag bit range, a segmented byte reader locates bytes through copied cumulative ends (bounded scan or binary search) and accumulates up to64 bits. Only byte transport differs from the contiguous reader. No concatenation, staging payload, allocation or per-item descriptor copy. Max item work is bounded by64+15+7 bit reads and the compile-time segment bound.

The initial segmented API deliberately omits coordinate/index helpers to avoid copying the contiguous class's currently inline repeat-order equations. The returned descriptor and raw linear item index remain sufficient for access. A later coordinated extraction of a public immutable SampleLayout from the frozen raw header can add shared coordinate/index methods to both views without a second traversal; it is not necessary for this draft's complete linear item access. Existing contiguous coordinates remain the authority for independent ordering tests. Dependency on the current detail offset helper is explicit and isolated; live promotion should elevate that helper into a documented shared layout API in one coordinated change, not silently duplicate it.

Memory fragments mean one packet payload split into byte regions; they are not packet/frame fragments. No frame-reassembly or packet reorder semantics follow from this API. The caller must honor frame identity, start/end indicators, common metadata and whole-structure packet boundaries (§6.1.2). This batch neither accepts splitting a packing structure across packets nor claims to verify multi-packet continuity.

## Verification and remaining scope

Independent literal DPF vectors must cover all32 numeric codes, all real/complex codes, width/frac mismatches, all VRT exponent sizes, exact IEEE widths, tag sum overflow, repeatflag/count constraints, vector65536 rejection and domain-specific eligibility. Include signed normalized log-power F=N-1 versus unsigned normalized exclusion so numeric interpretation is not accidentally derived from the fraction subfield for all formats.

Use literal nonuniform raw payloads with asymmetric tags and repeated complex/vector ordering. Compare contiguous and fragmented item access for every byte split, single-byte fragments, unaligned ranges, fields crossing32-bit words and fragments, widths1/3/12/17/31/33/63/64, and copied descriptor-array lifetime. Test exact/nonzero/implied-zero/absent pad evidence, excessive fragments, overflow, empty payload, borrowed-byte ownership and zero hot allocations. Reserved/unsupported descriptors must reject before item access or downstream callbacks. Numeric operations remain separately gated in the fixed/VRT and IEEE drafts.

No new required input blocks the bounded adapter/view implementation. Remaining explicit non-claims: processing-efficient>32 support, class authentication, physical tag meanings/reference levels, unspecified non-normalized polar phase scale, multi-packet frame assembly, automatic data-rate/clock association and hardware qualification. These are not concealed by generic sample access. Candidate is ready for independent review; no independent PASS is claimed.


## Complete structures versus partial final words

The coordinator requested a source recheck before adding a final-packet partial-structure exception. Rule6.1.1-3 p65 says to round payload bits up to whole32-bit words; nearby prose mentions unfilled final-word bits in final stream/time-frame packets. Neither establishes permission for an incomplete packing structure. Rule6.1.1.5-1 p73 requires integer packing structures, and Observation6.1.2-1 p74 disallows splitting structures across packets. The coordinator confirmed this distinction. No unsupported tail permission, inferred final flag, exact-item-count bypass or shadow raw-header fork was added. Complete structures can still end in a partially occupied final word, with explicit count/padding evidence as implemented.

## Developer evidence and freeze

Producer files are `include/vita/codec/sample_descriptors.hpp`, `include/vita/codec/segmented_samples.hpp` and `tests/descriptors.cpp` in this draft. `Descriptor::wire/packing/number/domain` are read-only accessors; `descriptor()` is the only constructor. `measure_payload()` centralizes count/extent/pad checks, used by both `contiguous()` and segmented construction. The segmented reader shares existing raw measure and bit_offset; the raw draft itself is unchanged.

Developer Debug and ASan/UBSan direct builds passed with Clang C++23, `-Wall -Wextra -Werror -fno-exceptions -fno-rtti`. Include directories are this draft's include, drafts/P14-samples/include, drafts/P14-sample-ieee/include, drafts/P14-sample-numeric/include and live include. Test source is tests/descriptors.cpp in this draft; sanitizers add `-fsanitize=address,undefined -fno-omit-frame-pointer`. No shared CMake/build changes.

Tests cover a literal IQ16 DPF, every32 numeric code, reserved/sample-kind/width/fraction/VRT constraints, unsupported processing64, tag overflow, vector65536 and repeat inconsistency, time and log-power eligibility, explicit phase units, and absent/mismatched/implied-zero padding. A literal3-bit sample payload is checked at every byte split and after the original segment descriptor array is cleared. Widths1/3/12/17/31/33/63/64, both supported packing methods, asymmetric tags, complex component-repeated vectors, every two-part byte split and single-byte segmentation compare with the existing contiguous reader and expected items. Tests observe zero ordinary C++ allocations through these operations. No platform-wide C-allocation claim is made.

The borrowed bytes must stay alive and immutable. Null nonempty segment spans are rejected, but constructing arbitrary invalid C++ spans is not a supported means to test real accessible buffer bounds; checked ownership remains the caller's obligation. Compiled MaxSegments controls copied-descriptor memory and per-item scan work; there is no runtime heap fallback. See manifest.sha256 for frozen producer source hashes. Physical/class authorization, multi-packet assembly and baseline runtime integration remain outside this isolated component gate.
