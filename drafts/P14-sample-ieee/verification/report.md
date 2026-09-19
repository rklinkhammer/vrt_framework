# Independent IEEE conversion candidate verification

Result: **PASS for the declared conversion/classification policy**, not complete IEEE754 conformance. This reviewer wrote a separate test and did not change the producer header, tests or rounding implementation.

The external reference checkout was verified against `artifacts/P14-softfloat-reference.json`: Berkeley SoftFloat commit `a0c6494cdc11865811dec815d5c0049fba9d82a8`, ARM-VFPv2 specialization. Inspected primary reference `s_roundPackToF16.c` and specialization `s_commonNaNToF16UI.c`, including before/after rounding tininess and explicit NaN quiet/sign/payload selection. Only test code links the external archive; no reference source is copied into production.

Independent test: **476,480 reference comparisons**, all four supported directions. Coverage includes all65,536 binary16 patterns widened to32, independent exponent/fraction category classification, all binary32 exponent codes with seven asymmetric fraction-edge patterns and both signs narrowed to16, and25,000 reproducible LCG binary64 bit patterns per direction converted to16 and32. Seed `0xd177be9ac8436025`, multiplier6364136223846793005, increment1442695040888963407. Compared result bits and invalid/inexact/overflow/underflow flags. Every finite reference result also checks exact-only acceptance versus inexact rejection; overflow results additionally check explicit finite saturation and sign.

Literal tests separately check negative zero, signaling/quiet NaNs, exact payload rejection, high-payload preservation, canonical positive NaN and payload-loss flags, nonfinite rejection, malformed source formats/high bits, and forged Decoded invariants. The recorded boundary `0x3f0ffc0000000000` binary64→half produces minimum normal0x0400 with underflow+inexact in the pinned reference and candidate. This is explicitly the selected reference-aligned tininess contract; the report does not extrapolate it to universal arithmetic or hardware exception behavior.

Both ordinary Clang C++23 `-O2 -Wall -Wextra -Werror` and `-O1 -fsanitize=address,undefined -fno-omit-frame-pointer` runs PASS. The external SoftFloat archive is not itself sanitizer-instrumented. The test deliberately mutates only SoftFloat's test-local rounding/exception globals; production conversion uses returned flags and explicit policies. Production source inspection finds bounded integer arithmetic, no allocator or mutable global state; this reviewer does not claim allocator instrumentation from the oracle target.

This gate does not exhaust all binary32/64 source patterns or all finite BinaryValue inputs, and does not claim arithmetic, decimal conversion, additional rounding modes, extended formats or hardware exception delivery. Existing raw-bit APIs remain necessary for unchanged signaling NaN transport. Producer evidence is additional and distinct from these independent476,480 comparisons.

Frozen SHA-256:

```text
1288e659b6131ab10ba0a808f05544d9f4286de3a3e1a28e0a08dcff538faef9  drafts/P14-sample-ieee/include/vita/codec/ieee_samples.hpp
359f28ff1edb2d0ea926604b020da6f7c9f1216c24561b047a8f46dfa9cc69a4  include/vita/codec/numeric_samples.hpp
cbe46929492e78998deeb892038e2c89fea2c7b29112dfab551214d51b10e5c8  drafts/P14-sample-ieee/verification/contract.cpp
```

## Live promotion split

Portable independent classification/policy checks are now `tests/verification/P14/ieee_contract.cpp`, with no SoftFloat headers or links. Plain and ASan/UBSan runs pass against the promoted live header. The476,480-comparison external oracle is preserved separately in `tests/verification/P14/ieee_oracle.cpp` and optional `run_ieee_oracle.py`; it repeats PASS against the live header and is not required by normal CMake/CI. The script validates the pinned revision, never fetches/builds dependencies, and reports unavailable external input through process failure.

```text
1288e659b6131ab10ba0a808f05544d9f4286de3a3e1a28e0a08dcff538faef9  include/vita/codec/ieee_samples.hpp
5fb3692b65839a4713386fea2e4b5c400fe4c702ad767a428e565c71e52c7ef5  tests/verification/P14/ieee_contract.cpp
cbe46929492e78998deeb892038e2c89fea2c7b29112dfab551214d51b10e5c8  tests/verification/P14/ieee_oracle.cpp
68a8f75334511ed3b65ca8e94da1a5dc2494df6533987a5da7df772f0d01b01f  tests/verification/P14/run_ieee_oracle.py
```
