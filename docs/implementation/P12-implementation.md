# P12 implementation — compiled POSIX UDP adapter

Candidate for independent software verification. No external-peer interoperability, Linux production qualification, NIC throughput or deployment conformance claim is made. Required deployment inputs remain tracked separately in `M4-inputs.md`.

## Configuration and integration

`vita::posix_udp` is an optional separately compiled target. Its `Socket` boundary contains the operating-system calls; bounded adapter/ownership templates remain public headers. Core-only consumers do not link the adapter. `FactoryConfig` and `factory()` supply the Runtime's transport factory with exact predeclared metadata, its actual admission/routes/counters and external RX providers. The default Loopback path remains available through neutral transport types and compatibility aliases. Runtime wiring and its own source manifest are in `P12-contracts.md`.

The reference transport explicitly binds three nonblocking UDP sockets: Data, ordinary Control/Context, and cancellation. Each has separate kernel queues and external RX pools. The three destination ports are static deployment configuration, not new VITA fields or an inferred port convention. A packet on the wrong socket lane is discarded before route callbacks. Distinct RX pool providers are mandatory. Physical TX slots reserve ordinary and cancellation capacity separately from Data; the per-sender/SID/type Data queue is capped at256 or the configured smaller bound.

`PeerBinding` maps trusted local sender identity to three fixed destinations, and exact configured source address/port/family/scope to remote PeerSession. Canonical IPv4 padding/scope is validated. Payload identities do not redirect responses. Source filtering is a static association check; it does not claim cryptographic authentication. Multiple configured mappings that resolve to the same route are deduplicated; genuinely ambiguous routes are rejected. Fresh generation association batches clone known peer-ID address mappings with bounded preflight/atomic commit. No addresses are learned from incoming packets.

## Datagrams, ownership and scheduling

`try_send` validates CPU-accessible segmented framing with at most68 actual prologue bytes inspected, total word count, counter/SID/type agreement, destination, configured MTU and capacity before acceptance. IQ payload is never flattened into staging storage. On acceptance it commits Packet Count once and owns the complete external `TxStorage`, completion token and credits. Synchronous rejection returns all ownership and never publishes an adapter completion. `sendmsg` gathers up to three regions into one datagram. Successful kernel-copy return releases framework TX ownership and publishes deferred local completion; it does not prove peer receipt. Errors after acceptance consume the counter and publish one failed completion. EAGAIN/EWOULDBLOCK/EINTR retain queued work for a bounded later attempt.

Each `progress_next` services at most one event with six-way rotation over TX/RX for all three lanes. `begin_cycle` resets a limit of64 Data and32 combined ordinary/cancellation send attempts; retries consume attempts too. Runtime calls it once per complete `progress`, so its repeated adapter polls cannot reset the limits. Direct adapter callers explicitly begin the next host cycle. RX remains bounded by the host's finite polling budget (Runtime has two320-event polling passes); malformed discard is one receive event and cannot monopolize other lanes. Completion consumption precedes sending in the Runtime binding.

RX peeks four bytes on its configured socket to check source/lane/declared extent before acquiring the correct external pool. `recvmsg` then receives one whole datagram into contiguous leased backing. Empty, short, inconsistent, truncated, over-MTU, unauthorized, unsupported and malformed packets are dropped with distinct bounded counters. Full envelope/packet/extension validation and correlation lookup precede route callbacks. User-retained RX leases survive adapter and Runtime destruction.

`close()` closes admission and RX delivery while accepted TX drains. `abort()` requests deferred failure for queued unsent submissions. Mandatory `detach()` prevents all future host registry/admission/callback access, closes sockets and settles remaining queued kernel-copy operations without calling application RX. It is idempotent and allows an external metrics owner to outlive Runtime safely. Plain copying POSIX sendmsg has no outstanding device reference to user buffers after the synchronous syscall returns; no zero-copy/DMA mode or invented quiescence proof is advertised.

## MTU, platform and observability

At configured IP MTU1500 the maximum VRT datagram is1472 bytes for IPv4 or1452 for IPv6. Checked adapter admission rejects larger complete packets; it never truncates or silently changes sample packetization. Socket creation enables Linux `IP_MTU_DISCOVER`/`IPV6_MTU_DISCOVER` with DO policy, or Darwin `IP_DONTFRAG`/`IPV6_DONTFRAG` with RFC3542 declarations. Unsupported enforcement fails creation. IPv6 sockets are explicitly IPv6-only. Kernel EMSGSIZE is a terminal configuration/send failure, with numeric errno in the dedicated `Error.native_error` field and stage retained; `Error.offset` remains a byte position.

The POSIX [recvmsg contract](https://pubs.opengroup.org/onlinepubs/9699919799.2013edition/functions/recvmsg.html) requires excess datagram bytes to be discarded and MSG_TRUNC reported; the adapter rejects that whole datagram. Linux's [path-MTU discovery contract](https://man7.org/linux/man-pages/man2/IP_MTU_DISCOVER.2const.html) documents DO enforcement and EMSGSIZE rather than fragmentation. Local SDK definitions and actual IPv4/IPv6 socket tests verify the Darwin path; Linux builds await CI execution; they are not inferred from local success.

Read-only setup statistics retain actual getsockopt SO_SNDBUF/SO_RCVBUF values, bound address, family/MTU and nonblocking/no-fragment status. Metrics distinguish accepted/completed/failed TX, attempts/retries, delivered RX and each framework drop class. Kernel/network drops before adapter receipt are not counted as framework drops; the local implementation does not claim a kernel-drop counter. Separate sockets isolate configured Data flooding from the ordinary/cancellation kernel queues but cannot guarantee delivery over an overloaded or hostile network.

## Native memory and validation

On the current Apple clang/libc++ ABI: `Udp<320,128,128,64>` is200760 bytes; Socket44; PeerBinding120. Factory predeclares200888 bytes including128 bytes of shared-owner allowance. Queue slots, peer table, metrics and all socket wrappers are included. Runtime reconciles this actual charge before invoking creation, including any simultaneously retained default transport structures. Socket buffers are separately configured OS resources: requested262144 bytes each for send/receive, and local getsockopt reported262144 each on all tested sockets (three sockets, six buffers). They are not presented as measured process RSS or hidden framework-owned packet storage.

Developer `udp-dev` tests pass native socket and full adapter IPv4/IPv6 RX/TX, gathered datagram identity, deferred completion and Runtime factory Control/Ack/Context/Data integration. The independent verifier owns literal packet oracles, truncation/source/lane/MTU rejection, queue isolation, send budgets, allocation/lifetime and final sanitizer gates. The additive core Error native-status field preserves existing aggregate defaults and requires the core regression gate. Source/runtime binding has its own implementation evidence. No throughput or30-minute qualification is implied by these functional tests; P13 follows the P12 gate.

## Frozen source hashes
- `adapters/posix_udp/CMakeLists.txt`: `9ec899e5ec76018865ec1a2daf1465d51b42f60d89804ea7c754c37ce5fa1cf9`
- `adapters/posix_udp/socket.cpp`: `3f0b0ef6960c56e9e159018d09fef11f23f94330078e51ac9712b728a223f6bd`
- `include/vita/adapters/posix_udp/factory.hpp`: `84233551a819e37490dbf20d08f6879ff38fdc4f045b29fc0811714182a84599`
- `include/vita/adapters/posix_udp/socket.hpp`: `8a51f36db007513c9f41da8a4088c46c43cb786d0a3719c90fb18c90c9f1e0b4`
- `include/vita/adapters/posix_udp/udp.hpp`: `b45e73059ff67794b852a7b427fe5467cb36e19f8257b82ab010454e23879b21`
- `include/vita/core/error.hpp`: `54719931c279e0032b93af83fd09de349a398550967db32fb6d77698bd91af74`
- `tests/unit/P12/CMakeLists.txt`: `b3c929e0fae2235f81d597a2fa87c526abecbe6f06c458c26bac8023bac72230`
- `tests/unit/P12/adapter.cpp`: `f1ba5c93d7d7269e327d64071e5d359cc70f5dc7b00481ca71ff74c34ba38bcd`
- `tests/unit/P12/socket.cpp`: `8415650e2596ee847c4db75c993dd6ac612d5004385ff1d7de7b232bc45634e3`
