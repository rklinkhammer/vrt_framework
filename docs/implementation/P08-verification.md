# P08 independent verification

Date: 2026-09-18. Verifier: V-P08, independent from I-P08.

**Verdict: PASS for local P08 clock, exact timeline, and boundary-selection foundations.** Real GPS/hardware timing is unqualified; complete transaction scenarios remain downstream integration.

Baseline Git revision `8435ab71d8004e9014a37a63d0f4576ea533252b`; frozen uncommitted source manifest SHA-256 `ad03ddbc777c234a16071ba2d77b0e700b5c3d57fdd8b0a5172106a3b3e03b7e` (sorted `sha256  path\n` records below).

## Oracle and evidence

Architecture §8, accepted timing profile, and literal T1–T6 fixture observations supply expected behavior. Tests do not reuse scheduling decision helpers to compute expected booleans. Exact sample endpoints were derived independently with Python fractions: successive sums of 10^12/r for rates 3,7,11,99999989,99999931; literal integer/residual numerator/denominator triples are embedded in the test. Final denominator is 2309998152000175329; adding the next coprime rate 99999959 must reject before mutation.

| Test | Evidence |
|---|---|
| p08_verify_timeline | Modern absolute epoch retained as seconds+picoseconds; carry/borrow and maximum overflow ; 1 Hz and 100 MHz one-second arithmetic; exact cross-rate residual continuity; checked LCM/resource-limit rejection preserves rate/revision/time; invalid rates and impossible scalar conversion reject. |
| p08_verify_clock | Unbound/acquiring reject Data start; PPS without time-of-day rejects; injected binding explicitly selected; drift+capture uncertainty; protocol clock step increments mapping generation but does not change monotonic deadline; holdover clears calibration, blocks new Data start and permits bounded continuation ; 2 s expiry faults; new qualified PPS relocks; uncertainty overflow rejects. |
| p08_verify_scheduling | Literal T1–T6 production checks; inclusive endpoints; earlier boundary tie-break; stale mapping/committed/backend-unready boundaries excluded; uncertainty envelope exact endpoint acceptance and 1 ns overflow rejection; mode0 ignores requested time and precision lead while faulted, without granting Data permission; modes 1–4 require qualification; bounded candidate count/horizon; fractional sample endpoint adds conservative 1 ps uncertainty and cannot claim a zero-width precision window. |

S5 foundation mapping: `p08_verify_clock` proves old mappings invalidate and independently stored monotonic deadline stays 50 ms after protocol step 1000→2000 seconds at 10 ms. Actual unarmed transaction suspension and timeout notification must be exercised in P06/P07 integration. S11 foundation mapping: faulted clock denies Data while `choose_boundary(mode0)` remains usable. Actual current-state query response during clock loss needs P06/P09 integration. Neither full scenario is claimed complete from helper tests alone.

## Commands and results

Apple clang 21/libc++, Darwin 27 arm64, CMake 4.4.3; no exceptions/RTTI.

```sh
cmake --preset dev
cmake --build --preset dev --target p08_verify_timeline p08_verify_clock p08_verify_scheduling
ctest --preset dev -R '^p08_' --output-on-failure
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan --target p08_verify_timeline p08_verify_clock p08_verify_scheduling
ctest --preset asan-ubsan -R '^p08_' --output-on-failure
```

Both modes pass 4/4 P08 tests (three independent, one developer); executed filters additionally included passing P04 token-accessor regression. Raw logs: build/{dev,asan-ubsan}/Testing/Temporary/LastTest.log (ignored and replaceable). These are serialized value/clock primitives; no P08 concurrency correctness claim or TSan requirement is implied.

## Inspected contracts and limits

Checked arithmetic decomposes elapsed samples/time rather than collapsing modern absolute epochs into 64-bit picoseconds. Residual numerator scaling stays within the checked common denominator; addition avoids overflow before carry. Failed advance/rate change preserves state. Generation replacement is conservative on every qualifying PPS mapping, so downstream work must revalidate rather than reuse stale evidence. Runtime must request a snapshot/current-time-aware Data gate before activity; no-argument state accessors only reflect the last update.

Clock epoch conversion is supplied by the binding/application. No actual GPS capture, host wake bound, or backend cutoff has been measured. Future generator/transaction integration must use qualified clock state, mapping generation, and full uncertainty consistently. No new architecture decision or wire interpretation was introduced.

Integration revision: same shared workspace manifest, no commit created. Relevant later changes require affected tests to rerun.

## Frozen source manifest

```text
1a088395deb9683f29ff74c4dda5e0513a1aab743ff9c4f40d355a39458385d1  CMakeLists.txt
c76159658b171bdfd1b30eec57297f27e44f2f619721dac358780faefc23ecca  include/vita/core/error.hpp
fa0be1dd3b89c2fdf27cc0d4cf584295e18ac5c9ce1a28821020af7d8c2983e6  include/vita/runtime/timing/clock.hpp
0ecf817f6b233addaac4edb5d418b2ece1874965f766a3f474888863ce1b2805  include/vita/runtime/timing/sample_timeline.hpp
74dc772739b2c188f20bb01d22d45e45833248566fc2043ad3cfaf0d007ff201  include/vita/runtime/timing/scheduling.hpp
3015ec14a5d16ee5dd68d86fdf793d3bb016802e83a842c989160d9ee8dfa285  include/vita/runtime/timing/time.hpp
79359409258ac5ba9b75edc90b22069a696e9881aa3f5c9c7b9df7cd30d56290  tests/unit/P08/CMakeLists.txt
9346744dc0d3677fe917436253da33b2136c0471067cf1b7d50c46f754d4d73c  tests/unit/P08/timing.cpp
56dbc0fb0ec47c551f131f6ae560fb7c88add0c77e734fe29c1bd9dd5422071a  tests/verification/P08/CMakeLists.txt
1ed3c4a347bb7bdfcb96a1cc720092d08817033d5392f643c71dadcb087dd26a  tests/verification/P08/clock_contract.cpp
91aa8b4f9bcfe72a30e17cec76c0b34b14978d0488d749b55d24363b7f9ca969  tests/verification/P08/scheduling_contract.cpp
6fb8c30eab9b49f37100c1a3e721a4bf1e31abec3b4cf94f65922c4139992d63  tests/verification/P08/timeline_contract.cpp
```
