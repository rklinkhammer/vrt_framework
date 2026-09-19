# V-P14 registered extension verification

Status: PASS for the live registered extension interface, independent all-family checks and full ASan/UBSan regression.

Scope: bounded registered extension codec/dispatch interface, all four extension families2/3/5/7, with existing checked common prologue validation. Independent source review used the supplied ANSI/VITA49.2-2017(R2024), §§6.4 pp80–81,7.2 p87 and8.6 pp122–123. Permissions8.6-2/3 explicitly permit custom CAM bits7..1; bit0 remains reserved despite introductory prose saying lower8. The common wire validator now enforces that mask for both Control and Acknowledge extension packets. This is a correction to the shared prologue validator, not permission to reinterpret other CAM bits.

`p14_verify_extensions` builds independent literal packet bytes for each family, with and without SID as required, plus extension Control and Ack with different declared custom-bit masks. Both extension Data families have literal trailer vectors. The validator uses the complete checked wire, matching payload address/offset and trailer after reparse, on both encode and receive. This catches the earlier isolated draft inconsistency where encode supplied empty wire; the producer repaired that contract before live promotion.

Independent checks include:

- Exact OUI/information-class/packet-class/family identity; unknown and classless packets stay opaque, with no semantic dispatch capability.
- All truncated prefixes, malformed payloads and insufficient work; callbacks only after framework checks; registered callback budget failure cannot execute semantics.
- Literal CAM bit0 failure through `decode_envelope`, registry validation, `encode_envelope` and registered encode. Every custom bit1..7 is tested against separate Control/Ack masks.
- Per-dispatch authorization/admission, denied admission without semantic callback, repeated admission on repeated dispatch, and rejection of a capability created by another registry.
- Preflight short-output, forbidden timestamp options and insufficient framework budget without encoder calls; unchanged short output. Once a registered encoder or post-encode validator fails, output may be partial as explicitly documented, but no semantic dispatch occurs.
- A measured all-family loop uses no ordinary/aligned C++ allocations. Deliberate positive probes exercise both hooks. This does not instrument arbitrary C allocators or claim process-wide allocation freedom.

Direct command passed:

```
clang++ -std=c++23 -O3 -UNDEBUG -fno-exceptions -fno-rtti -Iinclude tests/verification/P14/extensions_contract.cpp -o /tmp/p14-live-extensions
/tmp/p14-live-extensions
```

[Frozen production/verifier hashes](artifacts/P14-extensions/source-and-verifier.sha256) identify the tested working tree. Registry/context/wire lifetimes remain explicit borrows. Registered callbacks are trusted host code; the framework cannot preempt a callback that ignores its budget or allocates internally. This interface does not implement deduplication, transaction ordering, cancellation, completion or cleanup of host admission reservations. Those remain the caller's transaction contract.

Fixture OUI/classes are isolated test values. No deployed vendor payload, independent-peer interoperability, Linux/device qualification or complete P14/M5 coverage is claimed from this gate.

## Final sanitizer gate

The full rebuilt ASan/UBSan suite passes **171/171** in26.33 seconds, including both newly promoted extension and raw-sample test groups. [Final results](artifacts/P14-extensions/test-asan-ubsan.log), [build](artifacts/P14-extensions/build-asan-ubsan.log) and [corrected-unit rebuild](artifacts/P14-extensions/rebuild-unit-asan-ubsan.log) preserve the evidence. Commands: configure/build `udp-asan-ubsan`, followed by `ctest --preset udp-asan-ubsan --output-on-failure`.

The first aggregate run had one failure: the promoted developer test still expected `encode_envelope` to accept reserved CAM bit0. The coordinator corrected that obsolete expectation after the independent verifier flagged it. The independent malformed literal still proves decoder rejection, rather than relying only on the repaired encoder. The [provisional failure](artifacts/P14-extensions/test-asan-ubsan-provisional.log) is retained; the corrected unit was rebuilt before the final complete run. No production repair was required beyond the planned common-prologue mask correction.

All [extension production/verifier hashes](artifacts/P14-extensions/source-and-verifier.sha256) and [integrated sample/unit companion hashes](artifacts/P14-extensions/integrated-companion.sha256) match after final execution. Prior protocol, ownership, transaction, timing, lifecycle and reference-budget regressions pass in the full suite. Raw-sample semantic coverage remains documented by its separate independent verifier; inclusion here is integration evidence only.

The coordinator's final optimized Release gate passes **174/174**, including62 standalone public headers. [Release results](artifacts/P14-samples-extensions/test-release.log) and the companion frozen manifest identify the integrated checkpoint.
