# P13 payload preparation comparison

Candidate frozen for independent verification. Default packetization and normal workload remain256 IQ16 sample pairs per packet, four1MS/s streams and the existing100command/s plus burst workload. No Runtime or pool implementation change is included.

`--payload-mode generated` uses the canonical generated source. `precomputed-copy` copies one precomputed1024-byte canonical period-aligned packet into each acquired external payload lease. `prefilled-pool` initializes every payload block at setup while retaining all8192 leases, then releases them together. This avoids repeatedly initializing only the first available block. Its per-packet provider only checks format/extent, phase and registered block membership, then declares initialized coverage; it performs no payload fill or copy. Reacquisition, packet headers, timestamps, transport submissions, kernel copying and lease return remain the common production path. All modes reject anything other than IQ16/256pairs with first ordinal divisible by16. Because256 is a multiple of the canonical16-sample period, skipped whole packets preserve the same bytes.

The minimal `SampleWriteWindow::complete_from_wire()` API declares an already initialized full extent, including bulk writes or prior pool initialization. It validates semantic wire samples before marking coverage, so nonfinite float bytes cannot bypass validation. Runtime still calls its normal `validate_complete()` check afterward. This is not an unchecked payload or transport path.

Every native peer Data packet in all three modes receives the identical full1024-byte canonical content comparison in addition to framing/time/size checks. Prefilled mode adds a bounded binary search across registered block addresses to reject an unrelated external buffer. The application must keep the provider and supplied pool alive for the producer lifetime and must not mutate preinitialized buffers through another producer. The benchmark Run owns the provider before its Runtime member, so Runtime teardown occurs first.

The source provider is application-owned callback state:66,608bytes, reported separately from the framework ledger and excluded from `peer_application_bytes` to avoid double counting. Setup rejects if framework plus source exceeds64MiB. The latest prefilled smoke reports55,305,480framework bytes and55,372,088combined bytes. The three actual1MiB thread stacks retain their original accounting. Temporary prefill lease storage is262,144bytes at setup and is released before measured threads start. Mode, setup block count, setup scratch, producer call count, payload bytes written and copy calls appear in summary JSON; operation counts explicitly cover warmup, measurement and drain. Non-generated component-mode combinations reject rather than silently measure generated work.

Developer direct Clang ASan/UBSan test covers all32 blocks of a small caller pool, repeated reuse, exact canonical identity, mode-specific write/copy counters, phase rejection, invalid enum/name rejection and nonfinite-float bulk completion failure. Prefilled full-reference smoke completed84commands/420trace rows, initialized8192blocks and produced3100packets with zero payload writes/copies, zero peer payload mismatches and zero observed critical C/C++ allocations. This short smoke is functional evidence, not a throughput qualification or comparison conclusion.

## Manifest

```text
28f0f443ab4585178260ab1290ab4456e057582e95e33a4624b08272cb7087c6  include/vita/profiles/iq/source.hpp
ffbbb8670c272e2725cc4085eb150e2f1fe38b25ff41f7e77d158ee17c4b6fb3  bench/payload_source.hpp
5b210fc84598fbed2840c00d8da7834df91befbb79d4c7b4e500bd5394d5f224  bench/main.cpp
cd4a60029750fa3d0b1fdfcfda7b9403b0b067c6ddbbc2b54239477d7f794ce5  tests/unit/P13/payload_modes.cpp
970d576e0c7832b38fbb4d0e91882530359d8003713ea87c5eacbf2b6205be86  tests/unit/P13/CMakeLists.txt
```
