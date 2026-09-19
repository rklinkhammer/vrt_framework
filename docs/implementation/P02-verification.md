# P02 independent verification

Date: 2026-09-18. Verifier: V-P02, independent from I-P02.

**Verdict: PASS for the local baseline P02 codec gate.** This is selected exact-layout and malformed-input evidence, not complete VITA conformance or independent-peer interoperability.

Baseline Git revision `8435ab71d8004e9014a37a63d0f4576ea533252b`; frozen uncommitted source manifest SHA-256 `de157700451de2229c5adb11a8f702618b57648c26ff0b22835052239223dbf0` (sorted `sha256  path\n` records below).

## Oracle provenance

Used PDF skill read-only. Normative source: `/Users/rklinkhammer/Downloads/AV49DOT2-2017-R2024.pdf`, SHA-256 `909cb7052fba4ceb52ee64a73928252d40e0a2390e231198beea9066af3700a8`. Visually inspected rendered printed pages 50/53 (header inclusion/bit assignments), 92/94 (command prologue/CAM), 126 (CIF field and attribute matrix), 161 (signed Q20 Sample Rate). PDF page is printed page+16. Text also inspected pages 51–55 and 125. Rendered scratch remains under `/tmp/vrt-p02-verifier/`; no licensed PDF or rendered source page is committed.

W1–W5 are literal manually reviewed words from the architecture appendix, checked against source layouts; `vectors.hpp` only performs independent byte-shift serialization of those constants. Production encoder output never creates expected bytes. W6/W7/W8 are independent truncation/surplus/type mutations. Additional exact vectors cover Class ID/timestamp/trailer, UUID identities, two diagnostic groups, selector CIF7, integer endpoints, and IEEE float bit patterns. Test-only Class IDs are never production registration/OUI values.

Interpretation IDs: I1/I11 for conditional diagnostics and original request context; I2 for CIF7 presence. These selected project-dialect tests do not certify peer agreement.

## Tests and outcomes

| Test | Evidence |
|---|---|
| p02_verify_wire | W1 literal encode/framing decode; every truncated W1 prefix rejects; reserved packet/header/Class bits reject; short output is reported before mutation; literal combined optional-header vector; all 8 families x 4 TSI x 4 TSF combinations have independently calculated lengths; 128-bit IDs; exact wire size validation. |
| p02_verify_packet | W1–W5 complete body decode; W1–W4 literal encode; W6–W8 fail before callbacks; diagnostic values are 32-bit with all indicators before groups; missing original context rejects; W12-style summary without requested detail does not force body; CIF7 current/max/min selectors survive; negative raw Q20 remains observable but semantic rate validation rejects; expected-Class mismatch; unknown extent and reserved State/Event/DPF/CAM reject. |
| p02_verify_samples | W5 IQ16 exact bytes; signed IQ32 endpoints; IEEE 1.0/-0.5/-0/infinity exact bits; unaligned-safe access; partial-pair/index bounds; short output and overlap reject before mutation; 1,000 structural decodes and packing operations incur no counted ordinary new allocations; second translation unit links public codec definitions. |

Negative Sample Rate distinction was explicitly checked against printed 161: it is not a valid rate. Structural decoding preserves its raw signed value so future command validation can return diagnostics; valid semantic builders do not encode it. No test treats successful structural parsing as authority to execute a control.

Pre-freeze reviews led implementer to preserve selector attributes, constrain named packet semantics, and move unchecked helper writers into `detail`. Frozen candidate passed all independent tests without further repairs. Full packet structural validation precedes the first user visitor invocation, including known-field-then-unknown-field input.

## Commands and results

Apple clang 21/libc++, Darwin 27 arm64, CMake 4.4.3, no exceptions/RTTI:

```sh
cmake --preset dev
cmake --build --preset dev --target p02_verify_wire p02_verify_packet p02_verify_samples
ctest --preset dev -R '^p02_' --output-on-failure
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan --target p02_verify_wire p02_verify_packet p02_verify_samples
ctest --preset asan-ubsan -R '^p02_' --output-on-failure
```

Both suites pass 4/4 (three independent, one developer). Raw ignored logs: build/{dev,asan-ubsan}/Testing/Temporary/LastTest.log; later runs may replace them. No concurrency is implemented in this package, so no P02 race-freedom claim is made. Ordinary new-count instrumentation is scoped to tested paths, not a process-wide allocator audit.

## Bounds and integration obligations

Baseline has four descriptors and bounded scalar sample layouts; unknown general fields/attributes return unsupported_layout pending P14. Extension payloads are opaque at this layer. Framing-only `encode_envelope`/`decode_envelope` deliberately do not certify arbitrary standard-body bytes. P05 must integrate owned transport and class/route policy; P06 must apply semantic eligibility before callbacks with side effects; P10 adds source scaling/rounding and full profile checks. Linux qualification and independent peer captures remain absent. Relevant subsequent production edits require affected regressions.

Integration revision: same shared-workspace manifest, no commit created; coordinator owns milestone integration beyond package tests.

## Frozen source manifest

```text
1a088395deb9683f29ff74c4dda5e0513a1aab743ff9c4f40d355a39458385d1  CMakeLists.txt
150fa936fd99a1bc6cefdca27a5625a997efe75e6f9b2a4804367b03232d7000  include/vita/codec/layout.hpp
22c63c5b5e70f892317be156eb3fc6abb2ca41bc1d84464d5e4befa8db8edf4f  include/vita/codec/packet.hpp
3db37f96f7f61603aaa6f909959172adaa3c7a583d1fb5f3da021ffbf3f80298  include/vita/codec/samples.hpp
b7e4020d643a2065ddfb784dfaf244e0998ca313dce1690ff4df89fdaa2223b3  include/vita/codec/wire.hpp
ed9510455f242e53a3f0f9316890fdea1de5a4f8cd7f9dfeb41e28bed9d77277  include/vita/core/bytes.hpp
c7090fadba63f24786de0de12c2cda91c0069124db1b7b934949ee88a5c530eb  include/vita/core/capacity_policy.hpp
c76159658b171bdfd1b30eec57297f27e44f2f619721dac358780faefc23ecca  include/vita/core/error.hpp
9add4a941546a64ae088f55985554e74a886221c3501e23d94a1b088b91a2eaf  include/vita/core/fixed_vector.hpp
7d0a8c6896e0eb4d76d233b021cdd34731282ced6882a580e907c58a09636ab9  include/vita/core/version.hpp
606b0db7ee2f0a0cc5f776e8846143f2dd865abf8c9f24cb86a7ee63c311c655  include/vita/fields/arena.hpp
be20b065970cc2d188112ca22dc4922f0850d07a6afd88e6f1c030ed13c5fe2d  include/vita/fields/packet.hpp
2f02a01694f87107b6f69cd06289b6a5cf4715e9d821dd871138d69fd7f6eb65  include/vita/fields/types.hpp
9c1593e87fbf264955aed822c546319ca38e4843e7d229a0b036a7778da85034  tests/unit/P02/CMakeLists.txt
7b55c7769e73b04216bedc2f42278d5a064ce7972f43ea1e81adc16d5341117e  tests/unit/P02/codec.cpp
ef338ccfeab3d6b85ac4f097ed80dcd32aa56e3113e0a662d222ccbe6ecd842e  tests/unit/P02/other_tu.cpp
543cc3964fc5fd9a59b16ca4c8a939239a829d4fb6762317443950dfafc227b6  tests/verification/P02/CMakeLists.txt
b3149325131dd0d77fb85edebd312af678518de2258d8d60ac66719be01b9e07  tests/verification/P02/other_tu.cpp
68e9b9d302b3d01a93f7142dbd4bfd86d3242a7744873ca1efb0284037a8c330  tests/verification/P02/packet_contract.cpp
03a906cccddd3ea055f9b979747992f8dbfc8543e07c1ae09e3f1ff8b41e765c  tests/verification/P02/sample_contract.cpp
7bf62f901c81978a585c2bde3e847e1e4ba01202714931e45d6a141106404467  tests/verification/P02/vectors.hpp
fdc48e872c610a45f3bd0c7caf10ba2080112a7de9b450929277790d92c2410e  tests/verification/P02/wire_contract.cpp
```

## Additive segmented-prologue API verification

The separately added prologue.hpp is approved locally for P10 use. `p02_verify_prologue` passes Debug and ASan/UBSan: literal W5 prefix, literal Class/timestamp/flags prefix, untouched payload-region sentinels, independently calculated offsets/total/trailer location, checked short-output trailer/prefix errors, whole-packet limit/word-alignment rejection, and all 128 family/TSI/TSF prefix lengths. The API accepts payload length only, so it never needs IQ bytes to encode the header. Existing wire.hpp remains the exact original b7e4020d candidate and its P02/P05 regression passed after restoration.

Commands: configure each preset, build target `p02_verify_prologue`, then `ctest --preset <dev|asan-ubsan> -R '^p02_verify_prologue$' --output-on-failure`. Both pass 1/1. Updated/additional files:

```text
9f84b53e42b28ee21303541141a0bb4dafa63101cb1f2c12ae145b4b3c9d5e13  include/vita/codec/prologue.hpp
7538b92934b354e7a973fe15cbf7af0d9c9189bcc87e9383e26e724d4d7ba02a  tests/verification/P02/prologue_contract.cpp
7510c5df43fa9c6ee421c7b5840c619fd0226e65a495f6ad071e37893d7c28e8  tests/verification/P02/CMakeLists.txt
```

## I4/I11 corrective gate

Independent appendix review found two gaps missed by the original gate: permitted ordinary-Control change indicators were rejected, and missing diagnostic request context returned an error rather than the selected opaque result. Both are now repaired and verified. Permission 9.1.1-1 in the local normative reference explicitly permits Control use; the accepted scope covers ordinary Control actions 0/1/2, not separately defined Cancellation or Acknowledgement packets.

`p02_verify_packet` now checks literal change-bit packets in all three ordinary actions, rejection in Cancellation/State Ack/diagnostic Ack, generic Control encoding with the bit, and unchanged diagnostic correlation semantics. Without request context the literal diagnostic returns an opaque `PacketView`, `requires_request_context`, no fields, retained raw payload, and zero semantic visitor calls. This supersedes the original missing-context rejection assertion.

Corrective source SHA-256: `0b7291cb2658cfe8783133f20f454a4933f0f92da55f424a5d3eae4c8b08553e` (`include/vita/codec/packet.hpp`). All eight independent P02/P05 executables rebuilt and passed 8/8 in both Debug and ASan/UBSan, including P05 decode/retention integration. Command filter: `ctest --preset dev -R '^p0[25]_verify_' --output-on-failure`, repeated for `asan-ubsan` after rebuilding corresponding eight targets. Gate PASS for this corrected scope; external conformance evidence remains pending.

## Acknowledgement timing without an effect timestamp

A further independent normative check found the generic CAM parser had incorrectly applied the Control timestamp-presence rule to Acknowledgements. Rule 8.4.1.5-6 conditions zero Ack timing on the *original Control* having no timestamp, while Rule 8.4.1.5-2 requires timing status 111 when execution cannot meet time. The selected I10 policy omits invented actual-effect timestamps when no effect occurred. Generic parsing therefore preserves already legal Ack timing modes 1–4/7 without requiring an Ack timestamp; correlated profile logic handles the original request constraint. Reserved modes 5/6 remain invalid, and Control modes 1–4 still require their own timestamp.

`p02_verify_wire` independently supplies literal packets for all eight timing bit patterns in both Control and Ack framing. Corrected wire.hpp SHA-256: `cc2e6266f8802128e1ab4ead5e391ce22f5a342ca54edb63fed39b219253ba0a`. All eight P02/P05 independent targets rebuilt and passed 8/8 in both Debug and ASan/UBSan. Same build target list and `^p0[25]_verify_` CTest filter as above. P06 separately checks preserved timing-failure status and explicit response epoch encoding.
