# V-P14 CIF2 identifier verification

Status: PASS for the frozen CIF2 identifier batch and full ASan/UBSan regression. Scope is all29 named CIF2 fields at bits31..3, including snapshot-owned128-bit UUID values. Generic identifier support does not authorize device operations or alter packet routing, endpoint identity or correlation.

The supplied ANSI/VITA49.2-2017(R2024), printed pp199–208, supplies the field layouts. Country Code Figure9.8.7-1 on p205 was independently rendered and inspected. It draws12 code bits and labels the flag User Defined; the repeated explicit Rules9.8.7-1/2/3 on p204 specify11 code bits and bit15=1 for ISO,0 for user. The coordinator approved those explicit rules governing this codec: code10..0; reserved14..11 and31..16. No ISO assignment database or semantic authority is inferred from the flag.

Independent literal cases will cover every field, all truncation prefixes and reserved-bit barriers, scalar and native encoding against wire literals, UUID deep copy/snapshot relocation/materialization, immutable layout and bounded arena exhaustion. The existing16-selected-field bound remains explicit. Ordinary values, zero-byte selectors and one-word diagnostic entries require separate extent checks. New fields remain unsupported by the four-field IQ profile.

| Fields | Source and independent obligations |
|---|---|
|Bind|§9.8.1 p200: bit0 only, remaining bits reserved; no inferred binding side effect.|
|Cited/Sibling/Parent/Child SID|§9.8.2 pp200–202: one full unsigned32 SID each, not arrays.|
|Cited MID, Controllee/Controller IDs and UUIDs|§9.8.3/.4 pp202–203; UUID four network-order words per§8.2. Body IDs never replace prologue identity. Command-specific cited-message querying is a separate semantic constraint.|
|Information Source, Track|§9.8.5/.6 p204: one full unsigned32 linkage each.|
|Country Code, Operator|§9.8.7 pp204–205; country mask above; Operator low16 with upper16 reserved.|
|Platform Class/Instance/Display|§9.8.8 pp205–206: one word each, no invented Generic16 restriction.|
|EMS Device Class/Type/Instance|§9.8.9 p206: Class low16 organization15..14 (3 reserved), exciter13, receiver12, class11..0; Type/Instance full-word linkage.|
|Modulation Class/Type|§9.8.9 p207: low16, upper16 reserved.|
|Function/Mode/Event/Function Priority|§9.8.10 pp207–208: Generic16, no inferred command or priority ordering.|
|Communication Priority|§9.8.10.5 p208: full-word opaque linkage, class-specific priority interpretation.|
|RF Footprint/Range|§9.8.11 p208: full-word KML linkage; Range is not a physical distance. CIF matrix/appendix select2/3 despite local summary's conflicting2/5.|

No ISO/KML/application database is required to preserve these identifiers structurally. Such inputs remain necessary for assignment validation or device behavior. This report does not claim independent-peer interoperability or complete P14/M5 coverage.

The UUID source cross-check includes printed p97 Rule8.2.6-1 via§9.8 Rule4. Nonzero is an explicit identifier semantic constraint: structural decode may preserve zero, but typed native admission and materialization must reject it without changing the destination. No UUID version, assignment generator or external-standard compliance is inferred from four preserved words. This missing native validation was identified independently before freeze and sent to the implementation owner.

Cited-message command recall constraints (§9.8.4-2/3), body-ID control-point provenance (§9.8.3), identifier assignments and associated class behavior remain outside generic structural codec validation. The baseline IQ profile rejects the additional fields; their parseability does not implement these commands or replace prologue correlation.

## Independent test coverage

- `p14_verify_cif2_wire`:25 scalar identifiers at zero and high values; independent literal encode/decode; every truncated prefix; short-output unchanged; all forbidden Bind/Generic16 bits; all29 fields' selector and diagnostic extents; nonbaseline IQ validation and Context history rejection after satisfying the timestamp precondition.
- `p14_verify_cif2_boundaries`: Country Code 0/1/2047 with both flags and every reserved bit; EMS three organizations and all exciter/receiver combinations with class4095, reserved organization and high-bit rejection; transactional invalid native values; Current-only attributes;29 selected fields rejected under the current16-field limit before callbacks.
- `p14_verify_cif2_native`: literal two-UUID ordering, source mutation and builder destruction, snapshot copy, materialization, all truncation prefixes,16-byte arena exhaustion/100 replacements/reclamation, unsupported zero-capacity arena, unchanged layout after value changes, forbidden foreign native slice injection, rawzero versus typed/materialization rejection. A measured1,000-cycle owned build/copy/encode/decode/materialization loop performs no ordinary/aligned C++ allocations; deliberate probes exercise both hooks. No C allocator or process-wide allocation claim is made.

Direct optimized tests pass3/3 with `clang++ -std=c++23 -O3 -UNDEBUG -fno-exceptions -fno-rtti -Iinclude`. The initial native test incorrectly called two deleted rvalue snapshot getters and failed compilation; it was corrected to retain named lvalue snapshots, exercising the existing ownership API without production changes. Corrected native compile and execution pass. Source/verifier hashes are recorded in [production manifest](artifacts/P14-cif2/source.sha256) and [test manifest](artifacts/P14-cif2/verifier.sha256).

Full ASan/UBSan configure/build/regression passes **162/162** in38.65 seconds. Commands: `cmake --preset udp-asan-ubsan`, `cmake --build --preset udp-asan-ubsan -j4`, `ctest --preset udp-asan-ubsan --output-on-failure`. Durable [configure](artifacts/P14-cif2/configure-asan-ubsan.log), [build](artifacts/P14-cif2/build-asan-ubsan.log) and [test](artifacts/P14-cif2/test-asan-ubsan.log) logs record the actual execution. All production and verifier manifest hashes were rechecked unchanged after the gate.

The independent [size probe](artifacts/P14-cif2/sizes.txt) confirms SemanticValue16, scalar snapshot5312, native8192 snapshot13512, StateSnapshot136, UUID16 bytes. Existing baseline/reference-budget tests pass in the full suite. This is local macOS functional/sanitizer evidence; external interoperability, Linux/device qualification and complete P14/M5 are not claimed. The coordinator's complete optimized Release gate also passes **165/165**, including59 standalone public headers; its [test log](artifacts/P14-cif2/test-release.log) records that separate integration run.
