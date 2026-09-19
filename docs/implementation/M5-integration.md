# M5 integration progress

Status: final local continuation gate passes198/198 Release and195/195 ASan/UBSan checks, including69 standalone headers. All96 nonrecursive named fields, all13 attribute shapes, raw/numeric sample adapters and registered extensions are locally integrated. An explicit structural-only I9 Array-of-CIF API is separately tested. P14/M5 remains partial: native Array/peer semantic integration and full qualification are stopped on missing I9 agreement/evidence. P15/M6 separately remains blocked on selected hardware inputs. Earlier checkpoints below are historical.

## CIF0 scalar batch

Added 13 scalar descriptors with typed semantic values, fixed scalar encoding/decoding and reserved-bit checks. Shared traversal still controls field order and extent. Scalar snapshots retain their previous storage size; SemanticValue remains16bytes and baseline variant indices remain unchanged. New generic codecs do not add IQ-generator setters or Context-history fields.

Full optimized integration: **150/150 CTest checks PASS**, including **55 standalone public headers**, existing examples, codec, transaction, Context, receiver, ownership and budget regressions. Independent literal-wire, truncation, capacity, profile-rejection and allocation tests are part of that gate. Independent full ASan/UBSan integration passed **147/147** checks; results are maintained in [P14 verification](P14-verification.md). An initial compile failure in the new verifier test used unsupported range iteration; the verifier corrected its own test, then the full configure/build/test sequence passed. The failure log is retained and was not counted as verification success.

[Candidate manifest](artifacts/P14-scalars/manifest.json), [optimized test log](artifacts/P14-scalars/test-release.log), [implementation](P14-implementation.md), [independent verification](P14-verification.md), and [remaining coverage](P14-coverage.md).

M5 still needs the remaining structures, CIF1/2/3, CIF7, general sample conversions, registered extensions and generic limits. I9 retains its independent-peer/authoritative-clarification qualification condition. Earlier P13 performance reports apply to their archived measurement binaries; this scalar registry change has functional regression coverage, not a new performance sweep. M6/P15 separately awaits selected hardware-adapter inputs.

## Final CIF0 structured checkpoint

Six additional base layouts are implemented and independently verified: Formatted GPS, Formatted INS, ECEF Ephemeris, Relative Ephemeris, GPS ASCII and Context Association Lists. Together with the13 scalar additions and previous four baseline fields, the registry now covers all23 named CIF0 base field identities. This is not full CIF7/combination coverage: new fields support Current only, and the parser retains its16-selected-field default.

Arena-enabled semantic snapshots own deep copies of typed inputs; ordinary scalar snapshots retain their existing size. Fixed structures preserve embedded timestamps and unknown numeric values. Variable extents and work limits use shared native/wire traversal. GPS sentence semantics require an explicit caller validator; raw structural views do not assert a sentence grammar or physical reference frame.

Final optimized **157/157 PASS**, including **57 standalone headers**; independent ASan/UBSan **154/154 PASS**. Independent tests include literal11/13-word fixed structures, variable/count/padding limits, the VRT maximum-size packet under explicitly expanded decode limits,20,000 deterministic mutations, immutable snapshot lifetime, compaction/rollback, shape-sensitive cached layouts and1,000 native operations under ordinary/aligned C++ allocation counters. Deliberate allocation probes confirmed detection. No new C-allocation instrumentation or timing qualification is claimed by these tests.

`SemanticValue` remains16bytes; scalar snapshot5312; native8192 snapshot13512; existing runtime StateSnapshot136. Generic owners and their bounded candidate/compaction/materialization scratch are explicit caller storage. Existing reference-budget tests passed; native arenas were not inserted into baseline runtime slots.

[Final manifest](artifacts/P14-structured/manifest.json), [optimized test log](artifacts/P14-structured/test-release.log), [implementation](P14-structured-implementation.md), and [independent sanitizer/ownership/wire evidence](P14-structured-verification.md).

## Accepted continuation

Both readiness review and independent verification confirmed mutually incompatible normative rules for Beam Width and Barometric Pressure. The user explicitly requested stopping at unresolved architecture decisions. The user accepted lossless raw-code coverage without engineering-unit conversion for these fields; no corrected physical-value dialect was selected. [P14 decisions](P14-decisions.md) records the exact clauses, arithmetic and accepted scope. The next CIF1 fixed-field batch is in progress.

P14/M5 therefore remains incomplete. P15/M6 separately remains blocked on adapter/backend selection and device/SDK/qualification inputs ([independent hardware readiness](P15-verification.md)). No hardware implementation or deployment qualification is implied by the completed local codec gates.

## CIF1 fixed-field checkpoint

Added21 fixed CIF1 field identities with exact typed values, reserved-bit validation and shared scalar traversal. Beam Width follows accepted D-P14-1: two lossless component codes, no degree conversion. Threshold mode is supplied explicitly to a separate validator; generic transport does not infer class semantics. New fields remain Current-only and do not expand IQ-device permissions.

Full optimized integration **160/160 PASS**, including **58 standalone headers**. Independent fixed-field vectors cover all21 additions, signed/sentinel distinctions, malformed/truncated inputs before callbacks, mixed CIF0/CIF1 order, selector/diagnostic sizes and profile isolation. Full independent ASan/UBSan integration **157/157 PASS**; [verification report](P14-cif1-fixed-verification.md) records bounded approval and limitations. `SemanticValue` remains16bytes; scalar snapshot5312; native8192 snapshot13512; runtime StateSnapshot136. The explicit OptionalQ7 representation avoids the measured libc++ optional-pair variant expansion.

The provisional158-test run exposed obsolete P01 unknown-field fixtures after CIF1 support expanded; one verifier binary also preceded its fixture correction. Both fixtures now use unregistered CIF3bit0 and retain unknown-layout rejection. Their failure logs are preserved and are not counted as a passing gate. Final reconfiguration includes both new verifier targets; the rebuilt160-test suite passed.

[Manifest](artifacts/P14-cif1-fixed/manifest.json), [optimized log](artifacts/P14-cif1-fixed/test-release.log), [implementation](P14-cif1-fixed-implementation.md). No new performance measurement, peer interoperability, or M5 completion is implied.

A subsequent independent source audit found conflicting Probability percentage scaling (D-P14-3). The current verified batch is unaffected. [The decision proposal](P14-decisions.md) offers raw-code-only coverage and a general policy for similarly ambiguous engineering conversions; the user has accepted it and authorized continuation. M6 still lacks a selected backend/device/SDK and actual device evidence.

## CIF2 checkpoint

All29 named CIF2 field identities now have bounded structural codec coverage, including owned128-bit UUID values. Country Code follows the repeated explicit11-bit/ISO-flag rules over the inconsistent figure; RF Footprint Range follows the authoritative CIF matrix. Typed UUID construction/materialization rejects zero, while raw decoding preserves it for diagnostics. Body identities do not replace prologue routing identities; cited-message recall and class-specific identifier/device semantics are not implemented by this structural batch.

Full optimized **165/165 PASS**, **59 standalone headers**; independent full ASan/UBSan **162/162 PASS**. Tests cover literal words, reserved bits, truncation callback barriers, owned UUID lifetime/compaction/atomic failures, scalar/UUID selector and diagnostic extents, allocation probes and baseline profile isolation. Scalar/native snapshot sizes and runtime state budgets remain unchanged. All additions remain Current-only and retain the existing selected-field limits.

The verifier corrected two test expressions that attempted to call the deliberately deleted rvalue snapshot getter; no production API was weakened. Final tested sources and logs: [manifest](artifacts/P14-cif2/manifest.json), [Release log](artifacts/P14-cif2/test-release.log), [implementation](P14-cif2-implementation.md), [independent verification](P14-cif2-verification.md).

CIF3 is the next live batch. Exact raw samples and registered extensions may be prepared independently in isolated drafts; drafts are not integrated or advertised coverage until their own verification gates pass.

## CIF3 checkpoint

All19 named CIF3 field identities are implemented, including explicit enclosing timestamp-format binding for Age/Shelf Life and scope-aware Timestamp Details checks. Native and wire providers share temporal extent calculation. Binding changes are transactional, cached layout identity includes both codes, and encoding rejects envelope/binding mismatches before output mutation. Ordinary Age/Shelf values with TSI=TSF=0 remain unsupported; independent selector/diagnostic layouts do not require timestamps. Pressure is raw17-bit under the accepted policy.

Full optimized **170/170 PASS**, **60 standalone headers**; final independent ASan/UBSan **167/167 PASS**. Coverage includes every timestamp-code pair, semantic-versus-structural environmental checks, epoch table Cartesian cases, fractional-only scope, immutable owned duration lifetime, forged public view bounds and allocation/size regressions. LayoutContext48, FieldView32, SemanticValue16, scalar snapshot5312, native8192 snapshot13512 and runtime StateSnapshot136 remain unchanged on the measured host.

Independent review corrected an applicability API that initially considered only integer timestamp codes; fractional-only timestamps now remain applicable. Additional independent binding/view tests were included in the final rebuilt candidate. Earlier test logs are retained separately; final [manifest](artifacts/P14-cif3/manifest.json), [Release log](artifacts/P14-cif3/test-release.log), [implementation](P14-cif3-implementation.md) and [verification](P14-cif3-verification.md) identify the passing scope.

Raw samples and registered extensions are being promoted from isolated, independently reviewed drafts. Their own final live gates remain pending; numerical sample conversion, CIF7 and remaining CIF1 structures are still incomplete.

## Raw samples and registered extensions checkpoint

Live integration **174/174 optimized PASS**, **171/171 ASan/UBSan PASS**, including **62 standalone headers**. [Implementation](P14-samples-extensions-implementation.md), [raw-sample independent verification](P14-raw-samples-verification.md), [extension independent verification](P14-extensions-verification.md), [source manifest](artifacts/P14-samples-extensions/manifest.json), and [optimized log](artifacts/P14-samples-extensions/test-release.log).

Raw sample access/packing uses caller-supplied structure counts and exact bits/tags, with no automatic numerical conversion or RX lifetime extension. Processing-efficient widths above32 remain explicitly unsupported rather than assigned an invented grouping. Registered extension validation is separate from per-call authorized/admitted dispatch; all extension families, exact class keys, opaque unknowns, custom CAM masks, output failure behavior and allocation probes have independent tests. Actual vendor protocols and transaction/device integration are not implied.

The common extension CAM bit0 correction is independently covered. Its obsolete developer fixture was corrected and the full suites rebuilt; provisional failures remain archived. Existing runtime storage is unchanged. CIF7/generic capacities are now the live implementation batch; fixed/VRT numerical conversion is being prepared in isolation, and remaining CIF1 structures are pending.

## CIF7 and fixed/VRT numeric checkpoint

Full optimized **183/183 PASS**, **180/180 ASan/UBSan PASS**, including **63 standalone headers**. All13 CIF7 attribute shapes share native/wire traversal, including one-word Probability/Belief on structured fields. Typed transactions preserve sibling attributes and immutable old snapshots; explicit caller workspaces bound generic edit scratch. Probability remains raw-code-only. The IQ profile independently rejects every non-Current attribute; generic support adds no device controls.

The baseline decode API remains16 selected fields/64 views. Explicit `decode_packet_bounded<128,1664>` admits larger caller-owned results; an independent82-field/1066-view packet checks all known fixed base fields and attributes. Field occurrences and work are aggregate across diagnostic groups;128/129 boundaries are tested. Recursive depth/record budgets remain the later Array-of-CIFs gate, not a completed claim here.

Measured default sizes remain unchanged: SemanticValue16, FieldEntry328, LayoutContext48, FieldView32, scalar snapshot5312, PacketView2736. Explicit128-field/native8KiB workspace is50248bytes and1664-view result is66736bytes; those are caller-selected storage, not additions to the reference runtime ledger.

The exact fixed/VRT numeric module also passed the live Fraction-oracle259,784 cases and independent allocation/boundary checks. No IEEE numerical, DPF-domain or physical-unit claim follows from that module. IEEE conversion has separate independently verified draft evidence and is being promoted; remaining CIF1 structures and sample descriptor/fragment adapters are next.

[Implementation](P14-cif7-implementation.md), [independent CIF7 verification](P14-cif7-verification.md), [numeric verification](P14-numeric-verification.md), [source manifest](artifacts/P14-cif7/manifest.json), [optimized log](artifacts/P14-cif7/test-release.log). No gate failures or production corrections were needed after this candidate's freeze.

## Final available-input continuation checkpoint

The accepted D-P14-3/general raw-code policy has been implemented without assigning ambiguous engineering conversions. This continuation completes native/wire coverage for the four nonrecursive CIF1 structures (Pointing Vector Structure, Index List, Spectrum and Sector/Step-Scan), explicit-policy IEEE binary16/32/64 conversion, PayloadFormat/domain/padding adapters and bounded segmented sample access. The ordinary registry now has96 named nonrecursive field identities. All13 CIF7 shapes remain shared, and new fields add no IQ-device controls or Context-history permissions.

Final aggregate **198/198 Release PASS**, **195/195 ASan/UBSan PASS**, **69 standalone headers**. Release took33.92s and sanitizer tests55.63s on this macOS arm64 host. The Release source manifest covers the entire public include tree, P14 tests and fuzz sources and remained unchanged across the gate. [Release log](artifacts/P14-final-local/test-udp-release.log), [detailed output](artifacts/P14-final-local/LastTest-udp-release.log), [sanitizer log](artifacts/P14-final-local/test-asan-ubsan.log), [source manifest](artifacts/P14-final-local/manifest-udp-release.json), [freeze check](artifacts/P14-final-local/freeze-udp-release.json), [independent manifest](artifacts/P14-final-local/verifier-source.sha256).

Independent review caught and corrected two defects before final approval: Sector's encoded optional-header count must be0 despite its three physical header words; and a public native-record byte constructor could create invalid optional discriminators. The constructor is now private to trusted arena readers, with supported-type constraints and independent compile-time public-construction regressions. Initial196-test Release and193-test sanitizer runs are preserved as provisional; their passing tests did not cover the subsequently demonstrated invalid-bool case. See [CIF1 implementation](P14-cif1-structures-implementation.md) and [independent verification](P14-cif1-structures-verification.md).

Sample evidence includes the separately written476,480-case optional SoftFloat oracle, portable IEEE literal/policy tests,42,748 descriptor/fragment matrix cases, asymmetric tag and lifetime tests and instrumented C++ allocation checks. The baseline16/64 and explicit128/1664 packet decoders now share the fuzz invariant kernel and literal larger-capacity seeds;100,000 mutations pass. See [sample implementation](P14-sample-adapters-implementation.md), [IEEE verification](P14-ieee-verification.md), [descriptor verification](P14-sample-descriptors-verification.md) and [generic fuzz report](../../fuzz/P14-generic-capacity-report.md).

The explicit I9 structural API validates five fixed CIF masks, record indices, nested extents and shared depth/field/view/work budgets. Its separately authored restricted-schema oracle passes24,000 deterministic mutations (6,173 accepted;17,827 rejected), plus literal/timestamp/callback/resource boundaries. This mutation target is distinct from ordinary packet fuzzing. It exposes structural borrows only; the baseline packet registry still does not admit Array values, and no typed native Array, emission or peer semantic API has been enabled. [Implementation](P14-array-cif-implementation.md), [independent verification](P14-array-cif-verification.md).

Baseline sizes remain SemanticValue16, LayoutContext48, FieldView32, scalar snapshot5312, native8KiB snapshot13512 and ordinary PacketView2736 bytes. The generic Sector materializer has a measured41,568-byte optimized stack frame, plus nested setter calls; it is not an ordinary runtime stack claim. Descriptor72, default segmented view304 and ArrayView120 bytes are explicit caller storage. [Size evidence](artifacts/P14-final-local/sizes.txt) and the CIF1 report distinguish object sizes from stack frames. No baseline runtime pool grew.

### Required-input stop

There is no supplied I9 peer agreement or independent/authoritative layout evidence. Under the protocol interpretation register this prevents peer semantic/native integration and unqualified full-registry qualification; local round trips cannot close it. P14/M5 therefore remains partial. P15/M6 has no selected adapter/backend, device/OS/SDK contract or device access and remains blocked. Linux production-toolchain, independent-peer and actual device qualification are not established by these local macOS tests. P13 receiver characterization remains non-blocking and no new performance/deployment claim is made.
