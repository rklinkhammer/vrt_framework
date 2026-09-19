# P16 coverage and evidence

This additive post-M5 package implements the opt-in Tunable IQ v1 frequency-scan example. IQ Generator v1 and M5/D-M5-1 remain the accepted baseline. Final aggregate counts and source identities are in [P16 integration](P16-integration.md) and the [independent verification report](P16-verification.md).

| Capability | Executable evidence |
|---|---|
| Distinct class identity, literal RF Q20 wire words, exact integer-Hz range and v1 isolation | `p16_verify_wire`, `p16_verify_kernel`; existing v1 regressions |
| Five-field state, four-field commands, typed tune/query/cancel and full/changed Context | Kernel/public tests; mixed RF/SampleRate writes reject before effects |
| Owned backend lifetime, remote-only Controller and directional associations | `p16_verify_public`, `p16_verify_remote`; actual localhost IPv4/IPv6 |
| Packet-boundary effects and unresolved completion publication holds | `p16_verify_deferred`; initial/future/coarse-time/periodic-refresh cases |
| Immutable accepted headers and retained payload/metadata | `p16_verify_counters`, `p16_verify_scene_runtime`; retention beyond Runtime destruction |
| RF cancellation, contradictory late effects, unknown-state Data gating and explicit recovery | `p16_verify_rf_lifecycle`; actual scene fresh-epoch recovery at a nonzero ordinal |
| Tone offsets, Nyquist edges, out-of-band suppression and phase across skipped callbacks/retunes | `p16_verify_scene`, `p16_verify_scene_runtime` |
| Checked sweep/endpoint CLI, one in-flight tune, AckX plus matching AckS dwell, no timeout retry | `p16_verify_sweep`, `p16_verify_endpoint`, three independent process oracles |
| Typed validation outcome including rejection, late evidence and contradictions | `p16_verify_validation`, `p16_verify_validation_rejection`; actual application stops when negative AckV is the only delivered tune reply |
| Combined deterministic/wall-clock and separate-process Controller/Controllee examples | Four developer example tests; independent combined/repeated/continuous, UDP and silent-peer timeout process tests |
| UDP-disabled combined build and absent UDP targets/tests | Coordinator core-only build and two example tests; registration evidence in final artifacts |
| Bounded storage and shared-owner charging | `p16_verify_budget`, `p16_verify_owned_budget`; full-reference ledger 52,208,928 / 67,108,864 bytes |
| No new hot C/C++ allocations in the exercised RF Runtime/source path | `p16_verify_scene_hot_alloc`; optimized macOS interposition with positive probes |
| Ownership/cancellation concurrency | Six targeted ThreadSanitizer tests; full affected Release and ASan/UBSan regressions |

The allocation test covers the serialized operational test thread and instrumented allocation APIs. It does not claim allocation-free terminal logging or whole-process RSS accounting. The 317,456-byte reservation transfer reclaims only unused standalone-plan reservation and preserves the 64 MiB cap. Application/source stack objects are separate from the framework ledger.

Continuous mode obeys the existing bounded transaction-identity retention contract. Admission exhaustion reports and stops; it does not evict retained identities or silently retry. Scene/device recovery is demonstrated and tested through the explicit API, but the command-line applications report faults and drain rather than choosing new identities/device state automatically.

P15/M6 hardware, qualified GPS timing, independent-peer interoperability and cross-machine performance measurements remain separate. Array-of-CIFs is not enabled by P16. The lab-only localhost examples use simulated PPS and fixture identities. See [package status](P16-status.md), [README](../../examples/frequency_scan/README.md) and [porting guide](../../examples/frequency_scan/PORTING.md).
