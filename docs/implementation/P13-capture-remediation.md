# P13 capture remediation

The former native peer reused `mid % 512` entries whenever it sent a new command. It counted every entry lacking all V/X/S phases as `pending_overwrite`, including commands already rejected at admission. It also discarded later Acks whose overwritten entries no longer matched. This mixed harmless retirement of a rejected attempt with real loss of live correlation.

The peer now keeps no pending-MID table. `bench/peer_capture.hpp` checks configured source address, truncation, ordinary Ack type/action/phase, OUI/Class, Controller/Controllee identities, configured SID range and monotonically issued MID range. Every valid observation is appended to the existing bounded raw-event ring, including late and duplicate phases. The immutable send log supplies the exact SID/MID mapping for the disk-backed analyzer. A mismatched in-range SID cannot be accepted into the analyzed result merely because its MID was issued. No speculative capacity increase or weakened Runtime retention is used. Ring overflow, write failure and unmatched or conflicting evidence remain visible and invalidating.

The summary identifies `peer_capture_policy=stateless_checked_raw_v1`; the compatibility `pending_overwrite` counter is zero. Source diagnostics now retain measurement-end and final SourceStatus, transition counts, first fault-like status time, and the last actual progress error code with a separate presence flag. Thus the final normal pause cannot hide an earlier clock/Context/temporal fault. These are observations of existing public APIs, not a new recovery policy. A short functional run measured `sizeof(Run)=10144` application bytes, reduced from the pending-table composition; its four sources remained running at measurement end and stopped after pause.

The analyzer preserves old schema1 captures. For legacy policy it reconstructs the exact512-entry peer state from raw event order and verifies the counter. Evictions with received-only traces are classified as pre-admission outcomes, with explicit rejecting AckV distinguished from missing response. An evicted command with validation/execution evidence is a lost live-correlation capability and invalidates peer evidence; unexplained eviction or counter mismatch is also invalid. All sent commands remain in the denominator. Identical captured Ack duplicates are counted and coalesced explicitly for unique-phase latency; conflicting phase flags or identities are invalid. Duplicate trace stages remain invalid instrumentation.

Historical captures were not rewritten. Reanalysis to separate `/tmp` outputs gives:

- Original normal1800s:181856 sends,3333 reconstructed evictions, all received-only;2786 explicit rejection AckV and547 without Ack. Capture integrity is valid, while rejected/unconfirmed controls, latency and Data loss fail measurement requirements.
- Original overload60s:7264 sends,591 reconstructed evictions. Sixteen commands completed after eviction (about1.064–1.189s later); these remain invalid live-correlation evidence. The other575 are received-only. The intact per-command trace still supports its separately reported framework intervals; it does not repair missing peer observations.

Developer checks: helper normal/ASan+UBSan/TSan pass, including MID1 after next-issuedMID10000, retained duplicate, and wrong source/class/SID/identity/cancellation/future-MID rejection. A0.1s functional runner test produced10 sends/50 stages, valid JSON/status diagnostics, no C/C++ operational allocations and valid analyzer output; it is not performance qualification. The independent analyzer's eight corruption cases and paired-percentile oracle still pass. The independent verifier separately checks helper lifetime and legacy classification. The coordinator owns the next isolated60s diagnostics and any later1800s decision.

## Candidate source manifest

- `bench/main.cpp`: `4f4bcf0d2606c0f97c2647841021aba25ed3682419979ab14c377122e491b79f`
- `bench/analyze.py`: `042b10a8191725e594c985ad7bcefd77e506c979f95f8868f0ad94574e46b541`
- `bench/peer_capture.hpp`: `9d2de2362a172b8d77085277c401a6b44669f0d8173bfa146ce06d4a58c481fd`
- `tests/unit/P13/peer_capture.cpp`: `e27288629c8e492da0e62bf67d9e72b6a712a50f94cae6cf42c5696f67ee8d74`
