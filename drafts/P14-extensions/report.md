# P14 registered extension isolated candidate

Only `drafts/P14-extensions` changed. No live headers, CMake, runtime, packet schema or existing sample draft changes. This is a bounded locally registered interface, not a deployed vendor extension, authenticated transport or transaction-engine integration.

Public header `include/vita/codec/extensions.hpp`, namespace `vita::codec::extensions`:

- `Registry<N>` owns its descriptor array directly, is noncopyable/nonmovable, and freezes permanently. Setup `add` rejects duplicate full keys, exhausted capacity, invalid options and invalid CAM masks before changing storage. No heap storage.
- `ClassKey` contains exact extension packet type and OUI/information-class/packet-class identity. SID presence distinguishes types2/3; no wildcard class/family matching. ClassId pad count is per-packet metadata checked separately, not part of class identity.
- `HeaderRules` explicitly permits/requires option flags, TSI/TSF codes, identity kinds and pad counts. Checked existing envelope validation runs before policy and callback. Payload min/max bounds are whole-word sizes.
- Descriptor context and callback pointers are setup borrows. **The fixed-address registry, every descriptor context, and immutable wire storage must outlive each ValidatedPacket borrow and every use.** No claim that a borrow survives their destruction; no shared ownership overhead. Rvalue validation is deleted, and registry movement is deleted. This matches the existing explicit PacketView borrow contract.
- `validate(wire,WorkBudget&)` decodes the existing checked envelope, rejects nonextension families, charges envelope/payload work, applies class policy and calls only a pure registered validator. Unknown or classless extensions return bounded opaque views with no execution capability. No standard CIF parsing is imposed on vendor bytes.
- `ValidatedPacket` construction is private. Dispatch rejects opaque packets or a validated packet from another registry. `dispatch(packet,Authorization)` is a separate operation; it requires the host's authorization/admission callback on **every call**, then invokes the registered semantic callback. Validation never invokes either. The host callback must reserve resources and arrange cleanup/outcome handling. This does not itself provide deduplication, execution ordering, cancellation, completion tickets or release of host reservations after callback failure; those remain host transaction responsibilities.
- Optional paired measure/encode callbacks operate on caller-owned native state and exact caller output payload storage. Framework preflight (envelope/options/payload extent/trailer/output size/budget) occurs before writes. Once the registered encoder starts, its failure or post-encode validation failure may leave partial payload bytes; output is explicitly unspecified on these callback failures. No hidden staging is allocated. Caller native state must be immutable during the call and must not alias output if its encoder reads it while writing.

`WorkBudget::consume` fails without wraparound. Framework charges1+payloadwords for receive and1+payloadwords for emission; registered callbacks charge their own additional parsing/encoding work. Registered code is trusted setup code and must honor the budget/no-allocation/noexcept/pure-validator contracts. C++ cannot preempt an arbitrary misbehaving callback; this is not a sandbox or a claim to bound malicious registered code.

## Normative boundary

Reviewed §§6.4 pp80–81,7.2 p87,8.6 pp122–123 and architecture/protocol extension registration contracts. Extension Data/Context/Command retain their common prologues but have class-defined payloads. Similarity to standard CIF payloads is recommended, not mandatory. The interface reuses live `decode_envelope`, `measure_envelope`, `encode_envelope`; no duplicate prologue parser.

Control/Ack custom CAM masks are independently declared and must be subsets of `0xfe` (bits7..1), per explicit Permissions8.6-2/3. Bit0 is rejected even for an opaque unknown extension. Live generic envelope validation currently permits the entire lowbyte for type7; this draft imposes the narrower rule without modifying live code. All other standard CAM checks remain in the existing prologue validator. No extension registration weakens source authorization or overrides standard identity/timing fields.

A real extension needs its caller-supplied OUI/class, payload field layout, semantics and custom-bit documentation. Tests use explicitly isolated fixture OUI0xff0001/class4321:1234; this is not a production assignment/interoperability claim. No-Class-ID input remains opaque in this candidate. Supporting a separately documented classless static binding is a later explicit interface, not automatic vendor matching.

## Developer verification / storage

Direct C++23 build/run PASS with `-Wall -Wextra -Werror -fno-exceptions -fno-rtti`, draft include first, then live include. Same target with ASan/UBSan and frame pointers PASS. No shared build directory used.

```
clang++ -std=c++23 -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
 -Idrafts/P14-extensions/include -Iinclude \
 drafts/P14-extensions/tests/registry.cpp -o /tmp/p14-extensions
/tmp/p14-extensions
```

Tests include literal28-byte extension Command envelope/payload, exact identity/family matching, duplicate/full/frozen registration, private validated construction and nonmovable lifetime contract, retained borrow address, unknown/classless opaque rejection at dispatch, foreign-registry rejection, separate pure validation, per-call denied/accepted authorization, bit0/undeclared custom-CAM rejection, timestamp-option rejection, truncated/malformed body, work exhaustion, untouched short output, callback-failure contract and extension Context encoding. Zero ordinary C++ `new` calls observed over tested paths; not a universal C/aligned allocator instrumentation claim. Source contains no allocator path.

Local ABI measurements: `Descriptor=104`, `Registry<16>=1680`, `ValidatedPacket=176`, `WorkBudget=8` bytes. Caller contexts/output/wire/admission resources are additional caller-owned storage and are not hidden by this count. No setup shared allocation/control block. Independent verifier gate and live integration remain outstanding. Frozen code/test hashes are in `manifest.sha256`.

## Validator-view consistency correction

Coordinator-authorized repair after independent review: emission now performs framework preflight, invokes the native payload encoder, serializes the checked envelope/trailer into caller output, decodes that completed bounded wire with the existing `decode_envelope`, and invokes the registered pure validator on the resulting complete `EnvelopeView`. Receive and emission therefore both provide valid `wire`, `payload_offset`, payload alias and trailer metadata. No optional/empty-wire validator dialect is introduced. No allocation or additional serialization buffer is used; the existing in-place envelope helper preserves payload ownership.

Callback/post-encode validation failure still leaves output unspecified, as documented; framework preflight failures remain before any writes. Regression validator reparses its `wire` and verifies payload identity/offset, class/family and trailer coherence on every call. Command, Context and trailer-bearing Data emission/receive all pass this check. Direct Debug and ASan/UBSan builds/runs repeated successfully on the corrected candidate; ordinary C++ allocation instrumentation remains zero. `manifest.sha256` has been refreshed; prior hashes are superseded. No live or independent-verifier files changed.

## All-family developer coverage expansion

Producer test now covers all extension types2/3/5/7 under identical full class identities (type is part of the key),24 successful encode/validate/authorized-dispatch cases total: eight flag combinations each for SID-less/SID Data (trailer,ND0,spectral); four Context combinations(ND0/TSM); four Command combinations(ordinary/cancel Control and ordinary/cancel AckX). Each includes explicit GPS integer/picosecond fractional timestamp, nonzero permitted pad count, full-wire validator checks, and literal header type-nibble checking. SID-less input stays SID-less; SID families retain SID. Data trailer values survive full encode/decode.

Separate Control mask0x02 versus Ack mask0x04 is tested in both directions; undeclared low bits and reservedbit0 fail. Explicit short Controller/Controllee identities pass; class-disallowed UUID/absent kinds fail before validator. Wrong TSI/TSF/pad, mismatched trailer presence, and inbound missing required trailer fail before callbacks, and all encode preflight rejection cases preserve the output array byte-for-byte. Every successful semantic callback is paired with an admission call. Added cases remain inside ordinary-new allocation monitoring. Direct Debug and ASan/UBSan rerun PASS. Production draft header is unchanged; only producer test/report/manifest updated. This expands developer evidence; independent all-family verification remains a separate gate.

### Proposed live prologue consistency correction (not applied)

Current live `codec/wire.hpp::validate_cam` selects a low reserved mask of0 for extension Command, allowing bit0 in the generic envelope path. The registry overlay rejects it, but promotion should make generic prologue decode/encode consistent with explicit Permissions8.6-2/3(bits7..1). Proposed narrow change after the current live gate: extension low reserved mask `0u` becomes `1u`; standard Command remains `0xffu`. No other CAM logic changes and no accepted interpretation is overridden.

Direct prologue regression for promotion: use the independent28-byte Command fixture above and change CAM bytes at offset16 from `01 00 00 02` to `01 00 00 03`; checked `decode_envelope` must reject reservedbit0 before any class/body callback. Companion `measure_envelope`/`encode_envelope` on the same invalid Envelope must reject before output changes. Valid custombit1 ordinary Control and custombit2 AckX must continue passing generic framing, subject to registered per-class masks. Also test opaque unregistered extension Command with bit0 set: it is still reserved, not a vendor escape hatch. Current producer test explicitly demonstrates legacy envelope acceptance followed by registry rejection; replace its construction of the invalid packet with the literal byte mutation when the live fix is promoted. Do not retain an assertion requiring legacy permissiveness in the promoted test.
