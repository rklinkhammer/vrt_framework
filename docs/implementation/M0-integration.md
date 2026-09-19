# M0 integration gate

Date: 2026-09-18. Verdict: **PASS for local M0 baseline implementation**.

P00 build/harness, P01 semantic/layout contracts, and P02 baseline envelope/body/sample codecs have independent PASS reports. General M5 fields are not included in this gate. No independent-peer interoperability or Linux qualification is claimed.

Coordinator regression rebuilt the P00–P04 test executables and ran:

```sh
cmake --preset dev
cmake --build --preset dev --target p00_core_multitu p00_cxx23 p02_verify_wire p02_verify_packet p02_verify_samples p03_verify_pool p03_verify_envelope p03_verify_edges p04_verify_tickets p04_verify_admission_executor p04_verify_quiescence_budget p04_verify_coupled_admission p01_verify_core p01_verify_semantics p01_verify_allocations p00_verify_features p00_verify_multi_tu p02_codec p03_memory p04_runtime p01_semantics
ctest --preset dev -R '^p0[0-4]_|^architecture_fixtures$' --output-on-failure
```

Result: **24/24 passed**, including existing 190 specification checks, independent literal wire vectors, and ownership/completion foundations. P08 and P05 under development were excluded. Root CMake SHA-256: `1a088395deb9683f29ff74c4dda5e0513a1aab743ff9c4f40d355a39458385d1`. Source manifests and package sanitizer evidence are in [P00](P00-verification.md), [P01](P01-verification.md), [P02](P02-verification.md), [P03](P03-verification.md), and [P04](P04-verification.md). No commit was created.

Supplemental compile check: all 21 then-frozen public headers compile individually with `clang++ -std=c++23 -fno-exceptions -fno-rtti -Iinclude -x c++ -fsyntax-only -`. This verifies header self-containment on Apple Clang 21/libc++; it does not replace the configured Linux compiler matrix.

M1 still requires P05 end-to-end transport integration. M2 and M3 remain pending.

## Accepted-interpretation corrective revalidation

Later P06 oracle preparation found omissions in P02’s I4 and I11 handling. The codec now accepts the permitted ordinary-Control change indicator and returns opaque diagnostics with `requires_request_context` when correlation is unavailable. Independent literal regressions verify the new behavior, preserve rejection in cancellation/Ack forms, and confirm zero semantic callbacks for opaque diagnostic bodies. Eight affected P02/P05 targets were rebuilt and passed in both Debug and ASan/UBSan; see the corrective addendum and current hashes in [P02 verification](P02-verification.md). The affected local milestone scope is revalidated.

A further normative Ack timing correction permits legal Ack timing codes without an Ack timestamp when framing lacks the original request context; Control timestamp requirements and reserved-code rejection remain enforced. Current wire header SHA-256 is `cc2e6266f8802128e1ab4ead5e391ce22f5a342ca54edb63fed39b219253ba0a`. Eight affected independent codec/transport targets passed Debug and ASan/UBSan after this change. The current [P02 report](P02-verification.md) supersedes the earlier wire hash for this corrected scope.
