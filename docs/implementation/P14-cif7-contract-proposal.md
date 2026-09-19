# P14 CIF7 implementation contract proposal

Read-only proposal, 2026-09-19. Only this note changed. No live code, isolated drafts, tests or builds changed. Reviewed current `fields/{types,packet,arena}.hpp`, `codec/{layout,packet}.hpp`, appendix§3.3, temporal readiness and §9.12 of the supplied standard. General raw-only interpretation policy is accepted; Probability's exact code/function is supported without a guessed percentage conversion. This is an internal API proposal ready for coordinator approval, not implementation evidence.

## Stable representation and attribute shape

Keep `SemanticValue`16bytes, `FieldEntry`'s existing13 optional slots, snapshot native storage and `NativeBytes=0` specialization. Add small `ProbabilityCode{uint8 value,function}` and `BeliefCode{uint8 value}` alternatives at the end of the variant; preserve existing alternative indices. Neither grows the variant beyond16bytes. New transient input types must not be placed inside every stored FieldEntry.

Centralize shape selection in one helper used by native measure/encode and checked wire decode:

```
attribute_shape(field, attribute, body_kind, layout_context)
  -> selector | diagnostic_word | probability_word | belief_word | base_field
```

Order: selectors first (zero bytes), diagnostics next (one word), then Probability/Belief(one word each), then base-field extent for Current through ThirdDerivative. A structured field selected only for Probability therefore needs no structured native value or base timestamp binding. Reserved attribute bits18..0 reject; explicit CIF7zero remains invalid; API mask0 continues to mean omitted CIF7/implicit Current. Named bit order31..19 is preserved.

`validate_attribute_value` separates wire representation from mathematical applicability:

- Current preserves existing field validation behavior. Maximum/minimum use documented same-type rules where meaningful; precision/accuracy/statistical/derivative validation must not blindly reuse Current physical bounds.
- Probability checks its native type and one-word upper16reserved mask; Belief checks its own type and upper24reserved mask. Belief has exact ratio `value/255`; Probability retains raw code/function under accepted policy.
- A same-shape field attribute has its field codec and reserved-bit checks. Unknown mathematical interpretation does not justify an unknown wire extent or invented float/different scale. Runtime profile returns unsupported semantic capability if asked to act on it.
- Native semantic admission must never accept a public `NativeSlice` supplied in `SemanticValue`; slices can only be created by deep-copy typed setters in the owning candidate arena.

Replace hardcoded descriptor Current/min/max masks only for attributes whose extent is now implemented. Keep structural attribute support and device/engineering semantic support separate. No IQ device permissions expand because generic CIF7 can be decoded.

## Public editing API: compatible additions

Retain current `set<Field>(value)`, `replace<Field>(value)`, `set_value(...)` and `with_attributes(mask, span<const AttributeValue> supplied={})`. Their existing Current-only convenience semantics remain unchanged, avoiding surprising partial edits to packets already carrying multiple attributes.

Add:

1. `set_attribute<Field>(Attribute, Field::value_type)` and `replace_attribute<Field>(...)` for **an already selected field and already selected attribute**. This edits one value without changing the global CIF7 mask; it preserves every other field/attribute value. A new field or newly enabled attribute needs a complete transaction below. Probability/Belief have dedicated typed setters taking FieldId, so a large structured field need not masquerade as its own base type.
2. `AttributeInput{FieldId, Attribute, InputValue}` where `InputValue` is a transient variant of scalar `SemanticValue` plus the supported typed structured inputs (owned fixed structs or borrowed input spans). It is not a stored semantic alternative. Its constructors reject raw native refs; typed field factories may make correct association ergonomic.
3. `with_attribute_inputs(mask, span<const AttributeInput>)` for heterogeneous all-field transactions. Use a distinct name initially to preserve source compatibility of existing `with_attributes(mask,{})`; a second unrelated span overload makes that expression ambiguous. Legacy `with_attributes` routes its scalar entries to the same transaction kernel without allocating a converted input vector.
4. `set_field_attributes<Field>(span<const TypedAttributeInput<Field>>)` for inserting a new field into an existing attribute-bearing packet, requiring exactly every globally selected attribute. This can be deferred if only existing-field extension is needed in the first CIF7 gate; no incomplete public builder state is necessary.

Structured input spans are borrowed only during the synchronous edit. Inputs may borrow a prior frozen snapshot or the original builder; read all old values from original immutable state while constructing the candidate. The API must not publish a pointer into those inputs. Avoid a general callable writer exposing mutable private arena memory.

Snapshot `get<Field>(Attribute)` extends the current `get<Field>()` (which delegates to Current). It obtains the actual slot by reference, checks ownership before following a slice, and validates native shape before exposing a bounded borrow/value. Add typed Probability/Belief accessors. Keep rvalue borrowed getters deleted. `FieldView::materialize_into` must preserve its attribute and use the transaction API instead of always calling Current `set`.

## One-pass candidate reconstruction algorithm

The current `set_native` compacts by whole FieldId and creates a fresh Current-only FieldEntry; using it directly would erase sibling attributes. Current `insert` also copies/compacts again. Replace internal mutation plumbing with a single candidate reconstruction kernel; public behavior need not change.

For any mask/value edit:

1. Validate target mask, requested fields/attributes, duplicate `(field,attribute)` inputs and generation-overflow **before committing anything**. Existing mask0 normalizes to Current for presence checks, but preserve its omitted-CIF7 encoding distinction. Replacing a missing field/slot is an argument error.
2. Compute the final ordered field list and target attribute set. For each required slot choose exactly one source: supplied typed input, retained original slot, or missing/error. Inputs naming nonselected fields/attributes fail. Unselected old slots are dropped, not silently copied.
3. Build a fresh candidate's entries and fresh arena in CIF/bit then attribute order. For retained scalar slots copy the scalar. For retained structured slots borrow bytes through the original snapshot's ownership-checked accessor and append them to the fresh arena. For supplied structured inputs validate native shape and append their headers/elements directly, setting a newly created private slice in the candidate. Append only live selected slots; this naturally compacts holes and counts each attribute's own variable length.
4. Apply `validate_attribute_value` and shared extent/work checks to **every final required slot**. Whole-packet policy checks include temporal format consistency; no side effect/callback may happen during this kernel. A failure discards candidate and leaves original bytes, selected mask and generation unchanged.
5. Set candidate mask, rebuild indicators once, set generation to old+1 once, then assign the complete candidate. Freeze remains a value copy; older snapshots stay immutable and independent.

Do not call public `insert` recursively during candidate assembly: it bumps generations and repeats full copies/compaction. An internal trusted ordered insertion helper or direct candidate slot construction is appropriate after preflight. Empty/zero-entry lists remain explicit typed values if their field definition allows them.

Scratch is bounded by one candidate snapshot/arena plus a compact duplicate bitmap (`N*13`bits or `N`uint16 masks), not thirteen arenas or repeated snapshots on the call stack. Inputs live outside this scratch. For explicit large generic builders, offer an optional caller-owned `EditWorkspace<...>` to avoid a large automatic candidate; it is exclusive per edit and never stored as a pointer in a frozen snapshot. Baseline small builder can retain automatic scratch. Actual sizeof/stack totals must be measured at integration, not inferred from the native8KiB payload limit: field metadata is additional storage.

## Decode capacities without expanding baseline PacketView

Keep the ordinary public names and defaults:

```
template<size_t ViewCapacity> struct BasicPacketView { /* current members */ };
using PacketView = BasicPacketView<64>;
Result<PacketView> decode_packet(Bytes, DecodeOptions={});

template<size_t FieldCapacity, size_t ViewCapacity>
Result<BasicPacketView<ViewCapacity>> decode_packet_bounded(Bytes, DecodeOptions={});
// explicit generic call: decode_packet_bounded<128,1664>(...)
```

Both enter one implementation; ordinary `decode_packet` instantiates `<16,64>`. Do not make a new1664-view local inside the baseline wrapper or down-copy from a generic result. Default `FieldView` layout and all current flags remain unchanged. Larger result storage is selected explicitly and charged to the caller, never appended to Runtime/Loopback/ContextReceiver objects.

Replace decoding's `IndicatorView {QueryPacket selectors}` and semantic-placeholder construction with a compact bounded traversal plan: `LayoutContext`, an ordered array of `FieldId`, a count, and indicator byte count/change flag. It can expose the small `fields()/layout()/generation()` interface the shared walker requires. No13-value arrays, no native arena and no semantic setters are needed merely to describe wire selection. Remove the fixed48-element `AttributeValue` scratch in `layout_from`: it would incorrectly cap generic attributes and require meaningless placeholders for structured fields. The native provider remains over real snapshots; the wire provider uses this compact plan. Both share `walk_field_layout` ordering/body policy.

Generalize `append_fields`, selector visitation and diagnostic group handling to the result/plan capacities. All narrowing wrapper APIs should forward directly to the templated core; no second parser or second attribute traversal. `FieldView::value` and structured getters dispatch on `attribute_shape` first: Probability on UUID is a code, not a four-word UUID.

128fields*13attributes=1664value views is the explicit generic normal-packet upper bound. This does **not** mean every request fits a128-field semantic snapshot with8KiB arena: resource limits remain independent. Known legal input exceeding requested view/field capacity returns `resource_limit` (or the established explicitly documented capacity error at the public boundary), never malformed or truncated success. Check required flat view count before populating where possible, but still validate declared body extents/work before claiming the packet valid. No callback occurs on partially parsed results.

## Aggregate limits, diagnostic groups and future nesting

Use one per-decode `TraversalBudget` passed by reference through all normal/diagnostic groups and future recursive records: remaining selected field occurrences, work units and depth; per-field record/index/association limits remain in the policy. No fresh4096-unit budget for each attribute or child. Charge actual leaf/record work, not just a parent structure as one unit. Enter/leave recursion with RAII or an explicit decremented child depth, without modifying persistent global state.

Count selected field occurrences before attribute expansion. Count a field in both warning and error groups as two occurrences for this bounded decoding budget; that avoids using group repetition to evade a packet aggregate cap. With a128-occurrence generic budget1664views still covers the upper bound, including those groups. This is a conservative explicit parser resource policy, not a wire prohibition on a larger Ack. Callers wanting128fields **per group** must request a correspondingly larger aggregate field/view budget (up to3328attribute views) and account for that storage; do not silently advertise it from `<128,1664>`. Keep the existing correlated-request rules for diagnostic selector/group placement; an uncorrelated diagnostic remains opaque/requires-context rather than guessed.

Future nested records should expose bounded child views via checked child traversal rather than eagerly flatten every child into the top-level `FieldView` array. The128 selected-value-field/materialization budget and4096work bound still account for nested occurrences; lazy child access cannot reset the packet's validation budget or bypass original structural validation. Define a path/index handle only when nested materialization is implemented; no per-view heap tree or global enlargement is needed in this CIF7 batch. A top-level parent field and repeated child occurrences consume the agreed aggregate counters independently of view storage.

## Implementation freeze checklist

These choices are routine implementation contracts consistent with current architecture; no new user interpretation is required. Coordinator approval should fix the API names, aggregate diagnostic occurrence policy and optional generic workspace before code changes.

Independent vectors should prove all13 attributes and omitted Current; one-word Probability/Belief on fixed/variable base fields; variable attribute lengths differing within one field; temporal both-zero base rejection versus one-word-only success; byte-for-byte old snapshot survival; self-borrowed inputs across replacement/compaction; forged/copied slice rejection; all-or-nothing missing/duplicate/arena-full edits; exactly one generation increment; signatures changed by every dynamic shape; baseline16/64 sizes unchanged; explicit128/1664 success and next-entry capacity rejection; no semantic placeholder path; aggregate diagnostic/nested work and no callback before complete validation. Shared traversal changes require affected P02/P09 and malformed-input fuzz regression gates. Actual native storage, result and workspace sizeof must accompany the implementation report.
