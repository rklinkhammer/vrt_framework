# P11 implementation — recovery and lifecycle

Candidate for independent verification. P11 and M3 completion require the independent report; this report records implementation evidence only.

## Public lifecycle

`Controllee.pause()` pauses source Data while preserving sample progress. `stop()` remains its P10 compatibility alias. `Controllee.shutdown(mode,completion)` and `VitaRuntime.shutdown(mode,completion)` perform lifecycle admission closure, disarm/accounting, response and I/O draining. This distinction is explicit because source pause permits ordinary resume whereas completed lifecycle shutdown does not silently reopen admission. Fresh association recovery is available per stream through `recover` or runtime `recover_stream`.

`RecoveryConfig` supplies fresh SID, fully known confirmed state, peer-ready evidence, and optional adapter reinitialization callback. The reference-point field must name the new SID; rate is integral1..100MHz and DPF one of the three baseline formats. The callback returning true explicitly asserts known device state **and physical quiescence**, not merely reset-request acceptance. False remains pending; errors fail recovery. The deterministic virtual backend then settles pending modeled operations as cancelled before original transaction accounting drains. Without a callback, the backend's explicit native quiescence proof is required. Deadline expiry never substitutes for proof.

An optional `ClockReplacement` supplies qualified binding, current monotonic capture, conditioned protocol time and uncertainty. Replacement retains the configured epoch; cloned binding/mapping and affected timeline arithmetic are validated before recovery closes admission. Shared-clock mapping updates follow the existing overlap policy for other streams. Omission retains the existing qualified binding. SID replacement itself never resets the source epoch.

Lifecycle completion is delivered once on the framework progress domain with an immutable result copy. Phase, effect/I/O/capability counts and backend-proof status are available through `lifecycle()`. The callback snapshot is final; subsequent live inspection recomputes outstanding ownership after late proof. Old transaction callbacks are detached when lifecycle completion allows their application contexts to be destroyed. Same-domain blocking waits remain rejected. Global shutdown aggregates stream drain results without closing the shared adapter before accepted work finishes.

Graceful shutdown/recovery uses the unchanged2-second monotonic budget. Immediate shutdown skips optional waiting, but both modes retain unquiesced provider backing. Quarantine is a terminal logical report, not buffer reclamation. Exact generation-tagged transport tokens remain in bank-specific I/O accounting after local completion until the adapter proves quiescence; stale token slots cannot release current work. The isolated test helper forwards explicit proof to these actual transport tokens.

## Two-bank recovery and preserved progress

Every configured logical stream allocates two complete execution banks during setup. Recovery never allocates another bank. The old bank closes new execution admission but continues exact retained duplicate replay, old Acks and accounting under its copied SID/generation. The new bank installs confirmed state only after old effects/transport are safe. Old callback capabilities remain attached to their original result arena and cannot target new state.

A retired bank is reusable only after Engine/Manager operations, response sends, backend work, ticket writers, external completion capabilities and old relationship retention/references permit it. Retention is shared globally and remains30seconds after terminal outcomes, extended by references under the accepted cancellation policy. Pinned bank exhaustion rejects recovery rather than overwriting state. App-held immutable receive/revision leases retain their own backing and remain valid across recovery and runtime destruction.

Old Context history, waiting Data and unpublished revisions are detached after the safety barrier; retained immutable handles are not revoked. Old Data/Context routes stop delivery. Once a bank can be reused, its old routes become nonexecuting tombstones before the mutable bank identity changes. Route/counter installation uses bounded batches; fresh SID history remains append-only. The runtime supports32 total association installations (128 routes/counters, four per association), shared across all streams; initial streams consume that capacity. Exhaustion is checked before recovery changes the active association. Same SID is never reused even after retention expires. The64-entry Controller relationship bound is independently retained.

Recovery carries the old protocol and monotonic sample timelines, ordinal, rational residue and waveform phase. Downtime is accounted at the old rate, the new timeline is checked/reanchored to the qualified mapping, and confirmed rate/format apply at the next boundary. It does not create an implicit new generator session. A known full Context must be accepted for the fresh association before lifecycle completion reports running and before affected Data can pass the P09 gate.

## Actual reference composition

Two-bank default16-stream configuration accounts for **51,668,752 / 67,108,864 bytes** on Apple clang/libc++. Raw pools remain30,998,528; provider metadata/ownership allowance1,568,880; shared duplicate values8,388,608; duplicate index2,129,920; bank/stream storage7,647,744; other runtime storage935,072. The fresh sizeof ledger performs explicit headroom transfers, leaving200,192 unassigned category-reservation bytes. Native charged total retains additional unused role reservations; no process-RSS or deployment throughput claim is made.

There are32 physical Engine<16> banks (2048 backend ticket slots) plus320 TX ticket slots; the shared admission limits remain256 active transaction credits and1024 live completion credits. Free spare backing does not raise admitted concurrency. Both banks, global I/O tracking, lifecycle records and append-only SID history are charged at setup. No standalone projected plan arena is also allocated.

## Evidence and prerequisites

Developer `p11_runtime` and `p11_transactions` pass Debug. Independent preparatory S14/S15, readiness/known-state checks, pending/error reinitialization, repeated safe bank reuse, retained Controller pinning, ordinal/phase continuity and graceful-quarantine/live-proof cases passed targeted runs. Final sanitizer, allocation, old-callback and aggregate verdicts belong to the independent report.

Transaction hook implementation and its separate sanitizer evidence are recorded in `P11-transactions-implementation.md`. Runtime integration additionally changes public config/runtime, bounded route/counter installation and the exact-token Loopback outstanding accessor. Existing P03–P10 regression gates must be rerun where affected. No change to VITA wire policy or an invented peer reset packet is introduced.

## Frozen source hashes
- `include/vita/adapters/loopback/loopback.hpp`: `b6337ac6f83a390c3c91174f11afbb2a8761e0878277f813fb1e8257f35d08f6`
- `include/vita/runtime/public/config.hpp`: `ec35b88f55ceb5035c74428d1b98474cb10e9b37685bf94b5c688e0adcfaf238`
- `include/vita/runtime/public/runtime.hpp`: `374e630d2aa1d07231e46cbe8bad1d598d04f17af07c11d895b94fb08a182100`
- `include/vita/runtime/stream/counters.hpp`: `f8c9deabec3ebc4e5be0aa128f80855b5714523090d6f7d1011bd500261ad85f`
- `include/vita/runtime/stream/routing.hpp`: `c2cf518ca8d271a8f12a722fce0df1bceb510561f90a6100413670b154383a32`
- `tests/unit/P11/CMakeLists.txt`: `47aa9e2c025a0513775d5135360887508aba4bcbdba5e0ec9526e62c68b17ed5`
- `tests/unit/P11/runtime.cpp`: `851d1702a5192697c6a44128fe2e801c331a399fcef451bf841044e4f4f45e9a`
- `tests/unit/P11/transactions.cpp`: `31f7214cdb2f2519ca778ca76e0eb6126506cd7d3ca95538a210d21e37db1dee`
