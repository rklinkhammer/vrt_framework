# P06 response and observation implementation handoff

Owns `runtime/transaction/outcomes.hpp` and `tests/unit/P06/outcomes.cpp`; AckRecord layout and encode_response signature are unchanged from the jointly agreed interface.

A known response timestamp requires an explicit non-none epoch, valid fractional picoseconds and a representable 32-bit integer second. Encoding uses that epoch and TSF picoseconds even if the request used another timestamp representation. Unknown time emits no fabricated timestamp. Timing status is independent: a no-effect timed failure retains AckT=7 with no actual-effect timestamp, while an originally timestamp-free Control requires Ack timing0. The accompanying narrow generic wire correction is documented in the P02 report.

ControllerObserver distinguishes current phase evidence from proven real execution. Local send, validation, state observations, no-Ack silence and simulated execution leave the remote execution outcome unknown. Only full nonpartial real action2 AckX with SchX and no indeterminate/timing-failure indication sets confirms_execution. Success additionally requires arrival before local timeout. Partial/failure reports conservatively remain unknown at this aggregate observer layer; per-field detail belongs to the transaction result interfaces.

`observations()` exposes fixed-capacity per-phase evidence, `timeout_observation()` preserves an inspectable timeout, and `timed_out()` remains latched. A late Ack has kind late_response and its original response_kind. It may confirm later execution evidence but never changes the timeout or claims deadline success. Identical phase duplicates coalesce. Contradictory phase evidence marks that slot contradictory, unknown and unsuccessful. There are at most nine phase keys in the ten-element array; no runtime allocation or eviction is required. Opaque/context-required diagnostics are rejected until correlated decoding supplies semantic context. Cancellation observations are explicitly unsupported here and remain P07 work.

Correlation here is deliberately Message-ID-only for the isolated P06 observer, not a secure or complete wire transaction key. P07 must scope observations by peer/session/association, endpoint identifiers and generation before forwarding responses. This helper alone does not claim protection against a same-ID response from another peer.

Developer test compiled and passed directly with C++23, exceptions/RTTI disabled, in Debug and ASan/UBSan. It checks validation/simulation ambiguity, retained timeout plus late evidence, duplicate coalescing, contradictory phase results, mismatched ID, explicit epoch replacement, seconds/fraction bounds and no-effect timing status. The registered `p06_outcomes` CTest target subsequently passed in Debug and ASan/UBSan, including the outgoing-count overload. Independent verifier owns its separate literal response tests.

Outgoing count integration: the overload `encode_response(ack, output, outgoing_packet_count)` validates 0–15 and replaces only the outgoing envelope count. It never mutates the stored incoming request. The original two-argument signature delegates to zero for isolated codec tests. Transport binding must pass its sender/SID/type counter value and commit that counter only after accepted send. Developer regression proves incoming count5 produces explicitly requested outgoing count0 and rejects16.

Explicit ordinary-observer scope regression: a same-Message-ID cancellation AckX with SchX set is rejected with unsupported_capability and never confirms original execution. The existing production guard is unchanged; the developer CTest passed with this literal header-L-bit case.

Frozen manifest (SHA-256):

```text
19a7bc51e19cc95f7ed38cb8c0ae2acb6573ec04fd2ee925c894a0c8d152bdbc  include/vita/runtime/transaction/outcomes.hpp
df2debb691ed3b2dc07e7cb8cde6a01e27813baff32290e654300781a12b5105  tests/unit/P06/outcomes.cpp
```

## P07 extension

The previously frozen ordinary observer rejection of cancellation is intentionally superseded by the accepted D-P07-1 implementation. Cancellation now has separate evidence and timeout phases; it cannot confirm original execution. See [P07 controller report](P07-controller-implementation.md) for the new source manifest and regression results.
