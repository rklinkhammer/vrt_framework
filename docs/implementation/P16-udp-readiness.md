# P16 separate-process UDP endpoint readiness

Historical pre-implementation audit. The role separation below is now implemented; final evidence is in [P16 integration](P16-integration.md). The accepted example is localhost-only, as specified in [the contract](P16-contract.md) and [runnable README](../../examples/frequency_scan/README.md); the early external-address suggestion below was not adopted. No external deployment claim follows from this audit. This note covers transport/runtime composition for two executable roles; the P16 profile/state contract is independently owned by the contracts agent. Current reference points are `runtime/public/runtime.hpp`, `adapters/posix_udp/{factory,udp,socket}.hpp`, and the P12 real-UDP Runtime fixture. That fixture combines local roles and manually supplies a peer Ack for testing; it is not the desired separate-process example architecture.

## Existing transport is sufficient

The compiled adapter already supplies nonblocking IPv4/IPv6 sockets, one VRT packet per datagram, checked framing/decoding, external RX/TX pools, separate Data/ordinary-control/cancellation lanes, generation-scoped peers, and close/drain/detach lifetime handling. Its factory takes the Runtime's actual route/counter/admission registries, and setup records actual socket addresses/buffers. No new packet format, direct application Ack loop, source-driven reply address or socket-thread callback is needed.

PeerBinding is **directional local source to remote source**, with one remote socket address per lane. TX chooses the binding whose local_source equals the outgoing submission's source. RX matches the configured remote lane address and looks up a route under remote_source. Therefore:

| Process | One required binding | Installed receive routes | Outbound traffic |
|---|---|---|---|
| Controller | local Controller peer/generation → remote Controllee peer/generation; remote addresses are Controllee ports | Ack; optionally Context/Data reception if selected by profile | Control and cancellation |
| Controllee | local Controllee peer/generation → remote Controller peer/generation; remote addresses are Controller ports | Control/cancellation | Ack, Context/Data when its source is enabled |

Both processes need all three bound lane sockets even if a given test does not emit Data. Cancellation must keep its dedicated ingress/response lane. The peers/IDs/generation/SID/class settings must agree; source UDP address does not authorize arbitrary identities. Static source port matching is not authentication and these examples do not claim external untrusted deployment qualification.

`Udp::associate` can validate/commit either directed association alone; it finds the setup prototype by local and remote peer IDs. It does not require installing the opposite role. Existing Runtime install_association currently requests both directions, installs four routes and registers a Controller relationship for every local source. That combined-role default must not be reused unchanged for role-separated examples.

## Required public Runtime contract

Current `add_controller(const Controllee&)` only accepts a Controllee from the same Runtime. `add_controllee` allocates Engine/backend/source/revision banks and installs a request route as well as Ack/Context/Data routes. Calling those APIs just to represent a remote target would create a fictitious local device and reachable local effects.

The contracts agent is proposing `RemoteTargetConfig` and `add_remote_controller(config)` returning the existing Controller handle. It must:

- Require explicit remote SID, Controller/Controllee IDs, peer IDs, profile/class and any receiver format binding; validate route/counter/transport capacity transactionally before setup mutations.
- Install only Controller-owned outbound command counters, inbound remote Ack correlation and selected receiver routes; never a local request/execute route.
- Use only the local Controller→remote Controllee transport association. A Controllee-only role analogously installs only its reversed association and request route.
- Keep existing runtime-owned MID allocation, pending observations, monotonic deadlines, cancellation correlation, response decode and buffer return. Application code submits typed commands and consumes typed observations; it does not construct protocol Acks.
- Disable local source generation, backend dispatch, Context publication, recovery/start and local-state claims for the remote-only endpoint. It may reuse preallocated Stream/bank objects for minimal implementation, but their actual bytes remain budgeted and their effectful service path must remain unreachable.
- Scope Controller handles to Runtime/endpoint identity exactly as existing public handles. Rejected or foreign handles must not reach another process-role record.

The shared Controller public methods already expose observe/observation/state/deadline/release. P16's profile expansion should provide typed field operations through that existing transaction path; this note does not select field policies or widen IQ-profile writable fields. A query result is remote observed state, not the unused backend model possibly allocated inside a remote-only bank.

The contracts agent has received the exact directional binding finding above. A single role enum or separate endpoint constructor is an internal choice; no user architecture input is missing for this role split.

## Example configuration proposal

Use explicitly documented lab IPv4 defaults, overridable through setup CLI:

| Lane | Controller local | Controllee local |
|---|---|---|
|Data|127.0.0.1:41000|127.0.0.1:42000|
|Control/Context|127.0.0.1:41001|127.0.0.1:42001|
|Cancellation|127.0.0.1:41002|127.0.0.1:42002|

These are example defaults, not VITA-assigned ports. Default peer IDs1/2, SID1 and short Controller/Controllee IDs2/3 can be explicitly labeled isolated fixture identities. Do not imply an authorized production OUI: accept the caller's OUI/class mapping or choose a clearly named isolated-lab fixture mode. Any externally interoperating deployment requires its actual identifiers and agreed class semantics. The same CLI should allow the other machine's IPv4 address and all three ports; explicit numeric parsing/address validation avoids DNS or hidden network discovery in the runtime.

Use existing lab pool setup only where labeled. Keep Control/cancellation providers physically isolated from Data, preserve the emergency pool and actual startup ledger, and do not claim a smaller example instantiates the full reference configuration. Report bound addresses, lane mapping and role once at setup. A finite `--duration` option plus signal-driven shutdown makes integration tests deterministic without requiring a terminal.

## Clock pumping, idle deadlines and shutdown

`run_for()` is an injected-clock deterministic helper that advances virtual time without sleeping. It is unsuitable as the elapsed-wall-time loop for independently scheduled UDP processes. Both processes must call `progress(MonoTime)` from actual `steady_clock` elapsed nanoseconds, monotonically increasing from a process-local origin. That origin is local only; do not subtract sender and receiver monotonic timestamps across processes.

An idle Controller must continue progress even while waiting for user input or packets, so receive processing and transaction timeouts run without an implicit cancel. Prefer a single serialized loop with bounded stdin/nonblocking readiness polling and a maximum wait interval (for example1ms for the demo), or an application I/O thread that only queues bounded requests to the runtime owner. Never block on getline on the serialized progress thread. A command callback records a bounded result; formatting/stdout occurs after progress returns. Existing Controller::wait relies on run_for and therefore is not the real-time UDP wait implementation to advertise; use the live owner loop and typed observation predicate until a framework real-clock wait binding exists.

Untimed mode0 Controller operations require monotonic deadlines but no fictitious PPS measurement. The current Runtime setup clock validation may still require an explicitly bound clock configuration; that is different from asserting a locked source or calibrated hardware. For the Controllee's runnable lab Data source, configure the existing **injected** clock explicitly and feed periodic simulated PPS/time-of-day derived consistently from the same steady-clock origin and a declared lab epoch. A single initial PPS expires under the current2s holdover policy, so a long-running demo must either pump explicit simulated observations or provide a real clock source. Do not label that simulated pump GPS qualification. Timed production commands/source start retain the existing qualified-clock/device requirements.

On SIGINT or duration completion, set a flag and request graceful Runtime shutdown from the owner loop, then keep advancing real MonoTime/progress until the completion callback or the existing2s grace deadline. Signal handlers must not call Runtime, socket methods or allocating I/O. Controller-only shutdown must close new submissions and finish local transport/correlation accounting without inventing remote cancellation or execution success. Controllee shutdown follows real local engine/disarm/drain rules. If unresolved ownership survives the grace deadline, report the actual stopped/quarantined status; do not equate timeout with quiescence.

Keep setup, pools, factory context and any callback state alive until Runtime destruction. A retained factory.instance may outlive Runtime only because Runtime invokes the adapter's detach hook before host registries die. Do not call `Udp::close()` prematurely while graceful runtime draining still needs to send terminal responses; let the existing lifecycle policy control admission and final detach. UDP kernel-copy completion remains distinct from remote receipt or execution.

## Independent acceptance targets

Start two processes using the finite lab configuration and capture their actual bound addresses. Prove typed remote Control changes only the Controllee model; Controller process exposes no local request route/device effects. Receive and correlate actual remote V/X/S phases and compare query state. Wrong source port/identity cannot invoke effects. Suppress replies and verify real monotonic timeout while idle, with no automatically transmitted cancellation. Exercise explicit cancellation on its lane, source/Context delivery only from the Controllee, and both process shutdown orders. Existing same-host UDP evidence is a software integration test, not independent-vendor interoperability or clock/hardware qualification.

No new required deployment input blocks these explicitly labeled lab examples. Production identifiers/authentication, device/clock qualification and M6 hardware access remain separate existing input gates. Implementation should begin only after the profile/state/role API contract freezes.
