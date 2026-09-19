# Independent DPF and segmented sample verification

Bounded isolated component verdict: PASS against producer manifest `drafts/P14-sample-descriptors/manifest.sha256`. Live aggregate integration remains pending; the mutable CIF1 structure batch was not gated by these compilations.

The verifier reviewed both draft headers and the coordinator's `contract.cpp` oracle. That oracle derives expected code eligibility and packing offsets independently and writes expected bits without production packing helpers. A new separately authored `independent.cpp` adds both tag kinds simultaneously, nonzero unused bits, unaligned borrowed payloads, one-byte fragments, copied segment-descriptor lifetime, repeated complex/vector shapes, independent component-unit expectations, exact payload extents and invalid resource/padding inputs.

Both tests passed direct Clang C++23 ASan/UBSan with `-O1 -g -fno-exceptions -fno-rtti -fsanitize=address,undefined -fno-omit-frame-pointer`, draft include first and current live include second. The coordinator matrix reports **42,748 matrix/fragment cases**. The separate verifier test covers eight combinations of packing, complex kind and repeat mode, twelve independently assembled fields each, plus nine numeric unit mappings. There were no sanitizer diagnostics.

The separate test instruments ordinary and aligned C++ allocation entry points, verifies both with positive probes, and observes zero allocations during its bounded operations. This does not claim coverage of all process C heaps. Bytes remain caller-owned immutable borrows; copying a view only copies span descriptors. No ownership, packet reassembly, physical units beyond explicit unit tags, automatic class authorization or callback/runtime integration is inferred.

Source inspection confirms that layout offset calculation is shared with the existing raw reader, total segment extent is checked, supplied descriptor count is bounded before empty spans are skipped, and no payload concatenation is performed. Processing fields wider than32 bits remain explicitly unsupported. Padding evidence and structure count are explicit inputs, not guesses from leftover payload bytes. Non-normalized polar phase remains explicitly unspecified.

An initial `-Werror` attempt was blocked solely by warnings in the concurrently developing live `cif1_structured.hpp` dependency (signedness and indentation). Those warnings were sent to its owner; the successful isolated sanitizer runs used the same warnings without `-Werror`. This is not a producer-header repair or a claimed clean aggregate build. Promotion must include the normal frozen dependency/header gates.

Reproduction:

```sh
clang++ -std=c++23 -O1 -g -fno-exceptions -fno-rtti -fsanitize=address,undefined -fno-omit-frame-pointer -Idrafts/P14-sample-descriptors/include -Iinclude drafts/P14-sample-descriptors/verification/contract.cpp -o /tmp/p14-descriptor-matrix-asan
/tmp/p14-descriptor-matrix-asan
clang++ -std=c++23 -O1 -g -fno-exceptions -fno-rtti -fsanitize=address,undefined -fno-omit-frame-pointer -Idrafts/P14-sample-descriptors/include -Iinclude drafts/P14-sample-descriptors/verification/independent.cpp -o /tmp/p14-descriptor-independent-asan
/tmp/p14-descriptor-independent-asan
shasum -a 256 -c drafts/P14-sample-descriptors/manifest.sha256
```

Live promotion preserves the reviewed headers and tests. The full aggregate integration gate remains pending in [M5 progress](M5-integration.md). The shared packing-offset helper stays library-private in `general::detail`; both contiguous and fragmented readers call that single implementation. No public unchecked layout API was added.

Final aggregate checkpoint:198/198 Release and195/195 ASan/UBSan checks pass with69 standalone headers. [M5 integration](M5-integration.md#final-available-input-continuation-checkpoint) supersedes the promotion-pending statements above and preserves the bounded component scope.
