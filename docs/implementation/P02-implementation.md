# P02 implementation evidence

Date: 2026-09-18. Status: candidate awaiting independent V-P02. Production is header-only; no third-party dependencies, heap allocation, exceptions or RTTI.

## Implemented contracts

`codec/wire.hpp`: checked envelope measure/encode/decode for all packet types0–7. Supports SID-less0/2, mandatory SID otherwise, optional Class ID with reserved-bit checks, independent integer/fractional timestamps, family-specific header flags, optional Data trailer, Command CAM/Message ID and omitted/32-bit/128-bit identifiers. Reserved packet types, CAM action/timing/reserved codes, inconsistent SID/identity lengths, zero UUID, invalid header combinations, truncated/surplus datagrams, word alignment and 65535-word limit reject. Identifier type bits remain legal while their enable is absent, matching §8.3.1.1 observation1.

`encode_envelope` is an explicitly low-level framing API for opaque/registered extension/Data and externally supplied payloads. It does not certify arbitrary Context/Command body bytes. `decode_envelope` similarly establishes framing only. Use `encode_packet` / `decode_packet` for standard body validation. Unknown extensions remain bounded opaque bytes and produce no semantic/device callbacks. Deployment class registration and IQ-profile OUI/clock enforcement belong to later packages. No production OUI is selected; test-only class values exercise bit packing.

Envelope output capacity and options validate before mutation. Raw payload overlap is safe: payload relocation occurs before prologue writes. Sample packing rejects input/output overlap before mutation. Low-level unchecked endian helpers and output writers are explicitly internal `detail` helpers, not checked public parse APIs.

`codec/packet.hpp`: standard Context, Control, selector-only query/cancel, State Ack and V/X diagnostic forms. Normal encoding enforces named snapshot subtype, CAM action and optional selected Packet Class consistency. Indicator parsing plus P01 shared `walk_layout` determines all baseline extents; no universal TLV skips. Context Change is a structural flag, not a field. CIF7 selector attribute identities survive decode. Warning/error indicator groups precede both diagnostic value groups; each diagnostic is32 bits regardless of controlled field size. Reserved diagnostic bits and cancellation parameter bits reject. Original correlated `RequestContext` CAM determines requested detail inclusion; summary conditions without requested detail do not imply a CIF body. Missing original context for conditional diagnostics returns invalid_state.

Decoded `PacketView` borrows immutable input bytes, with up to64 checked field/attribute views. `decode_and_visit` validates the whole packet before its first application callback. `FieldView::value` returns native values without silently rounding Q20. Structurally well-formed negative Sample Rate bytes remain losslessly observable for P06 range diagnostics; P01's valid semantic builder still rejects negative Sample Rate. Structural parsing does not authorize executing any requested control. State/Event reserved bits and DPF reserved format codes, fraction rules and impossible item widths reject. Unknown general field/attribute extents return unsupported_layout pending P14.

`codec/samples.hpp`: scalar complex IQ16/IQ32/IEEE float32 network-order packing and checked unaligned-safe read-only SampleView access, complete pairs only. Native samples and byte views stay separate. These APIs pack supplied scalar values; waveform generation, normalized quantization and canonical oscillator rounding belong to P10. Float bit patterns are preserved.

No P01 production file was changed in this package. Existing shared layout traversal is consumed, not duplicated by the wire parser.

## Source review

Read previously extracted local normative reference text (`/tmp/vrt-architecture-reference/`) for printed pages50–61,80–87,92–114,161,228–230: §§5.1,6.4,7.2,8.2–8.6,9.5.12,9.10,9.13.3. Independent V-P02 owns visual PDF and literal golden-vector confirmation. Interpretations exercised: I1/I11 diagnostic detail inclusion and I2 CIF7 enable placement. No interpretation is presented as an official standards ruling.

## Exact developer validation

Apple clang21/libc++ arm64, CMake4.4.3:

```sh
cmake --preset dev
cmake --build --preset dev --target p02_codec
ctest --preset dev -R '^p02_codec$' --output-on-failure
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan --target p02_codec
ctest --preset asan-ubsan -R '^p02_codec$' --output-on-failure
git diff --check
```

Passed1/1 two-TU developer test in Debug and ASan/UBSan. Tests exercise query/control/diagnostic round trips, four-field Context, all8 envelope families with Class/timestamps/UUIDs/trailer, conditional diagnostic original context, CIF7 selectors, negative raw signed preservation, short-output atomicity, truncated callback suppression, IQ scalar bounds and overlapping raw envelope payload. These round trips supplement, not replace, verifier W1–W8 direct vectors.

Logs: `build/{dev,asan-ubsan}/Testing/Temporary/LastTest.log` (local ignored artifacts). Concurrency and external interoperability are outside P02. Canonical field class mappings, profile bounds, transmission ownership and routing remain later packages. No full M5 field/sample-format coverage is claimed.

## Candidate source hashes

| Path | SHA-256 |
|---|---|
| `include/vita/codec/wire.hpp` | `b7e4020d643a2065ddfb784dfaf244e0998ca313dce1690ff4df89fdaa2223b3` |
| `include/vita/codec/packet.hpp` | `22c63c5b5e70f892317be156eb3fc6abb2ca41bc1d84464d5e4befa8db8edf4f` |
| `include/vita/codec/samples.hpp` | `3db37f96f7f61603aaa6f909959172adaa3c7a583d1fb5f3da021ffbf3f80298` |
| `tests/unit/P02/CMakeLists.txt` | `9c1593e87fbf264955aed822c546319ca38e4843e7d229a0b036a7778da85034` |
| `tests/unit/P02/codec.cpp` | `7b55c7769e73b04216bedc2f42278d5a064ce7972f43ea1e81adc16d5341117e` |
| `tests/unit/P02/other_tu.cpp` | `ef338ccfeab3d6b85ac4f097ed80dcd32aa56e3113e0a662d222ccbe6ecd842e` |


## Segmented prologue addition for P10

Added `codec/prologue.hpp` without changing frozen `wire.hpp` (SHA-256 remains `b7e4020d643a2065ddfb784dfaf244e0998ca313dce1690ff4df89fdaa2223b3`). `measure_prologue(envelope,payload_bytes)` validates the complete packet through the existing envelope measurer and returns total/prologue/payload sizes and trailer offset. `encode_prologue` checks only the required prefix capacity, writes the full packet word count, and never reads or copies payload or writes trailer storage. `encode_trailer` performs a checked four-byte network-order store. Invalid options, full packet size overflow and short output return before modifying output.

The additive helper intentionally keeps its small emission routine separate from the frozen full encoder during P05 integration; there is no changed alias behavior or runtime allocation. Developer tests compare literal known prefix bytes and all packet-family/TSI/TSF combinations, preserve output sentinels beyond prefix/trailer, and exercise short-output and full-packet overflow. P02 codec and new prologue targets passed in Debug and ASan/UBSan. Independent prefix verification is separate.

Addition manifest (SHA-256):

```text
9f84b53e42b28ee21303541141a0bb4dafa63101cb1f2c12ae145b4b3c9d5e13  include/vita/codec/prologue.hpp
5ae660e3f01948bb1c900a5e6080ba2ceab96513079ab8b3729fd5c7d3da24b9  tests/unit/P02/prologue.cpp
21c8be10541c236cea60c28cb7d54d5977b936bbc092450736b1c470b63c889d  tests/unit/P02/CMakeLists.txt
```


## I4/I11 corrective integration review

The initial P02 implementation incorrectly rejected the CIF0 change indicator on every Command and treated all uncorrelated diagnostic summary flags as a failed parse. That was narrower than the already selected I4/I11 architecture contracts. These are corrections to accepted interpretations, not a new dialect.

For I4, checked the local normative §9.1.1 text (Permission 9.1.1-1, alongside the Control definitions in §8.3 and separate Control-Cancellation format in §8.5). Generic ordinary Control actions 0/1/2 now accept and preserve the change indicator; `encode_packet(...,change=true)` can emit it for these Controls. Cancellation, AckS and diagnostic bodies retain their prior rejection rules. Generator policy still emits this indicator only in Context. It is never a duplicate/exactly-once identity.

For I11, an otherwise framed and CAM-validated diagnostic with summary warning/error bits but no correlated request now returns `PacketView` with `opaque=true`, `requires_request_context=true`, diagnostics body kind, raw payload and no semantic fields. The parser does not guess warning/error indicator placement or diagnostic extents. `decode_and_visit` invokes no semantic callbacks on this view. A caller that needs diagnostic semantics must supply the original request detail mask and decode again. Correlated diagnostics retain complete structural validation.

Updated the obsolete developer assertion that expected an uncorrelated diagnostic to fail; it now verifies the explicit opaque/context-required state and zero semantic callbacks. Added ordinary Control change-indicator preservation. P02 developer two-TU codec tests passed in Debug and ASan/UBSan. Independent verifier corrections and downstream routing regressions are tracked separately. Only `codec/packet.hpp` changed among production headers.

Corrective candidate manifest (SHA-256):

```text
0b7291cb2658cfe8783133f20f454a4933f0f92da55f424a5d3eae4c8b08553e  include/vita/codec/packet.hpp
920898a21a0f4b03f943e1f0054ab3dd6ce74eddf585abf1190629020b32a350  tests/unit/P02/codec.cpp
```


## Ack timing without an actual-effect timestamp

Corrected `validate_cam` so the presence of a timestamp in an Acknowledge packet is not required solely because its timing status is 1–4 or 7. The generic parser cannot enforce rules referring to the original Control timestamp without correlation. Control requests retain the timestamp requirement. Modes 5/6 remain reserved. This follows reviewed Rule 8.4.1.5-2 (unable execution reports 111) and -6 (zero when the original Control had no timestamp), and permits I10's no-effect result to omit an invented effect time.

The new `p02_ack_timing` test checks all legal Ack timing values without own timestamps, rejects reserved values, and confirms timestamp-less timed Controls still fail. It and `p02_codec` passed Debug and ASan/UBSan. P06 response encoding separately preserves timing 7 for a timed no-effect failure and forces timing 0 when the original Control had no timestamp. This intentionally supersedes the previous frozen wire hash; the earlier prologue-only addition remains separate and unchanged.

Corrective manifest (SHA-256):

```text
cc2e6266f8802128e1ab4ead5e391ce22f5a342ca54edb63fed39b219253ba0a  include/vita/codec/wire.hpp
42ef6b4499c65eda5af322380aa176a0e27e83ae62b6dfdce660bc7a094f893d  tests/unit/P02/ack_timing.cpp
8fb3822714f855dc34f102acf0a0dfe379cbb87f19c9dc67469aca34900b0d31  tests/unit/P02/CMakeLists.txt
```
