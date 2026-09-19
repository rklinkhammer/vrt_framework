# P16 integration

Post-M5 frequency-scan work is complete and independently verified locally under the approved [contract](P16-contract.md). The final verdict is recorded below; earlier sections preserve historical checkpoints. M5/D-M5-1 acceptance remains unchanged. P15/M6 device work and deployment measurements remain separate.

## Preliminary shared-core checkpoint

The optimized UDP-enabled candidate configured and built successfully with `cmake --preset udp-release` and `cmake --build --preset udp-release -j 4`. The first full CTest run passed 208 of 211 checks. Logs are retained in [P16-core-integration](artifacts/P16-core-integration/). This is a diagnostic checkpoint, not a passed integration gate.

Two failures exposed baseline integration fixtures that marked every state slot known; they must initialize only their original four-field profile, leaving RF absent. The independent public test exposed a substantive delayed-completion defect: old Context/Data could be published at an unresolved promised RF-effect boundary. Its regression is retained while the implementation adds a bounded publication gate. Independent review also found controller-only lifecycle status reporting dormant local-backend quiescence; that must not be presented as remote device proof.

The [independent report](P16-verification.md) records passing scene/CLI, wire/kernel, budget and IPv4/IPv6 profile-routing checks. Public fixes, affected regressions and the dependent examples remain pending. The first build may precede subsequent verifier test additions; final acceptance requires a new build/test run against the final source manifest.

The current measured sixteen-stream core budget is 52,208,928 / 67,108,864 bytes, with an explicit 317,456-byte transfer from the unused standalone-plan reservation. See the [core report](P16-core-implementation.md) for accounting; this is neither process RSS nor performance qualification.

## First publication-gate repair checkpoint

The repaired candidate builds and passes 209 of 212 optimized checks. Direct independent sanitizer tests cover initial/future unresolved boundaries, including a one-second periodic refresh interval, and remote lifecycle status. The aggregate run exposes further continued-progress failures in `p16_core`, `p16_binding` and the existing `p10_example_combined`. These remain implementation defects to resolve, not waived baseline behavior. The [repaired-checkpoint logs and source manifest](artifacts/P16-core-repaired/) are preserved; dependent example wiring remains gated.

## Corrected shared-core Release gate

The next candidate passes **215/215 optimized checks**, including all 71 standalone public headers and the original IQ Generator v1 examples. [Logs and source manifest](artifacts/P16-core-final/) identify this checkpoint. Independent full **ASan/UBSan passes 212/212**, with source identities unchanged; see the [verification report](P16-verification.md). The core gate is approved and dependent example implementation is now authorized to proceed.

The fixes hold Context/Data at unresolved real-effect boundaries, preserve valid prior metadata intervals, and finalize each privately owned Data header's PacketCount immediately before transport submission. Counts still commit only on acceptance. Independent tests hold multiple packets behind Context backpressure, verify sequential accepted counts and compare accepted headers byte-for-byte across later sends. No accepted or retained header is mutated. The earlier failed checkpoints above remain historical evidence; no test assertion was waived to obtain this pass.

Six targeted **ThreadSanitizer checks pass** for pool ownership, asynchronous results, cancellation races, late completion, owned backend lifetime and accepted-header immutability. The [TSan logs](artifacts/P16-tsan/) record a separate `RelWithDebInfo` build with `VITA_SANITIZER=thread`; the verified core headers still match the Release checkpoint. This is targeted concurrency evidence, not a full TSan-suite claim.

## Final example gate: test isolation correction

The first full final Release run passed 228/229 checks. The independent remote-process oracle failed during Runtime setup with native error 48 (address already in use): its port reservation overlapped the simultaneously running silent-peer test. No tune was attempted. Both use a close-before-child-bind discovery sequence, so their test registrations now share the `p16_process_ports` CTest resource lock; the developer process tests use the same lock. Production code and test assertions are unchanged. The [first run and startup diagnostic](artifacts/P16-final/) remain preserved; the following final gate uses the corrected test registration.

## Final P16 local acceptance

**PASS / COMPLETE.** The frozen implementation passes **229/229 Release** checks in 19.29s and independent **225/225 ASan/UBSan** checks in 58.63s. The gates include **71 standalone public headers**, the original IQ Generator v1 examples and affected transaction, Context, timing, lifecycle and codec regressions. All six targeted ThreadSanitizer checks pass again on the final production sources. With POSIX UDP disabled, the combined target builds and both deterministic/wall-clock example tests pass; UDP process targets and tests are absent.

[Coordinator results, source manifest and logs](artifacts/P16-final/) and [independent sanitizer results and process logs](artifacts/P16-final-integration/) preserve the evidence. The coordinator's 379 listed source/build files remained unchanged through the final gate. Only the two documented CMake resource-lock registrations differ from the first final Release candidate. Later package-status/README edits are documentation-only. Historical core and failed final checkpoints above are retained rather than overwritten.

The independent suite covers real Runtime-to-scene effects across skipped production intervals, retained old payload and metadata after Runtime destruction, RF cancellation and unknown-state recovery with a fresh scene epoch, actual localhost Controller/Controllee separation, checked CLI, repeated/continuous scans and monotonic dwell. Typed AckV evidence now permits the application to stop promptly on explicit validation rejection even when AckX/AckS are lost; late or contradictory validation cannot establish execution. An optimized allocation test detects positive C/C++ probes and reports zero allocations for its exercised serialized Runtime/source operation sequence. Terminal logging and uninstrumented process threads are outside that allocation claim.

The full sixteen-stream reference ledger remains **52,208,928 / 67,108,864 bytes**, including the explicit **317,456-byte** unused-plan reservation transfer. Example startup charges are **12,505,328 bytes combined**, **12,258,456 bytes UDP Controllee** and **12,254,688 bytes UDP Controller** on this ABI. Caller-owned scene (64 bytes), scan policy (112 bytes), application stack and stdio/process overhead are not whole-process RSS claims. See [core accounting](P16-core-implementation.md) and [example accounting](P16-example-implementation.md).

Build and run from the repository root:

```sh
cmake --preset udp-release
cmake --build --preset udp-release -j 4
build/udp-release/examples/frequency_scan/vita_frequency_scan_combined --deterministic
```

The default scan has nine points from 100.000 through 100.200 MHz, with 25 kHz steps, fixed 100 ksample/s and 100 ms dwell after matching successful execution and RF readback. [README](../../examples/frequency_scan/README.md) provides separate-process commands; [PORTING](../../examples/frequency_scan/PORTING.md) explains the real SDR completion, clock, sample-boundary and ownership contracts.

No unresolved protocol decision or missing input blocks this virtual package. Continuous mode can stop at bounded identity-retention admission limits; this is reported rather than bypassed. The executables do not choose automatic device recovery state or fresh identities. Results are macOS arm64/AppleClang software evidence using isolated localhost and simulated PPS, not physical settling, production GPS, external-peer, Linux/toolchain or cross-machine performance qualification. M5/D-M5-1 acceptance stays unchanged, and P15/M6 hardware remains separate.
