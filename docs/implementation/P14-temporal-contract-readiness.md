# P14 temporal contract readiness

Read-only audit, 2026-09-19. Only this note changed; no production/test edits or builds. Reinspected the supplied ANSI/VITA49.2-2017(R2024) PDF, printed pp189–198, including rendered Tables9.7.3.4-1 through6; cross-checked §§5.1.4,9.12, current `LayoutContext`, `PacketBuilder`, `walk_field_layout`, and packet encode/decode. Printed page plus16 gives physical PDF page. This is a proposed contract, not implemented coverage.

Timestamp Details has a complete fixed wire layout. Age/Shelf Life has a representation explicitly bound to the enclosing packet's TSI/TSF. Neither requires an invented UTC/GPS mapping or a new clock service. Full applicability validation needs caller-supplied stream scope; that is normal explicit context, not an architecture blocker. Accepted general raw-only policy covers unknown engineering conversions, not guessing a missing wire layout.

## Timestamp Details: exact native representation

Field ID `{3,31}`, exactly two32-bit words (§9.7.3.4 Rule2/Figure1 p194). Native value can be a compact trivially copyable structure or two host words with typed accessors; it need not inflate every semantic-value variant or acquire a wire-buffer lifetime.

| Word / bits | Native component | Validation |
|---|---|---|
| 1 /31..24 | user code u8 | Unused bits must be zero; deciding which bits are assigned requires class documentation. Do not treat all eight as reserved merely because generic code lacks that document. |
| 1 /23..19 | reserved | Always zero. |
| 1 /18 | G/global | 0 applies to Context and paired Data;1 applies throughout the Information Stream. |
| 1 /17..16 | TSE u2 | 0 unspecified,1 UTC epoch,2 GPS epoch,3 POSIX epoch. TSE describes **word2**, not the enclosing timestamp type. |
| 1 /15..14 | LSH u2 | 0 leap seconds not applicable;1 counted as normal seconds;2 duplication;3 overflow. |
| 1 /13..12 | LSP u2 | 0 unspecified;1 current day86399seconds;2 86400;3 86401. |
| 1 /11..9 | Time Source u3 | 0 unspecified,1 atomic,2 satellite(e.g.GPS),3 terrestrial radio,4 PTP,5 NTP/SNTP,6 and7 distinct user-defined sources. None is a reserved code. |
| 1 /8 | E | Enables interpretation of POSIX offset. |
| 1 /7..0 | POSIX offset s8 | Signed two's-complement UTC-minus-POSIX seconds when E1. When E0 the value is undefined, **not required zero**. |
| 2 /31..0 | epoch u32 | Whole seconds in TSE's epoch. With TSE0 it has no meaning, **not required zero**. |

Source: Figure9.7.3.4-1 and Rules3–7 pp194–195; Rules13–14 p196; Rules18–23 pp197–198. Undefined components must round-trip unchanged in structural decoding. Canonical emitters may choose zero for their own undefined values without rejecting other legal encodings.

### Intrinsic and applicability checks are different

Intrinsic checks can run on the value alone: reserved mask, component widths, and `LSH==0 => LSP in {0,2}` (Rule14 p196). All TSE/LSH/LSP2-bit codes and Time Source3-bit codes are assigned. Native constructors must range-check before packing; truncating an oversized code is not validation.

Rules15–17 impose the following restrictions **when the field applies to streams using the stated timestamp formats**:

| Applicable timestamps | Allowed `(TSE, epoch)` |
|---|---|
| Any UTC, no GPS | `(0, any)`, `(1,0)`, `(3,0)`; TSE2 invalid. Table4 p196. |
| Any GPS, no UTC | `(0,any)`, `(1,315964811)`, `(2,0)`, `(3,315964800)`. Table5 p197. |
| Both UTC and GPS | TSE0, epoch ignored. Rule17 p197. |
| Other only | No UTC/GPS table restriction; word2 follows declared TSE units and supplied class-defined starting epoch. |
| No timestamps | Field has no applicability (Observation1 p194); still has its fixed two-word wire extent. Do not invent clock evidence. |

These are not rules that TSE equals TSI. A GPS timestamp can correctly use TSE1 with315964811, or TSE3 with315964800. These constants locate the GPS epoch in the other time scales; they are not a live leap-second count to update from the current date.

The current packet provides a **lower bound** on the scope: if its TSI is UTC/GPS and it is in the field's applicability scope, the corresponding restriction can already be checked. It does not prove that the paired Data/other Context streams use no additional type. Proposed semantic validator takes a bounded scope summary such as `{observed_tsi_mask, observed_tsf_mask, scope_complete, documented_user_bits, user_source_documented}`. Fractional-only timestamps (TSI0, TSF nonzero) still have timestamp applicability; no-timestamp status requires both masks and a complete scope. A partial scope may prove invalidity but cannot claim full applicability validation. Incomplete scope is an inspectable unresolved semantic result, not malformed fixed layout. G1 identity across relevant Context streams (Rule5) belongs to the stream/profile state validator, not a stateless field decoder.

LSH describes actual timestamp behavior:0 SI seconds without leap applicability;1 SI seconds including leap seconds;2 nominal seconds with fractional reset each SI second;3 nominal seconds with fractional reset each nominal second (Rules8–11 p195). Duplication/overflow leap periods cannot be treated as calibrated (Rule12 p196). Do not impose a universal fractional value `<10^12` on all raw timestamp encodings: overflow handling expressly allows real-time/sample-count values to pass unity. Conversely no Time Source code by itself proves synchronization, uncertainty or calibration. A class must document user source6/7; generic code must not call them reserved or assume PTP means qualified.

## Age and Shelf Life: extent bound to the envelope

IDs `{3,17}` and `{3,16}`. Rules9.7.2.1-2/3 and9.7.2.2-2/3 p192 select integer/fractional presence and format using the **Packet Prologue** TSI/TSF. Table9.7-1 p190 lists1,2,3words. These durations are not the signed femtosecond representation used for Skew/Rise/Fall/etc. (§9.7 Rules1–2 p189), nor the fixed embedded timestamp triplet in GPS/INS.

| Enclosing representation | Ordinary field words | Native payload |
|---|---:|---|
| TSI nonzero, TSF0 | 1 | unsigned integer-seconds u32 |
| TSI0, TSF nonzero | 2 | unsigned fractional/count u64, MSW first |
| TSI nonzero, TSF nonzero | 3 | u32 then u64 |
| TSI0, TSF0 | No meaningful duration representation | Reject ordinary value admission; see below. |

Preserve TSF1 sample count, TSF2 real-time picoseconds, TSF3 free-running count as distinct tagged meanings. Conversion of counts to time requires explicit rate/count origin where applicable; those inputs are unnecessary for exact code access. TSI selects representation, not permission to add a duration to1970/1980 automatically. Negative durations cannot be represented by reinterpreting the unsigned fields as signed.

**Selected ordinary value with TSI=TSF=0:** the cited rules choose neither component, while the table provides only1/2/3-word value forms and Rule1 requires a duration. Recommended bounded valid-value contract: reject before traversing a value body with an explicit unavailable/invalid timestamp-representation result. Do not silently materialize a numeric zero, consume an invented fixed-width body, or accept a zero-word value as ordinary supported coverage. This rejection is conservative handling of an unusable value representation, not a competing positive wire dialect. If future compatibility requires preserving a selected zero-component value, obtain an authoritative/explicit interpretation first and advertise it separately. The standard does not provide an explicit numeric representation for it.

This condition does **not** reject query/cancel selectors (zero value-body words by their own policy), one-word diagnostic bodies, or CIF7 Probability/Belief attributes (their independent one-word shape). Only attributes using the base timestamp representation require at least one component. The shared traversal chooses body/attribute policy first, then resolves temporal extent. This avoids making a legal selector request depend on timestamps it does not carry.

## Builder, snapshot and shared traversal contract

Keep one source of layout truth. Proposed additive `TimestampFormatBinding {tsi,tsf}` in `LayoutContext`, with an explicit bound/unbound marker; bound `{0,0}` must remain distinguishable from absent caller context. Native `StateDurationValue` can contain u32/u64 components with presence tags, fitting the existing compact storage target, or use the approved optional arena. The exact C++ names are routine choices.

A builder method binds representation transactionally and checks every selected temporal value's component-presence tags. It must not reinterpret already supplied sample counts as picoseconds. A change requiring new components needs replacement values in the same transaction, or fails without mutation. Successful binding increments generation. Snapshot keeps the binding immutable; earlier snapshots retain it. Avoid a second mutable binding hidden in encode-only state.

`measure`, `index_layout`, `walk_layout`, encode and checked decode call the same `temporal_extent(binding, field, attribute, body)` provider through `walk_field_layout`. Decode obtains binding directly from the checked envelope before walking. Encode verifies envelope TSI/TSF equals the snapshot's binding whenever context-dependent fields are present, before writing any output. A snapshot with no temporal-dependent values can retain the existing unbound behavior. The signature includes bound status and both codes even when the number of words is unchanged (e.g.sample-count versus picosecond). Cached offsets cannot be reused across a binding change merely because byte count matches.

Native validation and wire validation remain separate providers over this common extent policy. Wire parsing should preserve structurally readable but semantically invalid epoch combinations for diagnostics; reserved-bit violations remain structural. Timestamp Details remains two words independently of the Age/Shelf binding. Scope metadata needed for semantic applicability is separate from the binding needed for layout; do not put evolving live clock quality inside immutable packet sizing state.

Nested Array-of-CIF records and Sector Start Time inherit the **outer packet** binding unless their own explicit field definition supplies independent timestamp codes. They must use the same extent resolver and shared depth/work budget, not a second timestamp decoder. No recursive default can invent GPS or real-time picoseconds.

## CIF7 applicability without invented mathematics

Table9.12-2 pp219–220 defines base-size attributes and derivative dimensions; Probability/Belief have their separate word shape. It does not provide a universal mathematical interpretation for averaging an epoch selector, differentiating a UUID, or computing a median of a variable list. Keep structural support separate from meaningful device/engineering support:

- A known same-shape temporal attribute can retain its native code and checked extent; do not silently change its units to signed femtoseconds or IEEE float.
- Timestamp Details' bitfields are not numeric samples. Statistical/derivative operations on them require an explicit documented class interpretation; ordinary decode must not invent one.
- An unknown mathematical applicability should return unsupported semantic capability when used, while preserving known structural layout. It must not imply an unknown extent or invoke device callbacks.
- Attribute-specific validation must not blindly apply Current-value bounds to derivatives; a derivative's signedness/scale cannot be invented where the original unsigned representation is inadequate. Preserve raw codes until a documented applicable convention exists.
- Per-field attribute-shape descriptors and transactional all-field CIF7 edits remain the existing architecture. No new universal statistics policy is required.

The accepted raw-only ambiguous-conversion policy allows preserving Probability and Spectrum coefficient codes; it does not authorize a fabricated statistical meaning or missing timestamp body. Profile definitions of statistical population/window, user timestamp source, Other epoch, or rate for count conversion are caller inputs, not blockers to exact codec work.

## Readiness and verification boundaries

Timestamp Details and Age/Shelf valid representations are implementable with the above explicit context contract. No new required external standard was found for their wire layouts. Qualified clock conversion and cross-stream consistency need supplied live scope/time evidence and must not be claimed from packet fields alone. Both-zero ordinary Age/Shelf is deliberately rejected, not guessed; accepting it as a supported layout would require clarification.

Independent vectors should cover all TSI/TSF combinations; exact1/2/3word offsets; selectors/diagnostics and probability-only without timestamps; zero-code rejection before callbacks; envelope/snapshot mismatch; signature changes with equal byte size; all epoch-table pairs and mixed scope; E0 arbitrary offset/TSE0 arbitrary epoch; LSH0 forbidden LSP1/3; all8 sourcecodes; user-bit masks; output atomicity; nested inheritance. Include leap-overflow raw fractions and explicit incomplete applicability scope so future code does not conflate structural success with clock qualification.
