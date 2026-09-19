# V-P14 structured CIF0 verification

Status: structured CIF0 batch PASS. Scalar batch evidence remains in P14-verification.md. This batch does not complete M5.

The supplied ANSI/VITA-49.2-2017 (R2024) PDF was read directly. Printed pp139,141,225 were rendered and inspected independently to establish the fixed field sizes, embedded timestamp placement and association-count bit positions.

| Field | Bit | Source | Literal oracle contract |
|---|---:|---|---|
| Formatted GPS |14|§9.4.5 pp141–144|11words; embedded TSI27:26/TSF25:24; low24 OUI; independent fix time; signed Q22 angles/Q5 altitude/Q16 speed; unknown0x7fffffff |
| Formatted INS |13|§9.4.6 p144|same11word representation, distinct ID |
| ECEF Ephemeris |12|§9.4.3 pp138–140|13words; positions Q5, attitude Q22, velocity Q16; unknown values |
| Relative Ephemeris |11|§9.4.9 pp146–147|explicit Rule9.4.9-1 chooses13word ECEF representation; conflicting11word figure is recorded, not silently adopted |
| GPS ASCII |9|§9.4.7 pp144–145|low24 OUI; uint32 word count; exact2+N extent; ASCII bytes and trailing null padding; supplied sentence validator required for typed semantic completeness |
| Context Association Lists |8|§9.13.2 pp224–227|source count bits24:16/system8:0; vector31:16/A15/async14:0 in second word; source,system,vector,async,tags ordered lists |

Generic read support preserves physical representations, including semantically invalid numeric ranges, while typed semantic setters reject invalid values. Undefined embedded TSI/TSF retains all timestamp words and requires corresponding all-ones values. Sample/free-running count is not converted into picoseconds. Standalone scalar/profile rules do not supply altitude reference, Relative Ephemeris axes, OUI meanings or GPS sentence grammar.

The approved configuration limit counts **all association SID and optional tag entries together** against1024, and all visited units against4096. Wire source/system limits511, vector65535, async32767 remain separate. Structurally legal over-budget fields return bounded resource errors, not guessed offsets or a false malformed claim. Whole-packet validation precedes callbacks.

Native ownership gate: scalar default native capacity0 remains unchanged; arena-enabled snapshots own copied native values. Caller spans may change after a setter and builders may change or die after freeze without affecting existing snapshots. Getters borrowing snapshots must reject temporaries. Private native slices cannot be forged or transplanted through raw set_value/attributes. Failed allocation, semantic validation, attribute application or replace leaves old snapshot/generation/shape intact. Compaction reclaims removed/replaced storage; equal-total-size different field partitions invalidate shape caches. These tests measure bounded inline storage and explicit copy costs, not an unimplemented heap fallback or transport lease.

No new user architecture decision blocks this batch. I9 is unrelated to these six fields and remains the existing later interoperability gate. P15/M6 hardware readiness is separately blocked on missing selection/device inputs.

Later CIF1/CIF3 interpretation blockers are now recorded in [P14-decisions.md](P14-decisions.md). Their source contradictions and arithmetic were independently confirmed; the proposed raw-only continuation remains unaccepted. The six CIF0 fields in this report are independent of those disputed mappings.

Independent oracle sources (targeted ASan/UBSan PASS):

- `structured_wire_contract.cpp`: literal wire fields; typed fixed-value interpretation;13-word Relative boundary; ASCII/list extents; reserved bits; callback barriers; non-current materialization rejection.
- `native_contract.cpp`: snapshot/caller lifetime, native slice ownership, failed edit rollback, compaction, temporary-view restriction, equal-size unequal-shape rejection and equal-shape different-value reuse.
- `structured_limits_contract.cpp`:0/1/1024 entries,1025 overflow, tags counted within1024, decode-specific larger bounds versus materialization limits;4092/4093/4094-character ASCII boundary accounting.
- `native_allocation_contract.cpp`:1000 bounded native build/freeze/get/replace/remove/encode/decode cycles under ordinary/aligned C++ allocation counters, plus deliberate injection proving detection.

Early independent review identified inconsistent ASCII work accounting (native characters versus padded wire bytes), native object-byte hashing including padding instead of shape metadata, and scalar non-current FieldView materialization silently becoming Current. These are implementation findings, not new architecture decisions; candidate fixes and regression results will be recorded at the gate.


## Frozen targeted gate

Targeted command `ctest --preset udp-asan-ubsan -R '^p14_' --output-on-failure`: PASS11/11. All five independent structured targets and preceding scalar targets passed. The named mutations target executes20,000 deterministic mutations from four independently authored literal seeds; this is bounded local parser/materialization evidence, not exhaustive fuzzing or an interoperability claim.

All early findings were corrected before freeze: signatures use shape metadata, ASCII native/wire work accounting includes padded bytes consistently, materialization rejects non-current attributes, and impossible arena capacity rejects before sentence callback execution. The corresponding independent regressions pass. No production repair was performed by the verifier.

Measured object sizes: SemanticValue16bytes; scalar snapshot5312; native8192 snapshot13512; existing StateSnapshot136; Geolocation80; Ephemeris96. The generic arena is explicit caller-owned storage and must include its bounded edit/materialization scratch in the caller's budget. Baseline runtime objects did not silently acquire8192-byte arenas. These are sizeof measurements, not compiler stack-frame measurements.

Evidence: [eight frozen production source hashes](artifacts/P14/structured-source.sha256), [verifier source hashes](artifacts/P14/structured-verifier.sha256), [sizeof output](artifacts/P14/structured-sizes.txt), [targeted sanitizer log](artifacts/P14/structured-targeted-asan.log). The base Git revision remains dd1597e0a439bf180f41711b2725b44fd81fe0bd; hashes identify the actual uncommitted candidate. The source hash set matches the implementer's frozen manifest.

The scalar verifier's historical 'unsupported structured CIF0' negative fixture was updated to a still-unregistered CIF1 field after this batch added those six descriptors. This reflects new supported coverage, not removal of unknown-layout validation. The original scalar report/hash/log remains historical evidence.


## Full affected verdict

`cmake --preset udp-asan-ubsan`; `cmake --build --preset udp-asan-ubsan -j4`; `ctest --preset udp-asan-ubsan --output-on-failure`: **PASS154/154**,50.86seconds. The [full sanitizer log](artifacts/P14/structured-full-asan.log) includes P02/P09 traversal/history, IQ baseline/runtime, reference/category memory budgets, standalone public headers, original bounded malformed replay and the new structured targets. All eight production hashes were rechecked after execution and remained unchanged. No extra TSan claim is made: this batch introduces owned value copying and serialized traversal, not a new concurrent protocol.

The largest independent raw-bound test uses a65535-word Context containing65530 vector entries. Default1024-entry/work limits reject it boundedly; explicitly expanded decode limits admit its checked borrowed view and last-entry access. The reference typed materializer remains bounded to1024 combined SID/tag entries. Native and wire ASCII boundary tests exercise4092/4093/4094characters, including the different null-padding extent. These are distinct from the20,000 seeded mutations, which operate on bounded small packets.

Native operation allocation evidence covers1000 cycles of setters, snapshot copy/getters, replacement/removal, encoding and decoding. It instruments ordinary/aligned C++ new paths and checks both with deliberate allocations; no assertion is made about arbitrary external C allocation or all-process heaps. Reviewed arena/codec operations have no heap fallback. Caller-owned arena and bounded candidate scratch costs are explicitly separate from unchanged runtime reference-budget results.

Coordinator separately reports optimized integration157/157 and57 standalone headers in `artifacts/P14-structured/test-release.log`. Independent sanitizer/literal evidence above is local macOS software verification; it does not qualify external peers, GPS sentence formats, Relative frames or hardware.

Stop before the unresolved Beam Width/Barometric interpretation batches recorded in [P14-decisions.md](P14-decisions.md). M5 remains incomplete; P15/M6 still lacks its required selected device/backend inputs. No pending proposal or disputed engineering-unit mapping was applied by this verification.
