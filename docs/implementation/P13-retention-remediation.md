# P13 retention placement remediation

Candidate frozen for independent verification. Original normal/overload measurement artifacts are preserved.

The sustained trace exposes deterministic retained-occupancy scaling: normal validation median rises from35µs during0–5s to5.335ms during25–30s, resets to37µs during30–35s, and repeats with the30s retention lifetime. The previous arena placement restarted an entry scan for each occupied extent it crossed, producing quadratic occupied-record inspections.

A setup-owned sorted index now stores one32-bit entry/L identifier for each occupied original or cancellation record. First-fit placement traverses ordered extents once, bounded by2*Entries probes. Successful admission inserts the new index entry with a bounded shift. Rollback, final expiry and erase remove the matching index entries. Existing record objects and canonical/response bytes never move. Logical credit acquisition still precedes publication of the new record; failed capacity/admission does not mutate the index. No retention duration, duplicate key, immutable cancellation, callback ownership or expiration condition changes.

Reference Storage grows from10,518,528 to10,551,312bytes (+32,784):8,19232-bit index entries and two size counters. Runtime's existing sizeof-based duplicate-index ledger accounts for the change automatically; reference duplicate-index charge becomes2,162,704bytes. There is no hot allocation. The read-only last_placement_probes accessor permits reproducible complexity verification without a host-latency threshold. Key lookup and index insertion/removal remain bounded linear operations; this change does not claim constant-time lookup.

Developer validation: direct Clang C++23 ASan/UBSan test fills3,000 retained commands with variable command sizes and stored AckS values, observes exactly4,498,500 total placement probes, expires alternating holes, repeatedly reserves/rolls back in holes, preserves adjacent response contents and pointers, and checks original/cancellation rollback plus latest-terminal joint expiry. Existing independent P07 retention contract also passes against the new header. Fresh sustained performance evidence requires a new independently verified optimized build; earlier failed runs are not relabeled.

## Source manifest

```text
565d8257b72a5ec7646e28846db99c0a548fa2e150754c65d35198946ffef345  include/vita/runtime/transaction/retention.hpp
dcb8aaf320823bfea2df06ca5781869d19d3e3492a6a88f48c5fa2d0c43f4091  tests/unit/P13/retention_occupancy.cpp
a22e1f525520b989d6302ce938cf74e8a1643b92adf7babcd4ad914204818239  tests/unit/P13/CMakeLists.txt
```
