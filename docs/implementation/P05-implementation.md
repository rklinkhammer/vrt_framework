# P05 implementation evidence

Date: 2026-09-18. Candidate pending independent V-P05. No commits. Implements P05 and an executable M1 ownership/codec chain; does not implement transactions or claim delivery from transport completion.

## Contracts

`runtime/stream/routing.hpp`: bounded configuration-time exact registration by peer/session generation, SID presence/value, packet type, optional OUI/Information/Packet Class identity, and explicit absent/32-bit/UUID Controller and Controllee identities. Duplicate keys, including ambiguous same-peer/type/Class SID-less bindings, reject. Freeze prohibits subsequent registration. Paired Data/Context/Command share SID but retain separate type routes. Class pad bits are not part of class identity. Registered extension routes require explicit class identity, payload bounds and a nonthrowing layout validator; unknown extension semantics do not reach delivery callbacks. Request-context lookup is a read-only correlation hook so a Controller can supply original diagnostic CAM context without a second wire path.

`runtime/stream/counters.hpp`: one configured modulo16 owner per logical sender + optional SID + packet type, independently of Class. Registration freezes before use. `next` supplies the value for encoding; transport acceptance atomically verifies/advances that counter. Synchronous rejection never consumes a count; accepted async loss/failure does. A stale encoded count rejects and returns the complete submission.

`adapters/loopback/loopback.hpp`: caller-driven serialized deterministic adapter. `try_send(TxSubmission&&)` returns a token or `RejectedSubmission` holding the whole TX storage and still-reserved completion ticket. No adapter completion occurs on rejection. A caller later abandoning its returned ticket can produce P04's separate local abandoned record. Acceptance requires a reserved real ticket, checked full packet, registered route, RX copy storage, physical queue slot, and data/control/cancellation plus completion admission credits. Every subsequent step before returning accepted is a nonthrowing move.

Data, ordinary Control/Context, and cancellation have distinct external receive pools (distinct provider identities required), physical slot partitions and resource credits. Default32slots reserve1 ordinary-control and1 cancellation slot; each Data stream has a256 upper bound, with the smaller local slot partition allowed to exhaust first. Configurations require at least one slot for each lane. Admission resource `data_queue` has4096 reference credits (P04 additive change by its owner). Tickets are supplied already reserved; higher-level cancellation-ticket reservation is a later transaction concern.

The adapter gathers up to3 TX regions into a contiguous pool-backed RX allocation and explicitly advertises this copy. It produces one payload fragment; generic RxEnvelope's16-fragment limit remains P03's checked capability. Progress performs checked decode again, builds an RxEnvelope and invokes the registered receiver. No application protocol thread or application-owned parsing loop is required. Nested progress on any token in the same adapter returns would_deadlock; callbacks may enqueue work for later progress. Route callbacks cannot be concurrent because adapter methods require a single serialized execution domain.

Injection supports synchronous reject, deferred completion, explicit-token reordering, receive loss, duplicated delivery, asynchronous local failure and failure without quiescence. Loss can complete locally successfully with zero deliveries. Duplicate delivery has one TX completion. Indeterminate simulated I/O failure moves original TX storage into a preallocated QuiescenceGuard before publishing failure; only matching token/generation proof releases it. Destruction cannot free quarantined provider storage. No fabricated completion is quiescence evidence.

## Executed tests

Apple clang21/libc++ arm64, CMake4.4.3. Commands:

```sh
cmake --preset dev
cmake --build --preset dev --target p05_loopback p05_isolation p05_m1_chain
ctest --preset dev -R '^p05_(loopback|isolation|m1_chain)$' --output-on-failure
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan --target p05_loopback p05_isolation p05_m1_chain
ctest --preset asan-ubsan -R '^p05_(loopback|isolation|m1_chain)$' --output-on-failure
git diff --check
```

All3 developer/integration tests pass Debug and ASan/UBSan. M1 chain runs production encoding into a lease -> admitted loopback -> checked decode -> retained IQ sample access -> local completion -> transport destruction -> final RX return. Additional cases exercise all fault modes, count wrap/rejection, contiguous-vs-segmented logical bytes, stale proof, frozen/SID-less routing, Data saturation with ordinary and cancellation submission, distinct-pool enforcement and nested-progress rejection.

An initial broad `-R '^p05_'` invocation also selected verifier executables that had been registered but not built; those reported Not Run. The corrected exact developer regex above passed. This was not an implementation test failure or independent verification attempt. Independent tests are owned by V-P05.

Logs: `build/{dev,asan-ubsan}/Testing/Temporary/LastTest.log` (ignored local artifacts). Test segmentation splits an already encoded fixture into three external leases; it establishes transport equivalence, not P10's future direct segmented generator write path. That path still needs a checked prologue-only P02 helper; tracked with coordinator.

## Actual-size and allocation notes

Local sizes: `Loopback<32>`15760bytes; including32 preallocated QuiescenceGuard state objects24464bytes; `RouteRegistry<64>`9224bytes; `CounterRegistry<64>`2632bytes; `TxSubmission`264bytes. A default loopback plus these registries accounts for36320bytes before supplied pools/admission/completion structures (already separately budgeted). Setup allocates guard/provider shared state; submission/progress use no heap allocation. Allocator control-block overhead and final deployment thread stacks remain P13 reconciliation inputs. Coordinator owns integration ledger updates.

P03 added provider-identity comparison; P04 added actual ticket reservation observation and separate data_queue credits. Their owners ran affected package regressions and refreshed reports. P05 does not modify codec semantics or interpretations. Distinct provider identity assumes external pool suppliers honor their existing disjoint backing-memory contract. Network authenticity is not inferred from a configured peer identity; trust-boundary qualification remains a deployment input.

## Candidate source hashes

| Path | SHA-256 |
|---|---|
| `include/vita/runtime/stream/routing.hpp` | `34c4859e891a6c3a07b2fb16e756c276e7a1c26955a92c692f39ecc20c2e813a` |
| `include/vita/runtime/stream/counters.hpp` | `b984fd6a79f6a3b4d3bfd18cc7d3095e63720039207c118013faa442cc77f249` |
| `include/vita/adapters/loopback/loopback.hpp` | `b0c9ce305ad571e1ce950aeae0c65682a1c07d66e2d7c6de04b110c8101348ab` |
| `tests/unit/P05/CMakeLists.txt` | `42d3841cbca09e8a8b1d2ce430a5e5cf7a735bc069b017e8afa2acd5e95294dd` |
| `tests/unit/P05/support.hpp` | `58aaf62bb91e993f1c709d4d8390402541c0f72ec65c0dd4c092792c0667f331` |
| `tests/unit/P05/loopback.cpp` | `d795de6c2e3897b0baf1fe34d0edcb84fbacac4bdb7e3f27ba0a853d606eb8a7` |
| `tests/unit/P05/isolation.cpp` | `882ba197345336fb94ccf31f5c191aa06f4b098ba1f304c7a68cbc3c61776d7c` |
| `tests/integration/P05/CMakeLists.txt` | `6067b2c9128eff54c0a8383117f32f8e206275c0697552a798eead612dba2f30` |
| `tests/integration/P05/chain.cpp` | `355f4bb964ecce62591d3bf9df87905d395238f61650bcfd0c442d976749f821` |


## Startup budget integration addition

`runtime/budget/loopback.hpp` provides allocation-free, failure-atomic helpers for a configured reference-runtime ledger. `charge_loopback<N,Routes,Counters>` charges transport object and preallocated quiescence-guard state via the adapter's existing `metadata_bytes()` into adapters/stacks. `charge_routes<Routes>` and `charge_counters<Counters>` charge registry object sizes into scheduling; `charge_registries` combines these atomically. Shared registries are charged once by their owner, never implicitly for every adapter. Raw/provider storage remains independently charged.

For current arm64 defaults (three receive pool handles), one Loopback<32,64,64> is 24464 bytes including guard metadata, RouteRegistry<64> is 9224, and CounterRegistry<64> is 2632: total 36320 bytes. This is an additive component projection; it does not claim complete M3 feasibility or replace runtime-specific construction accounting. Callers must ensure the baseline ledger does not already charge the same concrete object before invoking an additive helper. Oversize counts and category exhaustion preserve the original ledger, including failure after the first of two registry charges.

Developer command `cmake --build build/dev --target p05_budget` and `ctest --test-dir build/dev -R '^p05_budget$' --output-on-failure` passed. The test checks exact concrete charges, two adapters sharing one registry pair, multiplication overflow, failure rollback and unchanged 64 MiB reservation total. Frozen P04/P05 production headers were not edited.

Addition manifest (SHA-256):

```text
be79298527d02902e84874af72e33d92a117a5b76e2378e18b6a80592087b3a0  include/vita/runtime/budget/loopback.hpp
fc38297fe036fe2d56cf4367f31d8063e1f16f9271fc18c31ceb2f550a54bc2f  tests/unit/P05/budget.cpp
545459a97f0bc362c2775a553bbf3cd938557fa685e291696354deb95f547877  tests/unit/P05/CMakeLists.txt
```
