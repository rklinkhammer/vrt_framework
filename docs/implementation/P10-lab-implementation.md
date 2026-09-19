# P10 isolated lab setup implementation

Implementer: contracts agent. This subtask provides setup helpers only; Runtime creation, complete budget aggregation, operational scheduling and package verification remain with their respective owners.

`vita::profiles::iq::lab::config(fixture_oui)` requires an explicit caller-supplied 24-bit OUI. It marks RuntimeConfig as isolated lab and configures an explicitly injected GPS-epoch clock with declared zero model uncertainty/drift and deterministic timing capabilities. It does not invent production OUI/stream/controller identities or claim GPS/device qualification. Stream identities remain explicit Runtime/StreamConfig inputs.

`lab::pools(PoolCounts)` creates independent providers for header, payload, trailer, ordinary control, cancellation, RX Data, RX control, RX cancellation, and the separately reserved emergency lane requested during P10 integration. Each provider owns one 64-byte-aligned CPU backing allocation. Shared provider/block lifetime retains the backing until the final lease is released, even after the returned ExternalPools frontends die. Applications do not supply buffer-return closures or manually return buffers.

Defaults use 32 blocks per role. Block sizes are 128-byte header, 64-byte trailer, and 2048 bytes for every other role. Payload and RX Data block widths are configurable positive multiples of 64 (for example, larger RX blocks when increasing the IP MTU); source packetization/MTU enforcement remains Runtime-owned. Counts must be nonzero. The helper does not advertise these small lab pools as the complete architecture reference configuration.

`measure(counts)` validates all roles, multiplication/addition overflow, and the explicit `max_setup_bytes` cap before any allocation. The default setup cap is 64 MiB for these pool objects only; this is not a claim that a complete Runtime also fits. Invalid counts/sizes/caps reject before setup allocation begins. Aligned-backing allocation failure returns a resource error; allocation failure in standard shared/provider metadata follows the existing project's no-exception setup behavior. Partial successful setup uses RAII and rolls back safely.

Default actual memory on the current platform: raw bytes 464896; provider metadata 23832; the nine returned pool frontends 144 bytes. Metadata matches the sum of existing `ExternalPool::metadata_bytes()` and raw storage matches `raw_bytes()`. `measure` uses the same concrete provider/block sizes for preflight. Shared-pointer/allocator infrastructure is excluded under the architecture's infrastructure exclusion. Runtime must count its own pool frontend storage once; it must not add the measured frontend number on top of an already charged containing Runtime object. Application-held copies are application storage. No operational allocations occur in acquire/return.

Developer tests verify explicit OUI bounds and isolated configuration, invalid-count/overflow/unaligned-width/setup-cap rejection without allocation, all nine provider identities distinct, 64-byte alignment, actual raw/metadata accounting, 1000 acquire/return cycles without ordinary or aligned allocations, and a lease surviving destruction of every factory frontend. Direct C++23 development, ASan/UBSan and TSan builds passed with no exceptions/RTTI. The main P10 owner registers `p10_lab` in the shared unit-test CMake file.

Frozen source SHA-256:

```text
2fe9edd24f1e7dfc9a958fc8d2e347d174ce8e84adb74c8a28882248053e0943  include/vita/profiles/iq/lab.hpp
c646547fe2d0f0e2115fb056c8a77da0d851821ff141073c0a247e7b624936bb  tests/unit/P10/lab.cpp
```

## Reference-pool adjunct

Main P10 integration requested an optional tenth structural-test provider. `PoolCounts::large` defaults to zero (the only optional zero count); nonzero counts use configurable `large_bytes`, default8192. `ExternalPools::large` is separately owned and accounted even if no normal packet uses it. `reference_counts()` selects header4096, trailer1024, payload8192, control1728, cancellation64, emergency256, RX Data3584, RX control448, RX cancellation64, and large128. This yields exactly30998528 raw bytes,1567600 provider metadata bytes, and160 frontend bytes on the current platform. It matches the architecture's raw total; complete Runtime fit still requires the Runtime's actual aggregate budget. Small lab raw/metadata values stay unchanged, while its containing frontend now occupies160 bytes.

Developer tests additionally check optional omission, configured large-provider acquisition, and exact reference raw measurement. Dev, ASan/UBSan and TSan direct tests passed. Corrected hashes supersede the earlier helper/test entries:

```text
587c16a9dab2e2d1b6ac4e17178656bcb5a2f2453dbabd7d729d087e14106197  include/vita/profiles/iq/lab.hpp
f3a573e8f1ead7ebf56e6aa9b84399911f564421f238dc806e22fd1fe561f461  tests/unit/P10/lab.cpp
```
