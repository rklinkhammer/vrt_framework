# M2 local integration gate: PASS

Date: 2026-09-18. Accepted specification includes D-P07-1. P00–P08 prerequisites passed independent package gates; P08 full integrated clock scenarios remain part of M3. No independent-peer, GPS/device or performance qualification is claimed.

The coordinator configured and built the combined Debug tree, then ran `ctest --preset dev`: **64/64 checks passed**. This includes the separate specification-arithmetic smoke test, not 190 additional implementation tests. All 39 frozen public headers also compile independently with C++23, exceptions disabled and RTTI disabled. Commands: `cmake --preset dev`; `cmake --build --preset dev -j 4`; `ctest --preset dev`.

The independent P07 gate rebuilt and passed all 24 P06/P07 tests in Debug and ASan/UBSan; three concurrency tests passed TSan, including 100 real races of callback publication/destruction against owner admission. See [P07 verification](P07-verification.md), candidate manifest `949a73727248ae46bc791e1bf0191327ebf89a5f5f65fb0ae53997f5dd54f970`. The P07 report supersedes changed shared transaction-header hashes in historical P06 evidence.

The integrated paths exercise leased Control decode, admission, partial execution, isolated dry runs, backend completion, per-field cancellation, immutable duplicate replay, encoded ordinary/cancellation responses, Controller observations and exactly-once lease reclamation. S1–S4 and S10 are closed by the tests indexed in [scenario evidence](scenario-evidence.md). S5/S11 await generator/runtime integration; S6–S9 and S14/S15 await P09/P11.

Local raw test log: `artifacts/M2/LastTest.log`; source manifest: `artifacts/M2/source.sha256`, manifest SHA-256 `476dda2555ae8d04fda2a962eec730724e9ec54c3486003345514b751b904d89`. These ignored artifacts preserve the gate before subsequent package edits. Host: macOS arm64, Apple Clang 21/libc++.

Resource findings carried to M3: shared duplicate store uses 8,388,608 arena bytes plus 2,129,920 index bytes (520 bytes/entry), requiring a 1,605,632-byte transfer from reference headroom for the index. Controller default storage is 385,024 bytes. Engine stores plans inline and setup-owned result/ticket arrays; do not also charge an uninstantiated generic SlotArena. Complete instantiated runtime budget reconciliation remains required in P10.
