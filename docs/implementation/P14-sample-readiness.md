# V-P14 general sample readiness

Continuation update (2026-09-19): D-P14-1/2/3 and the general raw-code-only policy are now accepted in [P14 decisions](P14-decisions.md). Pending-approval statements below describe the original audit checkpoint. No engineering conversion dialect or ambiguous wire layout is thereby selected.

Read-only independent review. Generic representation/access/packing work is ready with explicit format parameters. Lossy conversion behavior needs an explicit API contract before implementation; the baseline generator's policy must not silently become a universal wire rule. This does not block the current structured-CIF0 batch.

Sources inspected: protocol appendix sample coverage table/§3.3; architecture generator conversion paragraph; supplied VITA PDF §§6.1.1.1–6.1.1.5 pp65–73, §6.1.2 p74, §9.13.3 pp228–230 and AppendixD p323. No production changes, sample tests or numerical qualification were performed.

| Obligation | Normative source and independent oracle |
|---|---|
| Item/tag layout | §6.1.1.1 pp66–67: data item1–64bits, channel tag0–15bits rightmost, event tag0–7bits immediately left, unused bits immediately right of data item. Test asymmetric tags per component; identical tags within a complex sample are a recommendation, not a rejection rule. |
| Packing | §6.1.1.2 pp67–68: link-efficient cross-word placement versus processing-efficient left-justified words and right-side unused bits. Test widths3,12,17,31,33,63,64, fragment splits, unaligned buffers and complete structures. Processing unused zeros are 'should', so strict decoder rejection requires explicit policy rather than claiming normative malformed. |
| Repetition/vector ordering | §6.1.1.3 pp68–70: I before Q/amplitude before phase, repeated components versus repeated channels mutually exclusive, time order retained. Repeat1–65536; explicit Vector rule limits actual size65535 even though16-bit minus-one descriptor can represent65536. Preserve raw descriptor but reject invalid semantic configuration; no guessed65536 support. |
| Fixed numbers | §6.1.1.4 pp70–73: signed two's complement and unsigned normalized scaling; non-normalized fraction count comes from explicit descriptor. Test bit1 and64 extrema without signed-shift UB or passing every64-bit value through double. Signal Time excludes non-normalized formats; log-power spectral data is real1–16bit integer.fraction, not every arbitrary non-normalized format. |
| VRT exponents | §6.1.1.4 pp71–72 and AppendixD p323: exponent1–6LSBs, signed/unsigned mantissaMSBs, total2–64bits. Independent5-bit E2/M3 literals include11111 unsigned=7/8 and00100=1/64. Normalized scale is m*2^(e-(2^E-1)-M) unsigned, signed uses M-1. Not IEEE exponent/bias. Preserve exact components when numeric output cannot represent all bits. |
| IEEE half/single/double | §6.1.1.4 Rule13 p72: bit formats16/32/64. Separate bit-preserving views from numerical narrowing; include signed zero, subnormal, infinities and NaN payloads in raw oracle. Baseline IQ rejects nonfinite provider values; generic raw representation is not implicitly subject to that policy. |
| Polar phase | §6.1.1.4 pp71–72: unsigned fixed/VRT scales normalized phase by2π, signed byπ, IEEE phase already radians. Test signed minimum=-π, unsigned endpoints, component repetition and phase/amplitude distinct interpretation; no implicit Cartesian conversion. |
| Payload/frame/padding | §§6.1.1 pp65,6.1.1.5 p73,6.1.2 p74,5.1.3/5.1.6: exact complete structure count, explicit or inferable pad count, trailer/frame context. A missing count that permits multiple sample counts is insufficient input, not a guessed sample count. Frames cannot split a complex sample/packing structure. |

Ready API choices can preserve raw integer/mantissa/exponent/IEEE bits and exact component ordering, use caller-provided output storage and reject insufficient capacity before writes. Explicit descriptors must supply packing/repetition/vector/tag dimensions and applicable class/frame context. Physical tag meanings, channel association and engineering-unit scale remain deployment metadata; lack of those meanings does not prevent structural access.

Before a **lossy numerical conversion** implementation freezes, the API must specify: rounding mode; overflow saturation versus error; nonfinite handling; exact-output failure versus precision-loss acceptance; VRT equivalent-representation selection; and whether polar output is preserved or transformed. Architecture only explicitly chooses ties-even/saturation for the baseline IQ generator. Requiring caller-selected conversion policy (with raw lossless access independently available) can close this internally without inventing a deployment default. If a universal implicit policy is desired, it is an outstanding design choice and must be recorded rather than inferred.

IEEE754 numerical semantics are referenced by VITA rather than fully reproduced there. Bit-preserving raw support has sufficient VITA layout input; any claim of complete IEEE conversion conformance needs appropriate authoritative specification evidence and independent rounding/subnormal/NaN tests. No external standard or reference implementation has yet been supplied for that qualification.

## Separate later-field blockers confirmed

An independent follow-up inspection rendered printed pp137 and211 and confirmed two contradictions in normative rules, not merely in observations or extraction:

- Beam Width Rule9.4.2-2 requires0..360degrees inclusive, while Rule9.4.2-3 specifies signed16-bit Q7. Its maximum positive representable value is255.9921875degrees;360 cannot be represented. Figure9.4.2-2 has two16-bit halves. Supporting unsigned Q7 or restricting the semantic range each overrides part of the rule set and requires an explicit selected interpretation.
- Barometric Rule9.9.2-4 specifies a17-bit field, an LSB of1/131071Pa, and0xFFFF representing131071Pa in the same rule. Those numeric statements cannot all hold. A1Pa/count interpretation is plausible but not established by the existing architecture; the exact scaling/sentinel policy needs a recorded decision or authoritative correction.

These block the affected later CIF1/CIF3 semantic batches under the user's stop condition. They do not invalidate the completed scalar CIF0 batch or the current independent structured CIF0 work. No production interpretation has been invented here.

[Pending coordinator decisions](P14-decisions.md) records these verified contradictions and a proposed conservative continuation. Its arithmetic and printed/PDF page mapping were independently checked. The proposal is not accepted or applied; this report does not authorize a pressure scale or unsigned beam dialect.
