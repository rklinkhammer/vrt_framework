# V-P14 CIF1 fixed-field verification

Status: PASS for the frozen 21-field CIF1 fixed batch and affected regressions. User acceptance of D-P14-1/2 permits conservative raw-code support; it does not select engineering-unit conversions for the disputed fields.

Independent source: supplied ANSI/VITA49.2-2017 (R2024), printed pages below. Angular layouts on pp134,146,148 and paired SNR/EbNo layouts on pp156,164 were rendered and visually inspected. The proposed batch has21 fixed fields, Current attribute only; four-field IQ behavior remains unchanged.

| CIF1 bit | Field | Clause / printed page | Literal/semantic obligation |
|---|---|---|---|
|31|Phase Offset|§9.5.8 p157|signed low16 Q7 radians, reserved upper16|
|30|Polarization|§9.4.8 pp145–146|signed upper tilt/lower ellipticity Q7; no invented orientation|
|29|3D Pointing Vector|§9.4.1.1 p134|unsigned low azimuth Q7 up to65535, signed upper elevation Q7 within±11520|
|27|Spatial Scan Type|§9.4.11 p148|low16 identifier, upper16 zero; class semantics separate|
|26|Spatial Reference Type|§9.4.12 pp148–149|user ID31..16, reserved15..4, reference3..2, beam1..0; beam3 reserved|
|25|Beam Width|§9.4.2 p137; D-P14-1|upper/lower16 raw codes preserved including0xb400; no degrees conversion or invented signedness|
|24|Range|§9.4.10 pp147–148|unsigned32 Q6 metres, exact last representable code, class distance meaning separate|
|20|Eb/No + BER|§9.5.17 pp163–164|upper/lower signed Q7;0x7fff unused each; BER>0 forbidden except unused|
|19|Threshold|§9.5.13 p162|upper stage2/lower stage1 signed Q7; retain pair without inferring mode; explicit validator tests single-dB, single-dBm, window|
|18|Compression Point|§9.5.2 p151|signed low16 Q7 dBm; upper16 zero|
|17|Intercept Points|§9.5.6 pp154–156|upper second/lower third, signed Q7;0x7fff unused/no-distortion|
|16|SNR/Noise Figure|§9.5.7 p156|upper signed SNR,0x7fff unused; lower nonnegative NF,0 unused; preserve distinct sentinel meanings|
|15|Aux Frequency|§9.5.14 pp162–163|signed64 Q20 high word first|
|14|Aux Gain|§9.5.15 p163|same stage order and signed Q7 as CIF0 Gain|
|13|Aux Bandwidth|§9.5.16 p163|signed64 Q20 representation, nonnegative semantic value|
|6|Discrete I/O32|§9.11 p218|full unsigned word, no inferred device action|
|5|Discrete I/O64|§9.11 p218|full unsigned64 high word first|
|4|Health Status|§9.10.2 pp212–213|low16 identifier, upper16 zero|
|3|V49 Compliance|§9.10.3 p213|full word; assigned semantic codes1..4|
|2|Version/Build|§9.10.4 pp213–214|year7/day9/revision6/user10; day1..366, no invented leap-year rule|
|1|Buffer Size/Status|§9.10.7 pp215–216|two words: unsigned capacity then reserved16/level8/status8; summary's one-word count overridden by detailed field definition|

Gate plan: independent literal single-field and mixed CIF ordering vectors, native semantic inspection and encoding against literals, reserved-bit callback barriers, raw invalid-range preservation versus typed rejection, named sentinels, Current-only attributes, selector/diagnostic extent independence, exact size and allocation evidence, P01/P02/P09/runtime reference regressions. The selected-field bound remains16 until a separately verified traversal expansion;21 fields must not be silently accepted into a16-field container.

Threshold usage, buffer level/fullness scale, health/discrete-I/O meaning, polarization orientation and spatial reference remain explicit class/hardware inputs. Generic serialization does not invent those inputs. Beam Width engineering conversion remains unsupported by the accepted conservative decision. Unimplemented variable CIF1 layouts and other P14 work remain visible.


## Independent candidate evidence

`p14_verify_cif1_fixed` covers21 literal value vectors, native values, independent encode comparison, all truncation prefixes, unchanged short output, selector/diagnostic extents, mixed CIF0/CIF1 ordering, bounded rejection of21 fields under the16-field policy and fixed-IQ control policy rejection. Its measured loop executes21,000 fixed-field build/snapshot/measure/encode/decode operations with no ordinary/aligned C++ new calls; deliberate probes verify both counters. This excludes arbitrary C allocators or process-wide heap claims.

`p14_verify_cif1_boundary` verifies full signed Q7 phase/polarization representation without invented angle normalization, pointing elevation bounds, azimuth65535, reserved bits/code callback barriers, raw versus semantic invalid values, NF0 versus NF0x7fff, SNR/EbNo/intercept sentinel distinctions, explicit Threshold modes, Version day/revision/user bounds, raw BeamWidth codes and class-defined buffer level. Initial SpatialReference beam3 admission was independently flagged and corrected before freeze; the reserved-code regression now passes.

Direct optimized C++23 `-O3 -UNDEBUG -fno-exceptions -fno-rtti` compile/run: PASS2/2. Targeted ASan/UBSan P14 suite: PASS14/14. [Direct commands](artifacts/P14-cif1-fixed/direct-release.txt), [targeted sanitizer log](artifacts/P14-cif1-fixed/targeted-asan.log), [frozen production hashes](artifacts/P14-cif1-fixed/source.sha256), [verifier hashes](artifacts/P14-cif1-fixed/verifier.sha256), [measured sizes](artifacts/P14-cif1-fixed/sizes.txt).

Actual sizes remain SemanticValue16, scalar snapshot5312, native8192 snapshot13512 and baseline StateSnapshot136. Nullable Q7 pairs use the implementation's explicit OptionalQ7 value; independent tests distinguish checked empty and engaged access, rather than inferring null from an ordinary numeric zero.

Both existing 'unknown field' verifier fixtures were moved to unregistered CIF3 bit0 after CIF1 bits30/31 became recognized. The corresponding P01 developer fixture was updated by its owner. A first full ASan run used the stale P01 developer binary and failed its old unsupported-field expectation; [that provisional log](artifacts/P14-cif1-fixed/provisional-asan-stale-fixture.log) is retained. It is not a passing gate or a production defect; the rebuilt final regression passes below.

## Final frozen gate

The full rebuilt ASan/UBSan suite passes **157/157** in23.62 seconds; all four frozen production hashes still match. [Sanitizer results](artifacts/P14-cif1-fixed/test-asan-ubsan.log) and [build log](artifacts/P14-cif1-fixed/build-asan-ubsan.log) preserve the evidence. Commands: `cmake --build --preset udp-asan-ubsan -j4`, then `ctest --preset udp-asan-ubsan --output-on-failure`.

The coordinator's separately built optimized Release gate passes **160/160**, including58 standalone public headers. [Release results](artifacts/P14-cif1-fixed/test-release.log). The dependent reference-budget and baseline runtime tests pass without increasing SemanticValue or the measured snapshot/state sizes. Candidate identity is the recorded source manifest over the working tree; the Git base alone is not the tested source revision.

This is local macOS functional, sanitizer and regression evidence for this bounded batch. It does not establish full P14/M5 registry coverage, independent-peer interoperability, Linux qualification or P15/M6 device qualification. Probability's conflicting scaling rule remains a separate unresolved interpretation; no conversion for it was implemented or accepted by this gate.
