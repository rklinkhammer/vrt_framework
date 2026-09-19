# P14 interpretation decisions

Current scope notice: the user accepted M5 completion against the operational profile with Array-of-CIFs excluded from production support. The independent operational review now passes and the coordinator has closed the M5 local software gate for that scope. The explicit I9 structural utility remains optional, with no peer semantic, emission or universal VITA conformance claim. Missing Array peer agreement therefore does not block this M5 scope. P15/M6 remains separately blocked on required hardware inputs. See [M5 operational scope](M5-operational-scope.md) and [operational verification](M5-operational-verification.md).

Status: D-P14-1/2 accepted by the user on 2026-09-19; D-P14-3 and the general raw-code policy below are also accepted by the user. Resume remaining P14 batches with lossless raw-code coverage for Beam Width and Barometric Pressure. Engineering-unit conversion remains unsupported pending authoritative correction or an explicitly agreed peer dialect. The current M5 acceptance scope and final-review status are stated above; earlier batch evidence below is retained.

## D-P14-1: Beam Width

Source: ANSI/VITA-49.2-2017 (R2024), §9.4.2, printed p137 (PDF page153). Rule9.4.2-2 requires horizontal and vertical widths covering0–360degrees inclusive. Rule9.4.2-3 specifies a two's-complement16-bit representation with seven fractional bits. Its maximum positive value is32767/128 =255.9921875degrees. The 360-degree code would be46080 (`0xb400`), which signed Q7 interprets as -152degrees. Both requirements cannot hold simultaneously. Figure9.4.2-2 shows two16-bit components.

The implementer-readiness agent, independent verifier and coordinator inspected the rendered page. This is not an OCR problem or merely an observation conflicting with a mandatory rule.

## D-P14-2: Barometric Pressure

Source: same standard, Rule9.9.2-4, printed p211 (PDF page227). It simultaneously specifies a17-bit quantity in bits16..0, an LSB of1/131071Pascal, and `0xffff` representing131071Pascals. These values are inconsistent:65535/131071 is approximately0.5Pascal, not131071Pascals. The upper15bits are clearly reserved; physical-unit conversion is not clear.

This was also independently confirmed from the rendered source. A likely correction is not a normative resolution.

## Accepted resolution

Accepted conservative continuation: implement the undisputed wire extents and reserved-bit checks for these two fields, retain their component/raw integer codes losslessly, and leave engineering-unit conversion unsupported until an authoritative correction or explicitly agreed peer dialect is available. Publish both as interpretation-limited coverage; do not advertise complete physical-value interoperability. Continue the other unambiguous P14 batches. The user accepted this proposal and authorized continuation. Implementations and verification reports must retain the interpretation-limited coverage distinction.

Unselected alternative: explicitly select project dialects, such as unsigned16-bit Q7 Beam Width restricted to0–360degrees and unsigned17-bit integer Pascals for pressure. Those are plausible conventions but override conflicting source text; they require explicit acceptance, new interpretation-register entries, independent vectors and peer qualification. They must never be presented as official VITA corrections.

An authoritative correction or a supplied existing peer contract can instead settle the mappings. Existing I1–I12 are unchanged. I9 has its separate already-recorded independent-peer/authoritative-clarification qualification condition.

The coordinator checked the public [VITA standards listing](https://www.vita.com/Standards) and [VITA49 FAQ](https://vita.com/page-1855471) for an applicable published correction; none was located in those checked resources. This limited check is not proof that no erratum or committee clarification exists.

## D-P14-3: Probability percentage conversion — accepted

A subsequent independent audit found another conflicting conversion in Rule9.12-7, printed p221 (PDF page237). The low8-bit Probability code has an LSB of1/255 percent, but the same rule assigns `0xff` to100 percent. Literal scaling yields255/255 =1 percent. The neighboring Belief rule9.12-13 explicitly specifies1/255 of100 percent; its consistent definition does not settle the separate Probability rule. The readiness agent, independent verifier and coordinator confirmed the rendered source.

Undisputed Probability layout: one word, code in bits7..0, function in15..8, upper16 reserved zero. Function codes0/1 name uniform/normal distributions;2–255 are user-defined. No Probability implementation has started.

Accepted resolution: extend the accepted raw-code-only policy to Probability. Preserve the probability code and function losslessly, check reserved bits, and leave percentage/normalized conversion unsupported pending authoritative correction or an explicitly agreed dialect. Belief can retain its independently unambiguous scaling. The CIF1 fixed batch has completed independent verification.

To avoid repeated decisions about optional conversions, the accepted general policy is to apply the same limitation to other fields whose wire extent is unambiguous but engineering scaling is contradictory or unspecified: document each limitation, preserve exact raw values, and require explicit class/dialect input for conversion. This includes Spectrum Weighting Factor's unspecified alpha representation (§9.6.1.8, p176). It does **not** authorize guessing field extents, reserved bits, packet layouts, control behavior, or hardware contracts. Those unresolved matters still require a stop. The user accepted this general policy and authorized continuation on 2026-09-19.

## Other remaining inputs

P15/M6 independently lacks a selected hardware adapter/backend, target device/SDK and device access for visibility, completion, cancellation and timing evidence. See [P15 readiness](P15-readiness.md) and [independent verification](P15-verification.md). A decision about these two field mappings does not supply those hardware inputs.

The supplied normative PDF remains outside the repository. Clause references and arithmetic are documented here; the licensed pages are not committed.
