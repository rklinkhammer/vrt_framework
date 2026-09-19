# Independent Array-of-CIF structural draft verification

Verdict: PASS for the explicit structural-only draft, independently checked against the frozen live nonrecursive CIF1 provider. No producer or production source was edited by this verifier. This does not authorize peer semantics, native ownership, emission, application associations or unqualified interoperability.

Source audit: supplied ANSI/VITA49.2-2017(R2024) §9.13.1 Rules1–4, printed pp222–223, explicitly names three mandatory header words, five fixed CIFs0/1/2/3/7, HeaderSize7, and an unconditional index per record. Accepted interpretation I9 supplies the specific contradictory-header resolution: total8+record_width*count, no invented padding. The draft requires explicit dialect construction. Fixed CIF words remain physically present independently of enables. The coordinator-approved zero/nonzero CIF7 capability restrictions are reported as unsupported capability, not universal normative invalidity. Semantic agreement and external interoperability evidence remain unavailable; this structural API provides neither an emission nor native/admission capability.

The shadow layout header differs from the frozen live header only by the additive default descriptor resolver. Inspection and ordinary scalar decode confirm default behavior. Nested IndexList limits and padding validation demonstrate the new live CIF1 extent metadata survives this hook. Promotion must apply the additive patch; copying the shadow would risk overwriting later work.

Independent contract.cpp uses hand-written integer-word literals and its own restricted-schema recursive parser for RefPoint/nested arrays, without production descriptors, shapes or traversal in the oracle. A deterministic24000-case corpus mutates total/nested lengths, record widths/counts, header bitmap and field masks and varies field/view/work/depth limits. Exact acceptance and successful consumption counts agree:6173 accepted,17827 rejected. Failed validations leave the input budget unchanged. Successful visits match counted views.

Additional tests cover every proper byte truncation, all16 inherited Age timestamp combinations, explicit/implicit Current capability distinctions, repeated raw index values with distinct ordinal paths, postorder nested visits, callback error stopping, exact128 aggregate fields, depth4/5,256/257 records, shared4096 work across validations, unknown/reserved selections, IndexList limit/padding and default packet decode. Options and View cannot be default constructed. The view is an immutable-byte borrow, not an owning or mutation-safe capability; callers must preserve its backing contents/lifetime. Validation itself has no application callback and only returns a View after the complete field succeeds. Public visit is structural observation, not device execution.

Commands: clang++ -std=c++23 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -Idrafts/P14-array-cif/include -Iinclude verification/contract.cpp. Direct build/run PASS. Repeat with -fsanitize=address,undefined -fno-omit-frame-pointer PASS. Tests use bounded stack arrays; no independent global C/C++ allocation instrumentation claim is made. This draft review is separate from the concurrent CIF1 native-view provenance finding and does not declare that separate package passed.

## SHA-256 evidence

- `drafts/P14-array-cif/include/vita/codec/array_cif.hpp`: `8de2137c2855acd1553bf317eac57b40fcc25438aa9430f46458dbb781860f72`
- `drafts/P14-array-cif/include/vita/codec/layout.hpp`: `71eede1eafbca3342da4d2640df506e500eb04979b0905c70a81f2ff544b70b6`
- `drafts/P14-array-cif/layout-resolver.patch`: `b4a1dd34ceeea576afb1cfc658e2c35f43e6bc2c0695faf443e2a7d2986c005c`
- `drafts/P14-array-cif/verification/contract.cpp`: `ecbae2d22635c493326cf5e1915245e3cd9782cfb4f0fd4e1d2df21b6320ce40`
- `include/vita/codec/layout.hpp`: `0b00ed54e7bfcaabccbcb5cdcc0e4af929ab9c03b167b0b2ddf2d168c25f0e0b`
- `include/vita/codec/cif1_structured.hpp`: `13abc039152a14c17ee64824018dd1bf97bb0f44d07cd32200574069c2e41f7a`

## Live promotion

The reviewed header and both test sources were copied unchanged into the live tree. The descriptor-resolver patch was applied additively to the current shared layout header after the CIF1 native-view repair; the shadow header was not copied over production. The independent mutation test is registered as `p14_verify_array_cif` and runs in ordinary and sanitizer CTest. The complete final source manifest and aggregate gate are recorded in [M5 progress](M5-integration.md).

Final aggregate checkpoint:198/198 Release and195/195 ASan/UBSan checks pass with69 standalone headers. [M5 integration](M5-integration.md#final-available-input-continuation-checkpoint) supersedes the promotion-pending statements above and preserves the bounded component scope.
