# Accepted Decision 1: IQ Generator Profile v1

Status: accepted following user direction. This defines the initial application scope and baseline defaults, together with accepted Decisions 2–10 in the architecture review. Deployment values and detailed architecture deliverables explicitly identified below remain open. The filename is retained to preserve existing links.

## Scope and meaning of support

The application generates complex time-domain IQ samples. The reusable framework supports every packet type defined by ANSI/VITA 49.2-2017 (R2024), including the standard Command subtypes and extension packet envelopes.

“All VRT packets” means all eight assigned packet-type codes, 0x0–0x7, with their legal header options and subtype layouts. Codes 0x8–0xF remain reserved and are not emitted as valid packet types. Signal Time and Signal Spectral are distinguished within Signal Data; Control, Acknowledge, and cancellation use the Command type and associated indicators. See Table 5.1.1-1, Table 5.1.1.1-1, and Sections 6–8.

Support has three separate dimensions:

- **Wire support:** bounded encoding, decoding, validation, and dispatch for every defined packet family. Standard CIF fields, attributes, and structured values have codecs independent of application callbacks. Implementation milestones may introduce these incrementally, but a claim of complete standard-field support requires the coverage matrix to be complete.
- **Application behavior:** generate IQ, publish its context, and accept/query/cancel supported generator controls. Receiving a valid packet does not imply that the generator can execute every possible control or consume every signal representation.
- **Extension support:** encode/decode common envelopes and dispatch payloads by registered class. Unknown extension payloads can be exposed as bounded opaque views; their internal semantics cannot be validated without the class definition. Opaque handling is not full semantic support for arbitrary vendor extensions or the classes listed in Section 11.

Accepted Decision 8 requires separate coverage for general wire codecs, generator control semantics, and optimized sample codecs, with explicit CIF/attribute/array bounds. Wire-format views, conversion accessors, and caller-provided conversion storage are separate APIs. The IQ formats below define optimized generator output; zero-copy wire access does not imply native typed sample access. Unknown ordered-field layouts must not be skipped by guessing their sizes. Concrete codec limits and coverage remain architecture deliverables.

## Packet support matrix

| Packet type | Framework encode/decode | IQ generator behavior |
|---|---|---|
| 0x0 Signal Data without SID | Yes, including time/spectral distinction | Available for an explicitly configured standalone data-only binding; not the normal paired IQ stream |
| 0x1 Signal Data with SID | Yes, including time/spectral distinction | Emits complex time-domain IQ; spectral generation is outside this application's scope |
| 0x2 Extension Data without SID | Envelope plus registered payload codec | Available to extension providers on an unambiguous binding; no default emission |
| 0x3 Extension Data with SID | Envelope plus registered payload codec | Available to extension providers; no default emission |
| 0x4 Context | Yes | Publishes IQ format, sample rate, and applicable signal state |
| 0x5 Extension Context | Envelope plus registered payload codec | Available for genuinely additional metadata; no default emission |
| 0x6 Command | Control, AckV, AckX, AckS, cancellation and cancellation acknowledgements | Handles generator controls and queries; framework Controller API supplies the complementary initiating behavior |
| 0x7 Extension Command | Common command machinery plus registered class semantics | Dispatches registered custom controls; no invented default vendor command set |

Every row receives wire fixtures and malformed-input coverage. A packet type need not be emitted periodically to be supported. Extension classes are registered only when a concrete payload meaning is needed; standard IQ, context, and controls remain in standard packets (§§6.4, 7.2, 8.6).

## Information Class

| Component | Accepted definition |
|---|---|
| Name/version | `VRT Framework IQ Generator v1` |
| Purpose | Generate synthetic or application-provided complex baseband samples and convey the information needed to interpret and control that output |
| Information Class code | Project-local `0x0001`, scoped to the configured profile OUI; not a globally assigned value |
| OUI | Required deployment configuration from an authorized organization/profile namespace; do not appropriate VITA's OUI or claim an experimental value is globally unique |
| Streams | `iq.data`, `iq.context`, and `iq.command` for each logical generator; acknowledgements use the corresponding command relationship |
| Reference/control point | Logical sample-generation output immediately before packetization; effective control times refer to the sample timeline at this point |
| Association | Data, paired Context, and paired Command use the same SID; packet type distinguishes their roles |
| Identifiers | Explicit 32-bit Controller and Controllee IDs in the baseline command profile, assigned through configuration; framework also supports UUID forms |
| Discovery | Static configuration initially: peer, transport binding, profile/class mapping, SID, and endpoint IDs |

Use project-local Packet Class codes `0x0001` for IQ16 Data, `0x0002` for IQ32 Data, `0x0003` for IQ float32 Data, `0x0010` for IQ Context, and `0x0020` for generator Command. These are project-local assignments within the configured OUI and Information Class, not standard VITA allocations. Register concrete extension classes separately.

Accepted Decision 6 scopes routing locally while preserving wire SID pairing; transaction correlation includes Message ID, SID, endpoint identifiers, and peer/session context. Reject ambiguous registrations, coordinate packet counters by outgoing SID/packet type, and document bounded duplicate retention and Message ID reuse. The 4-bit count is only a limited diagnostic; this profile does not select VRL.

Normal profile traffic includes Class ID. The generic framework accepts legal omission; the generator accepts omitted Class ID only where the binding has an explicit, unambiguous class mapping. Unsupported classes are reported and not interpreted as this profile. A standalone SID-less data class must be documented separately and cannot participate in SID-based pairing.

References: §4.1; §§5.1.2–5.1.3; §§8.2.3–8.2.6; §10.1.3; §10.2 and Table 10.2-1. Final deployment class documentation must supply the configured identifier values and all remaining options before interoperability sign-off.

## IQ output defaults

These are accepted software/profile defaults, not requirements imposed by VITA:

| Property | Default |
|---|---|
| Source | Deterministic complex sinusoid, with application sample-provider replacement |
| Test waveform | Amplitude 0.5 of normalized full scale, positive frequency `sample_rate / 16`, initial phase zero; phase continues across packet boundaries |
| Sample rate | 1,000,000 complex samples/s; baseline accepts integer rates from 1 through 100,000,000 samples/s, subject to configured resource admission |
| Default format | Complex Cartesian signed normalized 16-bit I followed by 16-bit Q; `I0,Q0,I1,Q1,...` |
| Additional formats | Signed normalized 32-bit I/Q and IEEE-754 float32 I/Q, selected before starting a stream |
| Scaling | Signed integer values divided by 2^15 or 2^31 respectively; float values use normalized units. Saturate integer conversion; do not wrap |
| Packing | Processing-efficient, no event/channel tags, no sample-component repetition, vector size one; serialize words big-endian |
| Packet payload | Default 256 complete complex samples; reduce to fit the transport's packet budget; never split an I/Q pair |
| Padding | No pad bits for these complete-pair layouts; report pad count zero |
| Trailer | Omitted in the baseline class; optional trailer use requires a documented class variant |
| Transport | Loopback for deterministic validation; UDP as the first external adapter, with one VRT packet per datagram and an explicit MTU budget |

The general codec still supports other legal packing and trailer options; they are not part of the baseline IQ generator classes. Data Packet Payload Format must describe the actual selected format, including size/count fields encoded as value-minus-one where required. See §6.1.1 and §9.13.3, especially Rules 9.13.3-9 and 9.13.3-12 through 15.

This is a source application: no RF tuner, DAC, receiver, FFT, or physical power calibration is required. Incoming Signal Data can be routed to framework consumers/test sinks, but does not feed the generator by default.

## Time and context

For the deterministic baseline, use TSI=`Other` with a documented epoch at the logical generator session's start and TSF=real-time picoseconds. Integer seconds and fractional picoseconds derive from the generated sample timeline, not packet send time. This is simulated time and makes no UTC synchronization claim. Data and paired Context use identical TSI/TSF settings; Context uses TSM=0. A new session requires explicit reset coordination with peers; do not silently reuse a live time epoch.

A deployment requiring UTC or GPS supplies a documented clock binding and separate class configuration. Accepted Decision 3 requires framework-owned scheduling, a device timing-capability contract, and optional prepare/arm hooks. Concrete precision, rounding, clock-discontinuity behavior, and timing windows remain deployment configuration. Picosecond representation does not imply picosecond execution accuracy. See §§5.1.5.3, 5.1.5.5, and 7.1.1–7.1.3.

Publish Sample Rate, Data Packet Payload Format, Reference Point Identifier, and applicable State/Event Indicators. The reference point is the generator output identified by its SID. Do not fabricate RF frequency, RF bandwidth, or calibrated dBm values for a purely digital source.

Emit a full Context snapshot before initial Data submission, after a configuration change before submitting affected Data, and every one second of active stream time. Emit change updates at the effective sample timestamp. Transport reordering or loss can still deliver Data first. Accepted Decision 7 requires a receiver context cache with validity/effective-time information, explicit unknown state, and periodic refresh recovery. Cache/history limits and the choice to wait, deliver with unknown/stale metadata, or drop remain architecture/profile deliverables. Nonpersistent event fields must not be treated as persistent state.

## Controls and lifecycle

The baseline standard writable control is Sample Rate. A change applies at a packet boundary while preserving the accumulated sample timeline and oscillator phase; the test oscillator's frequency follows `sample_rate / 16`. Query responses can report the applicable published fields. Data format and source selection are local pre-start configuration in v1.

Support no-action query, dry run, execution, requested AckV/AckX/AckS, NACK-only behavior, and cancellation of pending supported controls. Parse all defined CAM modes; execute only when their requirements and the advertised device capabilities can be met. Decision 2 establishes virtual-model validation and register-write completion for real hardware; the software generator uses the corresponding committed model-state update. Decision 3 establishes the scheduling architecture; numerical timing guarantees remain to be specified. Accepted Decision 4 requires the explicit CAM response matrix, distinct packet layouts, and derived state-machine fixtures. Accepted Decision 5 requires per-field cancellation state and masks, a documented device cutoff, and separation of local cancellation from wire cancellation. Concrete race handling and retention bounds remain architecture deliverables. Unsupported controls receive field-specific diagnostics where the packet is structurally understood and the CAM requests a response; unknown layouts must not be guessed.

Local runtime APIs start and stop generation. Remote start/stop or waveform selection can be added through a documented extension class if no standard field accurately represents the intended behavior; do not repurpose an unrelated field merely to avoid an extension.

## Packaging and runtime defaults

Accepted Decision 9 requires a header-only codec, semantic types, and generic runtime core, with optional compiled transport/device adapters and their dependencies. Toolchain and platform policy remain architecture deliverables.

Accepted Decision 10 uses move-only external leases, callback-scoped views with explicit retention, per-Controllee serialization as the default control policy, asynchronous device completion, and reserved control/completion capacity across queues, pools, and transports. Nonblocking Controller calls from callbacks are supported; synchronous waits on the same execution domain must be prohibited or detected. Capacity values and performance guarantees require an explicit deployment envelope and measurements. The sample-rate and payload defaults in this profile are accepted configuration defaults, not benchmark results.

## Acceptance and remaining closure

Decision 1 adopts this scope: **IQ-only application, complete defined VRT packet-family support in the framework, explicit semantic support for generator fields, and registered extension payloads.** This does not claim that all device commands, all vendor formats, or all spectral applications are implemented.

Before declaring the profile deployable, finish the OUI assignment/configuration, complete Packet Class option tables, the standard-field coverage matrix, resource admission limits, and the detailed timing/transaction deliverables already tracked in the review. The accepted 100 MS/s upper configuration limit is not a demonstrated throughput guarantee.
