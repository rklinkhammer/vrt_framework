# P14 generic-capacity fuzz exposure

Scope: fuzz target, corpus and its CMake tests only; no codec changes. Both the original16-field/64-view path and explicit128-field/1664-view path now use the same templated invariant helper. Each is exercised with absent and supplied original-request correlation. Input is capped at65536bytes; work4096, records256 and list/association entries1024 are explicit. This is a bounded malformed-input campaign, not every legal maximum-size VRT packet.

The helper checks decode/visit agreement, exact callback field identity/attribute/group/extent, borrowed ranges inside input, no callbacks on failed complete validation, and callback failure stopping after at most one call. It then decodes again after callback failure and compares resulting state and field views with the original result, ensuring callback failure does not poison subsequent parsing. No I9 field is registered or admitted through this packet target; the independent I9 structural mutation target remains separate.

Two manually specified literal wire seeds were added; no framework encoder constructed the oracle:

- `wide_query_attrs.bin`: words60000008,00000001,a0040000,00000001,00000002,00000003,7fffc080,fff80000. This is a selector-only command with17 CIF0 fields (bits30..14), explicit CIF7 and all13 attributes, hence221 zero-byte views. Baseline16-field decoding returns resource_limit; generic decoding returns221 views. A work budget220 rejects it.
- `wide_context.bin`: words40000015,00000001,00000004,c6bffc00 followed by17 zero-valued words. CIF2 selects bits31,30,26,25,23,21..10, all one-word fields. Generic decode returns17 values. The fixture exercises actual value traversal beyond the baseline field count, not merely selector storage.

`capacity.cpp` asserts these expected observations with unconditional checks (independent of NDEBUG), then passes both literal packets through the mutation-target invariant helper. CMake registers `malformed_packet_capacity_contract`; existing replay/libFuzzer targets automatically consume the expanded helper. Corpus SHA256SUMS was regenerated with the two additions; the prior seeds remain unchanged.

The CIF1 owner confirmed the live production candidate was frozen before final sanitizer replay. Developer direct Clang C++23 builds use `-Wall -Wextra -Werror -fno-exceptions -fno-rtti -Iinclude`. The capacity target passed ordinary and ASan/UBSan builds. Replay used optimizationO2 ordinarily andO1 with `-fsanitize=address,undefined -fno-omit-frame-pointer`. Both completed:

```
seeds=14 mutations=100000 final_rng=13423925404679868655 checksum=3476854396166968465 cap=65536
```

Reproduce: compile `fuzz/replay.cpp` with those flags and run the binary with `fuzz/corpus 100000 0x564954413439`. Mutation families remain truncation, arbitrary bit flips, independent declared lengths, fresh random datagrams, appended tails and complete-word changes. This is actual deterministic mutation evidence; the two handwritten seeds are oracle anchors, not presented as fuzzing by themselves. Optional coverage-guided libFuzzer availability is unchanged and no new coverage-guided run is claimed. Independent review/integrated CTest gate remains coordinator-owned.
