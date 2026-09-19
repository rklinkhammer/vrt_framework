# Replacing the virtual tuning device

This is a porting guide for the frozen P16 contract, not a claim that an SDK or hardware adapter has been implemented or qualified. Keep the public typed Controller operations and Runtime-owned transaction processing. Replace the Controllee backend and, where needed, the sample provider and clock binding. The [P16 contract](../../docs/implementation/P16-contract.md) is authoritative for API details and its independently gated core implementation.

## Complete at a usable boundary

An SDK setter returning successfully may mean only that a request was queued or a register write completed. A real tuning backend must define when samples are usable at the requested center: for example, after a documented settling condition and an identifiable first usable sample. Successful completion reports the actual RF value and an honest effective boundary. Do not label SDK submission time as settled time or invent a sample ordinal from a guessed delay.

Validation must finish before effects. The tunable profile accepts exact integer-Hz centers from 1 MHz through 6 GHz and rejects fractional or out-of-range values without rounding. Sample rate is fixed per tunable session. A mixed SampleRate/RF write rejects as a whole before either effect. Hardware-specific restrictions belong in backend validation and should return truthful diagnostics.

Use the existing validation, begin, completion, disarm and quiescence seams. A backend binding supplies a setup-owned lifetime and actual storage charge; asynchronous result capabilities pin the required lifetime. The optional progress hook runs on the serialized owner. An external completion thread publishes only through its authorized completion capability; it must not mutate Runtime state or invoke application observers directly. Setup allocation is distinct from bounded runtime work; no heap fallback or unbounded SDK queue belongs in the hot path.

## Keep source metadata tied to actual samples

The effective-configuration source hook observes an already committed real effect. It is not another device setter. It advances the scene through the old configuration up to the effective ordinal and then changes future generation. Initial configuration supplies a known snapshot at ordinal zero. Dry runs, pending requests and Ack receipt do not trigger real source changes.

A hardware source must bind accepted sample intervals to the configuration actually in effect. Preserve the full Context-before-dependent-Data gate and immutable revision ownership. If tuning is known but its usable sample boundary cannot be established, hold Data rather than attach guessed metadata. If the source hook itself fails after a known backend effect, gate/fault Data and preserve the truthful RF execution outcome; a source error cannot turn a real tune into “not executed.” Recovery requires explicit known state and the existing fresh-association contract.

## Failure, cancellation and shutdown

Distinguish no effect, known partial effect and unknown effect. A timeout or lost reply is not evidence of no effect. Required state that becomes unknown stops Data until recovery establishes known state. Do not report simulated previews as hardware execution.

Cancellation is a separate transaction phase with its own immutable requested meaning under the existing retention policy. A successful disarm requires evidence that the selected pending operation can no longer execute. Missing SDK cancellation support cannot be replaced by a successful-looking result. An operation past its cutoff may be not cancelled; a partially completed request may yield different outcomes for its individual fields. Preserve the distinction between original execution AckX and cancellation AckX.

Graceful shutdown closes admission and continues protocol progress while local work drains. The monotonic grace deadline bounds waiting; expiration does not establish device or transport quiescence. Keep outstanding buffers, callback contexts and backend owners alive until actual proof permits their release. A late contradictory completion must not resurrect a cancelled effect or mutate a new association. A remote-only Controller can establish local transport shutdown, not remote physical device quiescence.

If an explicit reinitialization callback supplies recovery evidence, success must mean the documented known-state and physical-quiescence conditions really hold. “Reset command accepted” is insufficient. Preserve generation-scoped references and use fresh identities as required by recovery; do not recycle an old association simply because the application released its handle.

## Deployment inputs and qualification

Replace the simulated PPS pump with a qualified clock source when real timed operation is required. Describe clock epoch, quality, uncertainty, mapping changes and the relation to device samples. Keep local monotonic deadlines independent of protocol clock corrections. A source start that requires qualified timing must fail honestly when that evidence is unavailable.

Supply actual OUI/class agreements, peer identities, lane addresses, device/SDK behavior, settling evidence and shutdown guarantees for the deployment. The example's static UDP port checks do not authenticate a peer. Local loopback integration is useful software evidence but does not establish hardware timing, external interoperability or production network qualification. Those inputs remain part of the separate P15/M6 hardware work; this guide does not fabricate them.


## Concrete example seams

`virtual_tuner.hpp` provides `VirtualTuner::binding(const std::shared_ptr<VirtualTuner>&) -> DeviceBackendBinding`. Its `VirtualBackend<16>::binding()` supplies validation, begin, simulation, disarm and quiescence callbacks. `VirtualTuner::progress(void*, const runtime::transaction::OperationContext&) noexcept` completes at most one pending model operation per owner cycle. The binding stores the model pointer as callback context, the containing shared owner, and `sizeof(VirtualTuner)` as declared storage. Runtime accounts the owner once and pins it through completion lifetimes. Replace these callbacks with real SDK behavior; do not add a second transaction engine.

`StreamConfig::source` is constructed as `{&scene, VirtualRfScene::produce_callback, VirtualRfScene::effective_callback}`. The source and its state must outlive Runtime callbacks. `produce_callback(void*, SampleWriteWindow&) noexcept -> Result<void>` writes the supplied external window; `effective_callback(void*, const runtime::EffectiveEvent&) noexcept -> Result<void>` observes committed changes. `main.cpp` keeps both the scene and backend owner alive across graceful shutdown and destroys Runtime before its borrowed source context. The current executable does not attempt automatic recovery after a source or device fault; it reports and drains. A production recovery policy must explicitly coordinate scene/session state with the core's fresh-association lifecycle.

The Controller path uses `set_center_frequency(Hertz, CommandOptions)`, `query(QuerySelection, CommandOptions)`, `observe`, and typed `StateObservation::value<RFReferenceFrequency>()`/`value<SampleRate>()`. The remote executable calls `add_remote_controller(RemoteTargetConfig)` and installs no local source/device execution role. There are no application packet encoders, generated Acks, socket receive loops or manual buffer-return loops in the example.

### Explicit fresh phase epoch during recovery

`VirtualRfScene::create()` requires its first event at ordinal zero. Runtime recovery preserves the absolute sample timeline, so a replacement scene must instead use `VirtualRfScene::create_for_recovery(SceneConfig)`. Its first valid known, nonsimulated initial event supplies the actual ordinal and starts phase zero there. No caller guesses that ordinal. Later ordinary retunes preserve phase as before. A failed scene is not automatically rearmed.

The following illustrates the **virtual model** recovery callback. `owner.scene` must be the same stable-address object used by the source binding from setup. The request must also carry the explicitly confirmed tunable state, peer readiness and a valid fresh SID; omitted here are application-specific values, not implied defaults:

```cpp
struct RecoveryOwner {
    vita::profiles::iq::VirtualRfScene scene;
    vita::profiles::iq::SceneConfig scene_config;
    std::shared_ptr<VirtualTuner> tuner;
};

request.reinitialize_context = &owner;
request.reinitialize = [](void* context,
                         const vita::runtime::StateSnapshot&)
    noexcept -> vita::Result<bool> {
    auto& owner = *static_cast<RecoveryOwner*>(context);
    auto fresh = vita::profiles::iq::VirtualRfScene::create_for_recovery(
        owner.scene_config);
    if (!fresh)
        return std::unexpected(fresh.error());
    // This is the virtual backend's explicit reset/quiescence proof.
    // A real adapter must provide its own documented physical evidence.
    auto reset = owner.tuner->model.reinitialize();
    if (!reset)
        return std::unexpected(reset.error());
    owner.scene = *fresh; // Same address; no allocation or callback rebinding.
    return true;
};
// radio.recover(request, completion) runs through the existing lifecycle.
```

Runtime suppresses old source effects while recovering, copies the existing source binding into the new bank and delivers the new initial observation before producing samples. Do not call `set_source()` from the reinitialization callback: it executes inside Runtime progress and such reentrant rebinding is rejected. A real adapter must not return `true` merely because an SDK reset was accepted. The normal executable deliberately reports faults instead of choosing recovery identities or known device state automatically.

### Shutdown failure policy in this virtual executable

Borrowed scene, receiver, observer and shutdown callback contexts are declared before Runtime, and the clean path explicitly destroys/detaches Runtime while they are still alive. If graceful shutdown cannot establish a clean local result, the isolated virtual executable flushes diagnostics and calls `std::_Exit(1)` without unwinding owners. This is a visible process-failure policy for the software model, not permission to reclaim device-owned memory, not hardware quiescence, and not a general SDR shutdown recipe. A real device port must keep its quarantine/owners alive and establish the physical evidence required by its deployment rather than copying this process-termination fallback.
