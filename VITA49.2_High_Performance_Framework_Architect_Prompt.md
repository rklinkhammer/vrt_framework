# Software Architecture Prompt: High-Performance C++ VITA 49.2 Controller/Controllee Framework

## Role

Act as a senior software architect with deep experience in modern C++, real-time and high-throughput systems, zero-copy networking, SDR data paths, asynchronous I/O, heterogeneous/device memory, and ANSI/VITA 49.2.

Use the attached **ANSI/VITA 49.2-2017 (R2024)** specification as the normative protocol reference. Cite the relevant clauses, tables, and figures for important protocol decisions. Clearly distinguish requirements imposed by VITA 49.2 from software-architecture choices made by this framework. Do not assume that VITA 49.2 prescribes the software design; use the accepted IQ Generator Profile v1 below and complete its Information Class and Packet Class documentation.

## Accepted review decisions

Release-scope update D-M5-1 (user accepted2026-09-19): M5 acceptance follows the documented IQ operational profile and published bounded codec capabilities, with Array-of-CIFs excluded from production requirements. Retain its optional explicit structural inspection utility and I9 engineering notes without requiring Array native/emission/peer integration for this release. This overrides any complete-registry M5 objective below. See [current scope and acceptance](docs/implementation/M5-operational-scope.md); device and deployment qualification remain separate.

All ten decisions in [the architecture review](docs/architecture_prompt_review.md) are accepted and incorporated below. Treat these as project requirements, including the accepted defaults in [IQ Generator Profile v1](docs/iq_generator_profile_proposal.md), rather than alternatives to reconsider. The profile filename retains its original `proposal` suffix, but its status is accepted.

Complete the detailed architecture within those decisions. Explicitly distinguish accepted requirements, implementation choices still delegated to the architect, unresolved specification interpretations, and deployment inputs such as OUI values, timing windows, capacity limits, and performance targets. Acceptance of a decision does not imply that its design, tests, or measurements already exist.

## Objective

Design a modern, high-performance C++ framework implementing the selected portions of VITA 49.2. Its codec, semantic types, and generic runtime core shall be header-only; optional compiled transport/device adapters and their external dependencies are permitted. It must provide a reusable runtime for applications operating as Controllers, Controllees, or both.

Applications shall derive from or configure the framework and implement only device- or application-specific behavior. The framework shall own the common protocol machinery, including packet receive and transmit loops, packet validation and dispatch, Command/Acknowledge processing, Context/Status publication, Signal Data packetization, scheduling, transport interaction, and buffer lifecycle management.

The architecture must clearly separate:

- semantic VITA packet representation;
- packet views and encoding/decoding;
- Controller and Controllee behavior;
- command dispatch and transaction state;
- status/context generation;
- Signal Data production and consumption;
- transport;
- external buffer allocation and ownership;
- threading, scheduling, queues, and backpressure;
- application-specific callbacks.

## Accepted initial application profile

Accepted Decision 1: use [IQ Generator Profile v1](docs/iq_generator_profile_proposal.md) as the required initial application profile. Its scope, packet support matrix, class assignments, and baseline defaults are accepted. Complete the remaining deployment configuration and class documentation under Sections 4 and 10 of the standard.

The initial application generates complex time-domain IQ. The reusable framework supports all assigned VRT packet-type codes 0x0–0x7, including Signal Time/Spectral distinctions, standard Command subtypes and cancellation, and extension envelopes with registered payload codecs. Reserved codes 0x8–0xF are not valid emitted packet types. Packet-family support does not imply implementation of every device control or arbitrary vendor payload semantics.

Use IQ16 by default, with IQ32 and float32 alternatives, processing-efficient complex Cartesian packing, and the profile's deterministic source and sample-timeline timestamps. Publish paired Context and support the profile's generator controls, queries, and cancellation. Use loopback for deterministic validation and UDP as the first external adapter. Preserve the profile's class/identifier rules, static discovery, and external-buffer model.

Produce explicit wire-codec, semantic-control, publication, and optimized-sample coverage matrices. Do not treat accepted sample-rate configuration ranges as demonstrated throughput. OUI assignment, complete Packet Class tables, concrete timing windows, capacity limits, and measured performance remain required architecture/deployment deliverables.

## Fundamental runtime model

Center the framework on a `VitaRuntime` that can host:

- a Controller only;
- a Controllee only;
- a combined Controller and Controllee;
- multiple logical Controllers and Controllees in one process;
- multiple VITA Packet Streams per endpoint;
- multiple transport bindings.

Do not assume that one process, runtime, transport, or network address corresponds to one VITA entity.

Applications implement behavior through concise, type-safe callbacks. Conceptually, a radio might resemble:

```cpp
class MyRadio : public vita::Controllee<MyRadio> {
public:
    CommandResult set_frequency(const FrequencyCommand&);
    CommandResult set_bandwidth(const BandwidthCommand&);
    CommandResult set_gain(const GainCommand&);

    void produce_signal(SignalProducer&);
    void populate_status(StatusBuilder&);
};
```

Determine whether CRTP, C++23 concepts, compile-time descriptors, registration tables, customization-point objects, or a hybrid provides the best balance of performance, type safety, extensibility, readable application code, and low dispatch overhead. Avoid forcing applications to implement large virtual interfaces.

Applications must not implement protocol loops that receive a packet, decode and classify it, process CAM fields, construct acknowledgements, acquire buffers, encode packets, submit I/O, or release buffers. Those responsibilities belong to the framework.

## Command callback architecture

The framework shall receive and decode VITA Control packets, identify the addressed Controllee and requested controls, validate the request, and dispatch individual semantic controls to application callbacks.

Exploit the symmetry between Command and Context fields. Evaluate a common compile-time field descriptor that can define:

- field identity and CIF location;
- wire encoding and decoding;
- semantic C++ type;
- units, range, and validation;
- command setter/callback;
- context/status provider;
- metadata and profile support;
- error and warning mapping.

Avoid unnecessary runtime polymorphism and duplicated definitions of the same VITA field.

## Context-aware CIF representation and traversal

Model the Context/Command Indicator Fields as a structured semantic object, not merely independent bit flags or a packed C++ struct overlaid on wire memory. Separate the CIF selection, associated field/attribute values, and the interpretation context. That context includes packet family/subtype, Control action mode where applicable, selected CIF words and attributes, and the registered Packet Class/profile. Reuse field descriptors while applying packet-specific layout rules.

Provide typed operations to select, set, replace, and remove fields and attributes. Keep selectors, CIF enable bits, associated values, and calculated encoded lengths consistent. Define how edits that change CIF7 attributes affect all selected fields and handle missing or incompatible values explicitly. For query and cancellation packets, selection need not imply a value body. AckV/AckX diagnostic selections use their diagnostic layout, while Context and AckS use their applicable value layouts. Do not assume every set bit introduces a normal field value: some bits enable additional CIF words or express indicators.

Received-buffer decoding is likewise context-sensitive. First establish the packet/subtype and applicable prologue, then decode the indicator sets required by that layout. Traverse selected fields in specification-defined wire order with a bounded cursor. Before advancing to the next field, resolve and validate the current field's extent using its descriptor, selected attributes, and any count/length/structure metadata. Check bounds and arithmetic before reading or advancing. An unknown extent is an unsupported layout, not permission to guess the next offset.

Distinguish structural traversal from semantic materialization. A decoder may expose non-owning views or skip a structurally understood but unsupported field after validating its extent; it need not allocate or construct every value. Variable-length contents must be inspected as far as needed to establish safe boundaries. If random field access is offered, build a bounded offset index during validated traversal rather than assuming fixed offsets. Structural parsing must not execute device callbacks; dispatch follows the required packet/command validation.

Use shared layout descriptors or an equivalent common traversal model for sizing, encoding, and decoding so edits and CIF7/array variations cannot cause their interpretations to diverge. Invalidate or rebuild derived sizes/offsets when semantic edits change layout. Encoded receive views remain immutable; editing requires a separate semantic representation and encoding into external output storage. Include representative sketches for typed field updates, selector-only queries, attribute changes, and cursor-based decoding, with tests for truncated variable-length fields and mode-dependent layouts.

## Accepted Decision 2: validation and hardware execution

Use a virtual hardware model to validate supported controls, packet parameters, and protocol responses without physical hardware. The model shall support deterministic successful updates, invalid/unsupported requests, write failures, and partial completion. Validation and dry runs shall not commit changes to live device state; simulated changes shall use isolated model state.

For real hardware, validate packet structure, profile support, and known parameter constraints before execution. Device adapters may perform additional checks available without significant signal analysis. An update succeeds when its required registers have been written according to the adapter's documented completion contract. Merely enqueueing asynchronous writes is not successful completion; mandatory register readback is not implied.

Keep validation, execution, and completion distinct, with a backend contract usable by virtual registers and real device adapters. The real-hardware path need not run a full simulator. Report write failures and partial completion explicitly; do not assume hardware rollback. Define cross-field validation and revalidation of state-dependent constraints before delayed execution.

Accepted clarification: support partial execution explicitly. When permitted by the command's CAM settings and applicable validation, dependency, and timing constraints, execute eligible controls even when other selected controls cannot execute. Track validation and execution outcomes per field, including successful, rejected, failed, and unexecuted controls, and translate them into the applicable acknowledgement diagnostics. Validate the whole command before side effects and define an execution plan that respects cross-field dependencies. Adapters shall report partial register-write failures within a semantic control without marking that control fully successful. Completed writes are not automatically rolled back. Continue independent controls only where the command settings and adapter contract permit it. Support for partial execution does not override commands that prohibit it; document admission and failure behavior for those requests. Include deterministic tests for mixed valid/invalid controls, dependent controls, and failures after earlier writes have completed.

AckV shall represent available validation results. Successful AckX shall represent the defined register-write execution result, subject to CAM and timestamp requirements. AckS shall report available device/model state without presenting unmeasured signal behavior as measured fact. Distinguish requested, accepted, written, and measured values where applicable.

Verification that real hardware produces the intended RF/IQ behavior, settles correctly, or satisfies signal-quality requirements belongs to overall system testing and may require significant signal analysis. It is not a prerequisite for reporting a successful register update. The timing architecture must separately reconcile register-write completion with the selected VITA reference point; completion of an early staging write does not prove that a scheduled signal effect has occurred.

## Command/Acknowledge transaction engine

Implement VITA 49.2 control semantics rather than treating Command packets as generic RPC messages. Account for:

- Control packets;
- Validation Acknowledge (AckV);
- Execution Acknowledge (AckX);
- Query-State Acknowledge (AckS);
- Control Cancellation;
- Message ID correlation;
- Controller and Controllee identifiers/UUIDs;
- CAM behavior;
- dry-run validation;
- warnings and errors;
- immediate and timestamped execution;
- multiple acknowledgements for one Control packet.

Accepted Decision 4: provide an explicit CAM truth table and corresponding state-machine test fixtures. Cover action modes, all partial/warning/error combinations, NACK-only behavior, ReqV/ReqX/ReqS combinations, warning/error detail requests, SchX/AckP, and timing status. For each case, specify response obligations and suppression conditions, acknowledgement ordering, diagnostic inclusion, Controller-visible outcomes, and the applicable clauses. Identify invalid and reserved combinations. Include dry-run responses and query-only wire fixtures.

Share semantic field descriptors while defining distinct Control-with-values, query-selector, AckV/AckX diagnostic, and AckS state layouts. Query-only Control packets contain CIF selectors without control values. Each acknowledgement contains exactly one subtype; a Control packet may request multiple separate acknowledgements. Warning/error fields are 32-bit diagnostics, not encodings of the controlled field's semantic value. Apply Sections 8.3, 8.4.1.1, 8.4.1.2, and 8.4.2, including post-action AckS ordering where applicable.

The Controller API shall distinguish local send completion, validation, execution, state observation, cancellation, and timeout with unknown remote outcome. Explicitly represent no-Ack and NACK-only requests; silence or local send completion shall not be presented as confirmed remote execution success. Preserve apparent specification conflicts, including AckV/AckEr behavior, in a documented interpretation register with the chosen implementation basis and outstanding interoperability questions.

Define a framework-owned `CommandTransaction` abstraction containing the required identifiers, stream association, CAM state, timestamps, requested controls, validation state, execution state, cancellation state, and acknowledgement obligations.

Application callbacks shall return structured semantic outcomes. The framework shall translate those outcomes into the required AckV, AckX, AckS, warning, and error fields. Applications must not manually construct acknowledgement packets.

Specify behavior for duplicate commands, retransmission, timeout, cancellation races, partial validation, partial execution, expired timestamps, unknown fields, unsupported fields, and exceptions thrown by callbacks.

Accepted Decision 5: model cancellation with per-field state and cancellation masks, not only a transaction-wide flag. Cancel only selected controls that have not executed. The device adapter shall document the point beyond which cancellation is unavailable, including preparation, hardware arming, and disarm capabilities. The framework shall coordinate cancellation and execution to produce an unambiguous outcome for each selected field. Cancellation does not roll back completed register writes. Keep local wait cancellation and timeout distinct from issuing or successfully completing wire cancellation.

Apply Section 8.5: reuse the original Stream ID, Controller/Controllee identifiers, and Message ID; use the original CIF selection or a subset without control values. Apply cancellation timing and CAM requirements, including partial cancellation, no AckV response, and AckX as the minimum requested acknowledgement with optional AckS. Distinguish cancellation acknowledgements from original command responses. Define bounded retention of per-field outcomes and deterministic tests for cancellation before and after the device cutoff, including late completions.

Accepted Decision 6: scope transaction correlation by Message ID, relevant Stream ID and endpoint identifiers, and local peer/session context. Define Message ID uniqueness duration, wrap/restart and reuse rules, bounded duplicate retention, late-response handling, and retransmission policy. Message ID correlation alone shall not be treated as an exactly-once execution guarantee.

## Packet objects and external storage

A semantic VRT packet can be configured, updated, and encoded repeatedly into externally supplied storage. Keep packet fields and sample-format metadata independent of any particular encoded buffer. Updating the semantic object shall not alter an already encoded or in-flight transmission; encoding captures the values for that transmission. Define synchronization if configuration and encoding can occur concurrently.

This is a strict requirement: a semantic VITA packet object shall not own or embed its transmit or receive buffer. Do not place a `std::vector<std::byte>`, network buffer, DMA handle, or transport handle inside a packet.

Encoding and decoding shall have packet-family-specific storage contracts. Do not force every VRT packet through one contiguous-buffer API or treat receive decoding as simply the inverse of transmit encoding.

| Packet family | Transmit encoding | Receive decoding |
|---|---|---|
| Command and Context, including their extensions | One externally supplied contiguous packet buffer | One externally supplied immutable contiguous packet buffer; traverse the applicable CIF/payload layout |
| Signal Data | Header/prologue, IQ payload, and optional trailer segments | Separate header/prologue, IQ payload, and optional trailer views, with direct access to the IQ region |

For Command and Context packets, the basic interface can resemble:

```cpp
auto encoded = vita::encode(packet, mutable_buffer_view);
auto decoded = vita::decode(const_buffer_view);
```

For Signal Data, design distinct segmented transmit and receive interfaces. The transmit interface combines metadata encoding with existing or newly produced IQ storage. The receive interface validates packet structure and exposes the IQ region without re-encoding, coalescing, or walking individual samples. Extension Data may use the same segmented storage abstraction, but its payload interpretation depends on its registered class and must not automatically be treated as IQ.

For Signal Data transmit and receive, support up to three externally backed segments:

1. **Header/prologue buffer:** the packet header and all included Stream ID, Class ID, and timestamp fields preceding the IQ payload.
2. **IQ data buffer:** the payload in the selected VITA wire packing, including any required payload padding.
3. **Optional trailer buffer:** the trailer when enabled by the Packet Class; omit this segment when no trailer is present.

These segments form one logical VRT packet in wire order, not three packets. Compute Packet Size from their actual encoded lengths, excluding unused pool capacity. Preserve word boundaries, payload packing/padding, and trailer placement. The accepted IQ profile omits the trailer by default; supporting a trailer segment does not change that default.

Keep segment leases in a separate transport submission object or `BufferChain`, outside the semantic packet. The runtime binds the packet metadata to the externally supplied IQ payload, encodes the prologue and optional trailer into their buffers, and submits the segments as one packet. Use bounded, allocation-free segment descriptors for the steady-state path. The transport retains all referenced storage until it can no longer access it, and every lease is reclaimed exactly once.

Permit an existing producer- or device-supplied IQ buffer to be attached without copying when its wire representation and transport capabilities allow it. Scatter/gather does not perform endian conversion or sample packing: native samples that do not match the wire format require an explicit conversion into external storage or supported device processing. Do not modify or reuse an in-flight IQ buffer, header, or trailer. Reusing a semantic packet is independent of reusing its encoded segments.

Retain contiguous encoding for simple transports. Advertise transport scatter/gather capabilities and segment limits. A datagram adapter must send all segments as one datagram; a byte-stream adapter must preserve framing and track partial progress across segments without interleaving packets. If scatter/gather is unavailable, use an explicitly documented coalescing copy into a pooled contiguous buffer, or reject the configuration when a no-copy policy is required.

On Signal Data receive, return a decoded view exposing header/prologue metadata, the bounded IQ payload view, and an optional trailer view. Locate the IQ region from the validated header, optional-field lengths, total packet length, and trailer presence. Distinguish payload bytes (including wire padding) from valid samples, using the applicable Packet Class/context and padding information. Locating the payload does not require decoding each sample or traversing a Context/Command CIF body. If sample interpretation is unavailable, expose a validated opaque payload with explicit unknown format rather than guessing a sample count.

Support both transport-delivered separate header/data/trailer buffers and a contiguous received packet. For contiguous input, expose three logical subviews into the original allocation without copying IQ. For physically segmented input, validate segment boundaries and lengths rather than trusting the adapter's classification. Do not assume a generic scatter receive automatically splits variable-length prologues correctly; document adapter capabilities and any transport-fragment handling. If the payload itself spans physical fragments, expose a bounded payload chain or explicitly report the need for a contiguous conversion; never fabricate a contiguous span.

Keep receive ownership in an external receive envelope/lease set. Header, IQ, and trailer views may share one backing allocation or use separate allocations. Retaining the IQ view must retain every backing lease it references; reclaim a shared allocation only after its last dependent view is released. The receive buffer lifetime must be at least as long as every view referencing it. Make this relationship explicit in the type design and documentation.


Design the packet model by composition rather than assuming a deep inheritance hierarchy. Cover at least:

- Signal Data packets;
- Context packets;
- Command packets, including Control and Acknowledge;
- relevant extension packet types;
- mandatory packet header;
- optional Stream ID, Class ID, timestamps, and trailer;
- packet-specific prologues and payloads.

Accepted Decision 9: the codec, semantic types, and generic runtime core shall be header-only. Optional transport/device adapters may be compiled and may use external dependencies; the generic core shall not require those adapters. C++23 is the minimum language baseline. Specify the dependency boundary, supported compilers/OSes, exception/RTTI policy, and dependency policy. Evaluate optional C++26 facilities where they materially simplify typed packet updates, field metadata, or CIF modeling. Any such enhancement must identify its standardization and compiler support, be feature-gated, and retain a functionally equivalent C++23 path with identical wire semantics. Do not make a speculative language feature a requirement of the baseline.

## Buffer abstraction and lifecycle

Create a generic external-buffer model capable of supporting:

- heap and fixed-pool memory;
- lock-free pools;
- huge pages and shared memory;
- DMA and FPGA-accessible buffers;
- CUDA pinned, unified, and device memory where meaningful;
- DPDK mbufs;
- RDMA-registered memory;
- scatter/gather chains;
- custom hardware memory.

Provide contiguous CPU-addressable `MutableBufferView` and `ConstBufferView` plus external transmit `BufferChain`/submission and receive-envelope abstractions for segmented Signal Data. Define distinct ownership handles, borrowed views, and device-memory representations as needed. Do not pretend that all device memory is directly CPU-addressable.

Buffer management shall support multiple configurable fixed-size buffer classes within one runtime. Allow separate pools for small header/prologue buffers, large IQ payload buffers, optional trailer buffers, contiguous Command/Context packets, and contiguous transport fallback packets, with multiple payload capacities where needed. Each class defines block capacity, alignment, memory domain, pool count, and return destination; each lease separately records its used length. Do not require a universal buffer size or allocate a payload-sized block for every header or trailer.

Select compatible size classes using packet requirements and transport capabilities. Define exhaustion behavior per class, reserve control/completion resources, and reclaim already acquired segments if later acquisition, encoding, or submission fails. A chain may combine leases from different pools/providers; retain each provider's lifetime and return each segment to its originating pool. Fixed pool capacity shall not appear as padding or unused bytes on the wire.

The framework obtains a transmit buffer or compatible segment leases from providers, encodes directly into the supplied storage, transfers the leases to the transport submission, and reclaims them only when no I/O can reference them. For accepted asynchronous sends, reclamation follows completion; synchronous rejection follows the documented ownership contract. A contiguous send can resemble:

```cpp
transport.send(
    std::move(buffer),
    encoded_size,
    [return_to_pool = std::move(return_fn)](SendResult result) mutable {
        return_to_pool(result);
    });
```

The architecture should make double return, premature return, use after return, and lost buffers difficult or impossible. Define a buffer state model such as Available, Acquired, Encoding, Transport-Owned, and Returned. A buffer must never be returned merely because an asynchronous `send()` call returned.

Address cancellation, transport failure, shutdown with outstanding I/O, late completions, completion callbacks on arbitrary threads, and pool destruction ordering.

Accepted Decision 10: use move-only external leases, callback-scoped views, and an explicit retention operation when application consumption must outlive a callback. Document how retention preserves ownership, its resource limits, and shutdown behavior with outstanding leases. A raw span alone does not enforce borrowing lifetime. Keep leases outside semantic packet objects.

Specify send acceptance versus synchronous rejection, partial sends on byte-stream transports, exactly-once lease reclamation, and each adapter's completion meaning. Support asynchronous device completion without blocking protocol workers. Define CPU/NIC/GPU visibility and fence requirements where applicable; returning from send or submitting a device operation is not sufficient evidence for buffer reclamation.

## Zero-copy and minimal-copy paths

Transmit data directly into a pool-provided transport buffer wherever feasible. Do not require an intermediate serialized packet. On receive, decode Command/Context from a single packet buffer and expose Signal Data header/IQ/trailer views directly from the transport-owned storage. Release backing storage only after dispatch and application consumption complete.

For high-rate Signal Data, favor spans and views over materialization or sample copying. Identify unavoidable copies and explain why they are needed. Provide the header/IQ/trailer scatter/gather abstraction described above, with contiguous fallback for transports that do not support it. Hardware offload remains optional. Document the copies and memory-domain transitions required by each path.

## Encoding, decoding, and wire correctness

Provide bounded, allocation-free encode/decode operations for the steady-state path. Results shall carry explicit success or structured error information, consumed/written byte counts, and required capacity when a destination is too small.

Validate packet sizes, optional-field consistency, padding, alignment, packet type, class/profile constraints, trailer presence, and malformed inputs without out-of-bounds access.

VITA wire representation uses ordered 32-bit words and big-endian transmission. Provide efficient host-to-wire and wire-to-host primitives using C++ standard facilities, compiler intrinsics, and optional SIMD where useful. Encoding must not mutate semantic packet objects merely to convert byte order.

Define checked and deliberately named trusted/fast decode paths if both are needed. Never make the default network-facing path unsafe.

Accepted Decision 8: distinguish general wire encoding/decoding, supported semantic controls, and optimized sample codecs. Supply separate coverage matrices and explicit resource bounds for each. A field can have a valid wire codec without an application control implementation; an optimized sample format does not define the limits of the generic packet model.

Enumerate supported CIF words, CIF7 attributes, arrays, structured fields, and maximum lengths. Account for attributes that replace or augment standard values and alter encoded sizes. Distinguish known-but-unsupported controls from unknown wire layouts. CIF fields are ordered rather than universally length-tagged; do not guess the size of an unknown field or scan past it. Return a structured unsupported-layout error when safe interpretation is unavailable. Bounded opaque extension payload views remain permitted where the enclosing packet layout is known.

Define immutable wire-format views, conversion accessors, and conversion into caller-provided destination storage as separate APIs. A zero-copy byte view does not guarantee a native typed sample span: endian order, packing, alignment, and C++ object-lifetime/aliasing constraints must all permit such access. Do not mutate receive storage for conversion or hide mandatory intermediate allocation. State explicitly when conversion or copying is required.

Document sample numeric representation, scaling/fraction bits, real/complex ordering, packing method, item width, tags, repetition, vector sizes, spectral support, frame boundaries, and pad-count handling. Follow Sections 6.1.1, 9.1, 9.3, 9.12, and 9.13.3, including Class ID pad-count use where applicable. Include fixtures for variable-size attributes, malformed lengths, endian conversion, packing, and padding.

## Framework-owned execution

Applications must not create protocol-processing threads. The runtime owns or accepts executors for:

- receive and initial dispatch;
- command/control processing;
- status/context publication;
- Signal Data transmit and receive;
- scheduled command execution;
- transmit completion processing.

Avoid a hard-coded thread per stream. Support both modest embedded radios and high-rate multi-stream SDR systems. Compare dedicated threads, configurable worker pools, event loops, and caller-provided executors, then recommend a default.

Separate control-plane and data-plane resources so sustained IQ traffic cannot starve commands or status. Specify queue ownership, scheduling priorities, CPU-affinity options, and real-time considerations without requiring them in all deployments.

## Signal Data engine

Signal Data shall be framework-driven. Applications provide or consume samples through callbacks, providers, or bounded leases, while the framework handles:

- packet sizing and payload capacity;
- sequence/packet count;
- Stream ID and Class ID;
- timestamp insertion;
- sample-frame and event handling;
- packing and padding;
- trailer generation;
- buffer acquisition;
- packet encoding;
- transport submission;
- completion-based buffer return.

Provide optimized extensible paths for common real and complex sample formats, including 16-bit integer, 32-bit integer, and floating-point samples, while retaining support for general VITA packing formats. Keep packet metadata separate from the sample storage when possible.

Describe underflow, overflow, dropped packet, discontinuity, timestamp gap, and backpressure reporting.

## Context and status engine

Context/Status publication shall also be framework-driven. Support:

- change-driven publication;
- periodic publication;
- full snapshots;
- delta updates;
- query-driven state reporting;
- persistent context values between updates;
- association of Context streams with Signal Data streams.

Applications expose state through providers rather than manually building packets:

```cpp
register_status<Frequency>([this] { return radio_.frequency(); });
register_status<Gain>([this] { return radio_.gain(); });
```

Explain snapshot consistency when multiple status fields are read concurrently with command execution. Apply the configuration revision contract below to state shared by control execution, sample generation, and publication.

Accepted Decision 7: implement a framework-owned receiver context cache with per-field validity/effective-time information and explicit unknown state. Scope entries to the applicable stream associations and local session. Associate Context with Signal Data by its applicable time and TSM semantics, not merely by packet arrival order. Respect paired-stream timestamp compatibility and field persistence rules in Sections 7.1.1–7.1.4 and 9.1.1. Over-Range Count is not persistent; user-defined State/Event fields use their documented persistence.

Publish periodic full refreshes for recovery and expose effective-time information through state/event providers as well as current values. A refresh restores the state it describes; it does not reconstruct missed transitions or establish the interpretation of all earlier samples. Coalesce updates only where field semantics and timing mode permit it, preserving significant events and timing distinctions.

Define bounded context history and explicit behavior for startup without context, late/reordered updates, lost deltas, uncertain metadata, reset, and rebinding. Specify when data waits, is delivered with unknown/stale metadata, or is dropped, including capacity and timeout behavior. Do not silently substitute the newest state for historical context. Document each field's persistence, update trigger, and maximum publication delay, plus refresh periods and cache retention limits. Add deterministic tests for startup, loss, reordering, refresh recovery, nonpersistent fields, and session reset.

## Configuration revisions and effective boundaries

Accepted approach: use framework-owned configuration revisions to connect control execution, sample generation, Context publication, and acknowledgements. Keep requested values, validated pending changes, and effective committed state distinct. Each effective revision records the state governing samples, its effective timestamp and sample position where applicable, and per-field validity and execution outcomes. Revisions are internal framework metadata; do not introduce a new VITA wire field for them. Receivers continue to associate Context and Data through the documented stream and time relationships.

Validate the command and build an execution plan before side effects. Select an eligible packet boundary for generator configuration changes, satisfying the requested timing mode/window or rejecting the request. Account for generation ahead of the current timeline. Bind each generated packet to an immutable effective revision throughout generation and encoding; preserve the revision of already encoded or submitted data. For the baseline Sample Rate change, finish the current packet, commit the new configuration at the eligible boundary, submit updated Context, and generate subsequent packets at the new rate while preserving timestamp and oscillator-phase continuity.

Collect execution results per field. With partial execution permitted, construct effective state from successful changes and confirmed unchanged fields. If a failed operation leaves a value indeterminate, mark it unknown rather than retaining an old value as confirmed. Distinguish register-write completion from the time a change becomes effective at the reference point, including preparation and hardware arming. When controls take effect at different times, record separate effective revisions/transitions rather than assigning one artificial timestamp to the entire command. Define handling when a device transition falls within a sample buffer, including splitting affected Data at the transition where supported or explicitly reporting unavailable/uncertain interpretation.

Submit a full Context snapshot describing each applicable effective revision before submitting its first affected Data packet. If Context submission is blocked or rejected, hold affected Data under a bounded policy and specify timeout, drop, or stop behavior; do not silently bypass the required Context publication. Define how unknown fields are represented or withheld without presenting stale values as current. Submission ordering does not guarantee arrival ordering or delivery over UDP; retain the receiver context-cache recovery rules. Context submission failure does not roll back an already completed device effect.

Generate acknowledgements from recorded outcomes. AckX becomes eligible only when the applicable execution-completion and reference-point timing requirements are met; it does not wait for Data delivery. A post-action AckS uses a coherent state observation after the relevant outcomes have been incorporated, with its own observation timestamp and the ordering required by CAM. Preserve the distinct meanings of requested state, effective state, and observed state, including intervening changes from other commands. Applications and adapters supply results and timing evidence; the framework owns revision management, publication, and acknowledgement construction.

Specify bounded revision storage and lifetime management while packets, pending operations, or publication work reference a revision. Include a sequence diagram and deterministic fixtures for successful boundary changes, partial execution, indeterminate writes, distinct effective times, Context submission failure, and preservation of already submitted packets.

## Transport independence

Packet formatting must not depend on UDP, TCP, raw Ethernet, PCIe, shared memory, Serial RapidIO, DMA, or any other transport. Define a transport concept supporting asynchronous send, receive-buffer delivery, completion, error reporting, and shutdown.

Potential adapters include:

- UDP;
- TCP or framed streams;
- raw Ethernet;
- shared memory;
- PCIe/DMA;
- loopback/test transport;
- application-supplied transports.

Clarify framing responsibilities for byte-stream transports and MTU/fragmentation responsibilities for datagram transports. Do not hide transport-specific constraints inside semantic packet classes.

## Queues and backpressure

Use bounded queues between asynchronous stages; uncontrolled heap growth is unacceptable. Determine where SPSC, MPSC, or MPMC queues fit and where executor submission is preferable.

Define explicit policies such as blocking, reject, drop newest, drop oldest, coalesce status, signal overflow, or apply transport backpressure. Command and Signal Data traffic need not share the same policy. Commands should fail explicitly rather than disappear silently.

Specify queue sizing, overload metrics, shutdown/drain semantics, fairness, and how completion delivery avoids deadlock when pools or queues are exhausted.

Accepted Decision 10: reserve control and completion capacity across queues, buffer pools, and shared transports so IQ traffic cannot exhaust all buffers or transmit slots. Separate executors alone are insufficient. Choose lock-free structures, worker counts, and pool sizes according to measured needs, and explain how completion processing makes progress under overload.

## Allocation policy

The steady-state high-performance path should avoid dynamic allocation. Prefer pooled buffers and transactions, bounded queues, fixed-capacity metadata where practical, compile-time field descriptors, and reusable encoder state.

Dynamic allocation is acceptable during configuration or on lower-rate control paths when it materially improves usability. State precisely which paths may allocate rather than claiming universal zero allocation.

## Callback concurrency guarantees

Define exactly which callbacks may execute concurrently and on which executor. Include interactions among multiple command callbacks, status providers, Signal Data producers/consumers, scheduled execution, and shutdown.

Evaluate policies such as:

- serialized per Controllee;
- concurrent per Controllee;
- serialized per stream;
- application-supplied executor or strand.

Accepted Decision 10: use per-Controllee serialization as the default for control callbacks and control-state access. Define its interaction with status providers, data callbacks, scheduled operations, and asynchronous device completions; serialization of callback invocation alone does not order outstanding device effects. Applications should not require pervasive locking merely because the runtime has multiple workers. Support nonblocking Controller calls from callbacks. Prohibit or detect synchronous waits on the same execution domain that would deadlock progress. Define reentrancy and any opt-in concurrency policies.

## Scheduled commands and time

Accepted profile clarification: runtime operation advances with elapsed wall-clock time, and the IQ generator is paced against it. Use monotonic elapsed time for timeouts and duration-based deadlines independently of sample production and packet-clock corrections. Packet timestamps are expected to be conditioned on GPS time through GPS-driven PPS, but their encoded epoch/TSI need not be GPS seconds. Specify the time-of-day/epoch source associated with PPS, the mapping to sample times, synchronization/holdover behavior, and clock corrections. Preserve injectable clocks for deterministic tests. The earlier simulated session-relative clock default is superseded; the accepted profile specifies the remaining clock configuration inputs.

Sample-rate changes occur at packet boundaries and are expected to be highly uncommon. Favor a stable source rate; downstream applications may decimate as needed. Specify how a timed rate change selects an eligible boundary, handles already generated/submitted samples, and satisfies the requested timing mode/window or is rejected. Rare changes do not relax timing correctness, but do not require an architecture optimized for frequent source-rate transitions or a baseline generator decimator.

Accepted Decision 3: use framework-owned scheduling with a device timing-capability contract and optional prepare/arm hooks. The framework shall interpret VITA timestamps, validate commands, and either execute immediately or admit them to a bounded scheduler. Schedule controls to become effective at the documented VITA reference point, accounting for preparation, register-write, and device/processing delays. Calling an application callback at the requested timestamp alone is not a sufficient timing guarantee.

The device timing-capability contract shall declare supported clock domains and conversions, synchronization assumptions, device timing precision, application early/late windows, minimum scheduling lead time, and hardware staging/arming capabilities. Software timers are acceptable only within their documented achievable timing window. Device adapters may implement hardware timing while the framework retains protocol scheduling, transaction state, and acknowledgement obligations.

Apply the timestamp control modes in Table 8.3.1.7-1, including their distinct early/late permissions. Reject controls whose timing requirements cannot be met, respecting CAM partial-execution and response rules. Do not silently relax the requested timing mode. Define simultaneous/equal-timestamp ordering, rounding, cancellation, loss of synchronization, and behavior after clock discontinuities. Concrete timing values are deployment parameters that must be documented, not assumed guarantees.

Preserve Decision 2: real-hardware updates succeed on completed register writes, and physical signal verification belongs to system testing. Document whether the VITA reference point is the register/control bank or a downstream signal point. Treat early staging writes as preparation; they do not demonstrate that a scheduled effect has occurred. For downstream reference points, establish timing through a documented device timing model or completion evidence without requiring per-command signal analysis. Distinguish measured and estimated effective times.

Specify AckV, AckX, and AckS timing and timestamps according to their distinct scheduled-execution, actual-execution, and state-observation meanings in Section 8.4.1.5. Application and driver hooks provide device behavior and timing capabilities; they do not implement protocol loops or transaction scheduling.

## Stream abstraction

Model VITA Packet Streams explicitly. A stream configuration/state object should cover:

- Stream ID and packet class;
- Class ID and supported application profile;
- packet counter/sequence state;
- timestamp policy and clock association;
- Signal Data packing policy;
- context association;
- transport binding and destination;
- buffer provider and queue policy;
- statistics and lifecycle state.

Explain how multiple streams are registered, started, stopped, rebound, and destroyed safely while work is in flight. Define routing keys and collision handling when several logical entities share transports.

Accepted Decision 6: distinguish local routing namespaces from wire identifiers and reject ambiguous registrations. Legal Data/Context/Command pairing may share a Stream ID; use packet type and the applicable endpoint/profile context to distinguish roles. Define identifier omission, identifier allocation, response destinations, and routing across transport bindings, including rebinding and session restart. Local session identity shall not be assumed to be transmitted on the wire; document how delayed packets from an earlier session are handled when wire identifiers are reused.

Coordinate one counter owner for each outgoing wire stream's SID/packet-type domain. Increment the VRT Packet Count modulo 16 according to Rule 5.1.1-9; do not conflate paired packet types into one counter or maintain conflicting counters for the same emitted stream. Treat this count as a limited diagnostic, not reliable high-rate loss/reordering detection. Stronger detection requires explicit transport/profile assumptions. If VRL is selected, obtain and validate ANSI/VITA 49.1 as an additional reference before specifying its framing or counters; VRL is not selected by this decision.

## Controller API

Design a typed Controller-facing API that lets applications invoke specific controls without constructing raw packets. It should support asynchronous validation/execution acknowledgements, query state, cancellation, timeout, correlation, and optionally futures, callbacks, coroutines, or sender/receiver integration.

Conceptually:

```cpp
auto transaction = controller.command(target)
    .set<Frequency>(2.45_GHz)
    .set<Gain>(18.0_dB)
    .request_ack(AckMode::validation_and_execution)
    .execute_at(deadline)
    .submit();
```

This syntax is illustrative, not prescribed. Recommend an API based on correctness, composability, and real-time impact.

## Observability and diagnostics

Provide framework-owned metrics and structured diagnostics for:

- packets and bytes by type/stream;
- decode/validation failures;
- command latency and acknowledgement latency;
- queue depth and high-water marks;
- drops, coalescing, and backpressure;
- buffer-pool utilization and outstanding leases;
- transport errors and completion latency;
- Signal Data discontinuities;
- scheduler lateness.

Observability must be optional or low-overhead and must not introduce mandatory allocation on critical paths.

## Error model and lifecycle

Define typed errors for malformed packets, unsupported profile features, transport failures, callback failures, resource exhaustion, scheduling failure, cancellation, and shutdown. Avoid exceptions crossing thread or transport boundaries. State where exceptions are permitted and how they are translated.

Specify runtime, endpoint, stream, transaction, transport, pool, and scheduler lifecycle states. Include orderly shutdown, immediate stop, draining, outstanding views, pending commands, scheduled commands, and asynchronous completions.

## Security and robustness

Treat all received packet bytes as untrusted. Address bounds checking, integer overflow, excessive lengths, identifier spoofing, unauthorized control, replay/duplicate detection options, resource-exhaustion attacks, malformed CIF combinations, and callback isolation.

Do not invent security mechanisms as VITA requirements. Identify authentication, authorization, confidentiality, and replay protection as transport/system-profile concerns where the standard does not define them.

## Testability

Design for deterministic tests with a loopback/fake transport, virtual clock, deterministic executor, mock buffer pool, and packet fixtures. Include:

- compile-time descriptor tests;
- golden wire-encoding vectors;
- conformance tests derived from the specification;
- malformed/truncated packet fuzzing;
- round-trip encode/decode properties;
- command/Ack state-machine tests;
- virtual-hardware tests for parameter validation, expected responses, register writes, failures, and partial completion;
- separate real-hardware system tests for signal behavior and signal quality;
- scheduled-command and cancellation race tests;
- pool ownership and late-completion tests;
- segmented versus contiguous wire equivalence, multiple size-class exhaustion, acquisition rollback, cross-pool reclamation, and partial byte-stream send tests;
- Signal Data receive payload extraction from contiguous and segmented storage, variable prologues, absent/present trailers, padding, malformed boundaries, and IQ retention after other views are released;
- overload/backpressure tests;
- multi-stream routing tests;
- interoperability tests with an independent VITA implementation;
- throughput and latency benchmarks.

## Performance envelope and capacity inputs

Accepted Decision 10 requires explicit, reproducible performance and capacity criteria. Specify aggregate sample/byte rates, packet-size distribution and MTU, stream/entity counts, latency/jitter targets, outstanding and scheduled command limits, scheduling horizon, memory budget, target hardware, transport/framing, and CPU/NUMA assumptions. State which copies are avoided at application, kernel, and device boundaries.

Distinguish supplied requirements, proposed benchmark configurations, and measured results. Unprovided numerical targets remain identified deployment inputs; accepting the ownership/execution recommendations does not establish a throughput or timing guarantee. Derive queue, pool, scheduler, and retention capacities from the selected envelope and validate them with reproducible overload, latency, and throughput measurements.

## Required architectural output

Produce an architecture document, not a full implementation. It must include:

1. Executive summary and explicit assumptions.
2. Accepted IQ Generator Profile v1, completed Information Class/Packet Class documentation, support matrices, and intentionally unsupported features.
3. Component/dependency diagram.
4. Control-plane and Signal Data sequence diagrams, including configuration revisions, effective boundaries, Context-before-Data submission, and acknowledgement timing under partial execution and publication failure.
5. Buffer-ownership state diagram.
6. Command/Acknowledge transaction state machine, CAM response matrix, and representative derived test fixtures.
7. Thread/executor, queue, scheduling, and backpressure model.
8. Packet representation and header-only encoding/decoding design.
9. C++23 concepts, types, and representative API sketches, including context-aware CIF editing/decoding and any justified optional C++26 enhancements.
10. Application examples for a Controller, Controllee, and combined endpoint.
11. Transport and buffer-provider extension points.
12. Error, shutdown, and lifecycle semantics.
13. Test, fuzzing, conformance, and benchmark strategy.
14. Risks, tradeoffs, and rejected alternatives.
15. Phased implementation roadmap with independently testable milestones.
16. Decision traceability register mapping accepted Decisions 1–10 to the design, identifying remaining implementation choices and deployment inputs with their impact and required resolution.
17. Specification interpretation register covering the apparent inconsistencies identified in the review, including AckV/AckEr, CIF7 enable location, Packet Class documentation templates, and the Control change indicator. Record citations, implementation basis, and unresolved interoperability questions without presenting project interpretations as official VITA rulings.

For each important decision, provide the rationale, alternatives considered, performance implications, ownership/lifetime implications, and the applicable VITA 49.2 reference.

## Acceptance criteria

The design is acceptable only if:

- all ten accepted review decisions and the initial IQ profile are honored;

- application code implements device behavior rather than protocol loops;
- Controllers invoke typed commands and Controllees receive typed callbacks;
- the framework owns Command/Acknowledge, Context/Status, and Signal Data machinery;
- packet semantic objects do not own transport buffers;
- encode and decode operate on caller-provided storage;
- decoded Signal Data can be viewed without mandatory copying;
- asynchronous transport completion controls buffer return;
- buffer ownership and view lifetime are mechanically clear;
- control traffic cannot be starved by Signal Data traffic;
- queues and schedulers are bounded and have explicit overload policies;
- steady-state critical paths can operate without dynamic allocation;
- multiple Controllers, Controllees, streams, and transports can coexist in one runtime;
- the wire format is interoperable and traceable to VITA 49.2;
- the design is implementable incrementally and testable without physical radio hardware.

Do not generate thousands of lines of implementation code at this stage. Use compact, representative C++ sketches to prove the API and ownership model, and focus the response on architectural decisions that an implementation team can execute.
