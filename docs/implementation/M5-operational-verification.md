# Independent M5 operational-scope acceptance

Verdict: **PASS — revised local software gate** under user-accepted D-M5-1. The documented operational profile and retained bounded library capabilities have sufficient existing implementation evidence. Array-of-CIFs is excluded from production support, and its missing peer-semantic inputs no longer block this scope. No remaining required in-scope implementation gap was identified by this review. This verdict is not deployment, Linux, hardware, universal VITA conformance or unrestricted standard-field qualification.

## Scope review

Reviewed [M5 operational scope](M5-operational-scope.md) against the accepted [IQ Generator v1 profile](../iq_generator_profile_proposal.md), the revised architecture/protocol/implementation plan, the [P14 coverage matrix](P14-coverage.md), public Runtime APIs and existing implementation tests. The scope statements agree with the implementation:

| Commitment | Independent assessment |
|---|---|
| IQ Signal Data | Paired type0x1 time-domain complex IQ16, IQ32 and float32; existing bounded packetization, source and sample access. General envelope coverage does not imply runtime generation for every packet family or sample representation. |
| Context | Paired type0x4 with Reference Point, Sample Rate, State/Event and Payload Format; existing effective-time association, validity, refresh and recovery evidence remains applicable. Generic decoded fields do not become persistent profile metadata automatically. |
| Basic control | Existing type0x6 control/query/cancellation and AckV/AckX/AckS behavior, explicit timing/partial-execution policies and monotonic deadlines. `Controller::set_sample_rate`, `query` and `cancel` match the claim. Sample Rate is the only writable standard IQ control; `iq_validate` rejects other field identities. No RF frequency, bandwidth, gain or arbitrary SDR setter is inferred. |
| General library | Retains96 nonrecursive identities:23 CIF0,25 CIF1,29 CIF2 and19 CIF3, plus13 attribute shapes, bounded sample helpers and registered extensions. These are codec/storage capabilities, not an expansion of device execution semantics. |
| Runtime/transport | Existing ownership, admission, completion, clock, lifecycle, loopback/virtual-backend and POSIX UDP evidence is retained. Local software results do not establish an independent device or peer. |

The stated exclusions are material and remain visible: no ordinary/native/semantic/emitting Array support, no automatic frame reassembly, no invented vendor semantics, no processing-efficient sample fields wider than32 bits, and no invented engineering conversion for interpretation-limited raw codes. Explicit capacities, structure counts, padding evidence and class-specific inputs remain caller contracts. The broader library is not removed or silently described as unrestricted support.

The scope change does not waive a failed supported behavior. It changes the release requirement for an explicitly optional, unimplemented production feature. P13's prior source-generation misses and receiver characterization limits remain recorded under the separately accepted characterization direction; this review neither relabels those results nor asserts a new performance threshold passed.

## Evidence and exclusion checks

The previous final aggregate remains valid: **198/198 optimized Release**, **195/195 ASan/UBSan**, and **69 standalone public headers**. All149 entries of its source/test/fuzz manifest were independently hashed and found unchanged after the documentation-only scope revision. [Manifest check](artifacts/M5-operational/manifest-check.txt), [prior manifest](artifacts/P14-final-local/manifest-udp-release.json), [Release log](artifacts/P14-final-local/test-udp-release.log), [sanitizer log](artifacts/P14-final-local/test-asan-ubsan.log). No broad rebuild or full test rerun was needed for unchanged production.

A new bounded exclusion probe was justified because the previous standalone Array test proved optional validation but did not explicitly assert rejection by both ordinary decode capacities. The independently assembled I9 field is valid through the opt-in structural API. Wrapped in Context or Control, or selected by a query, CIF1/bit11 is rejected as `unsupported_layout` by both16/64 and128/1664 packet decoders, with zero visitor callbacks. The global descriptor is absent and native insertion rejects without mutation. Including the optional utility does not register it in the ordinary decoder. [Probe source](artifacts/M5-operational/exclusion.cpp), [ASan/UBSan result](artifacts/M5-operational/exclusion.log).

Reproduction of that probe:

```sh
clang++ -std=c++23 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-exceptions -fno-rtti -Iinclude docs/implementation/artifacts/M5-operational/exclusion.cpp -o /tmp/m5-operational-exclusion
/tmp/m5-operational-exclusion
```

Nine existing optimized tests were rerun and passed: source, packetization, rate change, wait semantics, CIF7 profile isolation, nonrecursive CIF1 profile isolation, transaction scenarios, lifecycle scenarios and Runtime transport binding. [Targeted test log](artifacts/M5-operational/targeted-release.log). The earlier full gates retain the remaining S1–S16, ownership/resource, allocation, budget, malformed-input, numeric/reference and example evidence; this small rerun is not presented as a replacement for those gates.

## Remaining limits

This is a local macOS arm64/Apple Clang software milestone. Linux production-toolchain coverage, deployment OUI/class/peer configuration, actual GPS/PPS/device-clock behavior, cross-machine performance and independent-peer interoperability retain their separate qualification status. Existing local replay results remain local replay results; no cross-host clock subtraction or receiver deployment claim is added.

P15/M6 remains blocked on selection of the concrete adapter/backend and required device, SDK/OS contracts and hardware access. This review does not select a device or turn missing hardware inputs into a passing hardware gate. Any future production Array support would require a new explicit scope/semantic decision and appropriate peer evidence; the retained I9 utility provides neither automatically.

The coordinator may therefore close **M5 local software for the declared operational scope**, while preserving those deployment and M6 limitations.
