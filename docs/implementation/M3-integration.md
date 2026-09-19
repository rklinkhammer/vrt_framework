# M3 integrated local gate — PASS

P00–P11 are complete for the requested deterministic local implementation scope. The accepted cancellation resolution D-P07-1 is implemented. Independent implementer and verifier ownership was maintained; dependent packages advanced only after their prerequisite gates passed.

## Final integrated candidate

The workspace remains uncommitted, based on `8435ab71d8004e9014a37a63d0f4576ea533252b`. The final [source manifest](artifacts/M3/source.sha256) covers 200 source, test, example, build and fixture files; its SHA-256 is `ad48c8588f68186134cfc49b3ae62fbcef6c75f2daf98f6861e033ab50b8a979`. [Complete CTest log](artifacts/M3/LastTest.log).

Host: macOS arm64, Apple Clang 21/libc++, CMake 4.4.3. Build uses C++23; tests include exception/RTTI-disabled and standalone public-header checks.

```sh
cmake --preset dev
cmake --build --preset dev -j 4
ctest --preset dev
```

Configuration and build succeeded, then **108/108 combined Debug checks passed**. This includes all 47 public headers compiled independently, three public Controller/Controllee examples, and P00–P11 unit, independent verification and integration targets. The documentation fixture target passes 190 arithmetic checks; it is counted as one documentation check, not framework or wire-conformance evidence.

## Independent package evidence

- [P10 PASS](P10-verification.md): 76/76 affected Debug and ASan/UBSan checks, 24/24 targeted TSan checks, 47 standalone headers, unchanged frozen manifest. Public S5/S11 close clock-step timeout and mode-0 operation during Data clock loss.
- [P11 PASS](P11-verification.md): 89/89 affected Debug and ASan/UBSan checks, 19/19 targeted TSan checks, 47 standalone headers, unchanged frozen manifest. S14/S15 close fresh-SID recovery and same-SID rejection. Additional checks cover repeated bank reuse, pinned references, explicit quiescence, exact two-second quarantine, old callbacks, retained data after destruction, source continuity and operational allocation.
- [Scenario index](scenario-evidence.md): all S1–S16 have executable production evidence; W1–W8 and T1–T6 retain their earlier independent evidence. Historical package manifests remain historical approvals; the manifest above identifies this complete integrated tree.

## Resulting behavior and bounds

Public bindings own packet processing, command execution and V/X/S observations, deterministic IQ generation, exact sample timing, Context association, recovery and lifecycle draining. Wall-clock elapsed time drives deadlines and pacing independently of GPS/PPS-conditioned protocol timestamps. Partial execution and the accepted immutable cancellation meaning are supported.

Recovery requires a fresh SID, explicit peer readiness, confirmed state and a qualified clock. Two setup-owned execution banks preserve old results and capabilities; reuse requires checked safety and retention barriers. Recovery preserves sample ordinal/phase, publishes a current full Context before new Data, and does not use timeout as physical-quiescence proof. `pause()` is the source pause operation (`stop()` is its compatibility alias); `shutdown()` performs lifecycle draining.

The independently measured two-bank sixteen-stream reference ledger is **51,668,752 / 67,108,864 bytes**, including **30,998,528 raw pool bytes**. Physical spare backing is charged; shared admission still bounds active work and completion credits. This native-storage ledger includes explicit ownership allowances and does not measure process RSS. Routing supports 32 total association installations across the runtime; initial streams consume this capacity. Exhaustion rejects before replacing an active association. Old SIDs are never reused.

## Scope limits

M3 is a local functional integration gate. P12–P15, POSIX UDP, external peer conformance, real GPS/device integration, Linux compiler execution and measured throughput/latency qualification remain outside this completed request. Production OUI, peer provisioning and hardware clock evidence were not fabricated; deterministic examples use explicit isolated lab configuration. No deployment or conformance claim follows from this gate.
