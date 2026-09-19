# P14 CIF3 temporal and environmental fields — implementer report

Candidate: 19 named CIF3 fields, Current-only. Independent verification and aggregate integration remain coordinator gates. Authority: supplied ANSI/VITA-49.2-2017 (R2024), printed pp.188–200 (§9.7), pp.209–211 (§9.9), plus the accepted raw-code policy in `P14-decisions.md` and the approved `P14-temporal-contract-readiness.md`.

## Implemented surface

| CIF3 bits | Fields | Native representation / wire extent |
|---|---|---|
|31|TimestampDetails|`TimestampDetailsValue{flags,epoch}`, two words|
|30|TimestampSkew|signed `Femtoseconds`, two words|
|27–20|RiseTime, FallTime, OffsetTime, PulseWidth, Period, Duration, Dwell, Jitter|signed `Femtoseconds`, two words|
|17,16|Age, ShelfLife|arena-owned `StateDurationValue{fractional,seconds,tsi,tsf}`, one to three words|
|7,6|AirTemperature, SeaGroundTemperature|`CelsiusQ6`, low signed16 / upper16 reserved|
|5|Humidity|`HumidityCode{raw}`, unsigned16; exact ratio is raw × 100 / 65535 percent|
|4|BarometricPressure|`BarometricPressureCode{raw17}`, unsigned17; no engineering conversion|
|3|SeaSwellState|`SeaSwellValue{sea,swell,user}`, codes0–9 and six user bits|
|2,1|TroposphericState, NetworkId|generic unsigned16 / unsigned32|

The pressure representation deliberately supplies no Pascal conversion under the accepted conservative policy. Unknown class-specific TroposphericState/NetworkId assignments remain raw identifiers. Duration/skew values have no implicit clock qualification or GPS assumption. Nonnegative interval semantics apply to rise/fall/width/period/duration/dwell/jitter; OffsetTime and TimestampSkew retain signed values. Structurally valid out-of-range values remain readable and fail typed materialization; reserved wire bits fail checked decoding.

## Explicit temporal binding

`TimestampFormatBinding{tsi,tsf,bound}` occupies existing `LayoutContext` padding. `builder.bind_timestamp_format(tsi,tsf)` validates and commits transactionally; it never reinterprets an existing tagged duration. A tagged native duration can be stored before binding, but measurement and encoding return `unsupported_capability` until a format is explicitly bound. Selected ordinary Age/ShelfLife with both codes zero has no supported representation. Selector and diagnostic bodies remain zero-value and one-word bodies independent of this binding.

The common traversal policy selects body kind before native/wire extent resolution. Age/ShelfLife use exactly `(TSI != 0 ? 1 : 0) + (TSF != 0 ? 2 : 0)` words and charge the same number of work units on both paths. Envelope codes must match the snapshot binding before output writes. Shape signatures include the actual codes, so equal-sized sample-count and picosecond layouts cannot reuse indexes accidentally. No conversion of fractional codes, absent components, or leap-second overflow is performed. Native absent components must be zero to prevent silently discarding supplied values.

`FieldView::duration()` independently validates its binding and exact byte extent, including forged public views. `materialize_into` copies a tagged duration into the destination native arena; the caller explicitly binds the destination before measurement/encoding. It does not infer or mutate destination envelope configuration. Five-argument FieldView initialization remains supported through a compatibility constructor; default views initialize safely.

## TimestampDetails validation boundary

`make_timestamp_details(flags,epoch)` and `validate_timestamp_details_intrinsic` check reserved bits and the LSH/LSP relation. The structural decoder retains raw epoch and offset words; in particular E=0 does not zero the undefined offset, and TSE=0 does not zero the undefined epoch. There is no universal fractional-below-one-second rule.

`validate_timestamp_details(value, TimestampDetailsScope)` returns `complete`, `incomplete`, or `not_applicable`, or rejects a proven invalid combination. The scope explicitly supplies observed TSI and TSF masks, completeness, documented user-bit assignments, and whether a user-defined source is documented. Fractional-only timestamps remain applicable. UTC/GPS epoch table checks use TSI observations; partial scope cannot claim complete validity. Source codes6/7 remain user-defined. Stream-global equality when G=1, truth of declared source, and actual clock discipline require the stream/deployment layer and are not claimed by this stateless helper.

## Memory and validation

Measured compile assertions preserve `SemanticValue=16`, `LayoutContext=48`, `FieldView=32`, scalar `PacketSnapshot=5312` bytes. Existing variant indices0–23 are unchanged; four compact alternatives are appended. `StateDurationValue` uses16 bytes in the explicitly sized native arena; no operational allocation or runtime StateSnapshot change is introduced. The existing default16-field snapshot bound is unchanged; this batch does not claim one default snapshot holds all19 simultaneously.

Developer `p14_cif3` passed direct Clang C++23 and AddressSanitizer/UndefinedBehaviorSanitizer builds. It exercises all16 prologue code pairs, native ownership/binding, exact extents, full-width fractional retention, mismatch/short-output rollback, every truncation, forged views, environmental semantic bounds, literal pressure bytes/reserved rejection, raw-negative versus typed interval behavior, scoped epoch validation, timestamp-free selectors/diagnostics, and equal-size distinct-format signature rejection. Prior CIF1-fixed and CIF2-UUID developer regressions also passed directly. Coordinator and independent verifier own the full gates; no conformance or deployment qualification is inferred.

## Frozen candidate SHA-256

```text
05f51c5d92d81654007f39310864e582b28a1e188dddb60241169560b0cc77d6  include/vita/fields/cif3.hpp
8a582b2f71445768dd00fd40cd284a221278b53839a98b31dd1bad1b2c5c6add  include/vita/fields/types.hpp
661f46422d2961cb60a6cd0e64e0cb4d435b5682c2cff5822b1a1bad8e21f948  include/vita/fields/packet.hpp
d8a16742c9272fc33495e78f1d50f1a77d3976d36c655f18176b1129ee795bed  include/vita/codec/layout.hpp
9228ce59278a4600315f5eddb04cafc53edafef377da9986c28518d468e2c3e6  include/vita/codec/scalar.hpp
ccde3c7b8dc309466412d042862ab82dc740e85c168595a11a40516ff55621fe  include/vita/codec/structured.hpp
39d6356def653cd5605028e5379e86b7c99870d8d4e3f14c28879099f9994a1e  include/vita/codec/packet.hpp
b36c04a7972e5e581cba174f19477bf58e62ea4a4c0c76bdf1601796bfe7e6d7  tests/unit/P14/cif3.cpp
4a0b6dbae025bcbe4efa22715286d2499cc37b145acc31449197da3b0addeda7  tests/unit/P14/CMakeLists.txt
```
