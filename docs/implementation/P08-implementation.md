# P08 implementation handoff

Implemented `runtime/timing/{time,sample_timeline,clock,scheduling}.hpp`. Public time values separate `MonoTime::ns`, `ProtocolTime::{seconds,picoseconds}`, and normalized `Duration`. Checked arithmetic rejects carry/borrow/overflow; modern absolute epochs are never multiplied into a signed picosecond scalar. Protocol seconds remain 64-bit internally: P10 must explicitly reject unrepresentable 32-bit wire integer timestamps, never truncate.

`SampleTimeline` covers the accepted integer 1–100000000 sample/s profile. Quotient/remainder arithmetic uses only standard 64-bit operations. It tracks sample ordinal and exact sub-picosecond numerator/denominator across rate transitions, while `time()` exposes the floored protocol timestamp. Cross-rate residue denominators use checked LCM of reduced sample-step denominators. The bound is UINT64_MAX; an unrepresentable new segment returns resource_limit without changing rate, revision, ordinal or timestamp. Before device effects, P06/P09 must validate a copy of the proposed timeline with `change_rate`, then commit only the accepted copy at the effective boundary. Rates 99999989 then 99999971 then 99999959 after one sample each demonstrate deterministic representational admission failure. No floating-point drift or mandatory nonstandard wide integer is used. Overflow/capacity failure is atomic.

`ProtocolClock` requires an explicit configured epoch and either qualified or explicitly injected binding. PPS observations require paired time-of-day already converted into that epoch by the binding. A pulse alone does not start Data. Every accepted replacement mapping increments generation, conservatively requiring scheduled-boundary revalidation even for ordinary PPS updates. This also invalidates schedules after clock steps; monotonic deadline values are completely separate. Disarming armed work and tracking uncancellable outcomes belong to the transaction integration.

Clock state is unbound/acquiring/locked/holdover/faulted. Data start requires locked; continuation permits bounded holdover. Explicit PPS loss clears calibrated indication immediately; normal snapshot refresh infers holdover after the one-second pulse cadence is missed. At 2 seconds since the last qualifying PPS, the default binding faults. Holdover uncertainty includes configured drift with conservative ceiling arithmetic. `data_start_allowed(now)` and `data_continue_allowed(now)` refresh before deciding. No-argument accessors are cached and require a same-domain `snapshot(now)` first. All state mutation is serialized on the control domain; no concurrent unsynchronized access is offered. No GPS hardware or production qualification is inferred by tests.

Scheduling implements inclusive mode 1–4 windows and actual effect-interval validation. Qualified/injected timing capabilities, preparation lead, horizon, mapping generation, clock and quantization uncertainty all constrain selection. `choose_boundary` examines at most 128 supplied candidate boundaries, chooses the closest represented protocol timestamp, and breaks ties earlier. Candidate generation belongs to the packetizer; callers can submit nearest candidates without materializing every boundary in a 10-second horizon. `make_boundary` carries one picosecond of conservative quantization uncertainty for a nonzero exact residual, preventing false success at a window edge. Backend-ready and committed flags exclude unavailable boundaries. Immediate mode 0 ignores requested time and timed preparation lead/horizon, chooses the next uncommitted backend-ready boundary, and remains available for Control while Data is stopped. A stopped generator can update pre-start configuration without generating a boundary, through the higher-level backend contract.

All P08 objects are allocation-free values. Current arm64 sizes: ProtocolClock 88, SampleTimeline 56, Boundary 48, ClockSnapshot 48 bytes. These belong to the scheduling/revision categories when instantiated by subsequent packages; P08 creates no hidden storage, executor or clock thread.

Developer tests passed:

- `cmake --preset dev && cmake --build build/dev --target p08_timing`
- `ctest --test-dir build/dev -R '^p08_timing$' --output-on-failure`
- Equivalent ASan/UBSan build and test.

Checks cover T1–T6, timestamps at a modern epoch, carry/overflow, exact cross-rate endpoints, rate bounds 1 and 100M, coprime-denominator rejection before mutation, paired PPS/time-of-day, holdover and exact two-second expiry, clock-step generation changes without deadline changes, closest-boundary earlier tie, uncertainty rejection, mode-0 independence and fractional-picosecond window-edge rejection. Independent verification remains separate.

Frozen production manifest (SHA-256):

```text
fa0be1dd3b89c2fdf27cc0d4cf584295e18ac5c9ce1a28821020af7d8c2983e6  include/vita/runtime/timing/clock.hpp
0ecf817f6b233addaac4edb5d418b2ece1874965f766a3f474888863ce1b2805  include/vita/runtime/timing/sample_timeline.hpp
74dc772739b2c188f20bb01d22d45e45833248566fc2043ad3cfaf0d007ff201  include/vita/runtime/timing/scheduling.hpp
3015ec14a5d16ee5dd68d86fdf793d3bb016802e83a842c989160d9ee8dfa285  include/vita/runtime/timing/time.hpp
```
