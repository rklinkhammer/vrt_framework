# P14 CIF7 core and explicit generic bounds — implementer report

Status: **candidate implemented and frozen for independent/integration gates.** Developer direct and sanitizer checks pass; no independent PASS is inferred here.

Authority: `P14-cif7-contract-proposal.md`, approved by coordinator. No engineering interpretation beyond the accepted raw Probability policy is added.

## Implemented API

- Append `ProbabilityCode{uint8_t value,function}` and `BeliefCode{uint8_t value}` without changing existing variant indices or16-byte size.
- `AttributeShape` and `attribute_shape(FieldId,Attribute,BodyKind)` choose selector, diagnostic word, Probability word, Belief word, then base representation. One ordering/extent policy serves native and wire traversal. Temporal binding applies to base Age/Shelf attributes, never their Probability/Belief words.
- `validate_attribute_value` preserves Current semantic checks; non-Current base representations validate representability and reserved encodings without copying Current physical bounds onto uncertainty, statistics or derivatives.
- Transient `AttributeInput{FieldId,Attribute,InputValue}` accepts scalar values and existing structured inputs. Raw `NativeSlice` is rejected before mutation. `with_attribute_inputs(mask,span)` and the existing `with_attributes(mask,span<AttributeValue>)` use the same one-candidate kernel.
- `set_attribute<Field>`/`replace_attribute<Field>` edit an existing selected slot. Probability/Belief counterparts take FieldId and their code types. `get<Field>(Attribute=Current)` reads the exact owning native slot; dedicated Probability/Belief getters return code values. Borrowed rvalue getters remain deleted.
- `EditWorkspace<Kind,N,NativeBytes>` owns one private candidate snapshot and is reusable only serially. Workspace overloads avoid a large automatic candidate for explicit generic builders. No pointer to it is retained by the builder or snapshots. The automatic convenience path also constructs only one candidate; it does not recurse through public setters.
- `set_field_attributes<Field>(span<const AttributeInput>)` is approved as part of the first gate to insert a complete new field into an already configured global attribute mask. This enables Probability-only UUID or variable fields with NativeBytes0, without fabricating a Current value. Every supplied input must match Field and every selected slot must be present; workspace overload follows the same contract.
- `BasicPacketView<ViewCapacity>`, alias `PacketView=BasicPacketView<64>`, and `decode_packet_bounded<FieldCapacity,ViewCapacity>` share one parser. Existing `decode_packet` remains `<16,64>`; explicit generic use is `<128,1664>`.
- Compact `IndicatorPlan<FieldCapacity>` stores LayoutContext, ordered FieldIds, count, indicator bytes and change state. No semantic placeholder,13-slot entries or native arena is used to represent wire selection. Aggregate field occurrences include both diagnostic groups; work remains shared across the packet. Known input exceeding requested capacity fails explicitly with no callbacks.

## Existing-test audit

Verifier-owned scalar/CIF1 Query minimum rejection assertions describe the old incomplete registry and must become positive generic acceptance plus meaningful reserved-mask rejection. The independent verifier was notified to own these updates. Unit tests editing a new mask without supplying required values remain valid failures; they need no weakening. Raw native-slice transplant rejection and missing-slot materialization remain required. The engine's request parser already marks all non-Current attributes unsupported, independently of generic descriptor masks; profile isolation regressions must preserve that behavior.

## Contract details and limits

The new scalar alternatives append at indices28/29. Existing `supported_scalar_attributes` remains the compatible Current/min/max convenience mask; descriptors now expose all13 generic wire shapes. Current semantic validation is retained. Previously supported SampleRate min/max still reject negative rates. Newly supported non-Current values preserve their base representation without imposing Current physical bounds (for example negative SampleRate derivative or GPS latitude derivative outside ±90°). Reserved-code/bit structure remains validated by the field codec. Mathematical or device applicability is not implied by generic representation support.

Native edits reconstruct one candidate in CIF/attribute order. Supplied and retained values are copied from the original immutable sources; no live slice is rebound into caller memory. Inputs are bounded to N×13 and duplicate detection uses N16-bit masks. Generation advances exactly once only after successful reconstruction. Generation exhaustion preserves ErrorCode::overflow. A failed edit may leave private reusable workspace scratch partially written, but the original builder/snapshots are unchanged. `set_native` also uses this kernel instead of recursively copying/compacting candidates. Native work/resource extents remain checked by `measure`/encode; as in the prior structured batch, a semantically valid caller arena may hold a value larger than the default traversal-work limit, which cannot be encoded under that limit.

`FieldView::materialize_into` preserves the actual attribute. It updates an existing selected slot, or inserts a Current field through the compatible convenience API. A non-Current field into an unconfigured empty builder fails rather than silently becoming Current. The caller can configure a complete target through `set_field_attributes`/`with_attribute_inputs`. Probability/Belief on structured fields never invoke a base getter or consume native storage. Age/Shelf binding checks scan only base attribute slots.

The parser uses one compact selection plan and shared walker for all result capacities. The128selected-field budget is aggregate across warning/error groups;65+64 fails `<128,...>` but can be explicitly admitted by `<129,...>`. Work is shared across groups and all attributes. Unknown fields remain unsupported. This gate has no recursive fields, so it does not claim nested depth enforcement; future nested traversal must extend the same per-packet budgets rather than restart them. Exact128 distinct ordinary named fields are not fabricated from reserved positions; the current registry's smaller number of named fields still benefits from explicit view capacity beyond64.

## Sizes and evidence

Local Clang arm64 measured bytes:

| Type | Bytes |
|---|---:|
|SemanticValue / FieldEntry / LayoutContext / FieldView|16 /328 /48 /32|
|Default scalar snapshot / default PacketView|5312 /2736|
|Default16-field native8KiB snapshot|13512|
|Explicit128-field native8KiB snapshot / EditWorkspace|50248 /50248|
|BasicPacketView<1664>|66736|
|AttributeInput / IndicatorPlan<128>|120 /328|

Generic result/workspace storage is explicitly caller-selected; no baseline Runtime or receiver object gains it. Large callers can supply reusable setup-allocated workspaces. No operational allocation, global mutable state or pointers into workspace storage are introduced.

Developer `p14_cif7` and `p14_cif7_bounds` pass direct Clang C++23 and ASan/UBSan. They cover all13 scalar attributes, exact Probability/Belief codes, probability-only UUID/Age/association without arena/binding, mixed-length native attributes and compaction, retained old snapshots, explicit workspace, raw-ref rejection, duplicate/missing/oversized transactional failures,260ordinary views, exact259/260view and work limits,128/129aggregate diagnostic occurrence limits, no callback on failure, and non-Current GPS raw representation. Prior P01 semantics, P02 codec (both translation units), CIF3, native structures and CIF2 identifier tests passed directly. Numeric sample unit registration was added at coordinator request; its implementation is owned and verified separately.

The independent verifier owns updates to stale generic Query minimum-rejection assertions. Unit tests whose new masks omit required values remain meaningful and unchanged. Full shared-header, profile and sanitizer integration is delegated to coordinator/verifier; this report does not claim conformance or qualification.


## Frozen SHA-256

```text
dafe93e94fb7d8c6ce20e30ce6ced9aa10df6c68a495b5c166be10a77543a8bd  include/vita/fields/types.hpp
bc5fe3ae55e50508fbe695bf1cc1ffc194cf755243a3877a5064084d15fb72c4  include/vita/fields/arena.hpp
22c5edb4775bfc599f7c97b3797306124c67f523e76923adf450add97f71a7c1  include/vita/fields/structured.hpp
651fb473078435d4f0f2828c764bfad66f154ad299a8e11a99c945e57d8d3094  include/vita/fields/packet.hpp
11559cf377b81c4083cca354a976fbdb6281855e9dc5a973a8b11c19893d8293  include/vita/codec/layout.hpp
f2781c343bccad479fedf387a34fc30eda32a11baef57805b1052f276e002e5f  include/vita/codec/scalar.hpp
196700829d18cb5a5ac1a17a8498f5df73660544ddd20ec6e38e14832e742f02  include/vita/codec/structured.hpp
b72c40e10a53eaf05105c78bcb2f36894110ea9e5173092651ea1fe72839799e  include/vita/codec/packet.hpp
2cbc7938026cd491e2d3af98e9780ad4c2b4a8e41403c270401d634e77604030  tests/unit/P14/cif7.cpp
d18713b5cb4dc885cd2402debb91c0cec98b7c49ec684f1d93bf6237846f47da  tests/unit/P14/cif7_bounds.cpp
707dbb4a894498f5392fc8b9796c5bef867326c18c166724185730a1fa31ca70  tests/unit/P14/CMakeLists.txt
```
