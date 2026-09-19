# M5 operational scope and acceptance

Decision D-M5-1: accepted by the user on 2026-09-19. M5 closes against the documented IQ Generator v1 operational profile and the published bounded codec capabilities, with explicit exclusions. Implementing every optional standard field is not a release requirement. This supersedes the earlier complete-registry M5 objective and its Array-of-CIFs input stop; it does not alter normative wire encodings for supported features.

Current gate result: COMPLETE / PASS after [independent review](M5-operational-verification.md); coordinator closure is recorded in [M5 integration](M5-integration.md#m5-operational-scope-closure--d-m5-1). No production source change was needed.

## Required operational behavior

| Area | Supported commitment | Evidence |
|---|---|---|
| Signal Data | Existing type0x1 paired time-domain complex IQ16, IQ32 and float32 classes; bounded packetization and sample access | P02/P10 and M3/M4 integration reports; P14 sample verification |
| Context | Paired type0x4; Reference Point, Sample Rate, State/Event and Payload Format snapshots; effective-time association, validity, refresh and recovery | P09–P11 verification; protocol appendix §2.2 |
| Control | Existing type0x6 command relationship, acknowledgements, queries, cancellation and partial-execution/timing policies; Sample Rate remains the only writable standard field in this application | P06–P08/P10/P11 verification; protocol appendix §2.3; no new RF-frequency/gain/device controls are implied |
| Runtime and transport | Bounded admission, ownership/lifetimes, preallocated pools, completion ordering, lifecycle, loopback/virtual backend and POSIX UDP | M0–M4 and P12 evidence; existing S1–S16 scenario index |
| General codec library | Retain the independently verified96 nonrecursive field identities,13 attribute shapes, sample conversions/adapters and registered extension interface as documented capabilities | [P14 coverage](P14-coverage.md) and final198/195 local aggregate gate |
| Receiver characterization | Existing local-replay measurement tools and results; production sender may run on another machine | P13 receiver model/validation; ongoing characterization is not a hard M5 performance threshold |

Generic field decoding does not authorize device execution, add profile controls or extend Context history automatically. Existing wall-clock progression, GPS/PPS-conditioned clock policy and packet-boundary rate changes remain unchanged. Local virtual-clock tests do not qualify a production clock.

## Explicit exclusions and retained limits

Array-of-CIFs (CIF1/bit11) is excluded from production packet/profile support and M5 acceptance. The normal decoder continues to reject it; no guessed size, silent skipping, typed native Array, packet emission or semantic dispatch is enabled. The separately included `codec/array_cif.hpp` remains an optional structural inspection utility with explicit I9 dialect selection, its own tests and bounded borrowed views. It is not registered in normal packet decoding. Retaining this utility requires no API removal, build change or newly enabled behavior.

I9 remains in the interpretation register as engineering documentation. Peer agreement and independent/authoritative evidence would be necessary for any future claim of Array interoperability; they are not prerequisites for this release because Array is excluded. Optional utility tests remain regression evidence, not a new product requirement to complete Array semantics.

Existing supported-scope limits remain visible: raw codes only for ambiguous Beam Width/Pressure/Probability/Spectrum coefficient conversions; no invented physical interpretation; processing-efficient sample packing fields above32 bits unsupported; fixed capacities and explicit sample/padding metadata; no generic claim of arbitrary SDR controls, automatic frame reassembly or vendor extension support. The coverage matrix remains authoritative for retained library capabilities. No failing supported feature is waived by this scope change.

## Revised local gate

1. Existing operational profile and retained advertised codec capabilities have passing independent functional evidence and the relevant ownership/resource/lifecycle regressions.
2. Array remains excluded through the normal API; the optional utility cannot silently enable production support.
3. Architecture, protocol appendix, plan, coverage and status agree on the exclusion and local-versus-deployment claims.
4. The tested source manifest still matches the implementation. Documentation-only scope changes reuse the recorded198/198 Release,195/195 ASan/UBSan and69-header gate; any source change needs appropriate additional verification.
5. An independent reviewer checks the scope and evidence before the coordinator records M5 complete.

See [independent acceptance review](M5-operational-verification.md) and [M5 integration](M5-integration.md). Completion means the local software milestone for this declared scope, not complete VITA standard-field coverage, universal conformance or deployment qualification.

Linux production-toolchain qualification, actual peer/OUI/class/clock configuration and cross-machine performance measurements retain their separate deployment status. P15/M6 still requires a selected adapter/backend, device/OS/SDK contracts and actual device access. This decision does not waive those requirements or select hardware.
