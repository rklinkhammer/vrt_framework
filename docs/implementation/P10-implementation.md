# P10 implementation — public IQ runtime

Candidate for independent verification. The verifier report, not this implementation report, grants the gate. No network/deployment qualification or measured throughput claim is made.

## Public composition and ownership

`VitaRuntime::create` accepts explicit OUI, clock binding/capabilities and externally owned pools. `add_controllee` and `add_controller` establish identities and configuration before first progress freezes registration. Controller methods submit typed rate changes, queries and cancellation; the framework owns checked encoding, transport submission, decoding, admission, backend progress, ordered V/X/S replies, retention/replay and Context/Data association. The three finite examples use these bindings without application protocol loops, manual Acks or buffer returns. Their fixture OUI is explicitly isolated lab configuration, not a production assignment.

Transaction handles carry runtime identity, stream identity and controller generation. Foreign-runtime/stream use is rejected. Observers receive distinct bounded phase events; callbacks are nonblocking and may submit asynchronous work. Same-domain blocking waits reject with `would_deadlock`. `wait(handle,budget,evidence)` drives only the explicitly injected lab clock and returns typed evidence-received, transaction-timeout or wait-budget-expired status; it does not change the original deadline or cancel. Evidence receipt is not necessarily successful execution: callers inspect the returned typed observation. Production callers supply qualified time/progress through their host integration.

The baseline uses separate external header, payload and optional trailer leases with the checked prologue/trailer codec. Providers write normalized IQ directly into external payload windows, then the runtime validates complete finite output. IQ16, IQ32 and float32 are supported. Format/class selection is immutable per configured stream; source-provider replacement is allowed while configured/stopped and preserves sample phase. Trailer emission requires an explicitly distinct configured class variant. Applications retain receiver data through existing lease/quota APIs; framework callbacks expose coherent immutable metadata.

## Timing and publication

Pacing uses elapsed monotonic time, separately from protocol timestamp mapping. At most one current packet is generated per stream per progress; overdue whole intervals are skipped using bounded binary search, with ordinal/phase and exact rational time advanced. A packet is due at its first sample. Start/resume/remapping discards a partial interval preceding the qualified full Context observation and defers the next packet until its first sample is due. The resumed packet receives a transient Sample Loss indication when samples were skipped; periodic refresh does not repeat it.

Rate changes advance the old-rate timeline to the actual effective ordinal before applying the new rate. Ready effects drain before publication, and packet interval metadata is frozen independently of the mutable generation cursor. Mapping corrections preserve monotonic sample progress and rational residue. A corrected next sample at or before a previously accepted last-sample timestamp or published Context highwater faults the association; tiny nonoverlapping corrections remain legal. Recovery requires the existing fresh-association procedure, implemented in P11. Numeric known-state queries remain distinct from association usability. AckS projects applicable calibration/validity at capture without rewriting retained historical replies.

## Admission and physical budget

Runtime creation preflights all provider raw/metadata bytes and native owned structures against the configured limit before constructing runtime objects. Stream registration charges actual native stream and owned arena sizes transactionally. The ledger starts empty, with explicit category headroom transfers; it does not instantiate or double count the older projected standalone plan arena.

On Apple clang/libc++ the default sixteen-stream reference configuration accounts for **47,831,664 / 67,108,864 bytes**, including **30,998,528 raw pool bytes**, **1,568,880 provider metadata/ownership allowance**, **8,388,608 shared duplicate bytes**, **2,129,920 duplicate index bytes**, **3,823,616 stream/engine/context scheduling storage**, and **922,112 other runtime/transport/controller storage**. Shared-pointer/control-block ownership uses conservative explicit setup allowances; this is a native-storage ledger, not process RSS or allocator qualification. `p10_budget` reconstructs and prints the ledger; ABI changes are remeasured at startup.

There are sixteen Engine<16> instances: 256 potential active plans and 1024 physical backend ticket slots. The shared admission pool limits 1024 live completion credits across backend and transport work. A separately charged 320-slot TX ticket arena supplies free physical backing; admission reserves its global credit before acquiring a ticket. Loopback validates a supplied credit belongs to the same pool and contains exactly that credit, and preserves it with rejected ownership or accepted completion. No double charge or uncharged accepted interval is permitted.

The runtime shares one 4096-entry/8MiB retention store and ControllerRegistry<256>, rather than multiplying them per stream. Revision stores are128 per stream; receive histories128 and waiting capacity64 per stream. The loopback has320 physical slots with64 ordinary and64 cancellation reserves. Smaller resource capacities can reject earlier than architecture maxima and are reported as bounded admission exhaustion, not promised capacity.

Reference external pools include4096 headers,1024 trailers,8192 payloads,2048 command-sized blocks partitioned1728 ordinary/64 cancellation/256 emergency,4096 receive blocks partitioned3584 Data/448 ordinary/64 cancellation, and128 large blocks. Data providers cannot alias control/cancellation/emergency providers; ordinary and cancellation lanes are also physically isolated. The optional large pool is budgeted even when unused by the baseline. Small lab pool defaults are separate from reference capacities. Explicit emergency storage and credit can emit a safe bounded diagnostic on admission rejection; inability to respond safely drops the packet rather than borrowing ordinary capacity or claiming execution.

## Validation and changed prerequisites

Developer Debug checks passed for all three source formats, runtime rate changes and coherent state, V/X/S observer delivery, callback wait rejection, reference budget, source/lab helpers, and all three executable examples. Independent preparation additionally exercises S5/S11, pacing, mapping overlap/equality, malformed providers, ownership, credits and allocation; see the independent report for frozen gate results. No duplicate full sanitizer gate is claimed here.

Additive prerequisite changes require regression coverage: ExternalPool raw-size/capability queries (P03); same-pool admission ownership (P04); pre-reserved TX completion credit (P05); AckS projection and pending-time inspection (P06); retained-terminal query used by reply draining (P07); checked mapping reanchor preserving ordinal/residue (P08). P09 production files are unchanged. Source and lab helpers have separate implementation reports.

P11 lifecycle/recovery remains deliberately pending this package gate. Current ordinary stop pauses Data but does not claim graceful shutdown or physical quiescence. No hot recovery allocation or same-SID history reset is introduced here.

## Frozen source hashes
- `examples/CMakeLists.txt`: `8e3e43982920cf7b42f935950b85e760e5ec1dbb1afc00998273709b716fe9f5`
- `examples/combined.cpp`: `4a63cfea2086e9543814d0cc565acacf683368c4cdadf52f2156b5969578b086`
- `examples/controllee.cpp`: `8c1aa7f302d8221c7679d02980a2bc6eae1aa973f9935e4be8768dde8cf5164b`
- `examples/controller.cpp`: `e99bf34c050c3d2e7648bb09a5d37c3a5ea1829ea87565497b542ce4c02e4a81`
- `include/vita/adapters/loopback/loopback.hpp`: `1c8d2be01b8c8714fa74d88fd79d20c64dfe424fbaad3ede2ba6acda62e4b5fe`
- `include/vita/memory/pool.hpp`: `62bc0de8e852809af10007d0ac4e2eb944207d5efc611135273fecb433e6cdb7`
- `include/vita/runtime/execution/admission.hpp`: `66cc03f6fb3ab2716c967fa00aaaf343a9170376e6ec56ac1b6b40675fd45be7`
- `include/vita/runtime/public/config.hpp`: `2485ecec51bad7114e5e70b6a9bb672557d43a6fdf90fe167fe56a1a948b68bf`
- `include/vita/runtime/public/runtime.hpp`: `34b1646eccf45f8c7ef3ce34dcf8c38c396a12b012b1c2e10f649249b409e88c`
- `include/vita/runtime/timing/sample_timeline.hpp`: `3fe07bad5ee65a63690be0b1d9f99dcc22930eb57cbcc618a9fba81b8980d08e`
- `include/vita/runtime/transaction/engine.hpp`: `7e90a5b40fffdb04a16a48ee1264e140bcd3e5933585a047e4dd82f38f532086`
- `include/vita/runtime/transaction/manager.hpp`: `0074e0e681070bda2c948faee55a1d60a1625475fce50f4ba51ff321cc1a7a61`
- `include/vita/runtime/transaction/retention.hpp`: `74dc1667459ae77663603622b87ed19ea8732e56ff46956344e121eae021da36`
- `tests/unit/P10/CMakeLists.txt`: `2cb87d350078a8466bcab315c3e0432fec9f80b5db7881e6a95e4cb54b0131d1`
- `tests/unit/P10/budget.cpp`: `79f2d9cbc529f8e03f33dd283ed7610d5259f63898022e1a8eadc9929d981e9d`
- `tests/unit/P10/lab.cpp`: `f3a573e8f1ead7ebf56e6aa9b84399911f564421f238dc806e22fd1fe561f461`
- `tests/unit/P10/runtime.cpp`: `d48c14e69d72bb47a330391744fd67debdcc06f789ad1301dd57d66174a3d91d`
- `tests/unit/P10/source.cpp`: `3edda084d1979f5231ef79a0ba617d2449eafe8d28e5a1e24988186d34c88872`
