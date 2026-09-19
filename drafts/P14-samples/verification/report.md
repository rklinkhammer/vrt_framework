# Independent raw-sample candidate verification

Result: **PASS for the explicitly bounded raw-bit API**, not complete sample conversion or whole-packet conformance. This verifier did not edit the producer header or producer tests.

Authority independently inspected: supplied ANSI/VITA49.2-2017(R2024), Rules6.1.1.1-1 through7 (data/tag placement and widths), Rules6.1.1.2-1 through4 (link and processing placement), Rules6.1.1.3-5 through13 (component ordering, repetition, vector limits), and the producer's explicit unsupported processing-width33–64 boundary. The latter returns unsupported rather than inventing a larger grouping convention.

`contract.cpp` constructs an independent string-of-bits reference representation, inserting whole-word processing gaps and tag/spare positions, rather than calling candidate offset helpers. It checks item widths1–64, both supported packing modes,37-item cross-word payloads, asymmetric tags, entire encoded payload equality, untouched output tails, every decoded item and end bounds. Literal coordinate lists test component groups of3 across vector width2; nested channel/time/component loops independently check channel repetition. Limits, multiply overflow, unsupported processing width, invalid tag/vector inputs, overlap, late-invalid-input rollback, short spans, empty payloads, and borrowed-view copying are included.

Direct Clang C++23 with `-Wall -Wextra -Werror`: PASS. Same independent target with `-fsanitize=address,undefined -fno-omit-frame-pointer`: PASS. The target's oracle uses dynamic strings/vectors outside the candidate; zero allocation is supported here by inspection of the header's bounded scalar/span operations, not by claiming allocator instrumentation in this verifier. Producer reports separate operational allocation instrumentation. No shared build directory, runtime file, or benchmark was modified.

The view is explicitly borrowed: the test demonstrates a copied view reading a subsequent caller buffer modification while that buffer remains alive. There is no ownership extension and no use after the source lifetime. Exact supplied structure counts avoid inferring payload sample counts from padding. No numeric eligibility, finite-value checks, DPF parsing, stream tag semantics, segmentation, physical timing or deployment qualification is claimed.

Frozen SHA-256:

```text
9017770d341547de041a93f11a47a2815cd65ee30bb558cace73429f0ce47597  drafts/P14-samples/include/vita/codec/general_samples.hpp
127a2af832f8ff9727a6821143c09a4815929469e30b7ed21a048b178fb54bc3  drafts/P14-samples/verification/contract.cpp
```
