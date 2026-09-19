# P13 transaction trace and inline model hooks

Status: developer-verified candidate; independent hooks, integrated benchmark, measured targets and deployment qualification remain separate gates. Ownership is transaction `trace.hpp`, `backend.hpp`, `engine.hpp` and `tests/unit/P13/hooks.cpp`. The transport pre-decode ingress seam, Runtime virtual-register endpoint, worker/trace allocation ledger and measurement tools are implemented by the separate Runtime/benchmark owner.

## Trace contract

`TraceKey` preserves association generation, operation, authorized peer, SID and MID. Per-field dispatch/device completion records carry field identity; terminal records describe the entire command and include outcome status and simulation status. `TraceBinding` pins setup-owned callback storage. `now_ns` must be bounded and thread-safe because a backend completion thread may invoke it; `record` runs on the serialized Engine domain. Production measurement uses actual `steady_clock` nanoseconds, not injected protocol time. A test clock may validate ordering without constituting latency evidence.

The five stages are received, validated, dispatch, device_done and recorded. Runtime captures received at its complete-envelope pre-semantic-decode handoff and later correlates the saved timestamp. Engine emits validated only after successful complete validation and resource admission; rejected work has no fabricated accepted stages. It emits dispatch immediately before backend begin. The winning AsyncResult writer writes outcome and captures device_done into its lifetime-owned result slot before publishing ready with release ordering. Engine's acquiring ticket scan reads both, emits the captured completion time, incorporates actual state/outcomes and emits recorded only on whole-command terminal incorporation/response recording. No non-atomic write occurs after ready publication. Duplicate losing publishers cannot overwrite the timestamp; a synthetic failure does not invent a real device_done event.

Multiple-field commands produce separate dispatch/device_done records and one terminal recorded event. A collector may derive first dispatch/last completion for the whole command, but that interval is not a single-register backend time. The prescribed software qualification uses one-field updates to the independent four-field model. The trace API never silently interprets failed, partial, simulated or incomplete commands as successful latency samples.

`storage_bytes` declares actual setup sink/staging and shared-owner overhead. Zero is permitted only for standalone caller-accounted use; Runtime requires positive accounted bytes, charges unique owners once, and rejects inconsistent aliases. The callback sink must mark overflow and invalidate the measurement; the hooks do not allocate or hide trace loss. ResultStorage holds the trace owner so a late capability keeps its clock context alive independently of an Engine frontend.

## Inline virtual-register model

The existing VirtualBackend retains queued behavior by default. `set_inline_completion(true)` selects explicit bounded inline model updates; changing mode with pending operations is rejected. Each accepted begin updates the fixed four-field model before publishing completion through AsyncResult. No device I/O, intentional delay, IQ generation, Context publication, sample-boundary wait or bypass of Engine completion consumption is introduced. The public `model()` view is the inline register model. Unknown-effect inline outcomes invalidate the affected model field. Dry-run uses the separate simulation path.

## Bounds and verification

On Apple arm64/libc++: TraceBinding48 bytes; TraceEvent56; TraceKey32; AsyncResult88; ResultStorage slot136; ResultStorage<64>8752; Engine<16>42136; per-plan1200; VirtualBackend<16>3552. Runtime must charge these actual current types, including enlarged generic backend state and setup trace owner, rather than reuse the historical M3 ledger.

The developer test passes direct normal, ASan/UBSan and TSan builds with C++23 and exceptions/RTTI disabled. It checks five-stage ordering, correlation, model-before-publication behavior, deferred ticket incorporation, two-field terminal timing, rejected admission without fabricated events, a bounded sink's visible overflow, zero ordinary allocations across100 operational cycles, and cross-thread outcome/timestamp publication with duplicate-winner rejection. The existing P06/P07/P11 transaction developer executables also pass. These tests verify semantics; they do not satisfy the actual-host30-minute measurement or Linux/peer deployment gates.

## Candidate source manifest

- `include/vita/runtime/transaction/trace.hpp`: `98e3f71b817ec5ccb1926541b5fad77ffb5e7b78a40982617570ae24a617d8a8`
- `include/vita/runtime/transaction/backend.hpp`: `d7a6e39ac527ef94d407118d4188096c812e5baba167d1f73ccb9cd71564fe16`
- `include/vita/runtime/transaction/engine.hpp`: `d004d5b2cdf5a2654ed9a381731b30ce34da16c5407659b735b643c045bbe690`
- `tests/unit/P13/hooks.cpp`: `ad0053a8bb80565ad3a6e4b761488803c76aef830fc9651d7afa2bac6e88078f`
