# P10 IQ source implementation

Implementer: contracts agent. This bounded subtask supplies the source/window layer; P10 Runtime, pacing, identity, lifecycle, examples and complete integration remain with the main implementer. Independent verification owns the package verdict.

`vita::profiles::iq::SampleFormat` selects IQ16, IQ32 or float32. Helpers expose complete-pair byte width, the baseline packet class (1/2/3), and the accepted 64-bit Data Payload Format constants. Invalid enum values produce zero width/class and are rejected by the checked window factory and payload validator.

`SampleWriteWindow::create(wire, format, first_ordinal, count, const StateSnapshot&)` accepts exactly the required external payload span, 1–256 complete pairs, and a representable ordinal range. The immutable effective configuration is borrowed for the callback lifetime. `write(index,I,Q)` writes directly into that external byte span, network-order I before Q. A 256-bit coverage bitmap detects omitted pairs without staging sample data. `validate_complete()` requires every pair and checks encoded float samples for nonfinite bit patterns, including any mutation through the exposed payload span. The Runtime calls it before treating provider output as valid.

Finite integer input is scaled by 2^15 or 2^31, rounded to nearest with ties to even, then saturated to the signed range. Explicit arithmetic avoids dependence on integer-conversion rounding mode. Both components are validated before either word of a pair changes. Float input preserves finite values representable in binary32; NaN, infinity and binary32 overflow reject before mutation. Negative zero and subnormals are preserved by normal float conversion. This helper does not impose an additional clipping rule on floating samples.

`SourceProvider{context,callback}.produce(window)` is a nonallocating callback binding. `default_source()` emits the accepted amplitude-0.5 positive-frequency fs/16 complex waveform from a fixed 16-entry cosine table with exact axis zeros. Q is the phase-shifted sine entry; phase derives solely from the absolute sample ordinal, so packet boundaries, skipped intervals, and rate changes do not reset it. Integer output uses precomputed binary64 constants; default floating output uses compile-time binary32 constants, then exact conversions, preserving canonical bits even if the caller changes the floating rounding mode. Runtime owns waveform phase policy through ordinal selection, all packetization/scheduling, external buffer acquisition, and profile identities.

The writer window is 80 bytes and provider binding 16 bytes on the current platform. They own no heap storage; the window references application/provider-owned external payload storage and a Runtime-owned immutable state. The fixed lookup tables are read-only program constants. Stack/local window objects must be included in Runtime/parser-stack accounting as appropriate; no extra persistent sample arena is introduced.

Developer tests verify the literal 16-pair IQ16 cycle, positive-frequency phase continuity, ordinal wrap rejection and final ordinal, both integer scales/ties/saturation, finite/overflow rejection without partial pair writes, exact output extent, missing-pair detection, zero heap allocations, and canonical binary32 output under FE_UPWARD. Direct C++23 development, ASan/UBSan, and TSan builds passed using `-fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic`. The independent verifier separately reports all 16 literal IQ16/IQ32/float32 values derived from high-precision radical identities. Shared CMake target registration is owned by the main P10 implementer (`p10_source`).

Frozen source SHA-256:

```text
613adb3676968a633fefbd3c35f1e5df7faebdfc920b06863c5c94b188a64079  include/vita/profiles/iq/source.hpp
3edda084d1979f5231ef79a0ba617d2449eafe8d28e5a1e24988186d34c88872  tests/unit/P10/source.cpp
```
