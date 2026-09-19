# P14 Array-of-CIF structural subset

Current scope notice: the user accepted M5 completion against the operational profile with Array-of-CIFs excluded from production support. The independent operational review now passes and the coordinator has closed the M5 local software gate for that scope. The explicit I9 structural utility remains optional, with no peer semantic, emission or universal VITA conformance claim. Missing Array peer agreement therefore does not block this M5 scope. P15/M6 remains separately blocked on required hardware inputs. See [M5 operational scope](M5-operational-scope.md) and [operational verification](M5-operational-verification.md).

Status: independently reviewed structural component promoted; final aggregate gate passed198/195. This is the explicit accepted I9 capability, not unqualified Array-of-CIF interoperability.

`codec/array_cif.hpp` requires explicit `Options{Dialect::i9_five_cifs_header7,timestamps}` and exposes transactional `validate(Bytes,Options,Budget&)`, a privately constructed borrowed View, checked record spans and structural visitation. The five fixed CIF words follow the three base words; encoded HeaderSize7 and total8+record_words*records implement accepted I9. No alternate size or padding convention is guessed.

All five masks are physically present. Permitted CIF0 change/enables are preserved without adding record fields. Reserved bits reject. The bounded capability supports zero CIF7 with bit7 clear as implicit Current, or nonzero CIF7 with bit7 set. Other enable/attribute combinations return unsupported capability, not a claim that every peer interpretation is malformed. Unknown nested field layouts reject without guessed offsets.

One shared field/attribute/order/extent walker handles nested record contents. An additive default descriptor-resolver hook permits the explicit Array validator to recognize CIF1/11; ordinary packet registry behavior is unchanged. The baseline packet decoder still rejects a values-shaped Array-of-CIF field. Callers must explicitly invoke this structural API on a known bounded field span; it is not an automatic packet-level semantic integration.

Budget defaults are128 aggregate field occurrences,1664 views,4096 work units,256 records per array and depth4; per-field index/association/record bounds flow through the same extent metadata. A shared cursor can cover multiple sibling validations; failed validation leaves it unchanged. Local size validation precedes resource classification. Record ordinal/index paths distinguish repeated raw indices. Nested structural visits are depth-first postorder. Validation itself invokes no application callbacks; later visitation requires immutable backing bytes and does not consume the caller's budget again.

The reviewed implementation and independent oracle/mutation tests were promoted unchanged. The live layout change applies only the additive resolver patch, preserving the newer CIF1 provider and native-view repair. Independent evidence covers24,000 nested length/mask/work/depth mutations against a separately authored restricted-schema parser, plus timestamp, limits, callback and literal cases. See [independent report](P14-array-cif-verification.md). Generic packet fuzzing separately covers both16/64 and128/1664 paths; it does not enter this standalone recursion API.

## Excluded production scope and separate required inputs

No peer agreement, independently interoperating implementation or authoritative clarification for I9 was supplied. No typed native Array setter/materialization, Array emission, peer semantic dispatch or application relationship semantics has been added. These capabilities remain unavailable under the protocol interpretation register's semantic-use gate and are now excluded from the accepted operational M5 production scope. Structural test vectors cannot substitute for peer evidence. The earlier partial-M5 assessment applied to the broader full-registry scope; it is not a blocker for the newly accepted operational scope. Full generic-registry/interoperability qualification is still not claimed.

P15/M6 separately requires selection of an adapter/backend, device/OS/SDK contracts and actual device access. None is inferred from I9 or the virtual benchmark. Existing baseline functionality remains usable while these inputs are pending.

Measured on the current macOS arm64 toolchain: View120bytes, Budget96bytes, four-level Path36bytes and Element72bytes. Recursive plans and paths use bounded stack storage. See [size probe](artifacts/P14-final-local/sizes.txt).

Final live integration:198/198 Release and195/195 ASan/UBSan checks pass,69 standalone headers. Source manifests, logs and limitations are in [M5 progress](M5-integration.md#final-available-input-continuation-checkpoint).
