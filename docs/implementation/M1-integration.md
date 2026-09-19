# M1 integration gate

Date: 2026-09-18. Verdict: **PASS for local deterministic M1 implementation**.

P00–P05 and the pulled-forward P08 foundations have independent PASS reports. P05 closes the actual encode → externally leased transport → checked decode → independently retained IQ → final reclamation path. Contiguous and segmented submissions deliver identical logical bytes. Loss remains distinct from local completion; rejected submissions preserve ownership and Packet Count; accepted failures consume the count; quarantined storage requires separate quiescence. Data, ordinary Control and cancellation use separate physical pools and reserved lanes.

Coordinator rebuilt the frozen targets and ran:

```sh
cmake --preset dev
cmake --build --preset dev --target p00_core_multitu p00_cxx23 p05_verify_chain p05_verify_routes p05_verify_faults p02_verify_wire p02_verify_packet p02_verify_samples p03_verify_pool p03_verify_envelope p03_verify_edges p04_verify_tickets p04_verify_admission_executor p04_verify_quiescence_budget p04_verify_coupled_admission p01_verify_core p01_verify_semantics p01_verify_allocations p08_verify_timeline p08_verify_clock p08_verify_scheduling p00_verify_features p00_verify_multi_tu p05_m1_chain p05_loopback p05_isolation p02_codec p03_memory p04_runtime p01_semantics p08_timing
ctest --preset dev -R '^p0[0-58]_|^architecture_fixtures$' -E prologue --output-on-failure
```

**34/34 passed.** The new prologue-only helper was excluded while awaiting its separate gate; P06 was not included. This final run checked the restored frozen wire header SHA-256 `b7e4020d643a2065ddfb784dfaf244e0998ca313dce1690ff4df89fdaa2223b3` before rebuilding. An earlier run during a subsequently reverted refactor is not used as the final milestone evidence.

The independent verifier separately rebuilt and passed all six P02/P05 tests in Debug and ASan/UBSan on that restored header. [P05 verification](P05-verification.md) records manifest `8074e6e84a0ee7661cae938c040f43090891aaf525bd2e8449481307bb3c21d4` and the corrected source-identification history. Relevant earlier foundations: [M0](M0-integration.md), [P03](P03-verification.md), [P04](P04-verification.md), [P08](P08-verification.md).

Test environment: Apple Clang 21/libc++, macOS arm64. No real UDP, independent peer, GPS, DMA hardware, sustained throughput or complete M5 field coverage is claimed. M2 and M3 remain pending.

## Accepted-interpretation corrective revalidation

Later P06 oracle preparation found omissions in P02’s I4 and I11 handling. The codec now accepts the permitted ordinary-Control change indicator and returns opaque diagnostics with `requires_request_context` when correlation is unavailable. Independent literal regressions verify the new behavior, preserve rejection in cancellation/Ack forms, and confirm zero semantic callbacks for opaque diagnostic bodies. Eight affected P02/P05 targets were rebuilt and passed in both Debug and ASan/UBSan; see the corrective addendum and current hashes in [P02 verification](P02-verification.md). The affected local milestone scope is revalidated.

A further normative Ack timing correction permits legal Ack timing codes without an Ack timestamp when framing lacks the original request context; Control timestamp requirements and reserved-code rejection remain enforced. Current wire header SHA-256 is `cc2e6266f8802128e1ab4ead5e391ce22f5a342ca54edb63fed39b219253ba0a`. Eight affected independent codec/transport targets passed Debug and ASan/UBSan after this change. The current [P02 report](P02-verification.md) supersedes the earlier wire hash for this corrected scope.
