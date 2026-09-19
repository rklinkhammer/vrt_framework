# P12 independent verification

Status: **PASS for the local P12 software integration gate.** Independent-peer and deployment qualification remain unclaimed.

The verifier owns tests/verification/P12 and this report. The implementer owns the compiled adapter and public/runtime integration. The frozen M3 evidence remains historical. A localhost socket test is local functional evidence, never independent-peer interoperability.

## Independent test design

| Contract | Independent evidence required |
|---|---|
| IPv4 and IPv6 | Actual bound UDP sockets, numeric source address/port authorization, one complete literal VRT packet per datagram; independent peer recvmsg inspects raw bytes rather than round-tripping only production encoder/decoder. |
| Gathered TX | Separate prologue/payload/trailer leases produce one exact datagram; completion follows kernel consumption of application bytes, with exact once return; receiver application need not run or acknowledge. |
| Rejection | Invalid destination/configuration/MTU/segment/domain/ticket/credit rejected before ownership transfer; retry remains possible; no prefix datagram or silent truncation. |
| RX framing | Empty, short, oversize/truncated, malformed-size and unknown-source datagrams are dropped or reported before semantic callback; checked contiguous storage; app-retained payload outlives adapter. |
| MTU and fragmentation | IPv4/IPv6 overhead enforced independently, configured smaller MTU rejects whole packet; checked platform no-fragment options; capability/error if unsupported, no claim of path MTU proof from loopback. |
| Bounded scheduling | Data saturation cannot consume reserved ordinary/cancellation/completion resources; receive/transmit work budgets finite; queued Data cannot indefinitely starve Control or cancellation. |
| Completion errors | OS send failure after accepted ownership produces exactly one local failed completion and safe release; success is local send evidence, never remote delivery or execution confirmation. |
| Callback/domain | No reentrant progress or blocking same-domain wait; callbacks may enqueue bounded asynchronous work; no invocation after callback teardown without retained lifetime contract. |
| Shutdown | Reject new work while draining accepted work; deferred callbacks/resources retain owners; close/release exactly once; no synthetic completion as a substitute for a remaining physical access obligation. |
| Build and budget | Optional compiled target, core-only configuration independent of sockets; adapter itself instrumented under ASan/UBSan/TSan; actual setup-owned metadata charged, no hot heap fallback. |

Use direct OS sockets for peer behavior and literal wire expectations. Deterministic syscall injection may supplement naturally reproducible OS error cases; it cannot replace successful actual IPv4/IPv6 traffic. Native SDK headers expose platform no-fragment options, but the gate must verify the compiled adapter handles setsockopt failure instead of assuming availability from macros alone.

## Qualification boundary

The P12 local gate does not supply an authorized production OUI, GPS/PPS deployment binding, independent peer, Linux qualification host or network path-MTU capture. Applicable missing inputs remain explicit in the M4 deployment report. P13 measurement must preserve actual monotonic receive/validation/record timestamps and its named inline virtual-register backend; localhost throughput alone cannot satisfy the independent-peer requirement.

## Independent executable evidence

| Executable | Oracle and boundary |
|---|---|
| `p12_verify_factory` | Overbudget preflight before creation; returned owner/metadata/slot/capability mismatch rejection; valid and rejected retained owners detach before borrowed host state dies. |
| `p12_verify_framing` | Literal signal packet over every two-split combination; literal maximum 68-byte UUID command prologue across segment boundaries; malformed type/length rejection. |
| `p12_verify_socket` | Actual IPv4/IPv6 native peer traffic, exact gathered datagram bytes, local completion before peer application read, retained RX after adapter destruction. |
| `p12_verify_receive` | Real empty/short/mismatched/truncated/oversize/wrong-lane/unauthorized datagrams and distinct drop counters; no semantic callback. |
| `p12_verify_isolation` | Six Data slots full while independent Control/cancellation slots admit; round-robin services all three lanes; accepted abort failures drain once and release credits. |
| `p12_verify_rx_isolation` | App-held Data exhausts its RX pool; sixteen queued Data drops do not block a separate Control socket/pool callback. |
| `p12_verify_mtu` | IPv4 MTU100 exact72-byte payload and IPv6 MTU1280 exact1232-byte payload; next word rejected whole, ownership/count retained, no prefix datagram. Checked socket setup reports no-fragment and nonblocking options. |
| `p12_verify_syscall` | Actual IPv4 kernel EMSGSIZE from oversized UDP request; native_error/stage/retryability retain correct meaning and byte offset remains zero; no peer datagram. |
| `p12_verify_cycle` | 65 Data and33 Control/cancel queued; one cycle attempts at most64/32; repeated inner polling cannot replenish allowance; next host cycle drains remainder. |
| `p12_verify_runtime` | Public Runtime with real UDP factory plus independent native IPv4/IPv6 Controller: literal query yields exact AckS field values/MID/IDs, Context and IQ bytes; retained factory owner remains safely detached after Runtime destruction. |
| `p12_verify_callback` | Actual RX callback rejects recursive adapter progress while allowing bounded asynchronous TX submission; completion drains normally. |
| `p12_verify_allocation` | Ordinary/aligned allocation instrumentation around100 accepted, completed and peer-read native UDP sends; no operational allocations after setup. |

The native peer uses direct POSIX socket calls and literal bytes, not the adapter endpoint/parser/encoder as its expected-value oracle. Bounded100–200ms polling only accommodates asynchronous host delivery; it is not a latency measurement. Context submission precedes dependent Data at the framework boundary, but network arrival order is not assumed across UDP lanes; the tests require both observed packet classes without falsely claiming ordered UDP delivery.

Independent review reproduced rejected-factory cleanup with an externally retained shared owner: a metadata mismatch previously returned before detach, leaving borrowed HostBindings at risk. The owner repaired rejection cleanup and documented factory-error cleanup obligations. A separate coordinator finding moved native errno out of Error.offset into native_error; the real EMSGSIZE oracle preserves that regression. Adapter subdirectory registration now occurs after sanitizer configuration; the final gate checks compiled socket.cpp flags explicitly.

## Frozen candidate

Uncommitted workspace above historical baseline Git revision `8435ab71d8004e9014a37a63d0f4576ea533252b`. Complete manifest below:81 files, SHA-256 `b4c8830437b12becad8f8c458f95c0703b90387fee6645922fc3a578035894cf`. It includes all public headers, compiled POSIX source/build file, P12 developer/independent tests, examples and build/preset configuration. Earlier M3/P11 reports retain historical source and budget evidence.

```sh
cmake --preset udp-dev
cmake --build --preset udp-dev -j 4
ctest --preset udp-dev --output-on-failure
cmake --preset udp-asan-ubsan
cmake --build --preset udp-asan-ubsan -j 4
ctest --preset udp-asan-ubsan --output-on-failure
cmake --preset udp-tsan
cmake --build --preset udp-tsan -j 4
ctest --preset udp-tsan -R '^(p12_|p03_verify_pool|p04_verify_ticket|p07_verify_guard_race|p09_verify_revisions)' --output-on-failure
```

Debug full aggregate: **123/123 passed**, followed by the added callback regression **1/1 passed**. ASan/UBSan full aggregate: **123/123 passed**, followed by the same callback regression **1/1 passed**. Targeted TSan: **20/20 passed**, including all sixteen P12 developer/independent tests plus actual pool/ticket/late-callback/revision concurrency regressions. Standalone header compilation in the full gate: **53/53 headers passed**. The full aggregate also checks core-only configuration without the optional socket target.

The final verifier-only callback addition changed only its source and P12 test registration. It was reconfigured/built/run with `ctest --preset udp-dev -R '^p12_verify_callback$'` and the corresponding `udp-asan-ubsan` command; TSan included it in the aggregate above. All production files retained their frozen hashes. The final81-file manifest was checked after the gate.

Ninja compile rules for `adapters/posix_udp/socket.cpp.o` contain `-fsanitize=address,undefined` and `-fsanitize=thread` in their respective builds, alongside C++23/no-exceptions/no-RTTI flags. This validates instrumentation of the compiled syscall implementation, not merely test callers.

The repeated default Runtime reference measurement is **51,669,128 / 67,108,864 bytes**, including unchanged30,998,528 raw pool bytes for16 streams; P12 neutral binding contributes the difference from the historical P11 ledger. P13 must reconcile the actual selected UDP/benchmark composition and OS socket memory separately. This is not an RSS or performance claim.

Local environment remains macOS arm64, Apple clang21/libc++, C++23 without exceptions or RTTI. OS socket buffers are outside the framework arena and must be reported separately by P13; no kernel-copy-free claim is made.

## Source manifest

```text
c83fde3afed97852bf93ce72803dac62629f7307d7b1557c964c57851fcc27b7  CMakeLists.txt
6eb16450f71f165753e68598341f4d525fb50463cee18bed5aabe56add00b681  CMakePresets.json
9ec899e5ec76018865ec1a2daf1465d51b42f60d89804ea7c754c37ce5fa1cf9  adapters/posix_udp/CMakeLists.txt
3f0b0ef6960c56e9e159018d09fef11f23f94330078e51ac9712b728a223f6bd  adapters/posix_udp/socket.cpp
8e3e43982920cf7b42f935950b85e760e5ec1dbb1afc00998273709b716fe9f5  examples/CMakeLists.txt
4a63cfea2086e9543814d0cc565acacf683368c4cdadf52f2156b5969578b086  examples/combined.cpp
8c1aa7f302d8221c7679d02980a2bc6eae1aa973f9935e4be8768dde8cf5164b  examples/controllee.cpp
e99bf34c050c3d2e7648bb09a5d37c3a5ea1829ea87565497b542ce4c02e4a81  examples/controller.cpp
b0a5a6d7b6c423fb757c1f285dc2dd2bbf9be20e677c4afdf0bd0246ffe8577b  include/vita/adapters/loopback/loopback.hpp
84233551a819e37490dbf20d08f6879ff38fdc4f045b29fc0811714182a84599  include/vita/adapters/posix_udp/factory.hpp
8a51f36db007513c9f41da8a4088c46c43cb786d0a3719c90fb18c90c9f1e0b4  include/vita/adapters/posix_udp/socket.hpp
b45e73059ff67794b852a7b427fe5467cb36e19f8257b82ab010454e23879b21  include/vita/adapters/posix_udp/udp.hpp
150fa936fd99a1bc6cefdca27a5625a997efe75e6f9b2a4804367b03232d7000  include/vita/codec/layout.hpp
0b7291cb2658cfe8783133f20f454a4933f0f92da55f424a5d3eae4c8b08553e  include/vita/codec/packet.hpp
9f84b53e42b28ee21303541141a0bb4dafa63101cb1f2c12ae145b4b3c9d5e13  include/vita/codec/prologue.hpp
3db37f96f7f61603aaa6f909959172adaa3c7a583d1fb5f3da021ffbf3f80298  include/vita/codec/samples.hpp
cc2e6266f8802128e1ab4ead5e391ce22f5a342ca54edb63fed39b219253ba0a  include/vita/codec/wire.hpp
ed9510455f242e53a3f0f9316890fdea1de5a4f8cd7f9dfeb41e28bed9d77277  include/vita/core/bytes.hpp
c7090fadba63f24786de0de12c2cda91c0069124db1b7b934949ee88a5c530eb  include/vita/core/capacity_policy.hpp
54719931c279e0032b93af83fd09de349a398550967db32fb6d77698bd91af74  include/vita/core/error.hpp
9add4a941546a64ae088f55985554e74a886221c3501e23d94a1b088b91a2eaf  include/vita/core/fixed_vector.hpp
7d0a8c6896e0eb4d76d233b021cdd34731282ced6882a580e907c58a09636ab9  include/vita/core/version.hpp
606b0db7ee2f0a0cc5f776e8846143f2dd865abf8c9f24cb86a7ee63c311c655  include/vita/fields/arena.hpp
be20b065970cc2d188112ca22dc4922f0850d07a6afd88e6f1c030ed13c5fe2d  include/vita/fields/packet.hpp
2f02a01694f87107b6f69cd06289b6a5cf4715e9d821dd871138d69fd7f6eb65  include/vita/fields/types.hpp
5ede1e74572f0bea379019596176dac7cd31e98beec7b2c09690669a2511eeb1  include/vita/memory/envelope.hpp
9d932caebbe321a6c83b641692677333d0049e8a728e98a0a794b2e634a7e32c  include/vita/memory/memory.hpp
62bc0de8e852809af10007d0ac4e2eb944207d5efc611135273fecb433e6cdb7  include/vita/memory/pool.hpp
587c16a9dab2e2d1b6ac4e17178656bcb5a2f2453dbabd7d729d087e14106197  include/vita/profiles/iq/lab.hpp
613adb3676968a633fefbd3c35f1e5df7faebdfc920b06863c5c94b188a64079  include/vita/profiles/iq/source.hpp
be79298527d02902e84874af72e33d92a117a5b76e2378e18b6a80592087b3a0  include/vita/runtime/budget/loopback.hpp
e7dc8211935e6c102c60ff9145dfe2686c1b13d4ebe9ec517ca92c849b5c5bbb  include/vita/runtime/completion/ticket.hpp
3c1d1469e8fde9cbef9812dc0f1f9b1b01df5e7c491b714a94db39002acb3999  include/vita/runtime/context/budget.hpp
3ede591ebdee7c55531ba249ae85159b47a8e4196892f172b64b024089e207ba  include/vita/runtime/context/publisher.hpp
c042b1460c46401007660ef941a5574535bdd419d699134f5afe1b37f2b8b6a7  include/vita/runtime/context/receiver.hpp
e68c39b0b74b64d5991d8e4b405e81b63405c9e31417d10c629e2dc918897f27  include/vita/runtime/context/revisions.hpp
66cc03f6fb3ab2716c967fa00aaaf343a9170376e6ec56ac1b6b40675fd45be7  include/vita/runtime/execution/admission.hpp
ed33dbd7a3917f296f568aff824e2c2b9c0f086550bb58bacd185a6b498e345a  include/vita/runtime/execution/arena.hpp
8b5279aaf8b40a1a7295b5bf0f7d86b1f847de03bbdce5e644dd268cc0a8e7d7  include/vita/runtime/execution/budget.hpp
354e99cc3c6f17906f5c24bb72409d9da0dff6362373cd5eb783108891631fcf  include/vita/runtime/execution/executor.hpp
8c3cc067511769f582d615978d79e3daa7c3c346bde5ef2cc449631b22887eec  include/vita/runtime/execution/operation.hpp
3de727b014566b0f4566f5517f0cc3e4581592ba14f97c0622136e6a83710c09  include/vita/runtime/public/config.hpp
354fd2537248e81c6df42709fed6001498b67755eca6b31710c994fda994f0bb  include/vita/runtime/public/runtime.hpp
7772e2d134d9c844f2f0d7dc6497505fb7b6a114158772854c8cc41722da6ad0  include/vita/runtime/state/contracts.hpp
f8c9deabec3ebc4e5be0aa128f80855b5714523090d6f7d1011bd500261ad85f  include/vita/runtime/stream/counters.hpp
c2cf518ca8d271a8f12a722fce0df1bceb510561f90a6100413670b154383a32  include/vita/runtime/stream/routing.hpp
fa0be1dd3b89c2fdf27cc0d4cf584295e18ac5c9ce1a28821020af7d8c2983e6  include/vita/runtime/timing/clock.hpp
3fe07bad5ee65a63690be0b1d9f99dcc22930eb57cbcc618a9fba81b8980d08e  include/vita/runtime/timing/sample_timeline.hpp
74dc772739b2c188f20bb01d22d45e45833248566fc2043ad3cfaf0d007ff201  include/vita/runtime/timing/scheduling.hpp
3015ec14a5d16ee5dd68d86fdf793d3bb016802e83a842c989160d9ee8dfa285  include/vita/runtime/timing/time.hpp
c387bba553de811c0923c3536409be18e70d7ce884b1b6e60c8203060b088bdd  include/vita/runtime/transaction/backend.hpp
4ec208ef16dccb2233a7ec0269eff73dc641d1b63927209ac432fccedf284dda  include/vita/runtime/transaction/cam.hpp
bb9a8f750564195ea2ca16e7eb75d57e41dd26e22a5d4fb48cf48b57dc04f383  include/vita/runtime/transaction/cancellation.hpp
f983c5bcce955f63215a6e8d721386a57e4c9f95ced57ba7487bebf95a7026a9  include/vita/runtime/transaction/controller.hpp
206bd77105ffc197334d685b8ed076c41c470feffe8acbea7ff0872e551ede4b  include/vita/runtime/transaction/engine.hpp
1d33f2cc8aa5efe33461c9235d22e432c63d7b4142a1f31b166fb270ebe76787  include/vita/runtime/transaction/manager.hpp
4bdf0c2e86e5d677ee5df70a874ea5df8e4c9e29f02004772dc8040c1cf3acae  include/vita/runtime/transaction/outcomes.hpp
831b6a40cbbd864057094db440ef33951e06f2344b8f9c16c739fa5fbf97f12e  include/vita/runtime/transaction/retention.hpp
ac1c82c381a5b6e4f83e251f26c291f4ca1c676cff14c39e10d16b35ecdefc57  include/vita/runtime/transport/binding.hpp
b301edcfec8b22f70672e892ad1e83b6d889086f09273bbc25c95a5bbd98af74  include/vita/runtime/transport/framing.hpp
af12faf53e6581985add7ff75f8c20eefac950e972d64fd85cb7541bbe6c1411  include/vita/runtime/transport/types.hpp
b3c929e0fae2235f81d597a2fa87c526abecbe6f06c458c26bac8023bac72230  tests/unit/P12/CMakeLists.txt
f1ba5c93d7d7269e327d64071e5d359cc70f5dc7b00481ca71ff74c34ba38bcd  tests/unit/P12/adapter.cpp
705cdf0f855f0b5c44d1a5b34718601ef39e10dbb0116cf87aad9935ab4b9d03  tests/unit/P12/binding.cpp
9933685d1265b9ef8d256b3f1a6ffbaca9d9d53a6b94958e368a75cbe283ea59  tests/unit/P12/runtime_binding.cpp
8415650e2596ee847c4db75c993dd6ac612d5004385ff1d7de7b232bc45634e3  tests/unit/P12/socket.cpp
942a62a8c6fb69d674590b62fdb5fdcc0d834476853bae065750ac057ebfa0f0  tests/verification/P12/CMakeLists.txt
2941f4f3c732d2613b44e6aaf687b745a17676132199a56f975d8a98c1b827c2  tests/verification/P12/allocation_contract.cpp
74ae7004e35314804eb0c47f2ffb1efc52906acce0f4b360c30a8389504f9481  tests/verification/P12/callback_contract.cpp
b50aa509a1b34b5ef8be3da97a6e0b48e892b444f388e3f6b7c3bbe6ae9c66a5  tests/verification/P12/cycle_contract.cpp
e86a826ab2c6c1cf4514542eae500877b8bf0af039271e52766aea6c87191154  tests/verification/P12/factory_contract.cpp
5512dd12a036e930ae0b6b2175a9fe1165fcae110f2941a67b85b06115cd8c4a  tests/verification/P12/fixture.hpp
b57a92c6a070680b7bf2e1816d542115ce56e4db8f5743ee6f417a27a5396b2c  tests/verification/P12/framing_contract.cpp
aae9668832e87c478a5d220bd7a145915de56b3263d8be4ac34fbadba89ab0d7  tests/verification/P12/isolation_contract.cpp
5886553e41cc8bfc14097dc8b0dccdf77adbfc0c6d1ca43b9ca4ebfa89b694c5  tests/verification/P12/mtu_contract.cpp
2e5d4ae1b8b2f9d70edcbe6cbacffae6b25ab6f97dd72cfa30dba12c00605a97  tests/verification/P12/peer.hpp
f270ee0bf6a864c495d4d2b49df1a7b1913e823a634f8e798cabe1833620f2fa  tests/verification/P12/receive_contract.cpp
fa8fec65c276df21049ee96220ea5d169ee23c1d050a68954aff6b0a9f33291b  tests/verification/P12/runtime_contract.cpp
7667cc78ba7d745f8a2a438c2b8265622156d567293b6bfdc8ad50bed4c93578  tests/verification/P12/rx_isolation.cpp
33a25ce21b0030d17a4d8b8f273db033478bf35422ebc44877dc17a421c2f431  tests/verification/P12/socket_contract.cpp
d5706a9987cb5c9e01bf15a81be063301351184576d4989c9fbd36aee8e99530  tests/verification/P12/syscall_contract.cpp
```
