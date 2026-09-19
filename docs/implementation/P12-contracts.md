# P12 transport integration contract proposal

Status: transport interface agreed and implemented as a P12 prerequisite; compiled UDP and independent verification remain separately gated. The initial audit below explains the selected boundary. Authority: architecture §§5, 10, 13 and implementation-plan P12.

## Existing coupling and minimal replacement

`VitaRuntime` owns `Loopback<320,128,128>`, calls `try_send`, `progress_next`, `outstanding(token)` and explicit test quiescence proof, indexes a bounded I/O record by returned token slot, and charges embedded transport storage. Runtime also uses Loopback submission, token, capability and fault types. Runtime's internal RouteRegistry, CounterRegistry and AdmissionPool are the adapter's authoritative registries; a second independent registry would break routing/counter admission.

Proposed minimal contract is a setup-bound, fixed-size `TransportBinding` with stable owner lifetime and callbacks for send, bounded progress, outstanding-token query, close-admission/drain, and explicit optional quiescence proof. Submission remains move-only `TxStorage + CompletionToken + PeerSession + CounterKey + completion credit`; rejection returns the entire submission. Token includes slot and generation, with advertised maximum slots <= Runtime's 320 I/O records. Acceptance commits packet count exactly once; all subsequent failures are deferred completion. Result publication never invokes application callbacks inline in submission. Neutral transport types may be extracted with compatibility aliases for Loopback. Fault injection remains an explicitly supported test capability, not an operational UDP mode.

Adapter creation needs the actual Runtime registries and RX providers. Prefer a checked setup factory callback receiving a bounded host binding (route lookup/dispatch, counter preflight/commit, admission and three reserved RX providers), returning owner-backed transport binding and exact metadata charge. A checked attachment before endpoint freeze is another valid implementation. Do not expose mutable registries to ordinary applications or create a runtime-header dependency on POSIX headers. Constructor/destructor/setup allocation belongs to compiled adapter code; runtime remains header-only. Default Loopback behavior and existing examples stay supported.

An abstract binding is not a capacity exemption: validate function pointers, slot bounds, CPU-access requirements, MTU, three TX regions, contiguous RX, and owner lifetime at setup. Charge selected transport objects once, including any simultaneously retained Loopback member, setup-owned queues/peer tables, and external shared-owner allocations. The previous M3 aggregate does not automatically cover this new composition.

## UDP implementation requirements

- Explicit bounded setup configuration maps local sender/counter identity to socket/destination and exact configured source address/port to authorized PeerSession. Preserve IPv6 scope identifiers where applicable. Unknown sources are dropped. Received payload fields never select response destinations. Same-socket multiplexing must retain SID/type/Class/identity routing checks.
- One complete VRT packet is one datagram. Gather up to three CPU-accessible regions with `sendmsg`; retain accepted storage until successful kernel-copy return or terminal send failure. Kernel completion is not peer delivery. EAGAIN retains pending work; EINTR retries are bounded per service turn. A partial/oversized/failed datagram never becomes a successful truncated packet.
- Nonblocking receive uses contiguous external backing. Detect truncation and reject whole packets. Reserved control/cancellation receive capacity cannot be consumed by Data flood. Classification may require bounded header peek before choosing an RX lane; malformed/unknown packets have an independent discard budget. Packet parse precedes route callback.
- Service completion consumption first, then at most 32 control/Context sends and 64 Data sends per cycle; independently bound receive work. Cancellation has its reserved lane. A busy socket cannot monopolize Runtime progress or starve another socket. Failures/counters remain observable without allocating error strings.
- Enforce configured no-fragment MTU before acceptance: IPv4 base payload 1472 or IPv6 1452 at MTU1500, further reduced for configured overhead. Enable platform no-fragment options where available; missing enforcement must reject the advertised capability or be explicitly scoped. EMSGSIZE is a terminal send failure and configuration signal, never truncation or silent MTU mutation. Packetization changes happen at checked packet boundaries.
- Close admission does not release queued accepted buffers without completion. Ordinary copying POSIX sockets have no device access after the bounded system call returns; serialize close with progress. No synthetic completion is used as separate physical proof. Socket close is idempotent and cannot revoke application-retained RX backing.

## Local platform and required inputs

Inspected host: Darwin arm64 kernel27, Apple Clang21/libc++; architecture permits macOS UDP functional qualification only. Local SDK exposes IPv4 `IP_DONTFRAG`; implementation must compile-probe IPv6 and Linux option variants, not assume identical constants. POSIX socket headers, local loopback interfaces, nonblocking sendmsg/recvmsg and bounded polling are sufficient for software integration. Linux production builds remain a separate toolchain/platform gate.

IPv4/IPv6 localhost tests can use explicitly configured loopback ports and caller-provided isolated lab identities. No authorized production OUI, external peer, production clock/GPS capture or NIC claim is inferred. These absent inputs block corresponding deployment qualification, not compiled UDP work. Local independent decoder tests are software oracles; they are not an independent VITA peer implementation.

## Implemented interface checkpoint

The frozen candidate uses `runtime::transport::{TransportFactory,TransportBinding,HostBindings,Association}` in `binding.hpp`, neutral existing submission/token/capability types in `types.hpp`, and checked `inspect(TxStorage)` in `framing.hpp`. The factory advertises required metadata bytes, maximum slot count and capabilities before Runtime allocates it; the returned binding must match all three exactly. `RuntimeConfig::transport` selects it; an empty factory keeps the deterministic Loopback default.

`associate(context,batch,false)` preflights the whole controller/controllee relationship batch; `true` commits atomically in the same serialized domain. UDP must provide this callback to support generation-safe setup/recovery. The required external `detach(context)` irreversibly removes all borrowed host references before Runtime members die, even if another setup owner retains the adapter. Detach cannot deliver receive/application callbacks; after detach, later adapter operations must not touch Runtime registries or admission. It is distinct from close-admission while draining accepted work. Provider-backed retained RX leases remain independently lifetime-owned.

Default Loopback is now separately setup-owned; it is not embedded alongside UDP. On this Apple arm64 build the default sixteen-stream/two-bank reference ledger is **51,669,128 / 67,108,864 bytes**, an increase of 376 from the historical M3 candidate. `sizeof(VitaRuntime<>)=230776`, `sizeof(TransportBinding)=160`, and `sizeof(TransportFactory)=88`. Default transport metadata includes its existing physical state plus an explicit128-byte shared-owner allowance. External factories must advertise their own complete native allocation charge; no UDP fit is inferred from the default figure.

`tests/unit/P12/binding.cpp` passes direct C++23 normal, ASan/UBSan and TSan builds, with exceptions/RTTI disabled. It tests overbudget rejection before factory invocation, returned metadata mismatch, transactional rejected association registration, successful generation recovery, and segmented framing. Existing P05 Loopback, P10 Runtime and P11 Runtime developer executables pass against the refactor. Independent factory/framing and complete UDP gates remain verifier-owned.

## Coordinated scheduling and real-socket regression

The optional `begin_cycle` callback runs once per outer Runtime progress; UDP uses it to reset its bounded 32-control/64-Data service budgets, rather than resetting on every inner adapter call. Already-ready completion tickets are consumed before adapter I/O and again after each bounded service round. This avoids repeated inner polling defeating fairness.

`tests/unit/P12/runtime_binding.cpp` exercises the public Runtime through the compiled UDP factory over IPv4 and IPv6: emitted full Context and IQ Data decoded by external sockets; framework-generated Controller command decoded externally and matched AckX returned over the socket; and factory-retained adapter safely detached after Runtime destruction. Control/Data sockets may reorder arrivals, so the fixture makes no network ordering promise. It passes direct normal, ASan/UBSan and TSan builds. Independent interoperability is not claimed.

## Independent rejection-cleanup correction

The independent factory oracle found that a factory-retained owner was not detached when Runtime rejected its returned metadata. Runtime now invokes the supplied detach callback before rejecting an otherwise returned binding. Factories returning an error must themselves detach/dispose all borrowed HostBindings; retained returned owners must provide a working detach even if another returned field is invalid. The independent factory regression passes, and the developer mismatch case now asserts detach; its ASan/UBSan build passes.

## Candidate source manifest

- `include/vita/runtime/transport/types.hpp`: `af12faf53e6581985add7ff75f8c20eefac950e972d64fd85cb7541bbe6c1411`
- `include/vita/runtime/transport/binding.hpp`: `ac1c82c381a5b6e4f83e251f26c291f4ca1c676cff14c39e10d16b35ecdefc57`
- `include/vita/runtime/transport/framing.hpp`: `b301edcfec8b22f70672e892ad1e83b6d889086f09273bbc25c95a5bbd98af74`
- `include/vita/adapters/loopback/loopback.hpp`: `b0a5a6d7b6c423fb757c1f285dc2dd2bbf9be20e677c4afdf0bd0246ffe8577b`
- `include/vita/runtime/public/config.hpp`: `3de727b014566b0f4566f5517f0cc3e4581592ba14f97c0622136e6a83710c09`
- `include/vita/runtime/public/runtime.hpp`: `354fd2537248e81c6df42709fed6001498b67755eca6b31710c994fda994f0bb`
- `tests/unit/P12/binding.cpp`: `705cdf0f855f0b5c44d1a5b34718601ef39e10dbb0116cf87aad9935ab4b9d03`
- `tests/unit/P12/runtime_binding.cpp`: `9933685d1265b9ef8d256b3f1a6ffbaca9d9d53a6b94958e368a75cbe283ea59`
