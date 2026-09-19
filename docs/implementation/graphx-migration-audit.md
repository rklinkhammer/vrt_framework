# GraphX migration audit at the Release baseline

Status: **implementation paused for an acknowledgment-semantics decision**.
This is a compatibility audit and proposed implementation contract, not a GraphX
implementation release or a declaration of P1 acceptance. No production code or
GraphX files were changed. Do not use this document's commit as a working GraphX
library dependency pin.

## Frozen inputs and precedence

- Library: `b4b97193347b1037a98f6c54e19046455ca5df2d` (`Release`).
- GraphX: `41d621fa46d9b18d15354b0e67651dc79cb0a339`, clean checkout during inspection.
- Required GraphX sources: [system brief][gx-system], [radio design][gx-radio],
  [implementation plan][gx-plan], and [command analysis][gx-analysis]. Also inspected
  [packet definitions][gx-yaml], [radio service][gx-source], and [independent harness][gx-test].
- Normative source: purchaser-supplied `AV49DOT2-2017-R2024.pdf`, ANSI/VITA
  49.2-2017 (R2024), SHA-256
  `909cb7052fba4ceb52ee64a73928252d40e0a2390e231198beea9066af3700a8`.
  References below are printed page numbers. Pages 104, 110, 118 and 124 were
  also rendered and visually inspected. The cover identifies September 2017
  errata and September 2024 reaffirmation; the inspected passages still differ.
  [VITA's standards catalog](https://www.vita.com/Standards) identifies this edition;
  the catalog does not resolve the acknowledgment interpretation.
- The user's explicit replacement of `vrtgen` takes precedence over the older
  dependency choice in the GraphX documents. There must be one VITA codec and
  protocol implementation, owned by this library.
- The user's earlier explicit OUI decision remains applicable: Data Class ID
  bytes are `00 FF FF FF 00 00 00 00` (unknown OUI, unspecified Information/Packet
  Class), **not eight zero bytes**. GraphX's documents, YAML and expectations
  still need migration. Context and Command continue to omit Class ID.

## Resolved normative questions

### Supported limits can use CIF7

Section 9, printed p124, explicitly describes using Sample Rate with attributes
for a device's "minimum and maximum supported sample rate". It also allows
Section 9 metadata in Context and Command packets, including acknowledgments.
Together with Section 9.12, Table 9.12-2 and Observation 9.12-1 (pp219-220), this
establishes a standards-based supported-limit representation. Min/max need not
be misrepresented as statistical extrema or current settings.

The proposed GraphX query is NO_ACTION with selected setting fields and CIF7
Maximum/Minimum only. AckS returns Maximum then Minimum for each selected field
in CIF order; Current is absent. CIF0 bit7 enables CIF7 under the existing I2
interpretation. The attribute mask is `0x0c000000`. Ordinary current-state queries
remain a separate path. Document these as global device-supported limits,
independent of the current sample rate; report current settings separately.

A global bandwidth maximum does not imply every bandwidth/rate pair is valid.
The GraphX rule `0 < bandwidth <= sample_rate` and any additional bounded device
constraint must be checked against the complete proposed configuration before
mutation. A profile-level constraint API can answer proposed-pair validity; it
must not masquerade as a generic CIF7 device-constraint wire field. Arbitrary
constraint-table discovery is not established by scalar Min/Max.

CIF7 Precision represents a +/- field precision (Table 9.12-2), not a supported
increment. Integral sample-rate rules are explicit GraphX profile restrictions.
Any public increment/discrete-choice configuration must have defined units,
origin and admission behavior; unused arrays are insufficient. This audit does
not assign a new on-wire selector for arbitrary step/choice tables.

### SSI and Class ID

Section 5.1.6.1, Table 5.1.6.1-1 and Rules 1-2 (pp61-62) support SSI values
SINGLE=0, FIRST=1, MIDDLE=2, FINAL=3 in bits11-10, with enables23-22. These apply
to time-domain sample frames too. One-packet bursts enable both bits and use zero.
Section 5.1.3, Table 5.1.3-2 (p55), permits unknown OUI `FF-FF-FF`; zero class
codes denote unspecified classes. No proprietary encoding is necessary for
these requirements after the user's OUI correction.

## Decision required before implementation

### D-GX-ACK: scheduling acceptance versus execution evidence

GraphX's radio design says a timed-start acknowledgment means scheduled, not yet
emitting (lines94-95). The service sets `armed=true`, immediately constructs
AckX with SchX=1, and copies the requested timestamp (radio.cpp lines216-253;
identity helper lines101-108). Its command analysis also describes scheduling
acceptance. This is observable wire behavior, not merely a missing adapter.

The supplied standard contains differing wording:

- Rule 8.3.1.5-4 (p104) requests AckX upon execution.
- Rule 8.4.1.5-4, under Execution Acknowledge Timestamp (p118), requires actual
  execution time; the rule itself uses the label `AckE`.
- Table 8.4.1-1, SchX row (p110), describes fields scheduled for execution in AckX.
  Observation 8.4.1.4-2 (p116) also discusses scheduled or executed actions.

The library already selects the execution-evidence interpretation in
[protocol design Section 4.6](../vita49_protocol_design.md#46-controller-observations-and-ordering):
AckV follows validation/admission; AckX follows terminal execution, and AckS
follows the requested action. An early GraphX AckX must not be fed to the existing
Controller observer and silently promoted to confirmed execution.

Related request-policy mismatch: GraphX's docs/service allow execution and/or
status replies, including EXECUTE with ReqS alone. Permission 8.3.1.5-1 allows
combinations, but Rule 8.3.1.5-7 specifically requires ReqX with post-execution
ReqS. The library records this tension as I8 and rejects EXECUTE+ReqS without
ReqX. NO_ACTION+ReqS status queries are unaffected.

**Recommended concrete migration contract, not yet applied:**

1. Use requested AckV for early validation/scheduling acceptance, with its
   scheduled-time timestamp. Request AckX when execution evidence is needed.
2. Emit AckX only after the device activation/configuration/stop action completes;
   use its actual effective time. Data emission is measured separately.
3. For an EXECUTE requesting state, request ReqX+ReqS and return AckX then AckS;
   AckS reports the post-action observation and observation timestamp. Pure
   NO_ACTION+ReqS queries remain read-only and valid while armed or streaming.
4. Update GraphX's client, service migration, docs and independent vectors to this
   contract. Retain the existing generator/frequency-tunable interpretation.

This decision changes the GraphX acknowledgment timing and accepted request
combinations. The user's instruction to stop on unresolved normative semantics
therefore prevents silently choosing either the early-AckX compatibility behavior
or this migration contract. If early AckX must remain, obtain an explicit peer
profile interpretation and add separate scheduled-versus-executed Controller
observations; do not claim the current observer is compatible.

The existing GraphX identity helper also copies request timestamps into all
responses. In contrast, Rule 8.4.1.5-5 (p118) requires AckS observation time.
That implementation behavior needs correction in migration, not replication.

## Requirement-to-evidence matrix

Status meanings: **gap** = required library behavior absent; **reuse** = relevant
primitive exists, with GraphX integration unverified; **host** = ordinary GraphX
adapter/application responsibility; **blocked** = decision above required.
None of the rows claims GraphX integration acceptance at the Release baseline.

| ID / GraphX requirement | Baseline evidence | Required implementation / verification | Status |
|---|---|---|---|
| P1 atomic four-setting configuration | [Controller](../../include/vita/runtime/public/runtime.hpp) exposes individual rate/frequency writes; [state](../../include/vita/runtime/state/contracts.hpp) excludes BW/Gain | One correlated command, full validation before writes, no partial path even for hand-authored CAM; negative combinations and backend failure tests | gap |
| Device configuration commit | [Backend](../../include/vita/runtime/transaction/backend.hpp) begins one `PlannedField` at a time | Explicit batch/commit contract with the device owner; do not describe sequential setters as atomic. Publish one coherent post-configuration state | gap |
| P1.4 supported-limit query | Generic [CIF7 codec](../../include/vita/codec/packet.hpp); [request parser](../../include/vita/runtime/transaction/engine.hpp) marks non-Current unsupported | Controller/controllee min/max exchange using the resolved Section9 semantics; separate current-state and capability observations | gap; semantics resolved |
| Global versus rate-dependent BW | Scalar min/max cannot encode the entire relation | Global bounds in capability response; documented `BW<=Fs` rule and bounded device constraint API for proposed settings; apply same validation at execution | gap |
| Capability no-mutation guarantee | Ordinary query machinery exists | Verify before configuration, stopped, armed, streaming; no device writes or changes to state version, sample ordinal, phase, epoch, pacing or source status | gap |
| Unsupported capability selector | Generic parser rejects unsupported layouts; profile scope is narrower than registry | Field-specific unsupported result where selectors are structurally known and diagnostic reply requested; no fabricated zero limits | gap |
| Full and selected status | Engine request capacity is four distinct fields | Support BW/RF/Gain/Fs/streaming together (five selectors), selected-only responses, empty/value-bearing query negatives | gap |
| Scheduled-start AckX and post-action S | Existing interpretation in protocol design 4.6 and I8 differs from GraphX | Resolve D-GX-ACK; independent CAM/timestamp vectors, X-only suppression, X+S ordering | blocked |
| Configure/start/stop lifecycle | Existing local source lifecycle; no GraphX wire discrete-I/O controls | Require successful initial config; stopped/armed/running states; reject configure or second start while armed; immediate stop disarms pending start; cancellation and replay tests | gap |
| Start/stop discrete mapping | Generic `DiscreteIO32` codec exists | CIF0=2, CIF1=64, value3=start/value2=stop; retain ordinary VITA fields; reject invalid enables/value bits and wrong timing modes | gap |
| SID1-4 and class policy | Runtime binds generator/frequency-tunable class identities | Separately named explicit GraphX profile, unknown OUI Data class, absent Context/Command class; mismatch rejection and remote-controller setup tests | gap; OUI resolved |
| Exact four-setting Context | [publisher](../../include/vita/runtime/context/publisher.hpp) includes baseline metadata; [receiver](../../include/vita/runtime/context/receiver.hpp) expects reference/format/state information | Emit and accept the 52-byte BW/RF/Gain/Fs Context; configure IQ format/validity locally under explicit profile rules; do not add baseline wire fields | gap |
| Context before Data and every burst | Generic publisher gates on revisions and refresh cadence | Reuse ownership/order guarantees; add burst-boundary cadence and test both sides with literal GraphX Context | reuse + gap |
| Signed BE IQ16 I,Q | [sample codec](../../include/vita/codec/samples.hpp) and [source window](../../include/vita/profiles/iq/source.hpp) | Preserve exact device-provided CS16 codes; verify opposite-sign extrema, component order, no extra normalization | reuse |
| 1-1024 valid pairs; short packets | `packet_samples` and `SampleWriteWindow` cap at256 | Public packet sizing through1024, correctly sized pools/receive limits, no silent MTU-driven substitution for required1024 packets; test1/2/1023/1024 | gap |
| Configurable1-262144-pair bursts | No operational SSI burst state | Bounded packetization, all four SSI transitions, short final payload, continuous ordinal/time/phase; test32,2050,262144-pair bursts and consecutive bursts | gap |
| Exact simulated picosecond time | [SampleTimeline](../../include/vita/runtime/timing/sample_timeline.hpp) preserves fractional remainder | Reuse injected UTC clock and exact cumulative timing; verify non-divisor rates, new-start epoch, host-pacing independence and skipped-sample accounting | reuse + integration gap |
| Timing lead20ms-10s | Generic [timing contract](../../include/vita/runtime/timing/scheduling.hpp) supplies execution windows | Profile start-lead bounds and deterministic injected-time tests; host maps UTC to steady clock once, no hardware precision claim | gap + host |
| Independent family counts modulo16 | [CounterRegistry](../../include/vita/runtime/stream/counters.hpp) keys sender/SID/type | Reuse; verify accepted sends, failures and retransmissions for GraphX layouts | reuse |
| Correlation, retry, cancellation | [manager](../../include/vita/runtime/transaction/manager.hpp), [retention](../../include/vita/runtime/transaction/retention.hpp), [Controller](../../include/vita/runtime/transaction/controller.hpp) | Preserve ownership/correlation; add radio-lifetime high-water rejection for evicted IDs and authenticated reconnect identity policy. Existing finite retention alone is not forever replay suppression | reuse + gap |
| TCP packet-size framing | [transport boundary](../../include/vita/runtime/transport/binding.hpp); UDP/loopback adapters | Library bounded stream framer, max-size preflight, exact consumed-byte/backpressure contract, complete validated packets through existing routes; fragment/coalesce/malformed/disconnect/stall/reconnect tests | gap |
| Mutual TLS and network routing | Host-owned transport binding is already extensible | GraphX manages certificates, peer authorization, sockets, idle/handshake/output deadlines and TCP writes; no TLS system or second VITA parser in library | host |
| UDP MTU and no fragmentation | Existing POSIX UDP adapter and packet size accounting | Host enforces jumbo path end-to-end; IPv4 minimum IP MTU4156 or IPv6 minimum4176 for4128-byte VRT; packet/pool limits verified before emission | reuse + host |
| SoapySDR acquisition | `DeviceBackendBinding`, `SourceProvider` extension points | GraphX adapter wraps the same Soapy device for atomic config, bounded CS16 reads and effective lifecycle; waveform/passband/gain/clipping remain device behavior, no parallel generator | host; batch API gap |
| Four radios / processor / OVS | Runtime supports independent streams; no GraphX deployment | One library-backed radio instance per container; four listeners, FFT/gap policy, detector, mirror recorder, OVS ownership, packaging and lifecycle remain GraphX P2-P6 | host |
| Independent verification and pin | Existing codec/runtime gates do not cover missing profile | Literal vectors plus a decoder independent of production descriptors; raw-CAM negatives, atomic rollback/no-effects, resource exhaustion and connection tests; new implementation commit only after gates pass | pending |

## Proposed acceptance and migration sequence

After D-GX-ACK is resolved, write the profile/API design before implementation.
Add operational configuration and capabilities through shared runtime mechanisms,
then exact GraphX Context/Data/lifecycle integration. Preserve current profiles
and their validation behavior; charge all added state/transaction storage to the
existing resource budget rather than merely raising static assertions.

Expose all device capability values from one owner, shared by reporting and
admission. A rejected full configuration must leave every setting unchanged.
Device failures after a physical effect require explicit unknown-state/fault
handling rather than a false claim of rollback or success.

Integrate the library through CMake `vita::core`, plus `vita::posix_udp` if the
host chooses that adapter. GraphX's mixed control-TCP/data-UDP host binding can
select transports by packet family without interpreting VITA fields itself.
Replace vrtgen/code generation, packet classes and GraphX's protocol engine only
once the library profile has independent interoperability evidence. Preserve the
existing Soapy device, credential loader and graph ownership code through adapters.

The migration changes must include the OUI correction and the selected
acknowledgment decision in GraphX's architecture, P1 plan, fixtures and dependency
inventory. P1.4 cannot be marked complete from a standalone CIF7 encoder test.
Soapy/mTLS loopback and Linux/OVS qualification are distinct from library unit tests.

## Verification record and commands

This audit does not run GraphX processes, privileged networking or TLS provisioning.
Only baseline build/test evidence is reported here; no implementation gate is
claimed for the missing GraphX profile.

Baseline audit commands on macOS arm64:

```sh
cmake --preset dev
cmake --build --preset dev -j4
ctest --preset dev -R 'p06_engine|p07_controller|p10_runtime|p14_verify_cif7_profile|p16_core' --output-on-failure
```

Result: configure and build succeeded; all five selected tests passed
(`p06_engine`, `p07_controller`, `p10_runtime`, `p14_verify_cif7_profile`,
`p16_core`). This is targeted baseline evidence, not a full-suite or GraphX
implementation qualification.

Required implementation gates, **not run or passed by this audit**:

```sh
cmake --preset udp-dev
cmake --build --preset udp-dev -j4
ctest --preset udp-dev --output-on-failure
cmake --preset udp-release
cmake --build --preset udp-release -j4
ctest --preset udp-release --output-on-failure
cmake --preset udp-asan-ubsan
cmake --build --preset udp-asan-ubsan -j4
ctest --preset udp-asan-ubsan --output-on-failure
cmake --preset udp-tsan
cmake --build --preset udp-tsan -j4
ctest --preset udp-tsan --output-on-failure
```

Repeat applicable commands on native macOS arm64 and Linux arm64; identify
compiler, standard library, sanitizer runtime and source SHA in the result. A
container platform label by itself is not evidence of a passing gate. Host
SoapySDR/mTLS and GraphX P2-P6 migration evidence remains separately required.

[gx-system]: https://github.com/rklinkhammer/graphx-docker/blob/41d621fa46d9b18d15354b0e67651dc79cb0a339/docs/vita_system.md
[gx-radio]: https://github.com/rklinkhammer/graphx-docker/blob/41d621fa46d9b18d15354b0e67651dc79cb0a339/design/four-radio-vita/radio-design.md
[gx-plan]: https://github.com/rklinkhammer/graphx-docker/blob/41d621fa46d9b18d15354b0e67651dc79cb0a339/design/four-radio-vita/implementation-plan.md
[gx-analysis]: https://github.com/rklinkhammer/graphx-docker/blob/41d621fa46d9b18d15354b0e67651dc79cb0a339/design/four-radio-vita/command-analysis.md
[gx-yaml]: https://github.com/rklinkhammer/graphx-docker/blob/41d621fa46d9b18d15354b0e67651dc79cb0a339/config/vita/radio.yaml
[gx-source]: https://github.com/rklinkhammer/graphx-docker/blob/41d621fa46d9b18d15354b0e67651dc79cb0a339/src/vita/radio.cpp
[gx-test]: https://github.com/rklinkhammer/graphx-docker/blob/41d621fa46d9b18d15354b0e67651dc79cb0a339/tests/test_vita_radio.py
