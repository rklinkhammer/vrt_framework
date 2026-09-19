# P04 implementation handoff

Implemented headers: `runtime/completion/ticket.hpp`, `runtime/execution/{admission,arena,operation,executor,budget}.hpp`. These compose frozen P01 errors and P03 external leases without creating transport dependencies.

Completion uses one atomic 56-bit generation / 8-bit state tag per stable slot. Allocation initializes operation metadata before release-publication of reserved. Producers claim reserved-to-writing acquire-release, write only after winning, and release-publish ready. Consumers acquire-claim reading, copy the complete record, and release-publish the next free generation. Maximum generation retires the slot. Failed claims touch no payload. `CompletionWriter` exposes a real claimed publication phase for deterministic delayed-publication tests; destroying an unfinished writer leaves writing and never steals its memory. Publisher capabilities and move-only tokens retain arena lifetime. Abandoned tokens publish a synthetic callback failure through the same claim path. Scan is bounded and queue-independent. An abandoned token leaves a ready failure record; the worker must drain it before the slot becomes reusable. Token destruction does not immediately return a ticket slot. `CompletionResult::effective_monotonic_ns` is a local monotonic value, not an absolute protocol epoch; typed protocol timestamps belong to P08.

`QuiescenceGuard` is separately setup-allocated and can be reused after explicit proof. Arm transfers TX storage and increments its generation. Adapters capture a copy of the armed guard by value; stale copies cannot prove a later generation quiescent. A self-retaining quarantine keeps memory alive if every external handle disappears without proof. This intentionally persists until an adapter proves quiescence or process termination; transaction failure/timeout/token abandonment cannot break it. Provider callbacks execute outside the guard mutex. Guard state is precreated per simultaneous operation at configuration, not constructed on the submission path.

`AdmissionPool` reserves a complete logical count/byte bundle atomically and supports transfer into longer-lived outcome/revision owners. Ordinary, cancellation and emergency capacities occupy separate counters. `SlotArena` supplies real bounded value storage with lifetime-owning leases. `AdmittedOperation::acquire` couples logical reservations, real slot storage and a real completion ticket; failures roll back all earlier acquisitions before returning. It rejects advertised capacities larger than supplied plan/transaction/completion backing. `AdmittedStorage` provides the corresponding byte-storage-only helper. P06 must request every necessary response/revision/duplicate/context credit before effects and retain each physical resource through its operation lifetime.

`BoundedExecutor` and `ControlStrand` use fixed function-pointer/context entries and serialized caller-driven progress. No inline callback is invoked during post. Callback contexts must remain valid through invocation; close rejects new work while allowing queued work to drain. Nested cross-domain progress maintains a stack-linked thread-local domain chain; blocking on any active ancestor returns would_deadlock. Run budgets and pending counts are explicit. Dedicated worker/I/O service wiring remains downstream integration.

`BudgetLedger` starts from the 64 MiB architecture partition, transfers 704512 bytes from headroom for concrete 944-byte retained handles, and rejects category overflow. `reference_budget()` uses actual sizeof for available provider/retention/completion/quiescence/plan/queue objects, including alignment inside objects. Additional plan bookkeeping and retention quota objects receive explicit headroom transfers. It performs no allocation and must be consulted before reference-runtime construction. Current arm64 projection charges 36834216 bytes for available components; future revision/history/transaction/codec categories remain reserved and uninstantiated, not falsely counted as implementation. Completion slots are 88 bytes; the 1024-slot arena metadata is 90200 bytes; the 256 x 8192-byte plan arena state is 2097472 bytes. Shared-pointer/allocator control infrastructure is separately excluded under architecture section 13's allocator infrastructure exclusion; actual framework state objects are charged. No complete-runtime feasibility claim is made yet.

Developer checks passed on local arm64:

- `cmake --preset dev && cmake --build build/dev --target p04_runtime`
- `ctest --test-dir build/dev -R '^p04_runtime$' --output-on-failure`
- Equivalent `asan-ubsan` and `tsan` configure/build/test runs.

Tests cover S12 delayed publication, S13 stale/duplicate generation, producer and consumer contention, generation retirement, token abandonment, S16 synthetic failure without quiescence, stale quiescence evidence, actual arena/ticket exhaustion rollback, independent cancellation admission, fixed executor saturation, same-domain wait rejection, and actual-size budget charging. Independent verifier tests and integration remain separate evidence.

Additive P05 integration accessors: `CompletionToken::active()` reports local token ownership; `is_reserved()` checks the exact generation/state tag for preflight. These observations are not an atomic transport acceptance barrier. Publishing before the ownership handoff is an adapter-contract violation. P04 developer and four independent suites rebuilt and passed after this addition.

P05 isolation addition: Resource::data_queue has a separate 4096-credit reference capacity (16 streams x 256). Saturating it leaves all ordinary and cancellation queue credits available. Five P04 developer/independent suites rebuilt and passed; actual reference charge increases by 16 bytes for the extra used/capacity counters.

Frozen production manifest (SHA-256):

```text
e7dc8211935e6c102c60ff9145dfe2686c1b13d4ebe9ec517ca92c849b5c5bbb  include/vita/runtime/completion/ticket.hpp
4076824209c3e1d9cacc21a99313b5cd88bf326ded79a22be7b4df8c8c3871da  include/vita/runtime/execution/admission.hpp
ed33dbd7a3917f296f568aff824e2c2b9c0f086550bb58bacd185a6b498e345a  include/vita/runtime/execution/arena.hpp
8b5279aaf8b40a1a7295b5bf0f7d86b1f847de03bbdce5e644dd268cc0a8e7d7  include/vita/runtime/execution/budget.hpp
354e99cc3c6f17906f5c24bb72409d9da0dff6362373cd5eb783108891631fcf  include/vita/runtime/execution/executor.hpp
8c3cc067511769f582d615978d79e3daa7c3c346bde5ef2cc449631b22887eec  include/vita/runtime/execution/operation.hpp
```
