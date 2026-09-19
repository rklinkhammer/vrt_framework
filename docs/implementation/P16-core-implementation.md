# P16 shared tunable-profile core implementation

Status: candidate frozen for independent public/runtime verification. Independent kernel verification already passed separately. This report covers shared production and focused developer tests, not the separately owned scene/sweep applications or final qualification.

Implemented the approved P16 contract in profiles/iq/profile.hpp, runtime state/transaction/Context contracts, public configuration/runtime and source callbacks. Tunable classes are information2, IQ0x101/102/103, Context0x110, Command0x120 under configured OUI. V1 defaults/classes/write permissions remain unchanged. RF center is exact integer-Hz1MHz..6GHz; tunable SampleRate is session-fixed/read-only. Four per-command plan/outcome/ticket slots remain four; state has five canonical fields, RF last, independent of wire order. Known RF in v1 state rejects; an inactive absent trailing field with default ID is tolerated for old four-element aggregate initialization.

Profile RF bounds/grid validate requested, backend-adjusted, actual executed and initial/recovery/received known state. Invalid executed actual values become unknown-effect rather than usable metadata. Mixed SampleRate/RF writes fail the whole plan before effects even with partial enabled. State-indexed diagnostics, queries, cancellation, dependencies, Context publication/history and immutable revisions cover RF. Unknown required tuning metadata gates Data. Initial/recovery profile consistency is checked before effects. Generic virtual register remains four-field behavior.

A safety correction rejects scalar IDs with no bounded state slot before Engine admission. Previously such an ID could reach make_ack and index past the four-entry diagnostics/state arrays. This was not a safe supported negative-Ack path; public Runtime retains pre-admission rejection handling without device effects. Generic codec parsing of those scalar fields is unchanged.

Public Controller adds set_center_frequency(Hertz), named QueryField/QuerySelection query overload and checked StateObservation::value<Field>(). Existing numeric query masks remain source-compatible, with four selected fields maximum. Successful tune evidence is retained execution plus known RF state; the current/latest observation may be AckS rather than AckX, so applications must retain distinct phases or use the existing evidence wait in injected-clock tests.

RemoteTargetConfig/add_remote_controller installs only inbound Ack/Context/Data routes and outbound command capability. EndpointRole::controllee_only installs only request receive and outbound responses/metadata/Data. Combined remains default. Each UDP process uses its actual directed peer binding; association generation is explicit, default1. Dormant setup banks/counters are still allocated/accounted; controller-only service never runs their engine or source. Shutdown drains local transport and makes no claim of remote backend quiescence. No new transport or raw application command loop was introduced.

Optional DeviceBackendBinding supplies the existing Backend, pinned owner, exact storage charge and optional bounded progress hook. Any nonempty malformed binding rejects rather than falling back to virtual. Missing simulation is unsupported; missing disarm never proves cancellation/quiescence; missing progress means externally driven completion. ResultStorage pins binding ownership for retained AsyncResult capabilities. Runtime does not auto-complete/reinitialize external devices or expose virtual fault controls for them. Shared owner byte declarations are consistent and charged once; identical backend context cannot serve two independent stream bindings. The active bank polls once per cycle; retired bank replay does not duplicate adapter polling. Explicit recovery evidence still must establish physical quiescence.

SourceProvider's optional effective callback observes every real running state revision after its reserved commit. It is never a second setter; a callback failure gates/faults Data without rewriting real completion truth. Start/restart supplies a known snapshot observation at current sample ordinal; stopped changes are staged until that observation. Every running RF transition is delivered even if payload callbacks were skipped. A fresh recovery association may require a freshly installed scene epoch; the separately owned scene helper documents its explicit reset policy. No lost intervals are integrated under a guessed newest center.

## Storage and reservation evidence

On this Apple arm64/libc++ build: StateSnapshot176B, EffectiveEvent304B, Revision472B (below512B), AckRecord400B, default VitaRuntime232448B. The fifth state slot increases dependent storage; no hidden heap representation was introduced. Callback/result ownership uses existing setup shared-control blocks and bounded result arenas; operational no-allocation verification remains an independent gate.

The actual sixteen-stream paired-bank reference composition charges52,208,928/67,108,864B, including all reference raw pools. With fifteen streams, scheduling charge7,624,320B and headroom190,832B left insufficient category reservation for the final508,288B, although total memory fit. Runtime does not instantiate the old standalone plan arena: real Engine plans are already charged inside Stream. The approved exact317,456B unused plan reservation transfer closes that category deficit, preserving the total64MiB reserve. BudgetLedger::transfer_unused checks valid distinct categories and unused bytes before mutation; failure is atomic. Runtime uses this fallback only from the uninstantiated plans category when scheduling headroom is insufficient. Generic reference_budget() still charges its actual standalone plan arena and is unchanged in meaning.

## Developer verification and freeze

Tests unit/P16/core.cpp, binding.cpp, remote.cpp and budget.cpp pass direct C++23 -Wall -Wextra -Werror builds/runs and AddressSanitizer+UndefinedBehaviorSanitizer. Core covers active packet-boundary RF execution/readback, named query, unchanged sample rate and denied rate write. Binding covers malformed bindings, owned external completion and virtual-control isolation. Remote uses two Runtime instances over actual localhost UDP, one directed peer per side, Controllee-only and remote Controller public APIs, RF Ack/readback and local shutdown. It declares8 control/2 cancellation TX reservations for the Ack/Context burst. This is localhost evidence, not cross-machine/device timing. Budget covers transfer failure rollback and full sixteen-stream instantiation.

Existing combined v1 example also compiles/runs. Independent verifier owns its additional literal RF, lifecycle/capability, no-allocation, profile-isolation and negative cases. Root owns aggregate Release/ASan gates and final status. Frozen source identity and developer result JSON are under artifacts/P16-core/. No deployment/hardware qualification claim is made.

## Corrective gate findings and final re-freeze

Independent public verification found two defects in the initial candidate. A deferred external RF completion could arrive after the Runtime had emitted old metadata/Data at the promised effective boundary. Engine now exposes its earliest unresolved eligible real effect boundary; Runtime holds both Context and Data when it reaches that boundary, clamps catch-up skips there, and rechecks after generation before publication. Query, dry-run and ineligible/resolved fields do not gate. Backend progress, ticket incorporation and acknowledgement service continue while held; valid earlier intervals remain eligible. Unknown boundary evidence holds the source rather than guessing. Independent tests include initial-boundary0 and a future boundary with catch-up/periodic refresh, then real completion. No synthetic result or revised backend timestamp hides the delay.

The controller-only lifecycle getter also previously read the dormant local Engine's quiescence. It now reports no remote physical-quiescence proof and counts only its local I/O. Initial manifest is retained as source-initial-rejected.sha256.json; source.sha256.json is the corrected freeze. Independent direct ASan/UBSan verification passed all8 P16 targets before the aggregate rerun; root owns the final integrated verdict.

## Complete before/after ABI and ledger

The baseline is git HEAD693b9c79ba25638ace55ce021eaf812907b865cb, extracted read-only with `git archive HEAD include` into `/tmp/p16-before`; no workspace rollback or production edit was used. The same portable size/registration probe compiled separately against that include snapshot and current headers. Results and reproducible probe source are in artifacts/P16-core/sizes-before.txt, sizes-current.txt and size-probe-source.txt. These are Apple clang/libc++ ABI measurements, not platform-independent constants.

| Object or charge (bytes) | Before | P16 | Delta |
|---|---:|---:|---:|
| StateSnapshot | 136 | 176 | +40 |
| EffectiveEvent | 264 | 304 | +40 |
| Revision | 432 | 472 | +40 |
| AckRecord | 352 | 400 | +48 |
| ControllerRegistryStorage | 385,024 | 405,504 | +20,480 |
| ControllerRecordStride | 1,480 | 1,560 | +80 |
| Engine16 | 42,136 | 45,136 | +3,000 |
| EnginePlan | 1,200 | 1,200 | +0 |
| EngineTransactionRecord | 1,272 | 1,456 | +184 |
| SourceProvider | 16 | 24 | +8 |
| Runtime | 231,808 | 232,448 | +640 |
| PairedStreamCharge | 481,168 | 508,288 | +27,120 |
| StreamDerived | 168,712 | 177,136 | +8,424 |
| FullCharge | 51,753,888 | 52,208,928 | +455,040 |

ControllerRecordStride is independently derived from the difference between1-record and2-record ControllerRegistry storage with otherwise identical configuration; private record layout was not exposed. StreamDerived subtracts the exact already-public RevisionStore, ResultStorage, CompletionArena and2048-byte setup allowance from half of the measured paired-bank registration charge. EnginePlan stays1200B: the per-command limit did not become five.

| Budget category | Before charged | P16 charged | Before reserved | P16 reserved |
|---|---:|---:|---:|---:|
| raw_blocks | 30,998,528 | 30,998,528 | 30,998,528 | 30,998,528 |
| providers | 1,568,880 | 1,568,880 | 2,621,440 | 2,621,440 |
| duplicate_values | 8,388,608 | 8,388,608 | 8,388,608 | 8,388,608 |
| duplicate_index | 2,162,704 | 2,162,704 | 2,162,704 | 2,162,704 |
| plans | 0 | 0 | 2,097,152 | 1,779,696 |
| transactions | 0 | 0 | 1,048,576 | 1,048,576 |
| revisions | 0 | 0 | 1,048,576 | 1,048,576 |
| context_history | 0 | 0 | 1,048,576 | 1,048,576 |
| queues | 0 | 0 | 3,145,728 | 3,145,728 |
| completion | 0 | 0 | 524,288 | 524,288 |
| scheduling | 7,698,688 | 8,132,608 | 7,698,688 | 8,132,608 |
| retention | 0 | 0 | 966,656 | 966,656 |
| metrics | 0 | 0 | 1,048,576 | 1,048,576 |
| adapters_stacks | 936,480 | 957,600 | 4,194,304 | 4,194,304 |
| headroom | 0 | 0 | 116,464 | 0 |

The charged total grows455040B while reserved total remains67108864B. Remaining uncharged category reservations are not process RSS or permission to omit later allocations. The standalone plan projection remains unused by this Runtime; its reservation decreases exactly317456B under the approved fallback, and generic reference_budget retains its separate arena charge.

DeviceBackendBinding is88B and already included in Stream; any configured external owner additionally charges its caller-declared storage_bytes plus128-byte shared/control ownership allowance once, before registration. VirtualRfScene64B and SweepPolicy112B are separately owned application objects, absent from the reference Runtime total; examples must identify their stack/owner storage and thread-stack accounting. SourceProvider's24B callback binding is embedded and already counted. The size probe creates sixteen tunable paired-bank streams with reference pools and no external backend owner, trace owner or application scene allocation, so it does not pretend to measure those optional application objects.

### Publication ordering repair after the rejected aggregate checkpoint

The first pending-boundary repair exposed a PacketCount ordering defect: an old-state Data packet could remain held when the cursor reached a pending effect, and the next packet could capture the same counter before the first transport acceptance. The counter itself must still commit only on acceptance. Runtime now progresses eligible publication before generating the next packet only when a real future effect is pending, the clock is strictly before that effect, and no catch-up, SampleLoss, or coverage-floor adjustment remains. This sends the earlier valid interval before entering the hold. The normal catch-up/event-recording then publication order remains unchanged in every other case. The final pending-boundary recheck remains in place.

Periodic/start observations no longer move the sample coverage floor: a newer full Context does not invalidate the already valid preceding interval. Explicit start and clock-reanchor paths still establish the floor. This avoids silently dropping the old interval and avoids publishing an observation ahead of a subsequently recorded catch-up SampleLoss event. Coarse jumps across the effect or across a refresh while still before the effect do not bypass these guards.

The rejected aggregate checkpoint remains recorded separately. Targeted developer checks cover the original immediate command followed by continued `run_for`, the external backend binding path, and the existing combined endpoint example. Independent deferred checks additionally cover initial and future boundaries, 11- and 256-pair packets, first progress at 1 microsecond, strict missed-time rejection, and a coarse refresh crossing before a future effect. Final aggregate results belong to the coordinator and independent verification reports.

A general held-packet invariant is now enforced independently of that scheduling optimization: `send_data` finalizes PacketCount immediately before `try_send`, using the current sender counter and the existing checked prologue decoder/encoder. The mutable pointer originates from the unique framework header lease, is tracked only while that packet is unaccepted, and is cleared upon acceptance or submission failure. Rejection returns exclusive ownership; a later retry may refresh its count. Accepted storage and its payload, timestamps, revision, and externally retained views are never rewritten. This changes no structure size and does not reserve or advance counters speculatively.

`tests/unit/P16/held_counts.cpp` rejects two initial Context sends, accumulates held Data, then checks sequential accepted counts and byte-identical previously accepted headers while physical completion is deliberately withheld. Its direct AddressSanitizer/UndefinedBehaviorSanitizer run passes. The external adapter has four reserved control slots so the test's intentionally unprogressed initial/full Context packets do not confound Data-count assertions.

### Named cancellation selection

Added source-compatible `Controller::cancel(TransactionHandle, QuerySelection, CommandOptions={})`, which delegates to the existing numeric overload. The legacy numeric/default API and its identity validation remain unchanged. The P16 core test uses named RF selection for a scheduled cancellation and checks that a foreign runtime identity is rejected. Direct AddressSanitizer/UndefinedBehaviorSanitizer validation passes. The approved pre-addition source manifest is preserved as `source-approved-core-before-named-cancel.sha256.json`.

### Recovery gap event composition

Independent RF cancellation/contradiction recovery testing found that a fresh association could publish its initial Context and subsequently attempt a SampleLoss revision at that identical boundary. Recovery's real skipped-sample event is now composed into the copied initial revision before publication; other StateEvent bits are preserved. `pending_loss` is cleared only after successful initial revision admission. Engine confirmed state and old immutable revisions are unchanged. The external binding developer test now recovers after a sample gap to a fresh SID and continues Data progress; direct AddressSanitizer/UndefinedBehaviorSanitizer execution passes. This is separate from the named-cancellation overload and remains subject to the independent lifecycle and final package gates.

### Typed AckV evidence

`Observation` now appends `validation_outcome_known` and `validation_accepted`. A checked AckV reports known admission disposition; acceptance requires SchV with no partial, error, indeterminate, or timing-failure evidence. Acceptance means whole-command, nonpartial admission. False does not establish that no subset had effects. Late AckV retains this evidence with `response_kind=validation`, without execution confirmation or retroactive deadline success. Conflicting observations for the same phase clear both typed facts and retain the contradiction marker.

The developer outcomes test covers accepted, rejected, error-summary, partial, late, and contradictory AckV using checked literal packets with explicit original-request diagnostic context. Direct AddressSanitizer/UndefinedBehaviorSanitizer execution passes. Tail padding accommodates both new booleans: Observation remains20B, ControllerObserver360B, Runtime232448B. The 16-stream budget regression still reports52,208,928B charged,67,108,864B cap,317,456B reservation transfer; no new reservation or storage is required. Full repeated sizes are in `artifacts/P16-core/sizes-after-validation-evidence.txt`.
