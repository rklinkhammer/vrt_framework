# P14 generic codec coverage

Status: final local continuation passes198/198 Release and195/195 ASan/UBSan checks, including69 standalone headers. All96 nonrecursive named field identities and sample/attribute extensions listed below are integrated. I9 has an explicit structural-only API; native/peer semantic integration and full M5 qualification remain blocked on missing peer inputs. All raw-code decisions are accepted. Historical checkpoints follow the current matrix.

| Batch | Scope | Status / evidence |
|---|---|---|
| CIF0 baseline | Reference Point, Sample Rate, State/Event, Data Payload Format | Previously implemented and verified in P01/P02; affected regressions passed in final157/154 optimized/sanitizer gates |
| CIF0 remaining scalars | 13 fields: bits29–22,20–17,10 | PASS:150 optimized checks and147 ASan/UBSan checks; per-field clauses and native representations in [implementation report](P14-implementation.md); independent vectors in [verification report](P14-verification.md) |
| CIF0 structures/lists | Formatted GPS/INS, ECEF/Relative Ephemeris, GPS ASCII, Context Association Lists | PASS: snapshot-owned arena and shared traversal; [implementation](P14-structured-implementation.md), [independent verification](P14-structured-verification.md); integrated157/157 optimized and154/154 sanitizer checks |
| CIF1 fixed | 21 spatial/signal/identifier/status fields | PASS: optimized160/160 andASan/UBSan157/157; [implementation](P14-cif1-fixed-implementation.md), [independent verification](P14-cif1-fixed-verification.md). Beam Width raw codes only under accepted D-P14-1 |
| CIF1 nonrecursive structures | 3D Vector Structure, Index List, Spectrum, Sector/Step-Scan | PASS in198/195 gate: [implementation](P14-cif1-structures-implementation.md), [independent verification](P14-cif1-structures-verification.md) |
| Array of CIFs (I9) | Explicit standalone structural validation/borrowed traversal | Structural PASS with24,000 independent mutations: [verification](P14-array-cif-verification.md). Ordinary packet/native/peer semantic integration and emission remain blocked; no full-interoperability claim |
| CIF2 | 29 identifiers and UUID fields | PASS: optimized165/165 andASan/UBSan162/162; [implementation](P14-cif2-implementation.md), [verification](P14-cif2-verification.md) |
| CIF3 | 19 temporal/environmental fields | PASS: optimized170/170 andASan/UBSan167/167; [implementation](P14-cif3-implementation.md), [verification](P14-cif3-verification.md). Barometric Pressure raw codes only under accepted D-P14-2 |
| CIF7 | All13 named attributes where layout is defined; transactional traversal | PASS: all13 shared shapes and atomic native edits; [verification](P14-cif7-verification.md),183 optimized/180 sanitizer checks |
| General samples | Real, nonbaseline fixed-point/exponent/IEEE widths, polar, tags/repetition/padding | PASS in198/195 gate: exact raw packing, fixed/VRT and IEEE conversion, DPF/domain/padding mapping and bounded segmented access. [Sample implementation](P14-sample-adapters-implementation.md), [IEEE verification](P14-ieee-verification.md), [descriptor verification](P14-sample-descriptors-verification.md). Processing-efficient widths>32 and unspecified engineering-unit mappings remain explicitly unsupported |
| Extensions | Immutable registered extension-class validation/encoding/dispatch contract | PASS: [independent all-family verification](P14-extensions-verification.md);174 optimized/171 sanitizer aggregate |
| Generic bounds | 128 selected/nested values,256 records,1024 list entries, depth4/work4096 | PASS for explicit128/1664 flat/diagnostic budgets and standalone I9 shared recursive budgets; baseline16/64 retained. Native recursive materialization remains pending the I9 semantic gate |

The selected I9 Array-of-CIFs variant remains peer-dependent. The protocol appendix requires independent implementation evidence or authoritative clarification for full-registry qualification; an internal round trip or a repeated self-generated vector cannot satisfy that gate. No alternate layout is guessed.

M5 requires completed advertised field/layout/attribute/format rows with clause-linked tests, nested-length/work-budget fuzzing, sample conversion evidence, and affected shared traversal regressions. Unsupported entries remain unsupported until their own candidate passes. No new field automatically becomes an IQ-generator control or persistent Context-history field.

Readiness inventories for continuation: [CIF1/2/3](P14-cif123-readiness.md) and [general samples](P14-sample-readiness.md). These inventories are historical preparation, not verification evidence. The default16-field/64-view decode API remains; explicit generic capacities and all13 attribute shapes are now integrated as recorded below.

Additional readiness: [CIF7, nested traversal, extensions and sample contracts](P14-remaining-contract-readiness.md). Probability scaling remains interpretation-limited under accepted D-P14-3; exact code preservation is authorized, not a percentage dialect.

## Final continuation

[Final M5 checkpoint](M5-integration.md#final-available-input-continuation-checkpoint) records198/195 tests,69 headers, the two independently caught CIF1 defects and repairs, source manifests, sample/reference/mutation evidence, memory costs and the required-input stop. This supersedes the in-progress language in earlier historical checkpoints. P14 remains partial for I9 native/peer integration; P15 remains blocked on hardware inputs.
