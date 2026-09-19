# P07 implementation candidate

Status: frozen for independent verification under accepted D-P07-1. No external interoperability or device timing qualification claim.

`TransactionManager` is the ordinary public orchestration path: it reserves shared retention storage before calling Engine admission, routes active retries to the same operation, and replays completed historical semantic responses. Its Engine requires `external_retention=true`, avoiding P06's isolated-test duplicate-credit placeholder. One runtime-wide `RetentionStore<4096,8MiB>` holds full identity keys and one original/cancellation pair per entry. Canonical comparison excludes Packet Count and ordinary Control Change only; CAM, selectors, values, timestamps, class and identity encodings remain significant.

D-P07-1 permits one immutable cancellation meaning per original full identity. L=0/L=1 records are separate; same-meaning retries attach/replay, changed meaning returns `identity_conflict` without effects. The joint retention deadline extends to at least 30 seconds after the latest original/cancellation terminal event; active work or outstanding references prevents eviction. First cancellation after the original engine slot has been released uses retained identity and reports not-cancelled without a stale handle. Stored AckS values/time are replayed unchanged with independently selected outgoing Packet Count. The byte arena reserves actual canonical bytes plus worst-case native response records before effects; full entries/bytes fail explicitly, never evict live results.

Manager cancellation ingress has separately bounded physical slots and logical inbox/response credits. Canonical/result bytes share the general cache arena: cache-byte exhaustion can reject a first cancellation before effects, distinct from ordinary queue saturation, which cannot consume cancellation inbox slots. Selector subsets, class and original execute action are validated before disarm. Backend disarm explicitly reports cancelled/not-cancelled/unknown; cancelled pending fields never begin. Partial cancellation X uses any-cancelled SchX and missing-cancelled AckP, while ordinary X retains its all-fields semantics. Original/cancellation responses and Controller phases remain distinct. Timed cancellations reserve their resources on admission, enforce configured lead/horizon and uncertainty, revalidate mapping changes and actual execution window, and produce unable/not-cancelled outcomes rather than out-of-window effects.

Cancellation races use generation-tagged completion claims. `AsyncResult` pins its bounded result slot while capabilities remain outstanding, so released transaction slots may exhaust earlier rather than recycle live result metadata. A later executed/unknown callback contradicting successful disarm cannot write the normal result or resurrect execution. It sets a generation-scoped atomic signal; owner progress or new admission faults the backend and marks affected state unknown for the same association. This trust failure does not fabricate an effective-time revision. P09/P11 must stop Data and recover from the explicit fault/unknown accessor; optional invalid-data publication cannot require allocating a normal revision. Accept drains pending contradictions before reusing zero-holder slots. Association replacement remains an explicit P11 operation, not inferred from an incoming generation.

Controller identity/deadline/observation implementation is described in `P07-controller-implementation.md`: bounded full-key/class correlation, no MID wrap reuse, monotonic timeouts without implicit cancellation, late evidence, immutable cancellation meaning, and distinct cancellation observations.

Developer evidence:

- Debug build and CTest `^p07_(transactions|timing|controller|loopback)$`: 4/4 pass.
- ASan/UBSan build and same developer suite: 4/4 pass.
- All currently prepared P07 Debug tests: 8/8 pass (preparatory verifier execution is not the final independent verdict).
- Rebuilt all thirteen P06 targets and ran `^p06_`: 13/13 pass after completion/observer changes. P06 evidence remains historical; the P07 manifest supersedes modified shared headers.
- Production loopback test sends original, cancellation, and canonical retry through external leased buffers, checked decoding, manager admission, backend disarm, response encoding, controller observation and fresh outgoing packet counters. All leases return once.
- Deterministic S3/S4 exercise cancellation at 9 ms/11 ms around 10 ms commit; S10 exercises A executed/B pending, partial cancellation, distinct original outcome, and replay/conflict behavior. Tests use an explicitly isolated generic/class-omitting fixture, not fabricated production identities.

Measured Apple arm64 Clang/libc++ sizes: Engine<8>20,944 B, Engine<16>41,552 B, plan1,168 B, record1,272 B, result slot128 B, ResultGuard32 B, AsyncResult72 B, AckRecord352 B, CancellationResult872 B. Engine result heap is 4N*128 B plus shared-owner overhead; completion storage remains 4N*88 B plus88 B state and allocator/shared-owner overhead. Retention frontend16 B; its sole setup allocation10,518,528 B consists of8,388,608 B byte arena plus2,129,920 B entry metadata (520 B/entry). Manager<8,...,CancelSlots64> inline size17,312 B includes its physical pending-cancellation slots. Reference global transaction/ticket limits must be partitioned across binding instances: these templates do not promise sixteen independently maximal engines. P10 owns complete runtime budget accounting; no separate SlotArena is allocated by Engine.

Remaining package integration: P09 revisions and fault-driven publication; P10 generator/clock binding; P11 fresh-association recovery. Earlier interrupted drafts in `drafts/P07-blocked` are historical only and not compiled.

## Frozen manifest

- `include/vita/core/error.hpp` `f0a625379793e9e95fc616e6248899df09fbc401ecbdcfa673c9d8688915ba52`
- `include/vita/runtime/transaction/backend.hpp` `73b72854d96cf4beddea5b7d94d38ec39f3366325522a55e43ae00b7bf5f6c53`
- `include/vita/runtime/transaction/cancellation.hpp` `bb9a8f750564195ea2ca16e7eb75d57e41dd26e22a5d4fb48cf48b57dc04f383`
- `include/vita/runtime/transaction/controller.hpp` `f18ce64ba96a6b17ba87b0543207d0bb421314a4e733e789172603b14a4fd9c6`
- `include/vita/runtime/transaction/engine.hpp` `c8af6facec09dc1cbf0c6713c85ffc128b5a007e4964e20bfb632c3fc4036508`
- `include/vita/runtime/transaction/manager.hpp` `c4d5be7b0391d36f7c8f3cc26db80e3e8a1af6086b3c5fe6d0b127d22c4b43b2`
- `include/vita/runtime/transaction/outcomes.hpp` `4bdf0c2e86e5d677ee5df70a874ea5df8e4c9e29f02004772dc8040c1cf3acae`
- `include/vita/runtime/transaction/retention.hpp` `f9f6580f2548f3084acba77f8f76fac8fa0bc07b1f419607ee805f42f3ff576d`
- `tests/integration/P07/CMakeLists.txt` `e48cc4fe0891784e5e55a6d72d1475a89ff07b1aed5f2381ed3c2fb4f1fbf0a2`
- `tests/integration/P07/chain.cpp` `9452465861a19e37aa795b3ec8df025ba6416d359d87e6539bc002cf9a30694a`
- `tests/unit/P07/CMakeLists.txt` `247b70b11858ae277adae6010491f0f6232b339105a8e052aae2c348a3fe0100`
- `tests/unit/P07/controller.cpp` `985ce272affe07c2d4334c800d449dbe58d4b83d8a0689445d127b2cf9d4d819`
- `tests/unit/P07/support.hpp` `54903a72f05ee15f2714a8bdcba73d27d1723e6ad71273ac4cd6a88e708548db`
- `tests/unit/P07/timing.cpp` `53288cfd46c272e53d598489ce4ec6597750ac098add587574f45720a8cc9868`
- `tests/unit/P07/transactions.cpp` `4890277fc4d093994ed8cb6d4a932d3b7ab63ea8c96655375990d77dd2f2182e`
