# P03 implementation handoff

Scope: external configurable size-class storage, explicit CPU/device capability, move-only leases, shared backing retention, two-level retention quotas, three-segment TX storage and 16-fragment RX payloads. `memory/memory.hpp` is the umbrella; core errors and CPU byte spans come from P01.

`ExternalPool::create(span<BufferSpec const>)` configures external buffers and allocates provider metadata once. The mandatory `BufferSpec::lifetime` keeps the complete backing storage and return-callback context alive. Caller guarantees the declared extent belongs to that owner; a raw address cannot establish allocation extent. Optional return callback is a nonthrowing reclamation hook, executed without internal locks. Its block remains unavailable until it returns. It may inspect the pool or acquire other available blocks. It must not block indefinitely. Domain/address compatibility is checked; CPU-only methods reject opaque memory.

`acquire(BufferRequest)` chooses the smallest available compatible block. Exhaustion never allocates. Move-only leases return a block once after the last dependent reference. Used length is checked separately from capacity. `TxStorage::acquire` rolls back earlier acquisitions on failure. Failed `append` preserves the incoming lease, including bounds and total-size overflow rejection. TX move transfers all ownership; transport acceptance/rejection and quiescence remain P04/P05 responsibilities.

`RxEnvelope` holds at most 18 allocations (16 payload fragments plus separate prologue/trailer). Add buffers, then set checked region references. Prologue must be CPU-readable. Payload can be opaque for compatible adapters; CPU fragment access fails explicitly. `with_payload` supplies a noncopyable callback-scoped `BorrowedBytes`; escaping raw spans or references is invalid use. `retain(consumer, global)` consumes one credit in each distinct quota, shares each unique supporting allocation once, and returns a move-only immutable handle. Quota failure rolls back consumer credits. Quota admission can be disabled for retention-age policy; elapsed-time enforcement belongs to runtime. Handles retain quota/provider metadata and backing storage after envelope, pool frontend and runtime destruction. There is no forced revocation.

Steady-state methods do not allocate: block metadata scanning is bounded by configured block count; reference operations use a provider mutex; quota accounting uses atomics. Setup uses `make_shared`/`make_unique`. Allocation exhaustion during configuration follows the standard library's exception-disabled allocation-failure behavior (process termination); invalid configurations return structured errors before allocation. No runtime heap fallback is present.

Local arm64 sizes: `BufferLease` 32, provider block 80, `RetainedRx` 944, `RxEnvelope` 1032, `TxStorage` 176 bytes. Provider `metadata_bytes()` reports the state and block-array object bytes, excluding implementation-specific allocator/shared-pointer control-block overhead; P04 must charge that overhead separately. Full 1024-handle retention reserve needs 966656 bytes, a 704512-byte transfer above the original 262144-byte category, before additional quota control-block accounting. Queue descriptors must be charged by concrete instantiated type. These are transparent budget inputs, not a claim that the complete runtime has met its 64 MiB cap.

Developer verification:

- `cmake --preset dev && cmake --build build/dev --target p03_memory`
- `ctest --test-dir build/dev -R '^p03_memory$' --output-on-failure`
- `cmake --preset asan-ubsan && cmake --build build/asan-ubsan --target p03_memory`
- `ctest --test-dir build/asan-ubsan -R '^p03_memory$' --output-on-failure`

All pass. Tests exercise shared contiguous allocation retention, independent handles, quotas and rollback, pool-frontend lifetime, acquisition rollback, fragment and TX limits, and opaque-device capability rejection. Independent verification, concurrency stress and downstream transport integration are separate evidence.

P05 integration addition: `ExternalPool::shares_provider_with` compares nonempty provider identity, allowing independent data/control pool validation; two empty pools are not providers and return false. Four P03 developer/independent tests rebuilt and passed after the read-only addition.

Updated production manifest (SHA-256):

```text
e46a7af35ff9a7035569295c0f7d7d8e4ab586c09d489b53b65fa644cadae31d  include/vita/memory/envelope.hpp
9d932caebbe321a6c83b641692677333d0049e8a728e98a0a794b2e634a7e32c  include/vita/memory/memory.hpp
863f73a4a74b6db81285f7440f0ad7cd72eadbb38942c41fe6be13c8f37a6d85  include/vita/memory/pool.hpp
```

## P09 retention extension

P09 adds `RetainedRx::retain(consumer,global)` and the corresponding BufferLease friendship. This permits optional application retention from internally waiting-backed callback views without requiring any retain for immediate borrowed delivery. No public layout changes. P03 ownership regressions were rerun in dev, ASan/UBSan and TSan; P09 adds clone/quota/borrow checks. The new exact source hashes are recorded in [P09 implementation](P09-implementation.md); the historical P03 manifest above remains historical evidence.
