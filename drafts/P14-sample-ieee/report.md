# P14 binary16/32/64 numerical conversion isolated implementation

Approved proposal implemented solely in this isolated draft. No live code, CMake or fixed/VRT draft changed. Scope is exact classification/decode and explicitly rounded conversion among binary16, binary32 and binary64, plus encoding the existing finite `numeric::BinaryValue`. This does not implement arithmetic, decimal parsing, floating environments, hardware exceptions or full IEEE754 conformance.

## Primary reference and policies

Reference is the author's Berkeley SoftFloat3e documentation sections6.1 (rounding),6.2 (underflow),7 (exception flags),8.3 (format conversion), and source at revision `a0c6494cdc11865811dec815d5c0049fba9d82a8`: `f64_to_f16.c`, `s_roundPackToF16.c`, corresponding binary32/64 converters, and ARM-VFPv2 `s_f64UIToCommonNaN.c`, `s_commonNaNToF16UI.c`, `specialize.h`. Paths/library are recorded in `artifacts/P14-softfloat-reference.json`. Documentation: https://www.jhauser.us/arithmetic/SoftFloat-3/doc/SoftFloat.html and https://www.jhauser.us/arithmetic/SoftFloat-3/doc/SoftFloat-source.html . Source is consulted as a primary implementation reference and linked only into a developer oracle outside the workspace; no code will be copied and no production dependency selected. SoftFloat specialization choices for NaNs are explicit comparison targets, not universal IEEE mandates.

Reuse the fixed/VRT draft's exact finite representation and four rounding directions (nearest ties even, toward zero, toward negative, toward positive). No host floating operations, mandatory `double`, compiler-specific128-bit arithmetic, heap fallback, or global rounding/exception state.

All conversion policy arguments are required, with deleted default construction:

- `Precision`: existing exact_only / allow_rounding. Applies to finite value loss; NaN information loss is governed separately by the mandatory payload policy.
- `Rounding`: existing four directions. No ties-away/round-to-odd in this batch.
- `Overflow`: error / ieee_result / saturate_finite. ieee_result returns signed infinity or maximum finite according to direction, using post-rounding exponent overflow; saturate_finite clamps only when the same overflow event is detected. This differs intentionally from fixed/VRT's pre-quantization domain check. exact_only rejects any inexact finite conversion, including overflow, regardless of overflow policy.
- `NonFinite`: reject / propagate. reject applies to input infinities and NaNs; finite overflow handling remains the separate policy above.
- `NaNPayload`: exact / preserve_high / canonical. exact rejects discarded nonzero payload bits; preserve_high aligns payload's most significant bits and reports any lost low bits; canonical returns the positive default quiet NaN, reporting discarded sign/payload information. Every accepted signaling NaN is quieted and reports invalid. This is numerical conversion, not raw NaN-bit transport; raw transport remains available separately.

Tininess is fixed to **after rounding**, explicitly documented and compared to the oracle configured accordingly. Underflow is raised only for an inexact tiny result; exact subnormals do not report underflow. Here after-rounding tininess is assessed at target precision with an unbounded exponent, before encoding onto the reduced-precision subnormal grid; it is not simply the final encoded exponent being zero. Result flags are local return values: invalid, inexact, overflow, underflow, payload_loss. No hidden floating exception environment is modified. No divide-by-zero flag is needed for conversion.

## Implemented API

Namespace `vita::codec::ieee` in isolated `include/vita/codec/ieee_samples.hpp`:

```
enum class Format { binary16, binary32, binary64 };
enum class Category { zero, finite, infinity, quiet_nan, signaling_nan };
struct Decoded {
    Category category;
    bool negative;
    numeric::BinaryValue finite;
    uint64_t payload;
    unsigned payload_bits;
};
Result<Decoded> decode_exact(Format, uint64_t raw_bits);
Result<numeric::BinaryValue> finite_value(const Decoded&);
Result<Converted> encode_finite(Format, numeric::BinaryValue, Policy);
Result<Converted> convert(Format source, uint64_t raw_bits,
                          Format destination, Policy);
struct Converted { uint64_t bits; Flags flags; };
```

`Decoded` is observational output, not accepted as a freely forgeable encode input. Only `decode_exact` and `finite_value` produce/consume it; `finite_value` validates its invariants and rejects nonfinite classes. Sign is preserved for zero in `Decoded.negative`; the returned canonical `BinaryValue` represents zero without sign. `encode_finite` deliberately accepts the input BinaryValue negative flag on zero as an explicit signed-zero request, and does not canonicalize it away. `convert` preserves signed zero through its separate decoded sign. Nonzero `finite.negative` matches the category sign; irrelevant fields are zeroed. NaN payload excludes the quiet/signaling discriminator bit and carries its source width (9/22/51). No numerical approximation is applied during decode and no signaling NaN exception is raised by raw classification alone.

For NaN numerical conversion, source sign is preserved under exact/preserve_high; destination quiet bit is always set, so zero remaining payload stays a NaN. Widening left-aligns payload, narrowing removes low-order bits. Same-format numerical signaling-NaN conversion also quiets/flags invalid; callers needing unchanged bits must use raw sample access. canonical uses sign0, quietbit1, payload0. Exact payload policy is orthogonal to quieting: it requires payload preservation, not signaling-state preservation.

`finite_value` bridges IEEE decode to fixed/VRT encode without implicit signed-zero retention; the caller sees the source Category/sign first. Fixed/VRT to IEEE is simply `decode_exact` there followed by `encode_finite` here with explicit IEEE policies. Item tags/packing/order remain caller-owned exactly as in the fixed/VRT draft.

## Bounded arithmetic and validation

Formats have precision11/24/53, exponent widths5/8/11, biases15/127/1023 and fraction widths10/23/52. Decode reconstructs exact significand and binary quantum, including subnormals. The largest significand fits53 bits and exponent range fits int16; raw bits above format width reject.

Finite encode derives the highest set-bit exponent using bounded integer bit width. Choose a normal quantum from desired precision or the fixed subnormal quantum. Round once from the original exact magnitude using explicit quotient/remainder and shifts>=64 branches. A significand carry renormalizes and may overflow the maximum exponent. No iterative floating approximation and no double rounding. Tiny exact/rounded zero retains input sign. Extremely large/small BinaryValue exponents are classified without constructing enormous shifted integers. Target range overflow and finite rounding flags follow the same selected rounding direction as the oracle; caller-requested rejection/saturation is layered explicitly afterward.

Test before broad implementation promotion: all65536 half patterns widened to32/64 and returned to16, with raw classification/quieting/payload policies checked; all finite half patterns cross-compared under four rounding directions; binary64 patterns around every important half/single boundary (minimum subnormal, halfway, largest subnormal to minimum normal, maximum finite/overflow midpoint), positive/negative zero, directed rounding, infinity, quiet/signaling NaNs and payload truncation. Compare result bits and flags with the pinned ARM-VFPv2 oracle using after-rounding tininess and matching preserve_high behavior. Add independent literal boundaries and reproducible fixed-seed binary64 cases. Exhaustive all-half inputs do not prove all binary64 behavior; report actual coverage. Nonfinite policy rejection, exact-only failures, malformed format/high bits and no-allocation operation tests remain distinct from numerical oracle tests.

No unresolved input blocks this scoped conversion implementation. Coordinator approved the API/policy proposal; the pinned SoftFloat source is available for independent reference checks. Fixed/VRT code remains frozen and is reused only through its public types.


## Developer evidence and source freeze

The header is `include/vita/codec/ieee_samples.hpp` under this draft. It uses the frozen fixed/VRT public BinaryValue/type declarations and canonicalization helper; it does not use their internal quantizer or alter that draft. All IEEE finite rounding operations are independently implemented as bounded unsigned integer quotient/remainder operations. There is no copied SoftFloat code, production link dependency, heap storage, mutable state or host floating environment access.

Developer `tests/ieee_samples.cpp` passed Debug and ASan/UBSan with Clang C++23, `-Wall -Wextra -Werror -fno-exceptions -fno-rtti`. Include paths: `-Idrafts/P14-sample-ieee/include -Idrafts/P14-sample-numeric/include -Iinclude`. Sanitized invocation adds `-fsanitize=address,undefined -fno-omit-frame-pointer`. Literal cases cover finite normal/subnormal boundaries, signed zero and its explicit loss through finite_value, extreme BinaryValue exponents and uint64 maximum, exact-only and overflow policies, all NaN policies, malformed formats and invariants. All65536 half encodings widen and return with the documented signaling-NaN quieting behavior. Zero ordinary C++ allocations were observed during these developer operations; no universal C-allocation instrumentation claim is made.

The test-only external oracle `tests/oracle.cpp` compares **2,014,000** conversions against the pinned SoftFloat ARM-VFPv2 library, across all four supported rounding directions. Every half bit pattern widens to32/64 and converts back; additional binary64 tests cover every exponent with five fraction-edge patterns and both signs, targeted half/single boundaries with neighboring raw bits, and50,000 deterministic xorshift patterns per rounding direction (seed0x69c01234ab87def1), also interpreted as binary32 for both target widths. Bits and invalid/inexact/overflow/underflow flags all matched. Payload-loss is an additional local policy flag, not a SoftFloat exception flag.

Reproduce from the workspace root with `python3 drafts/P14-sample-ieee/tests/run_oracle.py` or add `--sanitize`. The script checks the recorded and actual external checkout revision, compiles the test against the external archive in a temporary directory, then removes its binary. Both ordinary and ASan/UBSan oracle runs passed. The supplied external archive itself is not sanitizer-instrumented; sanitizer evidence covers this draft and test wrapper. No reference source or binaries are redistributed in this draft.

One differential finding clarified the adopted underflow semantics: binary64 raw0x3f0ffc0000000000 to half with nearest-even returns0x0400 **and** underflow+inexact in the pinned after-rounding reference. The exact midpoint is still tiny after rounding to11 significant bits with an unbounded exponent; its later reduced subnormal-grid encoding rounds to minimum normal. The implementation now performs the separate bounded target-precision tininess check, and the literal developer regression asserts both bits and flags. This is documented reference alignment, not a new wire dialect or a general IEEE conformance claim.

Frozen producer files are listed in `manifest.sha256`. No independent verification PASS is claimed until the separate reviewer completes it. Unsupported scope remains arithmetic, decimal input/output, ties-away/odd rounding, before-rounding tininess, extended/quad formats, hardware exception delivery and complete IEEE754 conformance. NaN specialization behavior is intentionally identified as ARM-VFPv2-compatible under preserve_high, with explicit alternative policy behavior tested separately.
