# Architecture prompt validation

Reviewed 2026-09-16 for `/Users/rklinkhammer/workspace/vrt_framework`.

Input: `VITA49.2_High_Performance_Framework_Architect_Prompt.md`.
Reference: `/Users/rklinkhammer/Downloads/AV49DOT2-2017-R2024.pdf`, ANSI/VITA 49.2-2017 (R2024), including the September 2017 errata identified on its cover. Page references below use printed page numbers; add 16 for the PDF page number.

## Assessment

All ten recommendations are now accepted and incorporated into the architecture brief. The complete architecture and deployment-specific interoperability documentation remain deliverables. Many decisions are deliberately delegated to the architect through “define,” “determine,” and “recommend.” Those are appropriate assignments, not omissions. The principal gaps are protocol-specific constraints that should narrow those assignments, a packaging contradiction now resolved by Decision 9, and missing deployment/performance inputs.

The buffer separation, completion-based reclamation, transport independence, bounded resources, framework-owned protocol machinery, and separation of software choices from protocol requirements are good foundations. None needs to be reversed on the basis of the reviewed reference.

This is a targeted architectural review of relevant clauses, not an exhaustive verification of every field in the 355-page PDF or a conformance certification. Recommendations below remain proposals unless explicitly marked accepted. Decisions 1 through 10 are accepted following user direction and incorporated into the architecture prompt. Acceptance settles the recommended approaches; it does not imply that detailed designs, tests, or deployment values have already been produced.

## Accepted decisions and remaining implementation detail

### 1. Define a concrete initial interoperability profile

**Accepted scope:** an IQ-generating application with framework support for every defined VRT packet type. See [IQ Generator Profile v1](iq_generator_profile_proposal.md) for the accepted packet support matrix, baseline IQ formats, stream/class assignments, context, and controls. Deployment values and detailed timing contracts remain open.

**Status: accepted following user direction. Prompt locations: Role, Accepted initial application profile, Required architectural output item 2.**

An application-profile feature list alone is insufficient. Section 4.1 applies compliance to Information Streams and requires their Information Class documentation. Section 10.2 and Table 10.2-1 require eight components: class name/code, purpose, stream names/purposes, packet classes, stream details, reference points, and associations. Sections 10.2.5–10.2.8 require the packet options and field meanings to be specified.

Use the accepted profile for the initial use case, class codes, OUI policy, packet coverage, fields, sample formats, time basis, reference point, associations, and Class ID handling. Complete its Packet Class option tables, deployment OUI/identifier values, resource bounds, and detailed CAM/timing contracts. Discovery and identifier provisioning need an explicit mechanism; VITA 49.2 does not supply one (§8.2.6, Observation 8.2.6-2).

**Accepted approach:** use the versioned IQ Generator Profile v1 with a support matrix distinguishing encode, decode, execute, and publish support. Preserve the separation between IQ-only application behavior and complete defined VRT packet-family support in the framework. Extensions should represent information unavailable in standard packets, not replace standard controls (§§5, 7.2, 8.6). Do not assume that interoperability with an arbitrary “VITA-compliant” device follows from correct packet parsing.

### 2. Validate with a virtual hardware model; complete updates on register writes

**Status: accepted following user direction. Prompt locations: Command callback architecture; Command/Acknowledge transaction engine; Testability.**

A virtual hardware model provides deterministic validation of controls, packet parameters, and protocol responses for the supported profile. It models parameter constraints, register state, and execution outcomes so the framework can be tested without physical hardware. It must cover invalid and unsupported requests as well as successful updates. Validation and dry runs must not commit changes to live device state; simulation may use an isolated copy of model state.

Real-hardware validation has a different scope. The framework validates packet structure, profile support, and known parameter constraints before execution. Device adapters may check additional constraints that are available without significant signal analysis. Successful execution of an update means that the required registers have been written according to the adapter's documented write-completion contract. Submission to an asynchronous write queue alone is not completion. Register readback is not universally required by this decision.

Register-write success does not establish that the device produced the intended RF/IQ behavior, settled correctly, or met signal-quality requirements. Establishing those properties may require substantial signal analysis and belongs to overall system testing. AckV reports the available validation result; a successful AckX reflects the defined register-write execution result, subject to CAM and timing obligations. AckS reports available device/model state and must not present an unmeasured physical effect as a measurement.

**Architectural consequences:** use a backend contract that can be implemented by both virtual registers and a real device adapter. Keep validation, execution, and completion distinct; do not require the real-hardware path to run a full simulator. Record per-control write failures and partial completion. A later failure does not undo earlier writes automatically. Requested, accepted, written, and measured values should remain distinguishable where they differ.

**Remaining details, not a reopening of this decision:** specify cross-field validation, handling of stale validation before scheduled execution, multi-register partial failures, and each adapter's completion mechanism. The accepted Decision 3 requires reconciliation of the register-write completion point with the chosen VITA reference point and timing claims; writing a staging register early does not establish that a future signal change has already taken effect.

Protocol basis: Table 8.3.1.2-1 and Documentation Rule 8.3.1.2-1 govern partial execution and adjusted values; Table 8.3.1.3-1 governs dry run. Sections 8.3.1.7 and 8.4.1.5 govern reference-point and acknowledgement timing. The virtual model and register-write success criterion are project architecture choices, not universal VITA requirements.

### 3. Schedule effects at the reference point, not just callback invocation

**Status: accepted following user direction. Prompt location: Scheduled commands and time.**

Rule 8.3.1.7-1 and its discussion, pp. 105–106, define the timestamp as when controls become effective at the reference point. Device and signal-processing delays matter. Dispatching a setter at that instant can therefore be too late. Table 8.3.1.7-1 defines modes 0–4 and distinguishes device precision from application early/late windows; a single generic “late-command policy” is insufficient.

The device timing-capability contract shall declare clock domains and supported conversions, device precision and application early/late windows, minimum scheduling lead time, loss-of-sync behavior, and support for staging/arming future hardware actions. The architecture shall specify how simultaneous controls take effect together and how the effective time is established, including whether it is measured or estimated. AckV, execution Ack, and AckS have distinct timestamp meanings (§8.4.1.5, p. 118).

**Accepted approach:** framework-owned scheduling with a device timing-capability contract and optional prepare/arm hooks. Software timers are acceptable only within their documented achievable timing window. A driver may implement hardware timing without taking over protocol scheduling. Reject controls whose required timing cannot be met, respecting CAM partial-execution and response rules; do not silently relax the requested timing mode.

**Relationship to Decision 2:** successful register writes remain the hardware update success criterion. Document whether the VITA reference point is the register/control bank or a downstream signal point. An early staging write is preparation, not proof of a later timed effect. If the reference point is downstream, the adapter must establish the applicable timing through its documented device timing model or completion evidence; it need not perform signal analysis for every command. Verification of physical signal behavior remains system testing.

**Remaining configuration:** concrete clock bindings, timing-window values, lead times, rounding rules, and clock-discontinuity policies must be specified for each deployment. Acceptance of this architecture does not assign numerical timing guarantees.

### 4. Make CAM and acknowledgement behavior an explicit matrix

**Status: accepted following user direction. Prompt locations: transaction engine, Controller API.**

The required CAM response matrix shall explicitly cover action modes, all partial/warning/error combinations, NACK-only responses, ReqV/ReqX/ReqS, detail-request flags, SchX/AckP, and timing status. Acknowledgements each contain exactly one subtype, while one control can request several (§8.4.1.1). The no-action query carries CIF selectors but no control values (§8.3, Rule 8.3-1). AckS reflects selected state, with post-action ordering where applicable (§8.4.2).

The Controller API shall define explicit completion states for no-Ack and NACK-only requests. Silence cannot demonstrate successful execution over an unreliable transport. Distinguish local send completion, validation, execution, state observation, timeout with unknown remote outcome, and cancellation.

**Accepted approach:** require a truth table and corresponding state-machine fixtures, including dry-run responses and query-only wire encoding. Share field descriptors but use distinct Control, query-selector, AckV/AckX diagnostic, and AckS layouts. Warning/error fields are 32-bit diagnostics, not copies of the controlled field's semantic type (§8.4.1.2).

**Required architectural output:** supply the matrix with clause references, response obligations and suppression conditions, ordering, diagnostic inclusion, and Controller-visible outcomes. Identify invalid/reserved combinations. Derive fixtures for valid cases, malformed combinations, and permitted response suppression. The matrix and fixtures remain deliverables; accepting this decision does not mean they have already been produced. Preserve the AckV/AckEr specification conflict in the interpretation register rather than silently resolving it through this acceptance.

### 5. Model cancellation per field and distinguish it from local cancellation

**Status: accepted following user direction. Prompt location: transaction engine.**

Section 8.5, pp. 119–122, cancels selected unexecuted controls. It reuses the original Stream ID, Controller/Controllee identifiers, and Message ID; its CIFs are the original selection or a subset. It has timing semantics, no control values, and no AckV response. If an acknowledgement is requested, AckX is the minimum, with optional AckS. Partial cancellation is supported; the discussion accompanying Figure 8.5-3 prohibits Ctrl-P=0.

The architecture shall specify the cancellation/execution race boundary, handling after hardware arming, retained state for completed fields, and correlation of cancellation responses versus original responses. A transaction-wide boolean is insufficient.

**Accepted approach:** per-field state and cancellation masks, with a documented point beyond which the device cannot cancel. Keep local wait cancellation and timeout distinct from an actual wire cancellation. Cancellation is optional in the standard, but it is explicitly within this prompt's intended framework scope.

**Required detail:** the device adapter declares its cancellation cutoff and disarm capabilities. The framework coordinates cancellation with execution so each selected field has an unambiguous outcome. Cancellation cannot undo completed register writes. Retention bounds and concrete race handling remain architecture deliverables, with deterministic tests covering preparation, arming, execution, and late requests.

### 6. Specify identity, pairing, counter ownership, and retry semantics

**Status: accepted following user direction. Prompt locations: Stream abstraction; transaction engine.**

Paired Data and Context streams share a Stream ID (Rule 7.1.2-2); paired Command/Data streams do too (Rule 10.1.3-1). Distinct packet types sharing an SID are not automatically collisions. Packet Count increments modulo 16 for consecutive packets with the same SID and packet type (Rule 5.1.1-9). Recommendation 5.1.1-1 warns it is unreliable for loss/reorder detection at high rates on transports such as UDP.

The architecture shall define the routing namespace across transport bindings, identifier omission rules, allocation authority, response destinations, and ownership of counters when logical senders share a wire stream. Specify Message ID uniqueness duration, wrap/restart behavior, duplicate retention limits, and whether retransmission is supported. Message ID correlation (§§8.2.3–8.2.6) does not itself guarantee exactly-once execution.

**Accepted approach:** distinguish a local routing namespace from wire identifiers; key transaction state with Message ID, the relevant SID and endpoint identifiers, and local peer/session context. Reject ambiguous registrations. Treat the packet count as a limited diagnostic; stronger loss detection requires additional transport/profile assumptions. If VRL is selected, obtain and validate the separate VITA 49.1 reference rather than inventing its format.

**Required detail:** assign one coordinated counter owner per outgoing wire stream and SID/packet-type counter domain. Define handling of omitted identifiers and packets arriving after rebinding or session restart. Specify bounded duplicate retention, Message ID reuse rules, and retransmission semantics without asserting exactly-once execution from Message ID alone. Concrete retention durations and retry policy remain architecture/profile deliverables; accepting this recommendation does not select VRL.

### 7. Specify the receive-side context state model

**Status: accepted following user direction. Prompt location: Context and status engine.**

The architecture shall explicitly cover receiver reconstruction as well as publication. Context must be associated with data by its applicable time, not simply its arrival time. Paired Context/Data streams match TSI and TSF. TSM distinguishes precise event timing from a data-packet sampling interval (§§7.1.1–7.1.3). Persistence has exceptions: Over-Range Count is not persistent and user-defined State/Event fields can have different persistence (§7.1.4; §9.1.1).

The architecture shall specify startup without context, late/reordered context, gaps and lost deltas, reset/rebind behavior, and bounded history requirements. Define whether data waits, is delivered with unknown/stale metadata, or is dropped. Document each field's persistence, update trigger, and maximum publication delay (Documentation Rules 9.1-1 through 9.1-3).

**Accepted approach:** a receiver context cache with validity/effective-time information and explicit unknown state; periodic full refreshes for recovery. Coalesce only when the field semantics and timing mode permit it. A current-state provider alone cannot reconstruct when a past change actually occurred, so publication needs effective-time information too.

**Required detail:** scope cached fields to the applicable stream/association and session, preserve per-field persistence rules, and use bounded history when associating delayed data with earlier context. A refresh restores the state it describes; it does not reconstruct every missed transition or prove the interpretation of earlier data. Missing or uncertain metadata must remain explicit. Concrete history limits, refresh periods, and wait/deliver/drop policies remain architecture/profile deliverables. Include deterministic startup, loss, reorder, refresh, and reset tests.

### 8. Bound CIF and payload-format support precisely

**Status: accepted following user direction. Prompt locations: shared descriptors; encoding/decoding; Signal Data engine.**

CIF7 can replace or augment a field with attributes and changes its encoded size (§9.1, Rule 9.1-7; §9.12, Figure 9.12-1). Arrays and structured fields add more than a single scalar callback can express (§9.3). CIF fields are ordered, not a universal self-describing TLV format: an unknown bit cannot necessarily be skipped safely without knowing its layout.

The architecture shall enumerate supported CIF words, attributes, arrays, and maximum lengths, and distinguish a structurally known but unsupported control from an unknown wire layout. Do not scan past unknown-length fields by guessing.

For samples, enumerate numeric representation, scaling/fraction bits, real/complex ordering, packing method, item width, tags, repetition, vector sizes, spectral support, frame boundaries, and pad-count handling (§6.1.1; §9.13.3; Tables 9.13.3-1/2). Explicit padding may involve the Class ID pad-count field (§5.1.3; §6.1.1, p. 65).

**Accepted approach:** distinguish general wire decoding, supported semantic controls, and optimized sample codecs. A byte view can be zero-copy without being a native `span<int16_t>` or `span<float>`: big-endian bytes, packing, and alignment may require conversion. Define wire-format views, conversion accessors, and caller-provided destination storage separately.

**Required detail:** maintain separate wire, semantic-control, and optimized-sample support matrices with explicit bounds and unsupported-feature outcomes. Preserve external-buffer ownership and immutable receive storage through conversion APIs. Native typed views require valid representation, alignment, and C++ object-lifetime/aliasing conditions; zero-copy byte access alone does not establish those conditions. Concrete CIF/array limits and optimized format coverage remain architecture/profile deliverables. Test variable-size attributes, malformed lengths, endian conversion, packing, and padding independently of hardware behavior.

### 9. Resolve the header-only requirement

**Status: accepted following user direction. Prompt locations: Objective; Packet objects and external storage. Software choice, not a VITA requirement.**

**Accepted approach:** require a header-only codec, semantic types, and generic runtime core; allow optional compiled transport/device adapters and their external dependencies. The Objective and packet-model requirements now use this boundary consistently. Compiled adapters remain optional dependencies of the core.

**Remaining detail:** the architecture shall select C++20 or C++23 as the minimum and specify supported compilers/OSes, exception/RTTI policy, and dependency policy. Acceptance of the packaging boundary does not select those toolchain settings.

### 10. Put numbers and ownership contracts behind performance requirements

**Status: accepted following user direction. Numerical capacity and performance inputs remain open. Software choices.**

The prompt already asks for queues, executors, backpressure, leases, and shutdown, but target values are absent. Required inputs include aggregate samples/bytes per second, stream/entity counts, packet sizes and MTU, latency/jitter targets, maximum outstanding/scheduled controls, scheduling horizon, memory budget, and target hardware. A throughput target without packet-size distribution is not enough to size dispatch resources.

The architecture shall specify the initial transport, framing rules, endpoint scaling, CPU/NUMA assumptions, and whether zero-copy means avoiding application copies or also kernel/device copies. Select a reproducible benchmark configuration before asserting performance.

Specify send acceptance versus synchronous rejection, partial sends for byte streams, exactly-once lease reclamation, and the meaning of completion for each adapter. CPU/NIC/GPU ownership and fences may differ. Define how application-held receive views can outlive a callback, how long retention is allowed, and what shutdown does with retained leases. Portable C++ spans alone cannot enforce borrowing lifetimes.

**Accepted approach:** move-only external leases with callback-scoped views and an explicit retention operation; per-Controllee serialization as a safe initial control default; asynchronous device completion; reserved control and completion capacity across queues, pools, and shared transports. Separate executors alone cannot prevent starvation if IQ consumes every buffer or TX slot. Support nonblocking Controller calls from callbacks; prohibit or detect synchronous waits on the same execution domain. Lock-free structures, worker counts, and pool sizes should follow measured needs.

## Apparent reference inconsistencies to record

These are inconsistencies in the supplied reference, not shortcomings of the prompt and not official VITA interpretations. The design should keep an interpretation register with citations, chosen behavior, interoperability evidence, and unresolved questions.

| Area | Conflicting material | Proposed handling |
|---|---|---|
| AckV errors | Table 8.4.1-1, p. 110, says AckEr is always zero for AckV; §8.4.1.2, Rules 6–9 and Observation 8.4.1.2-1, pp. 112–113, describe warning/error indication for AckV and AckX. | Propose the detailed diagnostic rules as the implementation basis, but record the conflict and confirm expected behavior with the intended peer or authoritative clarification before claiming interoperability. |
| CIF7 enable location | Table 9.12-1, p. 219, lists `1/7`; Table 9.1-1 and Rule 9.1-7 place the enable in CIF0 bit 7. | Use CIF0 bit 7 as supported by the main CIF definition; document the inconsistent table entry. |
| Packet-class documentation templates | Table 10.2.5.5-1, p. 242, labels the packet type “Context Packet” and offers omission of Stream ID. Table 10.2.5.2-1, p. 238, also offers Stream ID omission. | Use the actual packet definitions: Table 5.1-1 and Figures 7.1-1 / 8.3-1 / 8.3-2 require Stream ID for Context and Command. Treat the templates as requiring correction when producing class documentation. |
| Control change indicator | Table 10.2.5.5-1 lists it as inapplicable; Permission 9.1.1-1 permits its use in Control packets. | Explicitly choose whether the project profile uses it; do not make it the basis of duplicate detection. |

The packet field inclusion matrix and selected problematic tables were also checked visually in the PDF to distinguish source inconsistencies from text-extraction errors.

## Proposed additions to the prompt

The following can be added as a decision-closure requirement without predetermining the entire architecture:

> Produce a decision register distinguishing normative constraints, chosen framework policies, profile restrictions, and unresolved deployment inputs. Select defaults for software choices; identify unsupported features explicitly. For every unresolved item, state its implementation impact and the input required to close it.
>
> Supply concrete Information Class and Packet Class documentation per Sections 4 and 10, including identifiers, fields, CAM modes, time domains, timing windows, reference/control points, and associations. Include an encode/decode/execute/publish support matrix and a register of apparent specification inconsistencies.
>
> Define side-effect-free whole-command validation, requested/adjusted/actual values, device batch execution, and asynchronous completion. Schedule the effect of controls at the reference point, including preparation lead time and optional hardware arming. Specify per-field cancellation and all CAM acknowledgement combinations, including no-action queries, dry runs, NACK-only behavior, and timeout with unknown remote outcome.
>
> Specify receiver context reconstruction and effective-time association, persistence exceptions, CIF7/array limits, unknown-layout handling, sample packing and conversion, shared-SID routing, counter ownership, and Message ID lifetime. Quantify resource bounds and performance acceptance targets for an identified deployment.

Decision 9 has resolved the packaging contradiction in the Objective and packet-model requirements. The remaining work is to produce concrete architecture deliverables and deployment values within the accepted decisions.

## Recommended order of closure

1. Choose the initial peer/use case, platforms, transport, and measurable performance envelope.
2. Freeze the first Information Class / Packet Class profile and interpretation register.
3. Set the callback validation/execution contract, timing capabilities, cancellation model, and transaction response matrix.
4. Set receive context behavior, sample representation, identity/routing, and duplicate policy.
5. Detail the accepted packaging and ownership contracts, default executor implementation, capacities, and overload/shutdown policies.
6. Require the architecture document to close these decisions before implementing the framework.

CRTP versus concepts, queue implementations, coroutine convenience APIs, SIMD optimizations, and advanced DMA/GPU adapters can then be evaluated within these constraints. They need not all be decided by the project owner before the architect begins.
