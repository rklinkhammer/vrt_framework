# P16 frequency-scan independent verification

Status: **PASS — P16 local software and example verification complete.** Final ASan/UBSan225/225, coordinator Release229/229, targeted TSan6/6 and core-only combined checks2/2 pass. Hardware, external-peer interoperability, Linux and deployment timing qualification are not established by these local checks. The verifier owns tests/evidence and this report, not production changes. The [frequency-scan requirements](../frequency_scan_example_implementation_prompt.md) explicitly authorize a new opt-in tunable profile while retaining IQ Generator v1.

## Historical contract review checkpoints

The accepted application choices are integer-Hz centers from1MHz through6GHz,1Hz grid with rejection of unsupported/nonfinite/fractional inputs, default100MHz through100.2MHz in25kHz steps,100k complex samples/s,100ms dwell,1000ms command timeout and one sweep. The fixed scene tone is100.05MHz. The tunable association has a fixed configured sample rate: it is queryable but wire writes are unsupported. A new explicitly configured session may choose a new rate/phase epoch; original v1 Sample Rate writes remain unchanged. These are example/profile choices, not universal device capability or assigned production identifiers.

Before production example implementation, the contract must specify:

- A distinct wire profile/class discriminator used consistently for Control, acknowledgements, Data and Context. Original v1 routing and permissions must reject RF tuning; merely adding a generic codec descriptor is insufficient.
- A named public RF-center setter and query selection; public remote Controller binding without creating a fake local Controllee. The application must not construct CAM/CIF words or run an alternative protocol loop.
- The profile's state schema, field masks, required-known fields, query/cancel selection, revisions and Context propagation. Unknown RF state must gate associated Data and require explicit recovery rather than retaining stale known metadata.
- Exact meaning of backend completion: requested versus actual applied frequency, effective packet/sample boundary and usable/settled samples. Validation acceptance, local send completion and SDK write completion cannot independently start dwell.
- A defined virtual passband/Nyquist convention and phase behavior across tuning, fixed sample rate, unchanged ordinal/time progression and coherent per-packet state. Suppressed tones must not alias into-band.
- Monotonic local Controller dwell/deadlines, one tune in flight, bounded failure/reconciliation behavior, and cancellation/drain/quiescence that does not equate timeout with safe reclamation.
- Explicit budget and scratch/storage changes, public transport ownership and a replaceable backend boundary that can report delayed/failed/unknown effects and physical quiescence.

No physical SDR is required to verify the virtual profile. A future backend's range, settling, actual RF effects and hardware evidence remain distinct inputs; their absence does not block this example.

## Existing assumptions requiring verification

Read-only source review identifies the following shared contracts. The implementation must update them coherently, not simply add a setter to an example.

| Area | Existing assumption | Required independent check |
|---|---|---|
| State and plans | `baseline_fields`, StateSnapshot and ExecutionPlan contain four fields; field-index sentinel is4 | Profile-aware schema/index/masks; original v1 field meaning unchanged; no truncated fifth-field outcome |
| Request/ack/correlation | Request arrays, Ack state/diagnostic loops, controller requested masks and cancellation selection use four slots | RF field survives request, validation/execution/state, query/readback, duplicate/replay and cancellation paths |
| Revision/publication/history | Equality, validation, delta merge and required-known checks use four fields | RF initial/full/change Context; matching revision precedes affected Data; old snapshots/leases remain immutable; missing/unknown RF not labeled usable |
| Public Runtime | Fixed class selection and local Controllee-linked Controller setup; virtual backend stored internally | Distinct profile identity, public remote relationship and actual replaceable backend integration; unchanged v1 APIs remain valid |
| Timing/source | SampleRate-specific timeline changes and default source | RF changes only translate scene frequency at packet boundary; sample rate, ordinal, phase policy and clock-domain semantics obey the new contract |
| Storage | State copies appear in transactions, retention, history, revisions and two-bank recovery | Actual `sizeof` ledger propagation, startup precharge/failure rollback, no unaccounted storage or new operational allocation |

## Independent acceptance plan

1. **Sweep policy:** literal expected ascending sequences, off-grid stop, single point, finite repetition, explicit continuous mode, checked integer/duration parsing, overflow/range/step errors and finite defaults.
2. **Wire/profile:** independently calculated RF Q20 words and CIF0/bit27 masks, exact profile identity, v1 rejection, named query/readback, original/cancel transaction correlation and full Context schema.
3. **Controller evidence:** AckV-only does not advance; delayed execution and settling postpone dwell; explicit failure/unknown/timeout stops; local monotonic deadlines never subtract remote times. Duplicate or reordered responses cannot produce extra tuning or false success.
4. **Backend/runtime:** actual production transaction path with packet-boundary effect, delayed completion, cancellation before/after cutoff, late callbacks, failure/unknown state, physical quiescence and preserved required state across recovery.
5. **Signal/metadata:** independent tone samples/offsets, out-of-band and both Nyquist edges, documented phase continuity, coherent frozen center per packet, initial/changed Context before delivery, retained old buffers and fixed sample-rate/timeline behavior.
6. **Public integration:** combined deterministic loopback and separate-process localhost UDP using public setup/controller/observer APIs; no raw command construction in example flow; porting callbacks and completion responsibilities match real types.
7. **Resources/regressions:** instrumented hot-path allocation checks with positive probes, exact revised ledger, budget rejection before allocation/mutation, affected transaction/Context/timing/lifecycle regressions, sanitizers and standalone headers. Preserve source/test manifests and distinguish local simulation from hardware or cross-machine qualification.

The phase oracle includes skipped source callbacks spanning multiple effective RF revisions. For example, at100k samples/s with the100.05MHz tone, centers100.025/100.040/100.065MHz effective at ordinals0/3/7 yield phase0.55 cycles at ordinal11 from a zero-phase origin: three samples at+25kHz, four at+10kHz and four at−15kHz. Applying the newest center to the whole interval incorrectly yields−1.65 cycles (0.35 modulo one). The implementation must retain or receive enough serialized effective-boundary information to integrate every interval, including suppression and re-entry; a callback's current snapshot alone does not establish that history.

Exact production API signatures, test mappings and implementation results will be recorded as the candidate is independently exercised. No implementation PASS is claimed at this preparation stage.

## Independent contract verdict

The reviewed [P16 contract](P16-contract.md) is approved as a coherent bounded extension. This approves the specified interfaces and behavior for implementation, not their unbuilt correctness. It supplies distinct profile/class identities, a five-state/four-command separation, FieldId-based wire mapping, explicit receiver schema, public remote endpoints and an owned replacement backend. Original v1 permissions remain unchanged. The operational Array exclusion remains unchanged.

Two review clarifications were incorporated before approval:

1. A Controllee cannot infer an “active sweep” from the existing wire protocol. The tunable profile therefore has a fixed session sample rate and rejects wire SampleRate writes; queries remain supported. Explicit new-session configuration can choose another rate/phase epoch. This avoids guessing phase behavior across unannounced denominator changes while retaining original v1 rate control.
2. Optional backend callbacks have explicit limitations: absent simulation cannot fabricate dry-run evidence; absent disarm cannot establish cancellation or physical quiescence; absent progress permits externally driven completion while the framework owns serialized protocol progression. Successful tuning completion represents the effective usable sample boundary, not an SDK write returning.

The bounded serialized source effective-event callback resolves missed-production phase history without an unbounded queue. It observes committed real effects, never acts as a second tuning setter, and faults/gates Data on failure without rewriting a truthful backend outcome. This mechanism will receive the multiple-center skipped-callback oracle above. The contract also defines the half-open passband, local-monotonic dwell, both AckX and corresponding known RF AckS before advancing, actual owned-backend lifetime, real remote UDP directions and explicit budget recalculation.

No unresolved wire or profile decision was found that requires new user input before implementing this contract. Hardware-specific lock, cancellation and quiescence evidence remains a future backend responsibility; virtual zero-settling and localhost behavior do not qualify a device. The candidate must still pass the independent plan and affected shared regressions before P16 completion.


## Independent helper and kernel checks

The frozen scene/sweep helpers and shared kernel pass direct Clang C++23 ASan/UBSan checks. This is a bounded subsystem verdict; public Runtime, owned backend lifetime, remote routing, examples and aggregate regressions remain pending.

- `p16_verify_scene`: independent literal IQ16 positive/DC/reverse tones, both Nyquist edges, suppression/re-entry, multiple skipped-callback RF boundaries (phase0.55 at ordinal11), large ordinal arithmetic, invalid state, and instrumented ordinary/aligned allocation zero with positive probes.
- `p16_verify_sweep`: exact nine-point defaults and bounded repeated/off-grid sequences; strict CLI errors; both AckX/readback orders; duplicate evidence without dwell renewal; exact timeout and overflow boundaries. The public CLI requires positive dwell; the internal zero-dwell policy remains an intentional separate API permission.
- `p16_verify_wire`: independently calculated RF Q20 words, literal command/query/full five-field Context bytes and truncation callback barriers. Generic parsing remains class-neutral; public profile routing is tested separately when frozen.
- `p16_verify_kernel`: v1 isolation; RF endpoints/grid/range; whole mixed RF/SampleRate rejection even with partial execution permitted; five-state/four-command bound; unknown RF prevents known metadata; invalid initial snapshot/profile rejection; invalid adjusted RF fails before backend begin; invalid executed actual becomes unknown effect and unknown RF state.

The initial new completion assertion incorrectly expected a one-element outcomes container. The API returns a fixed four-element command array; the verifier assertion was corrected and the complete kernel test rerun successfully. No production correction resulted from that assertion.

Evidence: [helper source manifest](artifacts/P16-helper/source.sha256), [kernel source manifest](artifacts/P16-kernel/source.sha256), [direct results and flags](artifacts/P16-kernel/results.json), [kernel sanitizer log](artifacts/P16-kernel/kernel-asan.log), [literal-wire sanitizer log](artifacts/P16-kernel/wire-asan.log). Silent sanitizer logs indicate successful test execution; exit results are recorded explicitly in the results file.


## Public integration findings and repaired direct checks

Independent public tests found two production defects before acceptance. A controller-only lifecycle getter recomputed physical quiescence from its dormant local engine; the repaired role branch reports no remote physical proof. Separately, a delayed external RF completion could conflict with old Context/Data already emitted at its promised effective boundary. The repaired pending-effect gate holds publication and sample progression at unresolved real-effect boundaries while continuing backend/completion/control service. The coordinator identified and repaired the additional post-generation publication seam when catch-up reaches the boundary.

`p16_verify_deferred` preserves the original failing sequence and extends it to a future boundary (ordinal110 at1.1ms for100kHz/11-pair packets). Earlier old-center Data remains available. Pending completion holds ordinal/Data across catch-up and a one-second periodic-refresh interval; completion then resumes Data with known new RF metadata. Both initial and future cases pass direct ASan/UBSan. No fabricated later effective timestamp is substituted.

`p16_verify_public` verifies caller owner reset, an actual completion capability retained beyond Runtime destruction, rejected late publication, and final backend weak-owner expiry. Absent simulation produces no real begin/write or execution confirmation. Absent disarm with unavailable physical quiescence cannot produce a stopped/physically-quiescent shutdown. The monotonic shutdown deadline is exercised by an explicit time jump, not millions of redundant ticks. Controller-only lifecycle proof remains false.

`p16_verify_remote` uses real IPv4 and IPv6 UDP between separate Runtime instances. Matching tunable classes produce server backend writes and retained AckX/known RF AckS; original-v1 classes do not. The client progresses without a local source or PPS. Client shutdown does not control remote physical state. These are localhost integration checks, not independent-host or hardware qualification.

`p16_verify_budget` checks exact unused-reservation transfer and failure rollback, unchanged67,108,864-byte total reservation, the separate reference plan-arena projection, sixteen registered tunable streams charged52,208,928 bytes with317,456 bytes reclaimed from unused plan reservation, and too-small memory-limit rejection. Earlier test preparation compared the sixteen-stream total before registering streams and inspected latest AckS instead of retained AckX; both verifier mistakes were corrected without production changes.

Direct evidence: [initial lifecycle failure](artifacts/P16-public/public-initial-failure.log), [public lifetime check](artifacts/P16-public/public-asan.log), [deferred boundary check](artifacts/P16-public/deferred-asan.log), [budget check](artifacts/P16-public/budget-asan.log), [IPv4/IPv6 check](artifacts/P16-public/remote-asan.log). Aggregate frozen-source checks and complete example/phase integration remain pending.


## Rejected first aggregate core gate

The first repaired-candidate full ASan/UBSan gate passed206/209 tests in53.64s, with `p16_core`, `p16_binding`, and the pre-existing `p10_example_combined` failing during continued Runtime progression. All eight independent P16 tests passed, but their shorter completion checks do not waive this failure. The coordinator's Release gate reproduced the same three failures. Core acceptance remains open pending repair and a repeated-packet regression.

[Full sanitizer log](artifacts/P16-final-core/test-asan.log), [build log](artifacts/P16-final-core/build-asan.log), and [manifest verification](artifacts/P16-final-core/manifest-check.json) preserve this rejected candidate:80 include/verifier files remained unchanged throughout the gate. Later repaired runs must use separate artifacts rather than overwrite these results.


## Continued progression and packet-count repair

The aggregate failure exposed a second ordering issue: an earlier held Data packet had not committed its packet count when a later packet was constructed with the same count. The final repair assigns the current count immediately before transport submission using the privately owned, unaccepted header. Acceptance clears its mutable tracking entry. Payload, sample timestamp and revision remain unchanged, and accepted headers are never restamped. Publication gating also distinguishes unresolved effective boundaries from catch-up and periodic observation work.

The expanded deferred test now contains six independently selectable scenarios: initial boundary with11-pair packets; a future boundary held across periodic refresh; initial boundary with256-pair packets; first progression at1µs; a coarse jump past a strict deadline that must reject without effects; and a periodic refresh before a queued future effect. The latter uses an explicitly configured five-second holdover and an exact packet boundary at3.00003s, then proves admission by observing the backend begin at that boundary. This avoids mistaking clock expiry or mapping revalidation for successful queued work. Actual completion cases continue through repeated coarse progression and another10ms of the injected driver.

`p16_verify_counters` independently rejects Context submissions across four progress calls, then accepts the resulting held Data queue. It checks literal header count bits at every actual submission and compares every previously accepted header byte-for-byte while the adapter still owns its storage. Adapter progression is withheld during this observation and drained normally afterward. The first fixture mistakenly requested Loopback quiescence holding without the required failed-completion fault; that invalid fixture was corrected without a production change. Default Loopback registry template capacities were also corrected to match Runtime's128-entry registries.

All six deferred scenarios and the counter ownership test pass direct ASan/UBSan on the final producer manifest. [Deferred log](artifacts/P16-core-repaired/deferred-direct-asan.log) and [counter log](artifacts/P16-core-repaired/counter-direct-asan.log) preserve those results. The nine-test verifier source freeze and full repaired aggregate result are recorded separately from the rejected first gate.


## Frozen core gate verdict

**PASS for the bounded P16 core candidate.** The final full ASan/UBSan suite passed **212/212** tests in49.20s, including all nine independent P16 tests and the three previously failing developer/example regressions. The coordinator's matching Release gate passed **215/215**, including71 standalone public-header checks. The81-file include/verifier manifest and21-file producer manifest remained unchanged through the sanitizer gate.

Durable evidence: [sanitizer results and manifest checks](artifacts/P16-core-repaired/results.json), [full test output](artifacts/P16-core-repaired/test-asan.log), [individual test output](artifacts/P16-core-repaired/LastTest-asan.log), [source manifest](artifacts/P16-core-repaired/source.json), and [coordinator Release evidence](artifacts/P16-core-final/). The rejected first gate and intermediate failed scenarios remain preserved above.

This closes the core implementation prerequisite for dependent examples. It does not yet close P16 end-to-end acceptance: actual Runtime-to-scene effective-event fanout, RF-specific cancellation/recovery, C/C++ hot-allocation instrumentation during real RF operations, complete example CLI/correlation behavior and porting documentation still require the next integration gate. Local deterministic and localhost results do not qualify remote hosts or physical RF hardware.


## End-to-end preparation evidence

The independent Runtime/scene test now executes three RF commands at sample ordinals220,440 and660 through an owned external backend. Exhausting the payload pool suppresses all intervening sample callbacks while the serialized effective-event callbacks still report every committed center. On resumption, the real scene phase matches an independently integrated integer oracle. Dry-run produces no real source event. This passes direct ASan/UBSan.

The same operation sequence passes an optimized macOS allocation-guard build linked to the separate allocation interposition library. Positive `malloc` and aligned C++ `new` probes establish detection; after setup, the guarded PPS/start, RF commands, backend completion, payload pressure, effective-event fanout, resumption and dry-run record zero C and C++ allocations. This claim covers the serialized test thread and configured interposed allocation APIs; it is not an all-process or hardware claim. A separate owned-backend budget test confirms that two distinct contexts sharing one owner charge its declared storage plus128 bytes once and reject inconsistent owner-size declarations without changing the ledger.

RF-selected cancellation succeeds before begin and after dispatch. A contradictory late executed callback then makes RF unknown and stops Data. The test exposed duplicate SampleLoss Context at the recovery start boundary. The repaired path composes the gap indicator into the copied initial new-SID revision, leaving confirmed Engine state unchanged. The strengthened oracle sees known RF metadata and exactly one SampleLoss event after recovery, with continued Data and no identity conflict. This test currently uses the default source; explicit fresh-epoch recovery of the real RF scene is a separate integration requirement, not inferred from it.

Independent CLI and process oracles preliminarily pass on the available example binaries: exact default nine-frequency sequence, full handle pairing between request and confirmed tune, configured monotonic dwell, known IQ, real two-process localhost UDP with zero Controller backend writes and nine Controllee writes, and a silent peer timeout with no implicit cancellation datagram. Invalid CLI input must fail before RF requests; no undocumented specific exit code is imposed. Final source/binary manifests and the frozen aggregate gate remain pending.


## Explicit scene recovery and retained samples

The approved recovery factory is independently exercised through real Runtime recovery, not only as a helper. A fresh `VirtualRfScene::create_for_recovery` value replaces the caller-owned scene at the same address inside the explicit quiescent reinitialization callback. Its first known new-association event arrives at the actual nonzero ordinal after skipped samples and establishes phase zero. Subsequent real sample callbacks match the independently calculated phase from that new origin. Ordinary creation still rejects a nonzero first ordinal, ordinary retunes preserve phase, and a faulted object does not silently rearm. This explicit recovery epoch does not guess the unknown RF history.

The actual multi-retune test retains the first received payload and its metadata. Both remain byte/value identical across the RF changes and after Runtime destruction, then release their quotas normally. This retention occurs inside the same guarded operation sequence that reports zero hot C/C++ allocations.

Independent process coverage also includes two complete finite sweeps and a duration-bounded continuous sweep. These preliminary checks all pass; the final rebuilt source/binary gate remains the acceptance authority. [Direct results](artifacts/P16-integration/direct-results.json), [real-scene lifecycle log](artifacts/P16-integration/rf-lifecycle-asan.log), and [owned-backend budget log](artifacts/P16-integration/owned-budget-asan.log) preserve the direct evidence.


## Typed validation evidence

Independent checked-wire tests cover positive and rejected AckV, partial/error/indeterminate/timing-negative validation, warning-positive validation, late acceptance and rejection, retained timeout evidence, and contradictory same-phase responses. Known validation acceptance remains distinct from execution and success; contradictory validation clears both typed validation fields.

The application-level rejection test uses its actual Runtime Controller and a backend that rejects RF validation. A transport wrapper deliberately loses action2 AckX/AckS while leaving the initial query and AckV intact. The initial named query succeeds, then explicit negative AckV stops the scan before20ms against a1s deadline, with no backend begin/write and no confirmed point. Thus this result cannot be attributed to an execution Ack or eventual timeout. [Typed evidence log](artifacts/P16-integration/validation-asan.log) and [AckV-only application rejection log](artifacts/P16-integration/rejection-asan.log) preserve the direct sanitizer runs.


## Final independent verdict

**P16 PASS for the approved local software scope.** Final ASan/UBSan passed **225/225** in58.63s. Coordinator Release passed **229/229** in19.29s, targeted TSan passed **6/6**, and the core-only combined checks passed **2/2**. The final97-file include/verifier/example manifest is unchanged; all22 current core and13 example producer manifest entries match. The existing core215/212 checkpoint remains historical evidence rather than a substitute for the final gate.

The final gate includes real RF-scene phase integration, payload-pressure and retained-sample lifetime, typed query/cancel, validation-only rejection, unknown-state shutdown/recovery, explicit fresh recovery phase origin, exact budget/owner accounting, allocation detection and zero hot allocations, pure CLI boundaries, combined finite/repeated/bounded-continuous operation, separate-process localhost UDP, and idle timeout without implicit cancellation. Retention, admission and finite identity capacities remain enforced; longer continuous runs may stop explicitly at a capacity limit. No indefinite-operation or zero-loss performance qualification is claimed.

The only final Release test failure was an internal port-reservation race between parallel process tests. Shared CTest resource locks corrected the test isolation; assertions and production behavior were unchanged. [Registration-change record](artifacts/P16-final-integration/test-registration-change.json) preserves that distinction. Earlier production defects, fixture mistakes and rejected gates remain documented above.

Final durable evidence:

- [Independent results and manifest checks](artifacts/P16-final-integration/results.json), [source manifest](artifacts/P16-final-integration/source.json), [full sanitizer output](artifacts/P16-final-integration/test-asan.log), and [individual test output](artifacts/P16-final-integration/LastTest-asan.log).
- [Combined process log](artifacts/P16-final-integration/process-combined/combined.log), [two-sweep log](artifacts/P16-final-integration/process-combined/repeated.log), [bounded-continuous log](artifacts/P16-final-integration/process-combined/continuous.log), [remote Controller log](artifacts/P16-final-integration/process-remote/controller.log), [remote Controllee log](artifacts/P16-final-integration/process-remote/controllee.log), and [silent-peer timeout log](artifacts/P16-final-integration/process-timeout/timeout.log).
- Coordinator [Release](artifacts/P16-final/test-release.log), [targeted TSan](artifacts/P16-final/test-tsan.log), and [core-only](artifacts/P16-final/test-core-only.log) evidence.

The inspected porting guide maps completion to an actual usable sample boundary, preserves lifetime/cancellation/quiescence distinctions, describes explicit new-scene recovery and identifies missing hardware/deployment inputs. The virtual executable's failure termination policy is explicitly limited to that example and does not establish physical device quiescence. No unresolved in-scope software decision or failing required check remains.
