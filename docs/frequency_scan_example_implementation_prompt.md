# P16 implementation prompt: controller-driven frequency scan

Implement a well-documented example in this repository in which a Controller repeatedly tunes an IQ generator across a configured range of RF center frequencies. This represents a narrowband receiver sweeping a wider frequency range. The main teaching objective is how to write a Controller application; the Controllee command-processing path must also serve as a practical template for a future real SDR backend.

Use implementer and independent verifier agents. Parallelize only independent work, maintain implementation/verification reports, and preserve unrelated changes. Implement and verify the example, rather than stopping with a design proposal. Inspect applicable repository instructions first. Stop only at an unresolved wire/protocol decision or genuinely unavailable required input; lack of physical SDR hardware must not block the virtual example.

## Package and concrete project choices

Execute this as post-M5 package **P16-frequency-scan**. M5/D-M5-1 remains the completed baseline; do not rewrite it to imply tuning was already supported. P15 remains reserved for hardware/M6. Current implementation/status reports and source take precedence over historical pre-implementation descriptions in the plan. Maintain P16 contract, implementation, verification and integration reports. Freeze and independently approve the profile/state/API contract before dependent implementation or examples.

These are example policies, not generic VITA codec constraints:

| Choice | P16 initial contract |
|---|---|
| Wire profile | Opt-in Tunable IQ v1; Information Class0x0002; Data classes0x0101/0x0102/0x0103 for IQ16/IQ32/float32, Context0x0110, Command0x0120, under configured OUI. These distinct project-local codes are not assigned production identities. |
| RF model | RF Reference Frequency equals center; zero IF and RF offset. Inclusive center range1,000,000–6,000,000,000Hz, integer-Hz1Hz grid. Reject out-of-range/fractional-Hz requests before effects; no automatic rounding. |
| Sweep defaults | Start100,000,000Hz; stop100,200,000Hz; step25,000Hz; sample rate100,000samples/s fixed during a sweep; dwell100ms; command timeout1,000ms; one sweep. |
| CLI | `--start-hz`, `--stop-hz`, `--step-hz`, `--sample-rate-hz` accept unsigned decimal integers; `--dwell-ms`, `--timeout-ms` accept positive decimal integer milliseconds; `--sweeps N` accepts positive integers and defaults to1; `--continuous` is mutually exclusive with explicitly supplied `--sweeps`. No implicit suffix parsing. |
| Synthetic scene | One fixed absolute tone at100,050,000Hz, amplitude0.5. Receive passband[-sample_rate/2,+sample_rate/2), including negative but excluding positive Nyquist. Outputzero outside this interval. |
| Phase | Start tone and local-oscillator phases atzero for a new scene session; preserve both across retunes. Change only the LO frequency increment at the effective packet boundary. Advance both through suppressed/dropped samples and out-of-band intervals; no phase reset on re-entry. Use stable reduced-phase arithmetic, not ever-growing absolute floating phase. |
| Completion | Virtual completion means the selected RF center is effective at the reported sample boundary and samples beginning there are usable. Virtual physical settling delay iszero; packet-boundary scheduling still applies. Reuse existing asynchronous completion, without a separate lock callback. |
| Sample Rate policy | Tunable profile fixes Sample Rate per configured source session. It is queryable; runtime Sample Rate writes are unsupported. A newly configured source session may choose another rate with an explicit new association/scene phase epoch. Original IQ Generator v1 retains its existing writable Sample Rate behavior. No sweep-state wire protocol is added. |
| Combined controls | Reject any command containing both Sample Rate and RF Reference Frequency writes before any side effect, even when partial execution is requested. The example changes only center frequency during a sweep. |

Use a small profile schema for state identities/indices, writable/queryable/published/required masks, validators and Context inclusion. Five persistent fields may share storage with v1 if measured/accounted and profile-aware; do not prescribe global permission changes. Keep the existing four-field per-command execution limit separate from persistent-state capacity. Preserve old public APIs; give the example typed setters and named query/cancel selection.

The initial separate-process examples are explicitly isolated localhost demonstrations: IPv4 `127.0.0.1` only, using three consecutive lane ports. `--local-base-port` and `--peer-base-port` accept decimal values1–65533 and reject overlapping lane ranges. Controller defaults are41000/42000; Controllee defaults are42000/41000. External-address CLI operation is not enabled with fixture identities or simulated PPS. A deployment must configure its real identities, endpoints and clock binding separately. A Controller sample-rate option describes its expected remote session configuration; it never writes the remote Sample Rate.

Fresh-association recovery may preserve a nonzero absolute sample ordinal. An explicitly created recovery scene starts a new zero-phase epoch at Runtime's first known initial boundary, without guessing earlier phase. Default scene creation still begins at ordinal zero, and ordinary retunes never reset phase. The [contract addendum](implementation/P16-contract.md#recovery-scene-epoch-clarification) specifies stable-address replacement during confirmed recovery.

Package order: (1) independently approved profile/state contract; (2) shared state/runtime/Context/backend extension and gate; (3) virtual RF scene; (4) Controller policy/CLI and combined loopback example; (5) separate-process UDP gate when enabled; (6) documentation, measured memory ledger and affected regressions. Only independent work may proceed in parallel. Budget/sizeof evidence must be regenerated for changed objects; a full performance requalification is not automatically required unless changes invalidate measured claims.

## Existing architecture and scope

Read the current architecture, protocol appendix, implementation plan and M5 operational scope, plus the existing examples and runtime interfaces:

- `docs/vita49_framework_architecture.md`
- `docs/vita49_protocol_design.md`
- `docs/implementation/M5-operational-scope.md` and `docs/implementation/status.md`
- `examples/controller.cpp`, `examples/controllee.cpp`, `examples/combined.cpp`
- `include/vita/runtime/public/runtime.hpp`
- `include/vita/runtime/transaction/backend.hpp`
- The transaction engine, state/revision, Context publication/history and IQ source implementations.

The generic codec already defines `RFReferenceFrequency` at CIF0/bit27, using `Hertz`. The existing IQ profile permits Sample Rate as its only writable standard field. Consequently, this is an additive profile/runtime capability, not just an example calling an existing tuning API.

Implement the explicitly selected, wire-visible Tunable IQ v1 profile specified above. Preserve the original IQ Generator v1 permissions and behavior. Define RF Reference Frequency as the example's RF center, with zero IF and zero RF offset for this profile. Keep the tunable profile sample rate fixed for its source session, as specified above. Do not reinterpret Sample Rate as frequency or add arbitrary SDR setters. Array-of-CIFs remains excluded and is not required for this example.

Document the profile/class selection and compatibility behavior. Reuse the repository's isolated fixture identity conventions for local demonstrations; require configured identities for external operation. Do not present fixture codes as assigned production identifiers.

## Controller application

Provide a readable application demonstrating setup, target binding, command submission, asynchronous observations, readback and shutdown through public framework APIs. Add a typed center-frequency setter and named query selection where needed; application code must not build raw VRT command words or depend on internal field-array positions.

Provide CLI options for start frequency, stop frequency, positive step, dwell time, command timeout and number of sweeps. Use checked frequency/time conversions. Define ascending inclusive traversal: emit start+n*step while within stop; do not append an irregular last step when stop is off-grid. A one-point range is valid. Reject reversed ranges, zero step, overflow, invalid durations and unsupported frequencies. Use a finite default; expose continuous repeat explicitly and handle Ctrl-C cleanly.

Use one tune transaction in flight by default. For each point:

1. Submit a center-frequency command through the normal transport/transaction path.
2. Observe validation and execution outcomes distinctly. AckV or queue acceptance is not proof of tuning.
3. Establish confirmed applied frequency through supported execution/state evidence; report requested versus actual frequency if quantization is supported.
4. Start the dwell only after execution evidence for the defined effective-and-usable completion, with required applied-state readback confirmed. Use local monotonic time for Controller dwell/deadlines; do not subtract remote timestamps from the local clock.
5. Advance to the next point and repeat the configured number of sweeps.

Log the transaction identity, requested/applied frequency, result, locally measured command latency, dwell and sweep progress. Explain timeout uncertainty: a timeout does not prove the device did not tune. Default to stopping on rejection, unknown outcome or failed settling. Any retry/reconciliation policy must be bounded, documented and consistent with existing duplicate/cancellation contracts. Do not continue on guessed device state.

Shutdown must stop new submissions and use existing cancellation, drain and quiescence APIs. A shutdown deadline expiring must not authorize unsafe reclamation.

## Controllee and command processing

Route commands through the existing parser, identity/routing checks, profile validation, bounded admission, scheduling, backend submission, completion and acknowledgement machinery. Do not bypass this path with a direct generator setter, and do not create an example-specific protocol loop.

Extend the necessary state, query, revision and Context contracts for the opt-in profile. Include RF center frequency in its initial/full and changed Context. Apply virtual tuning at a well-defined packet boundary; preserve sample ordinal/time progression, and ensure each emitted packet has one coherent configuration. Publish or gate the matching Context according to existing rules before associated Data is released. Preserve old snapshots and in-flight buffers. Unknown required tuning state must follow an explicit validity/recovery policy.

Before coding, trace and document every Sample-Rate-only assumption affected by this addition, including field masks, state-array capacities, serialization, readback, cancellation, backend validation, revisions and Context history. Make the smallest coherent extension; avoid a broad unrelated runtime redesign. Preserve bounded pools and hot-path allocation guarantees, and account for any state/pool size changes.

## Meaningful virtual tuning

Make tuning observable in generated IQ, not just a changing log or metadata field. Supply a deterministic synthetic RF scene with one or a small fixed set of tones at fixed absolute RF frequencies. The selected center frequency translates an in-band tone to baseband at `tone_rf - center_rf`.

Use the passband and phase contract above. Suppress tones outside the modeled passband instead of aliasing them into the observed band. This simple virtual passband is not a model of a real RF filter. Keep amplitudes bounded and define phase behavior across tune boundaries so replay is deterministic.

Provide a simple documented demonstration that the fixed RF tone appears at the expected baseband offset as the Controller scans past it. A deterministic test or lightweight observation is sufficient; FFT-based detection, a spectrum UI, realistic noise and production search algorithms are not required. Describe this as a tuning/sweep example, not a validated signal detector.

Use the existing clock interfaces. Normal demonstration pacing follows wall-clock/monotonic progress; deterministic tests inject clock advancement without real sleeps. Clearly label the lab clock/PPS source. Do not fabricate a qualified GPS clock or claim hardware timing accuracy.

## Future SDR boundary

Keep scan policy in the Controller and device-specific tuning in a replaceable backend binding. Document how a future SDR implementation would supply supported range/step, quantization, submission, lock/settling evidence, actual applied frequency, failure/unknown-state reporting, cancellation limits and quiescence.

Distinguish an SDK/register write completing from RF tuning becoming effective and usable samples becoming available. The virtual backend uses the effective-and-usable completion contract above. Completion, revision validity, Context publication and Data release must agree on the same boundary; delaying a callback alone is insufficient. A future backend can delay completion until its actual lock/settling criteria are satisfied and must account for invalid samples during transitions. Do not assume a synchronous SDK return proves RF lock, or that cancellation can undo an already applied tune. No vendor SDK, physical adapter or M6 hardware qualification is required now.

## Deliverables and documentation

Provide:

- A Controller scan example and corresponding virtual tunable Controllee setup.
- A combined deterministic loopback demonstration for easy execution and CI.
- When `VITA_BUILD_POSIX_UDP=ON`, separate-process Controller/Controllee operation using the existing POSIX UDP adapter, with build/run instructions and independently verified remote endpoint wiring. When disabled, the combined example remains buildable and UDP targets/tests are omitted. Do not add a new transport or simulate a remote device with a local Controllee in the Controller app.
- A focused README with exact commands, a short example trace, supported profile options, failure behavior and limitations.
- A sequence diagram explaining Controller -> VRT Control -> Controllee validation/admission -> backend -> completion -> acknowledgement/Context/Data.
- A porting guide identifying the specific callbacks/types a real SDR backend replaces and which framework responsibilities remain unchanged.
- Updated profile, coverage and implementation/verification reports reflecting this explicitly authorized new tuning capability.

Comment the example at the points where readers need to understand transaction lifetime, acknowledgement meaning, clock domains, metadata ordering and backend responsibility. Favor a short readable main flow with well-named helpers over an opaque generic demo harness. Integrate targets and tests with existing CMake conventions.

## Independent acceptance

Verify at least:

- Sweep endpoints, off-grid stop, single-point range, repetition and CLI arithmetic errors.
- Independent expected RF Reference Frequency wire words, query/readback and applied-state publication; do not rely solely on encoder/decoder round trips.
- Original IQ v1 still rejects RF tuning, while the opt-in profile accepts it.
- No advance/dwell triggered by validation acknowledgement alone; execution, settling, timeout uncertainty and shutdown are handled as documented.
- Unsupported tuning, delayed completion, duplicate delivery, cancellation races and backend failure/unknown state without extra device effects or fabricated success.
- Packet-boundary tuning, matching Context/revision association, retained-buffer lifetime and unchanged sample rate/timeline.
- Synthetic-tone offset and out-of-band suppression at multiple tuning points and boundary cases; skipped source callbacks spanning several effective retunes must preserve the piecewise LO phase history, not advance the whole gap at the newest center.
- No new hot-path allocation or unexplained resource-budget growth.
- Loopback and, when `VITA_BUILD_POSIX_UDP=ON`, separate-process localhost UDP operation; localhost evidence must not be described as cross-machine or hardware qualification.

Run appropriate affected regressions and sanitizer checks, including shared transaction, Context, timing and lifecycle paths. Preserve results and source identity in verification reports. End with runnable commands, implemented behavior, verification results and explicit remaining deployment/hardware limitations.
