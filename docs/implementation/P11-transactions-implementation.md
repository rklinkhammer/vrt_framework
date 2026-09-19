# P11 transaction lifecycle hooks — implementation candidate

Scope: transaction-layer portion of P11; Runtime recovery, transport draining, routes, shutdown deadlines, and the complete M3 budget are integrated and verified separately. This report does not claim the complete P11 gate.

## Contracts implemented

- `Backend::quiescence` reports explicit known/quiescent/pending evidence. A missing callback or unknown evidence blocks reset even with zero completion holders. The deterministic VirtualBackend exposes proof availability and explicit reinitialization; it performs no modeled device write before completion. Its proof injection is not evidence for a real hardware adapter.
- `Engine::request_quiesce` closes new execution, attempts disarm once, consumes real completion, and finishes unstarted work through original requested responses. It neither sends a wire cancellation nor invents effective time. Timed work prevented by local quiesce reports AckT 7, including completion after explicit reinitialization. Running work that cannot be disarmed continues to retain its capabilities and resources.
- `drain_status` distinguishes active work, unread response records, running fields, completion slots, capability holders, uncertainty, and explicit backend evidence. `safe_to_reset` requires all resource obligations to drain and backend proof; synthetic terminal publication cannot satisfy physical quiescence.
- `reset_state` validates every known semantic value and required IQ metadata before mutation, requires a strictly newer nonzero association generation, and preserves completion/operation generations. It clears association faults only after the safe barrier; a retained old capability prevents that barrier.
- Manager closes admission while preserving identical active/retained replay. It rejects changed meanings, new MIDs, and previously unseen cancellation meanings while closed. Quiesce is generation-checked before mutation; reopening requires drained work and an Engine reset to a newer generation.
- Retention and Controller relationship queries match the full association except MID. Controller registration has a read-only preflight. Queries support Runtime bank reuse only after references and retained identities expire; they do not themselves prove transport quiescence or authorize SID reuse.

## Ownership and bounds

All added lifecycle paths use setup-owned fixed storage and existing capabilities. There is no recovery-time allocation. On this Apple arm64/libc++ build, `sizeof(Backend)=56`, `sizeof(EngineDrainStatus)=64`, `sizeof(Engine<16>)=41576`, and `sizeof(TransactionManager<16>)=17704`; `ResultGuard` remains 32 bytes and the default Controller backing remains 385024 bytes. Runtime charges its actual two-bank composition separately. A new local generation alone never makes an old wire identity safe to reuse.

## Developer verification

`tests/unit/P11/transactions.cpp` exercises original response drain/replay, rejection while closed, retained capability reset barriers, contradiction fault, transactional invalid reset, explicit unavailable/reestablished backend evidence, Controller association lifetime/preflight, timed unstarted quiesce, and an allocation counter around lifecycle operations. Direct developer builds passed with C++23, exceptions/RTTI disabled, normal warnings, ASan+UBSan, and TSan. The counter covers ordinary allocation; the independent final package tests provide broader operational-allocation coverage.

Earlier affected P06/P07 developer and independent targets also passed before the final local timing diagnostic correction; final aggregate regression and independent P11 approval are recorded by the coordinator/verifier. Independent engine/manager lifecycle oracles were preliminary PASS; this report intentionally leaves their final gate to the independent verifier.

## Frozen source manifest

- `include/vita/runtime/transaction/backend.hpp`: `c387bba553de811c0923c3536409be18e70d7ce884b1b6e60c8203060b088bdd`
- `include/vita/runtime/transaction/engine.hpp`: `206bd77105ffc197334d685b8ed076c41c470feffe8acbea7ff0872e551ede4b`
- `include/vita/runtime/transaction/manager.hpp`: `1d33f2cc8aa5efe33461c9235d22e432c63d7b4142a1f31b166fb270ebe76787`
- `include/vita/runtime/transaction/retention.hpp`: `831b6a40cbbd864057094db440ef33951e06f2344b8f9c16c739fa5fbf97f12e`
- `include/vita/runtime/transaction/controller.hpp`: `f983c5bcce955f63215a6e8d721386a57e4c9f95ced57ba7487bebf95a7026a9`
- `tests/unit/P11/transactions.cpp`: `31f7214cdb2f2519ca778ca76e0eb6126506cd7d3ca95538a210d21e37db1dee`
