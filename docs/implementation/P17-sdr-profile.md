# SDR radio profile

`profiles::iq::Profile::sdr_radio` is an explicit opt-in profile. Existing
`generator_v1` and `frequency_tunable` wire identities and command policies remain
separate. The [Release-baseline audit](sdr-migration-audit.md) identifies the
original gaps. The user approved its D-GX-ACK migration contract: AckV means
admitted/scheduled, AckX means terminal execution, and post-execution state uses
ReqX+ReqS. This document describes library support; SDR's executable/Soapy/mTLS
adapter migration and deployment acceptance are separate work.

## Wire contract

Data type 1 uses SID 1–4, present Class ID bytes `00 FF FF FF 00 00 00 00`, UTC
integer seconds and picosecond fractional time, signed big-endian IQ16, I then Q.
Context and Command omit Class ID. The unknown OUI is the user's approved
replacement for the invalid interpretation of eight zero bytes as unspecified OUI.
Context carries exactly BW/RF/Gain/Fs and is 52 bytes. Format and validity metadata
are inferred locally only when the SDR receiver profile is explicitly selected.
Data has a required trailer: enable bits 23/22 and SSI bits 11/10, SINGLE=0, FIRST=1,
MIDDLE=2, FINAL=3. Family counters are independent and wrap modulo 16.

`StreamConfig::maximum_samples_per_packet` selects 1–1024 pairs and `burst_pairs`
selects 1–262144 (default 262144, at most 1 MiB valid payload). A final packet contains
only remaining pairs. Packetization preserves the cumulative sample ordinal and
rational picosecond timeline across bursts. The source adapter owns physical
sample acquisition/phase and receives that ordinal; it must account for skipped
samples. Context precedes Data and is sent at each burst boundary. A new start
begins a new epoch. The existing default source is a test waveform, not a Soapy
adapter; SDR must supply its own SourceProvider for the same device it controls.

The IP MTU must accommodate the explicitly requested full packet; the profile
rejects a small MTU instead of silently reducing 1024 pairs. Minimum IP MTU is 4156
for IPv4 or 4176 for IPv6 at 1024 pairs. Provide payload blocks of 4096 bytes and
receive blocks of at least 4128 bytes. TLS control limits are configured separately.

## Public API and device ownership

Include `<vita/runtime/public/runtime.hpp>`. Configure `RuntimeConfig` with a UTC
clock binding and a declared runtime OUI (`profiles::iq::sdr_unknown_oui` is
available); the profile supplies its exact per-family Class ID policy.
Deterministic tests can inject UTC PPS/time. Add a `StreamConfig`
with SID/controller/controllee and authenticated peer identities, profile,
`SampleFormat::iq16`, `trailer=true`, packet/burst sizes and a sufficient MTU.
Use `add_remote_controller(RemoteTargetConfig)` for a controller-only endpoint;
this does not require a local radio backend. Supply a real transport factory to
communicate with a remote process; an isolated loopback has no remote peer.

Controller operations:

```cpp
vita::SdrRadioSettings settings;
settings.center_frequency = *vita::Hertz::from_integer(100'000'000);
settings.sample_rate = *vita::Hertz::from_integer(1'000'000);
settings.bandwidth = *vita::Hertz::from_integer(800'000);
settings.gain = vita::GainStages{10 * 128, 0};
auto configured = controller.configure(settings); // one correlated operation
// Progress the runtime; verify terminal execution and requested applied state.
auto admitted_start = controller.start({utc_seconds, picoseconds});
auto status = controller.status();                // all five current fields
// query(QuerySelection{...}) requests selected fields only.
auto limits = controller.query_capabilities();    // four global min/max ranges
auto stopped = controller.stop();                 // also disarms pending start
```

Use `wait(handle, budget, WaitEvidence::validation/execution/state)` or `observe`
for evidence, `state(handle)` for selected current values, and
`capabilities(handle)` for separately typed ranges. A handle does not prove that a
device action occurred. `CommandOptions` controls requested V/X/S and diagnostics;
EXECUTE+ReqS without ReqX is unsupported. X-only requests suppress S. AckV carries
admission/scheduled time; AckX carries device effective time; AckS carries current
observation time. Rejected operations with no effective time carry diagnostic
observation time and do not assert successful execution. Queries remain valid before configuration and while stopped,
armed or streaming. Cancel a scheduled start with `cancel(handle,
QuerySelection{QueryField::streaming})`; immediate stop also disarms it.

Initial settings are unconfirmed until a successful full configuration. Start is
a separate discrete-I/O command, CIF0=2/CIF1=64/value 3; stop is value 2. Start
requires 20 ms–10s lead with timestamp mode 1; stop uses immediate mode 0. A second
start or configuration while armed/running is rejected. Local `Controllee::start`
cannot bypass the SDR wire configuration/start lifecycle.

A physical `DeviceBackendBinding` must provide `Backend::commit` for atomic
configuration as well as the ordinary lifecycle backend callbacks. `commit` is a
bounded, nonblocking, synchronous callback on the serialized runtime domain,
receiving the completely validated ExecutionPlan and effective boundary. Return
actual effective time with `time_known=true` for SDR. Keep cross-setting
validation in `validate_plan` or the profile constraint hook; per-field validation
receives the prior snapshot. The complete plan is revalidated at dispatch. Success
means all four adjusted values took effect exactly; `failed` means no effects;
`unknown_effect` faults the stream and marks all four fields unknown. It must not
implement four potentially failing hardware setters and call that atomic.
A driver requiring blocking I/O must stage/own that work outside this callback and
supply a device-specific atomic commit guarantee. No physical Soapy driver is
certified by the virtual-backend tests. All settings and constraints are validated
before mutation. Raw partial CAM cannot enable partial configuration. One state
revision and one coherent Context publication record represent the whole batch.

Soapy CS16 adapters use `SampleWriteWindow::write_iq16(index, i, q)`,
preserving acquired integer codes exactly without host-side VITA sample encoding. The
callback must fill the requested bounded extent; partial device reads need a
bounded staging buffer in the adapter. Do not use an independent SDR VITA
encoder. SDR owns device lifetime, phase, passband/gain behavior and clipping.

## Scheduled sample epoch and host activation

For `sdr_radio`, a successful scheduled start establishes sample ordinal zero
at the requested UTC timestamp, even when device activation is late within the
configured tolerance. AckV retains admission/scheduling semantics. The backend
must return its actual activation time in `FieldOutcome::actual_time`; terminal
AckX retains that time, and AckS retains the current observation time. No backend
or host adapter should substitute the scheduled timestamp for actual execution.

`EffectiveEvent::sample_epoch` carries the requested timestamp for a SDR start.
`EffectiveEvent::actual_time` and `outcome.actual_time` remain the actual device
effect time, including in `SourceProvider::effective`. A source adapter can use
`sample_epoch` to establish its simulated signal epoch and use
`SampleWriteWindow::first_ordinal()` for continuous phase. The library owns all
wire timestamp encoding. `EffectiveEvent::context_time()` selects this sample
association time when present, otherwise the ordinary actual effect time.
Revision ordering and Context publication use that association time. The start
Context therefore precedes Data at the scheduled epoch; subsequent burst Context
and sample timestamps follow cumulative rational sample time, including rates
that do not divide one trillion picoseconds evenly.

Monotonic host pacing starts when activation completes. Accepted activation
jitter does not create an initial catch-up burst or skip the first samples.
Later missed host deadlines retain the existing bounded skip behavior and source
ordinal accounting. Context refreshes follow the sample timeline rather than
advancing metadata past delayed simulated samples. No new clock or transport
binding is required. Existing non-SDR profiles retain their timing policy.

Cancellation before execution creates no sample epoch. Stop followed by a new
successful start resets the sample ordinal and selects the new requested epoch;
idempotent retries neither reactivate the device nor reset the timeline. Dispatch
outside tolerance is rejected without activation. If a backend reports that an
activation physically occurred outside tolerance, AckX retains its actual time
with timing diagnostics; the stream faults and emits no samples for that start.
The host must handle that physical outcome through the existing fault/stop policy.

## Capabilities and diagnostics

`StreamConfig::sdr_capabilities` is copied at endpoint construction and supplies
admission/reporting bounds. Defaults are RF 1 MHz–6GHz, integral Fs 1000–2000000Hz,
BW 1–2000000Hz, gain −60..60dB with stage 2 zero. BW must also be positive and <=Fs.
Frequency and BW accept their Q20 representation; gain uses Q7. Invalid native
value types, range, unsupported precision, not-executed and timing errors are
field-specific. Driver-adjusted values are rejected for this exact profile.

VITA 49.2 Section 9 (printed p124) explicitly permits hardware-supported sample-rate
limits; Section 9.12 defines Maximum/Minimum. Capability queries are NO_ACTION with
CIF0 bit 7 and CIF7 `0x0c000000`; AckS returns Maximum then Minimum for every selected
field, Current absent. This is distinct from current settings and statistics.
Unknown supported-registry selectors receive unsupported diagnostics when requested;
structurally unknown/malformed packets are rejected. No zero range is fabricated.

`SupportedValues` expresses a local step anchored at an origin and/or at most 16
choices, in Q20 Hz or Q7 dB units. Admission enforces them; setup rejects choices outside the advertised range or
endpoints incompatible with the declared domain. These are not CIF7
Precision, and arbitrary step/choice tables have no invented wire selector.
`bandwidth_supported(context, BW, Fs)` adds a bounded noexcept device constraint;
its owner must outlive the runtime and the callback must be pure/nonblocking.
Scalar global limits do not promise that every BW/Fs pair is supported. Queries
never invoke this constraint callback or a device mutation callback. Treat capability
configuration as immutable during endpoint lifetime.

## TCP and resources

`<vita/runtime/transport/stream_framer.hpp>` provides `StreamFramer<Capacity>` and
`StreamIngress<Capacity, Routes>`. Storage is fixed at construction; a configurable
maximum is checked immediately after the four-byte header, before pool allocation.
Packet Size is the only frame length; there is no prefix. Feed arbitrary chunks,
including coalesced packets. StreamIngress performs full packet decoding with route
correlation, copies into the supplied bounded receive pool, and dispatches through
the existing HostBindings route registry. No borrowed input survives feed.

A successful `feed` consumes all input and reports dispatched packets. Malformed
framing, invalid packets or a sink rejection close the framer; the host must close
that connection and discard its unconsumed suffix. This is fail-closed admission,
not a retryable partial-consumption API. `disconnect` closes and reports truncation
if a frame is partial; `reconnect` clears all buffered bytes; `shutdown` rejects
further input. `stalled()` exposes partial-frame state. Host-owned monotonic deadlines
must shut down stalled peers; TLS authentication, certificate identity, handshake,
idle/output deadlines, socket reads/writes and backpressure remain host-owned.

A host factory receives `HostBindings` and constructs StreamIngress with its
route registry and receive pools. Feed decrypted TLS reads directly into it. For
outbound `TxSubmission` objects, retain storage while completing partial TLS writes,
and issue exactly one deferred terminal completion. Disconnect must release or
quarantine outstanding ownership using the existing transport contract. Keep the
runtime alive across authenticated reconnects so replay history and sample phase
survive; `detach` removes all borrowed HostBindings before runtime destruction.

SDR should instantiate a control ingress maximum 1024 bytes, input/output queues
2048/4096 bytes, and its 2 s deadlines. Include the framer and all adapter buffers in
`TransportFactory::required_bytes`. Transactions, replies, revisions, pools and
retention remain bounded and charged to the 64 MiB framework budget. The global
state has 8 slots, execution plans allow 4 settings and SDR status allows 5 query
selectors. Additional capacity is charged rather than hidden outside the ledger.
The runtime's template parameters bound retained records/bytes; choose a host
configuration consistent with SDR's 256 replay-entry requirement and number of
radio instances. Nonzero increasing message IDs are enforced for SDR; retained
identical retries replay old results, changed or evicted IDs cannot execute again.
Keep the authenticated peer/association identity stable across TCP reconnects.
Creating a new runtime is a new radio lifetime, not a reconnect operation.

## CMake consumption

```cmake
add_subdirectory(path/to/pinned/vrt_framework)
target_link_libraries(sdr_radio PRIVATE vita::core)
# Optional supplied UDP adapter:
# set(VITA_BUILD_POSIX_UDP ON CACHE BOOL "" FORCE) before add_subdirectory
# target_link_libraries(sdr_radio PRIVATE vita::posix_udp)
```

Use C++23 with the framework's no-exception/no-RTTI target requirements. The SDR
host must replace its generated packet classes/protocol engine with these APIs,
update dependency locks/licenses/SBOM, and supply its Soapy and secured TCP/UDP
bindings. The accompanying SDR design changes select that migration; they do
not claim its current executable has already been converted.

## Requirement-to-evidence matrix

| Requirement | Library evidence | Host migration / qualification |
|---|---|---|
| Atomic four-setting operation, no partial effects | `p17_atomic`, `p17_sdr_runtime`; Backend batch commit | Soapy device atomic commit adapter and hardware failure qualification |
| AckV admission, terminal AckX, X-only, X+S | `p17_sdr_runtime`, existing P06/P07/P16 transaction gates | Controller requests and independent cross-process checks |
| Capabilities distinct from current status | `p17_capabilities`, `p17_capability_duplicates`, `p17_sdr_runtime`, literal AckS in `p17_verify_wire` | Device-supported bounds and pure constraint binding |
| Full five-field current query | `p17_sdr_runtime` | Controller UI interpretation |
| Start/stop, timing, cancellation, armed rejection | `p17_sdr_runtime`; injected UTC | Host UTC-to-steady mapping and measured activation tolerance |
| Common scheduled epoch, independent actual AckX, Context association, rational bursts, replay/cancel/restart and deadline failure | `p17_start_epoch`: four radio identities at 0/0.1/0.5/5 ms delay; unchanged SDR reproducer in both modes | Soapy/mTLS application migration and independent acceptance pending |
| Exact 52-byte Context, unknown OUI, absent class rules | `p17_verify_wire` literal comparison, profile-aware receiver | Update independent SDR fixtures |
| Packets 1/2/1023/1024; short/full/max bursts, continuous rational time | `p17_verify_wire`, `p17_bursts` at 1000003 Hz for two consecutive bursts | Same Soapy source acquisition, phase/skip policy |
| SSI 0/1/2/3 and independent family counters | `p17_sdr_profile`, `p17_verify_wire` | End-to-end packet capture |
| Remote controller configuration | `p17_remote`, existing transport-boundary tests | Real authenticated control TCP and UDP routes |
| Fragments/coalescing/malformed/disconnect/reconnect/stall/shutdown | `p17_stream_framer` | Host deadlines and mTLS process integration |
| Replay/correlation/cancellation and bounded storage | Existing P07/P09/P16 gates; profile high-water check | Preserve authenticated peer identity; choose replay capacity |
| Build/platform/sanitizer gates | Verification record below | SDR portable/Soapy/mTLS and privileged OVS gates remain separate |

## Verification

The table and artifacts below record the original `60a290c9` profile qualification.
The scheduled-epoch correction has a separate verification record below.

Run on macOS arm64:

```sh
for preset in udp-dev udp-release udp-asan-ubsan udp-tsan; do
  cmake --preset "$preset" && cmake --build --preset "$preset" -j4 &&
    ctest --preset "$preset" --output-on-failure || exit 1
done
```

On Linux arm64 with GCC15/CMake/Ninja, use a separate build directory:

```sh
cmake -S . -B build/linux-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DVITA_BUILD_POSIX_UDP=ON
cmake --build build/linux-debug -j4
ctest --test-dir build/linux-debug --output-on-failure
```

Repeat with Release in a separate build directory. The Linux gates used an
unprivileged aarch64 Docker compiler container, GCC 15.2.0 and libstdc++;
macOS used Apple clang 21.0.0 (clang-2100.3.34.2) and libc++.
The exact local Linux image ID was
`sha256:e590ad0533b735bf7bd8555830c0416f4451e4bf5c74244df08ef7bc89eef5d6`.
For each Linux build directory, the final gate command was:

```sh
docker run --rm --entrypoint sh \
  -v "$PWD:/src:ro" -v /tmp/vrt-sdr-linux:/evidence \
  sdr-linux-verifier:local -c \
  'cmake --build /evidence/debug -j4 && ctest --test-dir /evidence/debug --output-on-failure'
```

Use `/evidence/release` for Release. The image is a local compiler environment,
not a published project dependency; the native CMake commands above reproduce the
configuration on an equivalent Linux arm64 toolchain.

| Platform | Gate | Passed | Failed |
|---|---|---:|---:|
| macOS arm64 | Debug | 235 | 0 |
| macOS arm64 | Release | 239 | 0 |
| macOS arm64 | ASan/UBSan | 235 | 0 |
| macOS arm64 | TSan | 235 | 0 |
| Linux arm64 | Debug | 235 | 0 |
| Linux arm64 | Release | 235 | 0 |

[results.json](artifacts/P17/results.json) records toolchains, counts and full build/
test log filenames. The [source manifest](artifacts/P17/source.json) hashes all 412
implementation/build/test inputs; those inputs were unchanged across these gates.
Linux sanitizer gates were not run; the sanitizer gates above ran on macOS arm64.

An earlier TSan run exposed a wall-clock dependency in the P13 receiver capacity
test: queued packets could expire during the capacity-fill loop. The test now holds
the expiry clock during that loop and retains its explicit 10 ms expiry assertion.
The final full TSan gate passed. The earlier failure log is retained as
[tsan-before-clock-fix.log](artifacts/P17/tsan-before-clock-fix.log).

The 16-stream charged memory is 53,452,064 bytes with libc++ and 53,446,224 bytes
with libstdc++, within the existing 64 MiB budget. These gates qualify the library;
SDR executable, SoapySDR, mTLS and privileged OVS integration remain pending.

### Scheduled-epoch correction verification

The correction starts from `60a290c9b1da2396d3d704ebe52ea6cbcf2fa398`.
`p17_start_epoch` independently checks four radio identities with a common
scheduled epoch and 0/0.1/0.5/5 ms activation delays. It checks AckV/AckX/AckS
wire timestamps, the source effect callback's distinct epoch/actual times,
Context-before-Data and Context association, three 2050-pair bursts at 1000003 Hz,
IQ sample ordinals, duplicate and retained-replay starts, cancellation, stop/start,
bounded post-activation deadline skipping, late dispatch rejection and an
out-of-tolerance reported device effect. The latter retains its actual AckX time
and faults the stream without emitting samples.

Exact macOS arm64 gate commands (from the library root):

```sh
for preset in udp-dev udp-release udp-asan-ubsan udp-tsan; do
  cmake --preset "$preset" && cmake --build --preset "$preset" -j3 &&
    ctest --preset "$preset" --output-on-failure || exit 1
done
```

The final Debug build used `-j4`; all other final builds used `-j3`.
Linux arm64 used the same local compiler image identified above:

```sh
for mode in Debug Release; do
  directory=$(printf '%s' "$mode" | tr '[:upper:]' '[:lower:]')
  docker run --rm --entrypoint sh \
    -v "$PWD:/src:ro" -v /tmp/vrt-sdr-linux:/evidence \
    sdr-linux-verifier:local -c \
    "cmake -S /src -B /evidence/$directory -G Ninja -DCMAKE_BUILD_TYPE=$mode -DBUILD_TESTING=ON -DVITA_BUILD_POSIX_UDP=ON && cmake --build /evidence/$directory -j3" || exit 1
  docker run --rm --entrypoint sh \
    -v "$PWD:/src:ro" -v /tmp/vrt-sdr-linux:/evidence \
    sdr-linux-verifier:local -c \
    "ctest --test-dir /evidence/$directory --output-on-failure" || exit 1
done
```

The SDR reproducer is compiled unchanged from the library root:

```sh
c++ -std=c++23 -O0 -fno-rtti -I include \
  /Users/rklinkhammer/workspace/sdr-docker/tests/repro_vita_start_epoch.cpp \
  -o /tmp/start-epoch
/tmp/start-epoch --on-time
/tmp/start-epoch
```

Both modes exit 0. Their output is:

```text
scheduled=1000:50000000000 actual=1000:50000000000 first_sample=1000:50000000000
scheduled=1000:50000000000 actual=1000:50500000000 first_sample=1000:50000000000
```

The bounded optional epoch increases the measured 16-stream charge to 53,550,368
bytes with libc++ and 53,544,528 bytes with libstdc++. The fixed reservation remains
67,108,864 bytes (64 MiB); the budget oracle checks both the total charge and the
remaining plan reservation. No additional host resource or timing binding is
required. SDR should update its immutable library pin, keep returning honest
backend execution times, and use the scheduled epoch for its simulated source
initialization. Its executable/Soapy/mTLS migration and application acceptance
remain separate and pending; no SDR privileged gates were run here.

Final correction results (all build and test commands exited 0):

| Platform | Gate | Passed | Failed |
|---|---|---:|---:|
| macOS arm64 | Debug | 236 | 0 |
| macOS arm64 | Release | 240 | 0 |
| macOS arm64 | ASan/UBSan | 236 | 0 |
| macOS arm64 | TSan | 236 | 0 |
| Linux arm64 | Debug | 236 | 0 |
| Linux arm64 | Release | 236 | 0 |

The [correction results](artifacts/P17/start-epoch/results.json) identify full test
logs, compressed build logs, the unchanged SDR reproducer's hash and outputs,
and the [413-file source manifest](artifacts/P17/start-epoch/source.json). Source
hashes were verified unchanged across the final gates. Linux sanitizer gates were
not run; ASan/UBSan and TSan qualification above is on macOS arm64.
