# V-P14 independent verification

Current scope notice: the user accepted M5 completion against the operational profile with Array-of-CIFs excluded from production support. The independent operational review now passes and the coordinator has closed the M5 local software gate for that scope. The explicit I9 structural utility remains optional, with no peer semantic, emission or universal VITA conformance claim. Missing Array peer agreement therefore does not block this M5 scope. P15/M6 remains separately blocked on required hardware inputs. See [M5 operational scope](M5-operational-scope.md) and [operational verification](M5-operational-verification.md).

Status: first CIF0 scalar batch PASS. This is not an M5 completion verdict.

Readiness authority: implementation plan P14/M5, protocol design §3.2 and interpretation register I9, current P02 traversal/P03 storage APIs, and user-provided ANSI/VITA-49.2-2017 (R2024). Normative references below use printed page numbers. The PDF text was read directly; temperature bit layout and the conflicting Array-of-CIFs figures on pp222–223 were also rendered and visually inspected.

| CIF0 bit | Field | Normative source | Independent oracle obligation |
|---|---|---|---|
| 29 | Bandwidth | §9.5.1 p150 | signed Q20, nonnegative semantic range; 1-Hz and least-bit literal bytes |
| 28 | IF Reference Frequency | §9.5.5 pp153–154 | signed Q20; negative and extreme bit patterns; contextual signal-range rules not invented by standalone codec |
| 27 | RF Reference Frequency | §9.5.10 pp159–160 | signed Q20, full signed range |
| 26 | RF Reference Frequency Offset | §9.5.11 pp160–161 | signed Q20, negative fractional value |
| 25 | IF Band Offset | §9.5.4 pp152–153 | signed Q20, negative whole/fractional values |
| 24 | Reference Level | §9.5.9 pp157–159 | signed low-half Q7, upper half reserved zero |
| 23 | Gain/Attenuation | §9.5.3 pp151–152 | Stage1 low half, Stage2 high half, independently signed Q7 |
| 22 | Over-Range Count | §9.10.6 p215 | full unsigned word; event is per paired packet, not persistent metadata |
| 20 | Timestamp Adjustment | §9.7 pp189–190; §9.7.3.1 p193 | signed 64-bit femtoseconds, high word first; distinct from picosecond timestamp |
| 19 | Timestamp Calibration Time | §9.7.3.3 p193; temporal table p190 | unsigned word; epoch interpreted by enclosing TSI, no implicit GPS conversion |
| 18 | Device Temperature | §9.10.5 p214 | signed low-half Q6, high half reserved; minimum legal quantized value -17481, reject -17482 semantically |
| 17 | Device Identifier | §9.10.1 p212 | low24 OUI in first word, low16 device code in second; reserved bits reject; all24-bit OUI encodings structurally representable, profile-specific permission separate |
| 10 | Ephemeris Reference Identifier | §9.4.4 p140 | full unsigned Stream ID word |

Batch gate includes independent literal-wire decode and encode comparison, ascending insertion versus descending CIF wire ordering, selector and diagnostic forms, truncated input/short output sentinels, reserved-bit callback barrier, semantic rejection before mutation, current-only attribute restriction, and explicit unknown-layout rejection for remaining structured fields. Baseline P02/P09 and actual reference-budget regressions must pass. The existing selected-field bound remains 16 for this batch; a legal 17-field body must fail boundedly without callbacks. Generic scalar recognition must not expand the four-field IQ profile or its metadata history.

Six remaining CIF0 structured fields (bits14,13,12,11,9,8), other CIF words, general attributes, arrays, samples and extensions remain separate work. Their unsupported status is not concealed by this scalar batch. Current-only new descriptors do not claim all13 attribute support.

I9 does not block these scalar fields. The architecture already selects a conservative Array-of-CIFs dialect: three base words plus five CIF words, mandated HeaderSize7 and total8+record_words*count. The conflicting prose/figures are present in the normative PDF. Implementing that explicit variant is possible later; its peer-independent interoperability qualification still requires independent implementation evidence or authoritative clarification, and semantic use remains disabled until agreement. No alternative layout or padding was inferred.

## Scalar candidate independent gate

Targeted ASan/UBSan gate: PASS, four tests. `p14_verify_scalars` independently constructs literal wire words for all13 fields and compares decoded semantic values and encoded bytes. It covers selector/cancellation zero-value bodies, one-word diagnostic extents, truncation and reserved-bit callback barriers, short-output immutability, signed bandwidth/temperature raw preservation versus semantic rejection, current-only attributes, unsupported structures, 17-field bounded rejection, and fixed-IQ validation/history rejection. `p14_verify_allocation` exercises1000 iterations of all13-field construction, snapshot, measure, encode and decode; ordinary and aligned C++ allocation counters remain unchanged. Deliberate allocation probes verify instrumentation is active. This measures C++ new paths, not arbitrary external C allocators; the reviewed scalar path contains no C allocation calls.

The initial verifier draft used range-for on a non-range FixedVector and an incorrect measure namespace. These test-only compile errors were corrected before execution. No production change was needed for independent findings.

Frozen production source hashes match the implementer manifest:

| File | SHA-256 |
|---|---|
| fields/types.hpp | ec1591dcee1a1a7225a3edab47adfa0ff752b5e0f6d4311af336db9aa96ff930 |
| codec/packet.hpp | c7e4d1f5cbafc6f819a5907a5025b99c0e1c630028003718c4bd2520d5250993 |
| codec/scalar.hpp | e76c7ad4099b5997b60713447af74d162455722cbd85d965911f4e45d55aa5d4 |
| verification/P14/scalar_contract.cpp | af94db8d1738215e82af28bdc3a96e89be62237b5b231b9d10c16a525c2d9bfb |
| verification/P14/allocation_contract.cpp | 7f786ffe80b155d91372ed439272dfa358a8ae96a9044a9987392338b4e91137 |
| verification/P14/CMakeLists.txt | 97891203c72bfe05775f335d9c01189645ab64aee9d36df7a2dae44500d14ff5 |

Git base is dd1597e0a439bf180f41711b2725b44fd81fe0bd; the above uncommitted candidate hashes identify the actual verified source. Commands: `cmake --preset udp-asan-ubsan`, `cmake --build --preset udp-asan-ubsan -j4`, `ctest --preset udp-asan-ubsan -R '^p14_' --output-on-failure`. Full affected-suite result follows.


Full affected gate: `ctest --preset udp-asan-ubsan --output-on-failure` PASS147/147 in33.51s. This includes P02/P09 traversal/history, fixed IQ runtime, all actual reference/category budget tests, standalone public headers and bounded malformed mutation replay. The unchanged16-byte SemanticValue assertion and dependent reference-budget oracles pass. Post-test source hashes remain identical to the frozen manifest. No shared-state concurrency behavior changed in this scalar batch; no additional TSan claim is made.

Durable evidence: [targeted ASan/UBSan log](artifacts/P14/scalar-targeted-asan.log), [full ASan/UBSan log](artifacts/P14/scalar-full-asan.log). Coordinator separately reports optimized integration150/150 in `artifacts/P14/test-release.log`; this independent verdict is based on the direct sanitizer run and literal oracles above. Local macOS evidence does not replace external peer or device qualification.


Structured-batch integration addendum: the six former unknown CIF0 structures are now separately verified in [P14-structured-verification.md](P14-structured-verification.md). The scalar negative fixture now targets an unregistered CIF1 field. Scalar functionality and allocation tests pass again in the154-test sanitizer gate; its current verifier/source manifest is linked from that report. Historical scalar freeze hashes above remain unchanged evidence of the earlier batch.
