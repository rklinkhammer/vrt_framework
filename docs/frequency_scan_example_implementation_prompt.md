# Implementation prompt: controller-driven frequency scan

Implement a well-documented example in this repository in which a Controller repeatedly tunes an IQ generator across a configured range of RF center frequencies. This represents a narrowband receiver sweeping a wider frequency range. The main teaching objective is how to write a Controller application; the Controllee command-processing path must also serve as a practical template for a future real SDR backend.

Use implementer and independent verifier agents. Parallelize only independent work, maintain implementation/verification reports, and preserve unrelated changes. Implement and verify the example, rather than stopping with a design proposal. Inspect applicable repository instructions first. Stop only at an unresolved wire/protocol decision or genuinely unavailable required input; lack of physical SDR hardware must not block the virtual example.

## Existing architecture and scope

Read the current architecture, protocol appendix, implementation plan and M5 operational scope, plus the existing examples and runtime interfaces:

- `docs/vita49_framework_architecture.md`
- `docs/vita49_protocol_design.md`
- `docs/implementation/M5-operational-scope.md`
- `examples/controller.cpp`, `examples/controllee.cpp`, `examples/combined.cpp`
- `include/vita/runtime/public/runtime.hpp`
- `include/vita/runtime/transaction/backend.hpp`
- The transaction engine, state/revision, Context publication/history and IQ source implementations.

The generic codec already defines `RFReferenceFrequency` at CIF0/bit27, using `Hertz`. The existing IQ profile permits Sample Rate as its only writable standard field. Consequently, this is an additive profile/runtime capability, not just an example calling an existing tuning API.

Introduce an explicitly selected frequency-tunable IQ example profile or documented profile version. Preserve the original IQ Generator v1 permissions and behavior. Define RF Reference Frequency as the example's RF center, with zero IF and zero RF offset for this profile. Keep the existing sample rate fixed during a sweep. Do not reinterpret Sample Rate as frequency or add arbitrary SDR setters. Array-of-CIFs remains excluded and is not required for this example.

Document the profile/class selection and compatibility behavior. Reuse the repository's isolated fixture identity conventions for local demonstrations; require configured identities for external operation. Do not present fixture codes as assigned production identifiers.

## Controller application

Provide a readable application demonstrating setup, target binding, command submission, asynchronous observations, readback and shutdown through public framework APIs. Add a typed center-frequency setter and named query selection where needed; application code must not build raw VRT command words or depend on internal field-array positions.

Provide CLI options for start frequency, stop frequency, positive step, dwell time, command timeout and number of sweeps. Use checked frequency/time conversions. Define ascending inclusive traversal: emit start+n*step while within stop; do not append an irregular last step when stop is off-grid. A one-point range is valid. Reject reversed ranges, zero step, overflow, invalid durations and unsupported frequencies. Use a finite default; expose continuous repeat explicitly and handle Ctrl-C cleanly.

Use one tune transaction in flight by default. For each point:

1. Submit a center-frequency command through the normal transport/transaction path.
2. Observe validation and execution outcomes distinctly. AckV or queue acceptance is not proof of tuning.
3. Establish confirmed applied frequency through supported execution/state evidence; report requested versus actual frequency if quantization is supported.
4. Start the dwell only after confirmed execution and the declared settling condition. Use local monotonic time for Controller dwell/deadlines; do not subtract remote timestamps from the local clock.
5. Advance to the next point and repeat the configured number of sweeps.

Log the transaction identity, requested/applied frequency, result, locally measured command latency, dwell and sweep progress. Explain timeout uncertainty: a timeout does not prove the device did not tune. Default to stopping on rejection, unknown outcome or failed settling. Any retry/reconciliation policy must be bounded, documented and consistent with existing duplicate/cancellation contracts. Do not continue on guessed device state.

Shutdown must stop new submissions and use existing cancellation, drain and quiescence APIs. A shutdown deadline expiring must not authorize unsafe reclamation.

## Controllee and command processing

Route commands through the existing parser, identity/routing checks, profile validation, bounded admission, scheduling, backend submission, completion and acknowledgement machinery. Do not bypass this path with a direct generator setter, and do not create an example-specific protocol loop.

Extend the necessary state, query, revision and Context contracts for the opt-in profile. Include RF center frequency in its initial/full and changed Context. Apply virtual tuning at a well-defined packet boundary; preserve sample ordinal/time progression, and ensure each emitted packet has one coherent configuration. Publish or gate the matching Context according to existing rules before associated Data is released. Preserve old snapshots and in-flight buffers. Unknown required tuning state must follow an explicit validity/recovery policy.

Before coding, trace and document every Sample-Rate-only assumption affected by this addition, including field masks, state-array capacities, serialization, readback, cancellation, backend validation, revisions and Context history. Make the smallest coherent extension; avoid a broad unrelated runtime redesign. Preserve bounded pools and hot-path allocation guarantees, and account for any state/pool size changes.

## Meaningful virtual tuning

Make tuning observable in generated IQ, not just a changing log or metadata field. Supply a deterministic synthetic RF scene with one or a small fixed set of tones at fixed absolute RF frequencies. The selected center frequency translates an in-band tone to baseband at `tone_rf - center_rf`.

Define the simulated receive bandwidth and Nyquist-edge convention explicitly. Suppress tones outside the modeled passband instead of aliasing them into the observed band. This simple virtual passband is not a model of a real RF filter. Keep amplitudes bounded and define phase behavior across tune boundaries so replay is deterministic.

Provide a simple documented demonstration that the fixed RF tone appears at the expected baseband offset as the Controller scans past it. A deterministic test or lightweight observation is sufficient; FFT-based detection, a spectrum UI, realistic noise and production search algorithms are not required. Describe this as a tuning/sweep example, not a validated signal detector.

Use the existing clock interfaces. Normal demonstration pacing follows wall-clock/monotonic progress; deterministic tests inject clock advancement without real sleeps. Clearly label the lab clock/PPS source. Do not fabricate a qualified GPS clock or claim hardware timing accuracy.

## Future SDR boundary

Keep scan policy in the Controller and device-specific tuning in a replaceable backend binding. Document how a future SDR implementation would supply supported range/step, quantization, submission, lock/settling evidence, actual applied frequency, failure/unknown-state reporting, cancellation limits and quiescence.

Distinguish an SDK/register write completing from RF tuning becoming effective and usable samples becoming available. The virtual backend must specify which event its completion represents. Do not assume a synchronous SDK return proves RF lock, or that cancellation can undo an already applied tune. No vendor SDK, physical adapter or M6 hardware qualification is required now.

## Deliverables and documentation

Provide:

- A Controller scan example and corresponding virtual tunable Controllee setup.
- A combined deterministic loopback demonstration for easy execution and CI.
- Separate-process Controller/Controllee operation using the existing optional POSIX UDP adapter, with build/run instructions; do not add a new transport.
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
- Synthetic-tone offset and out-of-band suppression at multiple tuning points and boundary cases.
- No new hot-path allocation or unexplained resource-budget growth.
- Both loopback and localhost UDP operation; localhost evidence must not be described as cross-machine or hardware qualification.

Run appropriate affected regressions and sanitizer checks, including shared transaction, Context, timing and lifecycle paths. Preserve results and source identity in verification reports. End with runnable commands, implemented behavior, verification results and explicit remaining deployment/hardware limitations.
