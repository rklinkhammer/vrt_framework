# V-P14 CIF7 and explicit generic bounds verification

Status: PASS for the independent CIF7 contracts and full ASan/UBSan integration, including live exact fixed/VRT numeric conversion. Frozen working-tree identity is recorded in the [production](artifacts/P14-cif7/source.sha256) and [verifier](artifacts/P14-cif7/verifier.sha256) manifests.

The supplied ANSI/VITA49.2-2017(R2024) §9.12, printed pp219–221, defines attribute order31..19, eleven base-shaped attributes and separate one-word Probability/Belief layouts. Explicit CIF7zero and reserved bits18..0 remain invalid. Probability raw code/function is tested under the accepted conservative policy; no percentage conversion is selected. Statistical population, derivative interpretation and class meaning are not inferred from structural success.

## Independent cases

`p14_verify_cif7_wire` uses literal wire values for all13 attributes on a64-bit field, every truncation, reserved attribute bits and explicit-empty CIF7. Probability/Belief on UUID, GPSASCII, associations and both temporal durations proves shape selection precedes native-arena/base-timestamp requirements. Every reserved bit of those code words is checked. Three GPSASCII attributes use distinct lengths; another three total4506work units, proving the4096budget is shared rather than reset per attribute.

An independent registry inventory covers **82 fixed-base fields ×13 attributes =1066 views**, with field/attribute ordering, exact wire widths and readable semantic alternatives checked. Explicit `<128,1664>` succeeds; ordinary16/64 rejects;81field or1065view capacity rejects. A20field×13 packet separately exercises exact259/260 view capacity and work rejection. No reserved or unsupported fields were invented to manufacture128 distinct ordinary fields; the current registry does not contain that many.

`p14_verify_cif7_bounds` proves the exact128aggregate occurrence limit with64warning+64error fields, rejects65+64 under128, and accepts it under explicit129/129. The ordinary API rejects before callbacks; one packet-wide work bound covers both groups. Missing request correlation remains opaque/requires-context even with larger generic capacity. Future nested-record occurrence/depth evidence is a separate pending registry batch.

`p14_verify_cif7_native` checks heterogeneous scalar/native transactions, caller workspace, exactly one generation increment, mixed-length sibling attributes, immutable earlier snapshots and encode identity, snapshot-borrowed input, attribute-preserving materialization, duplicate/missing/arena-exhaustion rollback, foreign native-slice rejection, complete new UUID insertion, and zero-arena Probability-only GPS/UUID/Age. NonCurrent derivative representations preserve a GPS latitude code beyond Current physical latitude bounds and a negative SampleRate derivative. The complete exercised path allocates zero ordinary/aligned C++ objects; positive probes exercise both counters. No arbitrary C-allocator or process-wide claim is made.

`p14_verify_cif7_profile` sends each of the12 nonCurrent attributes through the actual production Engine. Every case parses generically but reports unsupported, performs zero backend begins/writes and preserves effective SampleRate. The baseline receiver history also rejects these nonCurrent values after valid timestamp prerequisites are supplied. Registry expansion therefore does not grant IQ device permissions.

Two legacy verifier assertions were updated from Query-Minimum rejection to generic acceptance plus reserved-bit18 rejection. Those assertions described the prior incomplete generic registry; missing values, forged slices and device nonCurrent rejection remain tested. Historical earlier reports/manifests are unchanged.

## Numerical companion

The live exact fixed/VRT numeric promotion is included in this integrated gate. Its independent Fraction oracle enumerates representable tables and neighboring values without calling production decode for expected results:259,784cases across98 tiny specifications plus64-bit and extreme exponent anchors. The stdin driver is a build target only; CTest invokes its Python oracle. Separate numeric boundary tests include6,400 instrumented allocation-free conversions and positive allocation probes. The initial isolated [independent report](../../drafts/P14-sample-numeric/verification/report.md) details its mathematical scope; IEEE numeric conversion and engineering-unit interpretation are excluded.

No full P14/M5, external peer, Linux or device qualification follows from this local macOS gate. Generic output/workspace capacities are explicit caller costs, not a claim that every128-field native packet fits8KiB.

## Final independent integration

The full configured/built ASan/UBSan suite passes **180/180** in44.91 seconds, with no failures or test/source corrections during the frozen gate. [Configure](artifacts/P14-cif7/configure-asan-ubsan.log), [build](artifacts/P14-cif7/build-asan-ubsan.log) and [test](artifacts/P14-cif7/test-asan-ubsan.log) logs are preserved. Commands: `cmake --preset udp-asan-ubsan`, `cmake --build --preset udp-asan-ubsan -j4`, `ctest --preset udp-asan-ubsan --output-on-failure`. All nine production and ten verifier manifest hashes match after testing.

The independent [size probe](artifacts/P14-cif7/sizes.txt) confirms SemanticValue16, FieldEntry328, LayoutContext48, FieldView32, ordinary PacketView2736, scalar snapshot5312 and native8192 snapshot13512. Explicit generic view1664 costs66736bytes;128-field/8192native snapshot and workspace each cost50248bytes; transient AttributeInput120bytes. Generic capacities therefore remain explicit caller storage. Existing runtime/reference-budget, old ownership/lifecycle, protocol and malformed-input regressions pass in the same suite.

The coordinator's optimized Release gate independently passes **183/183** in30.47 seconds, including63 standalone public headers. [Release results](artifacts/P14-cif7/test-release.log) and [detailed test output](artifacts/P14-cif7/LastTest-release.log) record that checkpoint.
