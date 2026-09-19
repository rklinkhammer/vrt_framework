# P09 implementation: revisions, Context publication, receive history

Implementer: contracts agent. Candidate ready for independent verification; only the separate verifier report grants the package gate.

## Runtime interfaces

`RevisionStore<128>` allocates stable bounded shared backing at setup. Its `binding()` is the production `EffectSink` consumed directly by the existing Engine. `reserve(count)` acquires physical slots before backend effects; `RevisionReservation` releases unused slots. Each actual `record` copies the immutable `EffectiveEvent` and takes ownership of its transferred revision/publication AdmissionBundle. Requested/adjusted/pending field plans remain in the Engine's existing `ExecutionPlan`; a physical reservation is not an effective value. No second command execution path exists.

`current()` returns a copyable lifetime-owning immutable `RevisionHandle`. Admission slots include pending reservations, unpublished revisions, current state, and external handles. `collect()` releases obsolete published revisions only after the final handle is released. IDs do not wrap; generation changes detach old storage without mutating old event snapshots. An old association callback cannot fault or alter the new association. Internal effect callbacks require the matching reservation issued by this store; they are trusted framework composition, not a wire input API. Event access is immutable across threads, handle refcounts are atomic, and store/publication mutation is serialized on its control/sample domain.

`ContextPublisher<128,64>` owns the production bounded Context-before-Data gate. `PublisherBinding` synchronously attempts a borrowed full Context frame or externally owned TxStorage; transport acceptance must copy/own the Context and order it before affected Data. A rejected Data submission preserves storage ownership. `encode_context` provides checked wire encoding and caller-supplied output/counter envelope. Applications do not construct Acks or implement publication loops; P10 binds these framework hooks to transport progress.

`start` requires known required values. Every start publishes the final coherent state at the current qualified observation time before Data, including ordinary known-state stop/start and old effective revisions. Distinct timed pending revisions still publish separately in order. Untimed pre-start changes retain their unknown effect times locally and may be represented by the final start observation only when no intervening Data/nonpersistent-event dependency would be lost. Unpublished same-time/same-ordinal changes likewise coalesce only without such a dependency. Individual immutable field outcomes remain available. Already-published time overlap or unrepresentable same-time dependent changes fault the temporal association rather than rewrite history.

`progress(now,ClockSnapshot)` supplies actual clock usability/calibration. The lower-level timestamp overload defaults clock usability to false. Outgoing state projects Calibrated Time enable/value (31/19), Valid Data (30/18), and removes Sample Loss event (24/12) on refresh. Full refresh occurs once per elapsed active second without replaying bursts. Context rejection retries for at most 10 ms or the bounded 64-held-Data capacity; exhaustion drops held Data and faults publication, independently of AckX completion. `pause` may clear a publication-only operational failure with known state; it preserves backend/metadata/temporal faults. All starts renew their initial gate.

`backend_fault(observed_state)` stops Data and retains the supplied coherent unknown-state observation. Invalid-Data Context is attempted only with a usable qualified observation timestamp strictly later than the published highwater, never an invented effect timestamp. `temporal_fault()` preserves known numeric values but stops Data and requires association recovery. `published_highwater()` lets P10 combine Context and accepted-Data intervals when detecting a backward mapping overlap. `detach(new_generation)` resets the publisher only through generation advancement; P11 must validate fresh SID and peer readiness before invoking it. No automatic wire-history reset is implemented.

## Receive semantics and ownership

`ReceiverHistory<128>` stores immutable value snapshots/observations ordered by effective protocol time. Full snapshots anchor intervals; generic deltas retain explicit incomplete-history confidence. Equal-time conflicting state becomes ambiguous until a later full snapshot; arrival order never selects a winner. Identical replays do not renew observation age. A refresh applies only from its own time, not across older unknown intervals. History is bounded by 128 entries and two seconds of monotonic observation age; protocol intervals of two seconds without a later observation are stale as well. This dual bound prevents late arrival of an old snapshot from making a long missing-refresh interval known. Sample Loss is returned as a nonpersistent event at its own effective time, not folded into later persistent state.

`ContextReceiver<128,64>` exposes callback-scoped `BorrowedSignalRx`. Known immediate Data delivery takes no retention quota and performs no mandatory retain. Consumers may explicitly call `retain(consumer,global)` and copy the small metadata snapshot. Missing metadata uses a separate bounded waiting quota and external payload lease; matching Context releases it before the 10 ms deadline, otherwise it drops. A consumer retain at waiting delivery takes independent application quotas. Optional unknown-metadata delivery requires an explicit fixed class PayloadFormat; dynamic metadata is marked unknown and is never presented as a known rate. Receiver detach drops waiting/cache state without revoking externally retained payloads or copied metadata.

The P03 additive `RetainedRx::retain` enables optional retention from waiting-backed callbacks, preserving fragment layout and sharing each distinct allocation once. Consumer/global quota acquisition rolls back transactionally. `BufferLease` gains only a RetainedRx friendship for existing private share access; no layout or pool behavior changes.

## Actual budget and allocation boundaries

Current Apple clang/libc++ layout:

| Object | Bytes |
|---|---:|
| Revision including event, credits, publication/lifetime state | 432 |
| RevisionStore<128> backing + binding object | 55352 |
| RevisionStore wrapper | 32 |
| ContextPublisher<128,64> | 13704 |
| ReceiverHistory<128> | 21536 |
| ContextReceiver<128,64>, including history and physical waiting slots | 84080 |

`charge_context(ledger,streams)` charges revision backing/wrapper to revisions, contained history once to Context history, publisher plus remaining receiver/waiting objects and waiting quota metadata to queues. Sixteen default streams charge 886144 bytes to revisions, 344576 bytes to Context history, and 1220352 bytes to queues (24 bytes of waiting-quota metadata per stream). These fit their category ceilings in isolation. No whole-M3 budget claim is made; P10 must compose actual Engine, transport, source, retained-handle and worker allocations without also instantiating the older projected plan arena. Shared-pointer/allocator infrastructure is excluded under architecture §12's explicit infrastructure exclusion.

Waiting slot storage is charged even while empty. Payload bytes remain in provider pools. App-retained handles can move out and outlive the runtime under the shared global quota; application-provided destination handle storage is externally supplied. If runtime instead owns a retained-handle arena, the final ledger must charge that arena separately to retention and must not count the same handle storage twice. The P04 1024-handle reference retention projection is not implicitly allocated by this receiver.

All dynamic allocation occurs at setup (RevisionStore shared backing/binding and retention quota objects). Operational reserve/record, publication, receive/history, retain and expiry allocate nothing. Setup OOM follows the project's no-exception allocation policy. Callbacks are bounded, nonblocking and nonreentrant with respect to their invoking domain.

## Validation

Developer targets `p09_context`, `p09_receiver`, `p09_effects` cover immutable snapshots, physical reservation exhaustion, old-generation completion, two distinct actual effect times through the real Engine/EffectSink, AckX despite publication rejection, transferred credits after engine release, publication timeout, new-start observation, same-boundary coalescing, untimed startup, calibrated holdover projection, backward refresh refusal, history conflict/staleness, retained waiting Data, explicit retain quota failure/rollback, and zero hot-path allocations. A counting allocator covers receiver operations and actual Engine/effect/publication composition after setup.

Configured/built using `cmake --preset <preset>` and `cmake --build --preset <preset> --target p09_context p09_receiver p09_effects p03_memory p03_verify_envelope p03_verify_edges -j 4`, followed by CTest for those targets. Dev, ASan/UBSan and TSan passed. The final frozen candidate is rechecked after the final fault-frame projection edit. Independent P09 tests and verdict belong to the verifier report.

## Candidate SHA-256

```text
9ebcb1487f27419b4d2d19e7e4b6ef0b7c41f8d2b3315b4f89ac451bcf6c67cb  include/vita/runtime/context/revisions.hpp
3ede591ebdee7c55531ba249ae85159b47a8e4196892f172b64b024089e207ba  include/vita/runtime/context/publisher.hpp
8ac7a53c8a4a754bcf9232a2b6b4c22ab70f522de4598efa160e900020cfb128  include/vita/runtime/context/receiver.hpp
3c1d1469e8fde9cbef9812dc0f1f9b1b01df5e7c491b714a94db39002acb3999  include/vita/runtime/context/budget.hpp
5ede1e74572f0bea379019596176dac7cd31e98beec7b2c09690669a2511eeb1  include/vita/memory/envelope.hpp
820197487e6fd856a0a9388516fd26da594bc3b722ac59e4793df4f959533b89  include/vita/memory/pool.hpp
a71d27b56741b3b4dfb0f88ef2e691a58224540b8348ac8073993d06ff02015b  tests/unit/P09/context.cpp
8c44b694d1182c8063988b1bc583df7a36dfb697d49d5e21b630b3fe6b5dc1ef  tests/unit/P09/receiver.cpp
8ff20c35f645cd93d5fb066912b54b090e9ce71eb28de8d1b8cf4b56e4b17613  tests/integration/P09/effects.cpp
```

## Independent verification repair: semantic Context admission

The verifier found that direct history insertion and decoded Context admission could mark a wrong semantic variant or negative Sample Rate as known. `ReceiverHistory::insert` now validates every known field through the existing descriptor `validate_value` before any mutation; receive uses the same path. Public `RevisionStore::initial` applies the same validation and canonical field-ID checks before slot reservation. Unknown/absent fields preserve their existing semantics. Generic wire decoding remains unchanged so invalid Control values can still be diagnosed by the transaction layer. Developer regression checks prove invalid snapshots leave history/revision storage unchanged. Dev developer targets passed after repair; the independent verifier will rerun its cases and sanitizers on the corrected candidate.

Corrected candidate hashes superseding the corresponding earlier entries:

```text
e68c39b0b74b64d5991d8e4b405e81b63405c9e31417d10c629e2dc918897f27  include/vita/runtime/context/revisions.hpp
c042b1460c46401007660ef941a5574535bdd419d699134f5afe1b37f2b8b6a7  include/vita/runtime/context/receiver.hpp
d02c95211a53b6dd44e03746f66a5c12b8364e8d9b327e189967031c873380de  tests/unit/P09/context.cpp
```
