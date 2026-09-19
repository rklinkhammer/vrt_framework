# P07 accepted decision: repeated cancellation identity

Date: 2026-09-18. Decision D-P07-1 was accepted by the user on 2026-09-18 ("accept the proposed resolution adn continue"). The policy is now recorded in architecture §6 and the protocol profile; P07 execution has resumed.

## Evidence

Architecture §6 defines a transaction key from binding generation, authorized peer, Stream ID, Controller/Controllee identity and Message ID. Identical retries attach/replay; same key with different meaning conflicts. Cancellation is an orthogonal per-field operation, not a replacement transaction.

Protocol design §§2/4 and the supplied VITA 49.2 reference §8.5 rules 2–5 require cancellation to reuse original identifiers and Message ID while allowing a selector subset. Rule 8 distinguishes cancellation acknowledgements with L=1. Rules 12–15 describe cancellation outcomes but do not provide a cancellation-attempt identifier. Successful execution acknowledgements can omit diagnostic selector bodies; their timestamps are outcome times, not echoed request nonces. Packet Count is not transaction correlation.

The specification does not say whether a second cancellation with different selectors, CAM or timestamp is a new allowed attempt or a conflict. Different attempts can receive indistinguishable successful acknowledgements. Serializing requests alone does not resolve delayed UDP acknowledgements. The implementer, independent verifier and contracts reviewer independently found no explicit resolution in the supplied authorities.

## Accepted decision

Permit one immutable cancellation request meaning per original transaction identity during that transaction's active and retained lifetime. Keep separate ordinary-command and cancellation records under the original key, distinguished by L. The first admitted cancellation may select any valid subset, preserving partial cancellation support. Identical cancellation retries attach to the pending result or replay the original cancellation result and state observation. A different cancellation meaning is rejected locally by the Controller; the Controllee performs no new cancellation effects for such a conflict and reports a bounded conflict diagnostic when safely permitted. Retain cancellation correlation/results together with the original transaction, extending retention as necessary for the latest terminal outcome and outstanding references.

This bounds correlation and preserves deterministic retry semantics, but prevents a later cancellation of a different subset under the same original Message ID. The user explicitly accepted this restriction.

An alternative is to permit multiple attempts while exposing ambiguous acknowledgement attribution, or define a coordinated extension carrying an attempt identifier. Either requires additional public API, retention and peer-interoperability decisions; it is not an implementation detail.

## Execution and verification requirements

Record the selected policy in architecture §6 and the protocol profile, then implement P07 and independently verify identical retry, different-subset conflict or ambiguity, delayed duplicate acknowledgements, retained AckS observations, retention exhaustion and cancellation races. Complete P09–P11 only after their prerequisite gates pass. P08 is already independently verified; P07, M2 and M3 are not complete.

Independent review: [P07 verification report](P07-verification.md). Unverified implementation sketches are preserved under `drafts/P07-blocked/` with `.hpp.txt` extensions and are excluded from the build. The compiled source tree was restored to the exact P06 verified manifest and passed the final aggregate regression in [execution status](status.md).
