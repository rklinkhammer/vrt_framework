# P16 scene and scan helper implementation

Helpers and Runtime example wiring are implemented and independently verified locally. The [final integration report](P16-integration.md) records 229/229 Release and 225/225 ASan/UBSan checks, targeted TSan and the UDP-disabled combined gate. This report does not claim hardware qualification.

`VirtualRfScene` is a caller-owned serialized source. It maintains separate integer modular tone and LO phases. Every effective event first integrates the unproduced gap using the previous center, then changes the LO slope. Integer modulo arithmetic handles arbitrary uint64 ordinal gaps without iteration. The initial event requires ordinal zero and known matching fixed session SampleRate and RF center. Invalid or stale events latch a source fault. Out-of-band samples are zero, while both phases continue; the passband is [-Fs/2, Fs/2). A fresh scene is required for a fresh generator session. Bind its two static callback thunks to the core SourceProvider once that API is gated.

`SweepPolicy` yields one submission attempt at a time. Successful nonsimulated, nonpartial execution evidence and known matching RF readback may arrive in either order. Dwell begins at the later observation. Matching duplicate evidence during dwell does not restart it. The binding must filter by the current transaction identity before calling these methods. Timeout or failed submission stops the sweep without automatic cancellation. Finite sweep counts and explicit continuous repetition are supported. The internal policy permits zero dwell for deterministic composition; the public CLI requires strictly positive dwell and timeout.

`examples/frequency_scan/options.hpp` parses integer options without allocation: --start-hz, --stop-hz, --step-hz, --sample-rate-hz, --dwell-ms, --timeout-ms, --sweeps, --continuous and --help. Defaults are 100–100.2 MHz in 25 kHz steps, 100 ksample/s, 100 ms dwell, 1000 ms timeout, one sweep. Explicit --sweeps conflicts with --continuous in either order. Unknown flags, malformed integers, zero durations/counts and overflow are rejected. The scene tone remains the fixed default 100.05 MHz in this CLI.

The tuning predicate establishes backend-effective tuning and RF state evidence. It does not establish reception of actual IQ packets: transport or subsequent source faults may interrupt Data. A source-hook failure gates Data while preserving the truthful already-committed RF outcome.

## Developer evidence

Direct clang C++23 builds with -Wall -Wextra -Werror -fno-exceptions -fno-rtti pass. Both scene_scan and scene_scan_options also pass AddressSanitizer/UndefinedBehaviorSanitizer. The scene test checks the literal .55-cycle retune oracle, both Nyquist edges, out-of-band phase continuity, huge ordinal arithmetic, fault latching, no ordinary C++ allocation, evidence ordering, timeout, duplicate dwell stability, finite repeated sweeps and continuous progression. CLI tests cover defaults, integers/overflow, missing/unknown options, zero durations/counts and conflicting finite/continuous flags in both orders. This is targeted developer evidence, not a universal allocation or numerical qualification claim.

Root owns test registration: p16_scene_scan and p16_scene_scan_options. No shared production or CMake files were edited by this helper task.

## Earlier helper-only manifest (superseded by final candidate below)

- `include/vita/profiles/iq/frequency_scan.hpp`: `e85dd2ac0389be0c4d0b7a10cbc1c14a4ec04f67cb71f6293b6b68bb46c81ef6`
- `examples/frequency_scan/options.hpp`: `156fc4a464987f7f0d9c686bd020817a6d4dd552bfbe0b8404acf18e345463c7`
- `tests/unit/P16/scene_scan.cpp`: `8b51ba91331ddff3da2d6743f0fb6a8f55c27508cd3c2aec11447ecfb50439b4`
- `tests/unit/P16/scene_scan_options.cpp`: `706fa1654ef738a00a6e3b2b32ea500036d67a9fdd21c1586f5e5493aa4467db`

## Pure endpoint parsing addendum

`endpoint_options.hpp` adds role-specific loopback-only defaults and checked three-lane base ports without Runtime or adapter calls. The helper rejects invalid roles, nondecimal/overflowing ports, overlapping ranges, repeated base options, unknown flags and missing arguments. It delegates frozen sweep flags to the existing parser; it does not configure a remote sample rate. The bounded forwarding scratch holds32 argument tokens. No external address or production identity is inferred. `scene_scan_endpoint.cpp` passes direct strict C++23 and ASan/UBSan. Root registration requested as `p16_scene_scan_endpoint`.

- `examples/frequency_scan/endpoint_options.hpp`: `00a25ac3a98c5473f0ce6fb9dd620da2a13ee3753eeda74f94b6692c9d1f1a81`
- `tests/unit/P16/scene_scan_endpoint.cpp`: `f37eb65c4cc504b90a2766b99596c2280ac4814f2f6678d5386e98dc791389a6`


## Runtime example candidate

The gated public core now backs `vita_frequency_scan_combined` and, with POSIX UDP enabled, `vita_frequency_scan_controller`/`vita_frequency_scan_controllee`. The latter Controller uses only `add_remote_controller`; its backend write count is zero and it neither constructs a scene nor injects PPS. The Controllee explicitly configures `DeviceBackendBinding` through the documented `VirtualTuner` wrapper rather than relying on an invisible default backend. The wrapper delegates existing VirtualBackend validation/begin/simulation/disarm/quiescence and completes one pending modeled operation per serialized progress hook; there is one framework-owned transaction engine.

`ScanApplication` first makes a named SampleRate/RF query, checks the expected fixed rate using typed values, then holds one tuning handle at a time. Bounded observers record distinct phases. Outside callbacks the application combines successful nonsimulated execution and matching known RF state, logs public handle identity, requested/applied value, local confirmation latency, dwell and point/sweep progress. Retained observations are scoped by the active handle. It never constructs packets, Acks or buffer-return loops. Real-clock owner progress continues during idle waits. Source roles inject periodic simulated PPS; deterministic combined mode advances100us per loop. These are teaching clocks, not calibration evidence.

Ctrl-C and duration completion request explicit best-effort typed RF cancellation only for an outstanding tune, observe for at most250ms with a200ms cancellation deadline, then request graceful shutdown and continue progress up to2.1s. Ordinary command timeout does not send cancellation. Unknown source status, admission failure and substantive transport errors are reported without asserting no effect. No automatic device recovery is implemented by these executable flows.

Developer Release CTest evidence: `p16_example_combined`, `p16_example_wallclock`, `p16_example_udp`, `p16_example_timeout` all pass. The actual two-process driver requires nine confirmed client points, zero client backend writes, nine server writes and clean shutdown. A separate silent-peer test proves idle monotonic timeout and checks its bound cancellation UDP socket received no datagram. The driver uses actual OS-assigned availability checks for six loopback ports and bounded child cleanup; there remains a documented close-before-child-bind race that yields an explicit startup failure if another process wins a port.

Additional developer runs: deterministic1ms operator stop observed successful pending cancellation and zero backend writes; continuous wall-clock2150ms run completed21tunes with840known IQ packets, zero receiver drops and clean shutdown. A typical separate-process nine-point run received12known IQ packets and4receiver drops due to startup/network/metadata timing; the tool reports these counters and does not claim losslessness. Strict standalone combined compilation with -Wall -Wextra -Werror also passes.

The actual printed startup charges are12,505,328bytes combined,12,258,456bytes UDP Controllee and12,254,688bytes UDP Controller on this build. Runtime template is1stream/16transactions/shared4096retention entries/8MiB bytes. VirtualRfScene64B and SweepPolicy112B are caller stack objects (the inactive optional's stack storage is still reserved); runtime ledger charges the explicit backend owner separately. The process main stack, libc/stdio/application storage are not claimed included in this Runtime ledger, nor is this a whole-process64MiB proof or a new throughput qualification. Existing application/source no-hot-allocation gates remain separate; this logging demonstration is not claimed allocation-free end to end.

Exact build/run/CTest commands and limitations are in `examples/frequency_scan/README.md`. Example target registration resides solely in its owned subdirectory CMake.


## Approved recovery scene epoch

The additive `VirtualRfScene::create_for_recovery(SceneConfig)` factory permits the first known nonsimulated recovery initial event to establish an arbitrary absolute ordinal with zero new-session phase. Default `create()` still requires ordinal zero. Later retunes never reset phase; unknown/mismatched/simulated input faults the replacement and cannot rearm it. The application explicitly replaces its stable-address scene inside a successful reinitialization callback. `PORTING.md` supplies the exact virtual-model callback example and warns against reentrant set_source or treating SDK acceptance as physical proof. Scene sizeof remains64bytes (the added bool occupies existing padding); scan policy remains112bytes. The targeted helper developer test passes strict C++23 with ASan/UBSan, including nonzero origin, pre-initial production rejection, ordinary phase continuity, simulation/mismatched rate rejection and default-origin preservation. Independent end-to-end Runtime recovery evidence is verifier-owned.

Shutdown-lifetime review correction: all borrowed callback contexts are declared before Runtime; the clean path explicitly resets Runtime while they are alive. Failed/unproven shutdown exits the isolated virtual process without owner unwinding after diagnostics, rather than implying the grace deadline permits reclaim. PORTING documents this limited software fallback and the separate real-device quarantine obligation. The final candidate consumes the core owner's typed validation_outcome_known/validation_accepted fields and stops on explicit negative AckV without raw protocol inspection. A negative partial validation outcome is not represented as proof that no subset effects occurred.


## Final example candidate freeze

Final producer Release checks pass5/5: scene helper plus four example CTests. Startup charges after the AckV addition remain12,505,328bytes combined,12,258,456bytes UDP Controllee and12,254,688bytes UDP Controller. Final captured runs are in `artifacts/P16-examples/{combined,udp,timeout}.log`: combined9confirmed/9writes/10knownIQ/0drops; real UDP Controller9confirmed/0writes/14knownIQ/3drops and Controllee9writes; silent Controller0confirmed/0writes/clean localshutdown with explicit deadline failure. These packet counts describe those actual runs, not a stability/loss qualification. Core and application sources are now frozen for independent final review; future changes require a concrete failing check. No additional CMake registration beyond the four example tests was added during final cleanup.

Final owned source manifest:

- `include/vita/profiles/iq/frequency_scan.hpp`: `e85dd2ac0389be0c4d0b7a10cbc1c14a4ec04f67cb71f6293b6b68bb46c81ef6`
- `examples/frequency_scan/CMakeLists.txt`: `ff577530fd9824d0399628f393df8d65edc519c2d24e179eb4b3420f198e1b58`
- `examples/frequency_scan/PORTING.md`: `8e6445ae66269e1c3bbe3be3c2824a248c6be98559a8033ddf187091679bc319`
- `examples/frequency_scan/README.md`: `985a771421626b61aeabd8a548cf18a364bb8917c1cf0b01bba903a1588dbe39`
- `examples/frequency_scan/application.hpp`: `b7bfa9117b12f30500b441d6417e62a119981ba8a3dd74711906a2415e12712d`
- `examples/frequency_scan/endpoint_options.hpp`: `00a25ac3a98c5473f0ce6fb9dd620da2a13ee3753eeda74f94b6692c9d1f1a81`
- `examples/frequency_scan/main.cpp`: `144f55a154b3d032290f9e7dba37e6b7184c80a3caffa7644974d8d4f7b88b57`
- `examples/frequency_scan/options.hpp`: `156fc4a464987f7f0d9c686bd020817a6d4dd552bfbe0b8404acf18e345463c7`
- `examples/frequency_scan/run_pair.py`: `9d0706785f0ace8102ff8aac4ea6c2a0b9019290f8980a2475b2d5d6311ae658`
- `examples/frequency_scan/virtual_tuner.hpp`: `5fc26107943bc148160967f361918e04734faa4b89269d1afc8287e11da7ebab`
- `tests/unit/P16/scene_scan.cpp`: `8b51ba91331ddff3da2d6743f0fb6a8f55c27508cd3c2aec11447ecfb50439b4`
- `tests/unit/P16/scene_scan_endpoint.cpp`: `f37eb65c4cc504b90a2766b99596c2280ac4814f2f6678d5386e98dc791389a6`
- `tests/unit/P16/scene_scan_options.cpp`: `706fa1654ef738a00a6e3b2b32ea500036d67a9fdd21c1586f5e5493aa4467db`


## Test-only port lock correction

The parallel Release gate exposed a shared localhost lane-search collision between developer and independent process tests. Both developer UDP tests now use CTest `RESOURCE_LOCK p16_process_ports`, matching the verifier's test lock. Production behavior and assertions are unchanged. The preceding candidate manifest remains historical; its machine copy is preserved as `artifacts/P16-examples/manifest-before-port-lock.sha256`. The current machine manifest is refreshed; the coordinator owns the final rerun.

- `examples/frequency_scan/CMakeLists.txt`: `07ede835098378b0b2f503e0f8d8680a976518a6b3afc2b7df8218cbdc8e6a87`


Final documentation closure: README and this status paragraph were updated after the successful source-frozen gates. The producer manifests above preserve their verified candidate snapshots; this documentation-only closure changes no implementation, assertions or test registration.
