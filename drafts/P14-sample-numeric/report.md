# P14 exact numeric sample conversion isolated implementation

Approved proposal implemented in this isolated draft, 2026-09-19. No live headers, CMake files or other drafts changed. Scope is fixed-point signed/unsigned normalized/non-normalized and VRT signed/unsigned exponent1..6 decode/encode. Re-read supplied ANSI/VITA49.2-2017(R2024) §§6.1.1.4–6.1.2 pp70–74, §9.13.3 pp228–230, AppendixD pp323–325, and independent `P14-sample-readiness.md`. IEEE raw special-bit preservation already belongs to the raw packing draft; numerical IEEE conversion is not claimed here.

## Exact value and format contracts

Implemented exact value:

```
struct BinaryValue {
    uint64_t magnitude;
    int16_t exponent;
    bool negative;
}; // value = (-1)^negative * magnitude * 2^exponent
```

Canonicalize zero to `{0,0,false}` and remove factors of two from nonzero magnitude while checking exponent overflow. This retains unsigned64 maximum and signed64 minimum without converting through signed absolute-value overflow or `double`. Fixed/VRT decode always fits64 magnitude bits; measured and asserted sizeof16bytes. Raw original bits remain available separately when original VRT equivalent encoding matters. There is no negative zero in these finite fixed/VRT number formats.

`NumericSpec` supplies kind (`unsigned_fixed`, `signed_fixed`, `unsigned_non_normalized`, `signed_non_normalized`, `unsigned_vrt`, `signed_vrt`), total widthN, and applicable fractional or exponent width. Reject invalid/irrelevant nonzero parameters. Fixed widths1..64; VRT N2..64,E1..6,M=N-E in1..63. Non-normalized fractionF must be less thanN; when represented through standard DPF it is also at most15 because that field is4bits. Do not silently accept a generic mathematical fraction width that cannot be conveyed by the chosen class/DPF contract. Normalized and VRT formats require DPF fraction code0 (§9.13.3 Rules8–11).

Raw decode returns `BinaryValue` exactly:

| Format | Exact value before normalization |
|---|---|
| Unsigned normalized fixed | raw * 2^-N |
| Signed normalized fixed | signed_twos_complement(raw,N) * 2^-(N-1) |
| Unsigned/signed non-normalized | corresponding integer * 2^-F |
| Unsigned VRT | mantissa * 2^(e - ((2^E)-1) - M) |
| Signed VRT | signed_twos_complement(mantissa,M) * 2^(e - ((2^E)-1) - (M-1)) |

VRT exponent occupies the lowE bits; mantissa occupies highM bits. It is neither IEEE exponent/bias nor an unscaled integer left shift. Decode checks all input bits aboveN are zero; use bounded unsigned mask/two's-complement magnitude operations, handlingN64 explicitly to avoid shifts by64 and signed negation overflow. WithN<=64 the smallest possible unsigned VRT exponent in this table is-121 (E6,M58), easily inside int16. Canonicalization can increase exponent for values with trailing-zero magnitudes; mathematical input accepted by encode must still be range-checked without oversized shifts.

Source golden anchors: AppendixD FigD-1 p323 `11111` unsigned E2/M3=7/8, `00100`=1/64; signed FigD-2/TableD-2 p325 `11111`=-1/4, `11100`=-1/32, `10011`=-1, `01111`=3/4. TableD-1 p324 contains an apparent arithmetic typo: row `10000` lists mantissa100/e0 but1/32; the normative shift model gives4/64=1/16. Do not copy that erroneous row as an oracle. The explicit representation rules, adjacent examples and signed table provide a settled formula; no new numeric dialect is needed.

## Implemented APIs

```
Result<BinaryValue> decode_exact(NumericSpec, uint64_t data_bits);
Result<EncodedValue> encode_numeric(NumericSpec, BinaryValue, ConversionPolicy);
Result<EncodedValue> convert_numeric(NumericSpec source, uint64_t bits,
                                     NumericSpec destination, ConversionPolicy);
```

`EncodedValue{uint64_t bits; bool rounded; bool saturated;}` reports losses explicitly. `ConversionPolicy` has a deleted default constructor and requires all four policy arguments:

- `Precision::exact_only | allow_rounding`;
- `Rounding::nearest_ties_even | toward_zero | toward_negative | toward_positive`;
- `Overflow::error | saturate`;
- `VrtEncoding::lowest_exponent` initially (documented canonical choice, not a VITA requirement).

`exact_only` rejects any nonzero quantization residue **or clipping**, regardless of selected rounding/overflow settings. `allow_rounding` applies rounding; representable-domain violations still return error unless saturation is explicitly selected. Saturation clamps to the true target minimum/maximum (negative input to unsigned can clamp to0 only under saturation); it sets `saturated=true`. Rounding changing a value sets `rounded=true`; saturation need not be mislabeled ordinary rounding. Error results use existing bounded error codes with an inspectable reason/status if needed; no exceptions or allocation.

The raw sample API already exposes `Item.data_bits`, separately preserving channel/event tags. Numeric conversion consumes/produces only those data bits; an adapter creates the output `Item` with the caller's explicit tag policy and uses raw `pack`. Do not silently drop or alter tags, vector ordering, repeat count, class eligibility or output item width. Initial batch can stay scalar; later span conversion must preflight all values/capacity or document explicit partial results, with no hidden staging allocation.

`BinaryValue` is a dimensionless exact number. For polar phase, normalized unsigned/VRT is multiplied by2π and signed byπ; expose this as an explicit semantic unit tag or separate phase interpretation. Do not approximate irrational radians inside the exact binary representation. No automatic polar-to-Cartesian transform, transcendental library calls or engineering-unit reference-level calibration belongs in this batch. Time-domain non-normalized prohibition and spectral log-power constraints remain descriptor/profile checks (§6.1.1.4 Rules3,17–19); exact generic arithmetic does not imply every numeric kind is legal for every signal class.

## Bounded encode algorithm

For fixed formats, compare exact input against target bounds using sign, leading-bit position and exact unsigned comparison before shifting. Scale to the target quantum with checked left shift or quotient/remainder right shift; shifts>=64 use explicit branches (zero quotient plus remainder classification), never C++ undefined shift behavior. Rounding uses retained quotient parity and exact relation of remainder to half a quantum. Compute signed rounding directions from the sign; do not round unsigned magnitude toward positive and then accidentally apply the wrong signed direction. Check rounded magnitude against the asymmetric signed limits before forming two's-complement bits.

For VRT, at most64 exponent candidates exist. Select the finest allowed quantum that can represent the policy-rounded value without mantissa overflow. Because successive quantum grids nest, no unbounded search or floating error comparison is needed. For equal exact values use the lowest exponent that carries the mantissa; zero canonicalizes to allzero bits. Near a mantissa boundary, a rounded finer candidate may overflow while a coarser candidate succeeds; recompute from the original exact value for each candidate to avoid double rounding. Check global representable bounds first and apply explicit saturation policy. Exact-only accepts any exact candidate and picks lowest exponent; equivalent raw encodings are not promised to survive numeric decode/reencode.

Use portable unsigned arithmetic with explicit carry where a one-bit rounding increment is needed. A two-limb helper may be used if it simplifies exact comparisons, but no mandatory compiler-specific128-bit type and no arbitrary-precision heap fallback are needed. Maximum64 candidates and bounded64-bit operations per candidate provide a fixed work ceiling. Input exponent is int16; arbitrary large/small exponents can be classified against target bounds without allocating a giant shifted integer.

## Verification plan and remaining source inputs

Independent literal tests should include1bit signed/unsigned endpoints, unsigned64 maximum and signed64 minimum, every E1..6, N64 VRT mantissas, AppendixD anchors, equivalent codes with distinct exponent bits, min nonzero values, exact fixed/VRT conversions, one-half ties with even/odd quotient and both signs, just-outside extrema, directed rounding, negative-to-unsigned, saturation status, exact-only rejection, underflow-to-zero and no double rounding near exponent transitions. TinyN exhaustive enumeration can compare rational integers against independently constructed tables rather than merely testing matching encode/decode. No hot allocation and unchanged raw tags/order require separate integration tests later.

No missing external numeric source blocks **this fixed/VRT proposal**. The supplied VITA representation rules are adequate and include independent illustrative anchors. The AppendixD single bad arithmetic row is documented, not used to create an alternative layout.

IEEE754 numerical semantics are expressly delegated by Rule6.1.1.4-13 p72. The supplied VITA document does not fully define binary16/binary32/binary64 conversion rounding, subnormal/NaN/signaling behavior, exception flags or NaN payload transformation. The coordinator subsequently located the primary Berkeley SoftFloat3e author documentation and source (https://www.jhauser.us/arithmetic/SoftFloat-3/doc/SoftFloat.html), providing an available reference path for a separate numerical IEEE batch; no dependency has been selected here. Preserve IEEE raw bits now; a later numerical IEEE batch needs authoritative primary-reference evidence and a scoped supported contract. This is not a claim that every use of host IEEE floats is blocked, nor permission to market full IEEE conformance from raw wire access.

The fixed/VRT candidate is ready for independent review. No general sample/packing completeness claim follows: processing-efficient>32bit field grouping remains visibly unsupported in the raw draft, segmented input is not yet implemented there, and engineering-unit conversions require explicit class/source metadata.


## Developer evidence and frozen scope

Files: `include/vita/codec/numeric_samples.hpp` and `tests/numeric_samples.cpp` beneath this draft. Namespace `vita::codec::numeric`; dependencies are the live core error/expected type plus C++ integer utilities. The test additionally includes the frozen raw-sample draft to prove explicit data-bit conversion preserves caller-carried channel/event tags through packing. There is no physics/unit API: dimensionless exact values are the only supported numerical output.

Developer Debug and ASan/UBSan direct builds passed with Clang C++23, `-Wall -Wextra -Werror -fno-exceptions -fno-rtti`. The common include paths are `-Idrafts/P14-sample-numeric/include -Idrafts/P14-samples/include -Iinclude`; source is `drafts/P14-sample-numeric/tests/numeric_samples.cpp`. Sanitizers add `-fsanitize=address,undefined -fno-omit-frame-pointer`. No shared build directories or other candidates were modified.

Tests include literal AppendixD values (including the corrected normative value of the erroneous table row), all fixed widths1..64 and all VRT exponent widths1..6 at64-bit item width; extrema, tie/directed rounding, negative/unsigned saturation, exact-only clipping rejection, extreme input exponents, invalid enum/specification rejection and raw-tag integration. The small-format oracle enumerates the entire representable set independently as signed integers on a common rational grid, checks every quarter-minimum-quantum input across and just beyond its domain, and searches neighboring representable values for all four rounding directions. It covers signed/unsigned fixed widths1..6, every non-normalized fraction below those widths, and all VRT exponent widths1..3 that fit. It checks canonical output codes, not merely encode/decode round trips. This oracle exposed and fixed final canonicalization after coarse rounding: the chosen output mantissa is doubled, within its sign-specific range, while reducing the exponent without further rounding.

Maximum search is64 candidates plus at most63 exact canonicalization steps. All shifts explicitly avoid64 or greater; comparisons use normalized leading-bit positions without compiler-specific128-bit integers. No floating point, dynamic allocation, external callbacks or mutable shared state occurs in conversion. The developer test observes zero ordinary C++ allocations during the exercised operations; this is bounded evidence, not a platform-wide C-allocation instrumentation claim. `BinaryValue` is16bytes on the tested platform, with no arena or hidden storage.

Error contract: malformed specifications/policies/raw high bits and exact-only quantization failure use `invalid_argument`; domain overflow and forbidden exact-only clipping use `overflow`. A failed result has no output value and conversion never writes caller-owned input. Saturation is separately flagged and does not set `rounded` merely for clipping. The header is not promoted and no independent PASS is claimed here. See `manifest.sha256` for the frozen producer source hashes.
