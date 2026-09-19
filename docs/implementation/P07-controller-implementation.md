# P07 controller implementation

Implementer: contracts agent. Candidate ready for independent verification; this report does not grant the package gate.

The controller registry owns bounded transaction tracking and MID allocation. A relationship never wraps its 32-bit MID counter: a fresh wire identity is required after exhaustion. Local binding/session generation changes alone cannot recycle the same peer/SID/controller/controllee relationship. Registration accepts an explicit initial MID for deployment setup and boundary testing. Relationship slots are setup lifetime and are not recycled; exhaustion is explicit.

`track` returns a copied Envelope with its allocated MID; the caller must encode/send that envelope. Admission creates one external reference; users must release it. Every record remains until both original and cancellation phases are terminal, 30 seconds have elapsed since the latest first terminal transition, and all references are released. Duplicate responses and retries do not renew this retention window. Timeout terminal time is the original monotonic deadline; `receive` enforces it even when the progress pump has not called `advance`. No timeout sends a cancellation.

D-P07-1 is enforced by retaining the first canonical cancellation packet (maximum 512 bytes by default), ignoring only outgoing Packet Count. A changed cancellation meaning rejects; exact retries reuse the existing deadline and observations. Ordinary and cancellation response evidence and immutable first AckS state/timestamp snapshots remain separate. Ack correlation checks local binding, peer/session, SID, both identifiers, MID, Class ID presence/value, preserved CAM request flags, requested response phase, and corresponding selected-state field subset. Wrong identities or conflicting AckS replay reject. `ControllerObserver` alone remains MID-only; production correlation uses `ControllerRegistry`.

The P06 outcome implementation is deliberately extended: `AckRecord::cancellation` emits header L, and cancellation AckX/AckS/timeout have distinct observations. Cancellation success never confirms original execution. The old P06 developer assertion requiring cancellation rejection was replaced with the new distinct-evidence contract. All other response encoding rules, explicit epoch, outgoing packet counter, and late original-response semantics are retained.

Construction allocates one fixed `Storage`; operational APIs allocate nothing. The serialized controller domain owns mutation. Default record size is 1480 bytes; 256 records plus 64 relationship records total 385024 bytes, below the 524288-byte controller-role allocation. The registry object and allocator infrastructure must be accounted according to the startup ledger's ordinary object/infrastructure rules. Setup OOM follows the project's no-exception allocation behavior. Larger template capacities must be charged using `storage_bytes()`; no whole-M3 fit is claimed.

Developer validation: `p07_controller` and `p06_outcomes` passed dev, ASan/UBSan, and TSan presets. Existing independent `p06_verify_controller` and `p06_verify_outcomes` also passed dev after the additive change. Tests cover last-MID allocation/wrap refusal, fresh-wire restart, immutable cancellation retry, rejected changed subset, timeout separation, wrong-peer rejection, cancellation evidence isolation, delayed response without prior progress, duplicate nonrenewal, and reference-gated expiry. Independent P07 verification remains a separate report.

Frozen candidate SHA-256:

```text
f18ce64ba96a6b17ba87b0543207d0bb421314a4e733e789172603b14a4fd9c6  include/vita/runtime/transaction/controller.hpp
4bdf0c2e86e5d677ee5df70a874ea5df8e4c9e29f02004772dc8040c1cf3acae  include/vita/runtime/transaction/outcomes.hpp
985ce272affe07c2d4334c800d449dbe58d4b83d8a0689445d127b2cf9d4d819  tests/unit/P07/controller.cpp
4c5c83d264d640c6e67437798502e36a6ee7d144f1941d729651a9bd0bf130cb  tests/unit/P06/outcomes.cpp
```
