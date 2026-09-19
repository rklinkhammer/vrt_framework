# Independent exact fixed/VRT numeric draft verification

PASS for this isolated draft, local macOS. No live production or producer draft files changed. `manifest.sha256` records source and independent oracle/test hashes.

The independent Python oracle uses `fractions.Fraction`, exhaustively enumerates each tiny format's raw representable values, and selects neighboring values directly for rounding. It does not invoke production decode to derive expectations, use host floating arithmetic, or mirror the production exponent-search algorithm. It checks canonical lowest-exponent output codes, explicit loss/saturation flags and errors for all four rounding modes, both precision policies and both overflow policies.

Optimized and ASan/UBSan drivers both pass **259,784 cases**:5,592 decodes and254,192 encodes across98 tiny format specifications plus64-bit/extreme-exponent anchors. Cases cover every adjacent representable interval at quarter/half/three-quarter positions, both domain boundaries and just outside, negative-to-unsigned behavior, exact-only rejection and canonical zero. [Optimized result](oracle-optimized.json), [sanitizer result](oracle-asan.json).

The independent C++ boundary test passes ASan/UBSan. It includes explicit AppendixD anchors, invalid formats and policies,1/64-bit boundaries, negative zero normalization,6,400 decode/exact-encode/conversion cycles, and ordinary/aligned C++ allocation hooks with positive probes. The measured numeric loop allocates zero times. C stdio used by the separate bulk oracle driver is outside that claim. No universal heap or C-allocation assertion is made.

Commands:

```
clang++ -std=c++23 -O2 -fno-exceptions -fno-rtti -Idrafts/P14-sample-numeric/include -Iinclude drafts/P14-sample-numeric/verification/driver.cpp -o /tmp/p14-numeric-oracle
python3 drafts/P14-sample-numeric/verification/oracle.py /tmp/p14-numeric-oracle
```

Sanitizer driver uses `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer` in place of `-O2`; the same Python corpus then runs against `/tmp/p14-numeric-oracle-asan`. `boundaries.cpp` uses the same sanitizer flags and include paths and runs as `/tmp/p14-numeric-boundaries-asan`.

Scope excludes IEEE numerical conversion, polar trigonometry/engineering units, live DPF/profile integration and processing-efficient packing beyond the separately supported raw interface. This is not complete sample-format or P14/M5 qualification. No production defect was found by these independent tests.
