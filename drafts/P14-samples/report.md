# P14 exact raw samples — isolated candidate

Scope: `drafts/P14-samples` only. No live headers, CMake, benchmark or runtime changes. Candidate implements bounded exact raw item access/packing; it does not implement numerical conversion, DPF decoding, transport segmentation, whole-frame assembly or claim complete M5 packing coverage.

Header: `include/vita/codec/general_samples.hpp`, namespace `vita::codec::general`. Uses only live core error/byte types. Public API:

- `PackingSpec` supplies item/packing widths, channel/event tag widths, link/processing mode, real/Cartesian/polar kind, mutually exclusive component/channel repetition, vector size and repeat count.
- `measure(spec, structure_count, Limits)` gives exact item/byte counts with checked arithmetic. Limits default4096items and262140payloadbytes; packet-envelope overhead must be enforced by the integrating packet layer. Exact raw payload APIs also allow an empty0-structure span; that does not assert that every packet class allows an empty payload.
- `PackedSamples::create(Bytes,spec,structure_count)` requires the exact computed span extent, keeps an immutable borrow, and offers checked `at`, `coordinates`, `index`. Caller must keep input alive; there is no false ownership/retention promise.
- `pack(spec,structure_count,span<const Item>,MutableBytes)` validates all inputs/ranges/capacity/overlap before any output mutation. After preflight it writes directly into caller storage and leaves excess output capacity untouched. No staging, allocation or source mutation.
- `Item` contains exact unsigned data bits and separate channel/event tag values. Signed fixed, VRT and IEEE representations are unchanged bit patterns. No floating cast is used. Numeric format eligibility/scale and physical meaning are separate inputs outside this raw API.

## Normative derivation and visible limits

Source: supplied ANSI/VITA49.2-2017(R2024), printed §§6.1.1.1–5 pp65–73 and§9.13.3 pp228–230.

Item and packing widths1..64; item+channel+event must fit. Item occupies high bits; channel tag is rightmost; event immediately left of channel. Spare bits between item and tags are emitted zero. Link packing crosses32-bit word boundaries without splitting a field across packets. Output payload rounds to whole32-bit words. Explicit structure count prevents guessing which trailing bits are meaningful.

Processing mode implements whole fields in32-bit words, maximum `floor(32/packing_bits)` per word, high-justified, right gaps zero on emit. Reader accepts nonzero gaps because §6.1.1.2-3 says unused bits *should* be zero, not *shall*. **Processing packing width33..64 returns `unsupported_layout`.** That rule describes only whole fields fitting32-bit words and gives no larger grouping rule. DPF Rules9.13.3-12/13 permit encoded widths through64 but do not supply a64-bit processing group rule. This candidate neither declares those combinations universally forbidden nor invents a dialect; further support requires a defensible source/interpretation. Link-efficient64 remains supported.

Native vector size1..65535 follows explicit Rule6.1.1.3-13; zero/no-vector is represented canonically by1 in this API. Repeat count1..65536; no-repetition requires1. Component repetition on real data is invalid. `RepeatMode` prevents simultaneous component/channel repetition. Descriptor raw minus-one encodings and the representable but semantically oversized vector65536 are outside this API; no silently expanded support.

Coordinates are `{structure,time,channel,component}`. Each structure contains `components * vector_size * repeat_count` items. For no repetition, channel then component order, with a single time. Channel repetition groups each channel's repeat-count samples, each sample's components adjacent. Component repetition groups R consecutive logical samples in time-major/vector order: all first components for the group, then all second components; the next group follows. This implements the stated repeat count of consecutive equal components (§6.1.1.3-9/10), rather than changing it to R*vector_size. Both maps are bounded arithmetic and checked inverses. Actual tag values do not reorder or redefine these synchronous vector coordinates.

No semantic validation of NaNs, infinities, fractional width, VRT exponent, spectral/time numeric eligibility or polar angle occurs here. Those require a later descriptor conversion layer. Processing padding tolerance and explicit complete structures are visible contracts, not claimed universal permissive decoding of all historical partial-complex packets. Segmentation is a later independent extension.

## Developer evidence

Direct compiler: local `clang++`, C++23, `-Wall -Wextra -Werror -fno-exceptions -fno-rtti`, draft include first then live `include`.

```
clang++ -std=c++23 -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
  -Idrafts/P14-samples/include -Iinclude \
  drafts/P14-samples/tests/raw_samples.cpp -o /tmp/p14-raw-samples
/tmp/p14-raw-samples
```

PASS. The same target with `-fsanitize=address,undefined -fno-omit-frame-pointer` also PASS. No shared build directory was used.

Test oracles include hand-derived literal3-bit crossing, asymmetric tags plus unused packing bits,11 three-bit processing fields, two33-bit link fields, exact64-bit IEEE NaN/signed-zero bytes, independent repeat/vector coordinate lists, widths1..64 sweep, invalid enum/dimension/overflow/resource limits, exact-span mismatch, short output and late invalid-input atomicity, overlap rejection and empty-span behavior. Ordinary C++ `new` instrumentation observes zero allocations for tested API paths. This is not universal C/aligned allocator instrumentation; implementation has no allocator/callback path by source inspection.

The code is stateless except caller-owned views/output; no threading or callback ownership was added. Independent verifier approval and later live integration remain outstanding. Refer to `manifest.sha256` for the frozen production/test candidate hashes.
