# P13 qualification readiness and measurement plan

Status: preparation only. P12 software must pass before finished-runtime P13 measurements are accepted. Authority: implementation-plan P13/M4 and architecture §§1, 13, 16. M4 software integration and deployment qualification have separate statuses.

## Required measurements and evidence

| Requirement | Required harness/evidence | Valid local scope |
|---|---|---|
| Actual 64MiB budget | Finished selected-adapter sizeof/alignment/count ledger, all setup allocations and ownership allowances, bounded stacks, explicit headroom transfers | Host-specific native-storage proof; report excluded OS socket buffers/executable/allocator infrastructure and external application buffers separately |
| No critical-path allocation | Instrument ordinary/aligned C++ allocation and relevant C allocation around steady-state TX/RX, commands, overload and shutdown; construction/warmup excluded explicitly | Actual host operational paths, not inference from fixed arrays |
| Four streams, 30 minutes | Four 1MS/s complex IQ16 streams, 256 samples/packet, IP MTU1500; real elapsed host monotonic time; per-stream generated/accepted/received/dropped counts | Mac localhost/load evidence; no Linux real-time or independent-peer delivery claim |
| p99 validation <=1ms | Per-command `t_validated-t_rx`, through actual decode/admission path under the four-stream load | Software virtual-register backend only; retain raw observations |
| p99 receive-to-recorded <=2ms | `t_recorded-t_rx` for an independent synthetic four-field Controllee, no device I/O/delay/timed request; inline backend publication after model update | Must not substitute generator boundary execution or direct backend calls |
| 120% overload | Explicit offered-rate definition (e.g. four1.2MS/s streams relative to four1MS/s baseline), finite duration, bounded queues/pools, reserved control progress, loss/rejection counters | Local measured bounded behavior; passing below overload is insufficient |
| 100MS/s stress | Separate one-stream IQ16/256 test, admission rejection if unsustainable; report independently | Optional distinct capability; does not replace normal-load gate |
| Component costs | Codec, lease/transport, conversion and full-runtime paths separately; warmup and overhead stated | Host-specific comparative results |
| Peer interoperability | Independent peer report plus wire capture, explicit interpretation agreement and identities | Unavailable until deployment inputs supplied; localhost cannot satisfy it |

For latency capture record `t_rx` at complete-envelope adapter handoff, `t_validated` after validation/admission, `t_dispatch` before backend callback, `t_device_done` at ready publication, and `t_recorded` after strand incorporation. Preserve per-command association/MID, statuses and all five monotonic timestamps. Report backend, predispatch and consumption intervals from paired samples; never subtract percentiles. Kernel queueing before handoff and Ack/network delivery are excluded from the stated two targets and should have separate observations. Instrumentation must cover actual Runtime transaction execution, not a synthetic timing-only loop. Simulated protocol time may be explicitly injected for Data semantics, but latency and duration use actual host monotonic time.

A bounded raw trace for a 30-minute run uses the required 100 ordinary commands/s plus bursts of 64 and needs capacity for at least 180,000 baseline observations plus declared bursts before start. Check total observation capacity; overflow is an invalid measurement with a counter, not silent sampling. Streaming raw rows to disk may perturb measurements and must be identified/accounted outside the critical path. Do not accumulate unbounded vectors. Report allocation-instrumentation and trace overhead separately.

## Reproducibility record

Record exact commit/source hashes, build flags/compiler/library, CPU/core topology, RAM, OS/kernel, affinity/scheduling policy, adapter and NIC/firmware (or explicitly localhost/no NIC), address family/MTU/packet sizes, actual socket buffer settings, copies, clock binding, warmup/run duration, sample/control rates, format, pool/queue limits, completion/revision reserve occupancy and maxima, per-stream loss methodology, latency sample counts/percentiles/max, and instrumentation overhead. Use payload/sample timestamp/ordinal evidence for loss where available; modulo16 Packet Count alone cannot prove losslessness. Keep raw outputs and executable invocation beside the report.

The M3 reference ledger was 51,668,752 / 67,108,864 bytes on Apple clang/libc++, including raw pools30,998,528. It is a historical starting point, not a P13 result: UDP binding/storage, benchmark backend, instrumentation arrays and any explicit worker stack reservations must be reconciled. Application output trace storage versus framework-owned staging must be stated so neither is omitted or double-counted.

## Required inputs not available locally

- D1: authorized production OUI and endpoint/SID assignments. Tests may only use caller-explicit isolated fixture identities or generic vectors omitting optional fields.
- D2/D3: qualified GPS/PPS+TOD binding, epoch and calibrated device uncertainty/lead/cutoff evidence. Injected clocks support the named software benchmark but do not qualify GPS/timed hardware.
- D4: named Linux production machine (x86-64/AArch64, eight available cores,16GiB,10GbE) and production acceptance owner; this Darwin host supports development functional and measured local-software evidence only.
- D5/D6: independent peer implementation, interpretation agreement, trust boundary/authenticated transport. Configured localhost source is a lab selector, not cryptographic authentication.
- D7: deployed packet-lifetime and restart coordination assumptions. Fresh local fixture identities do not establish deployment quarantine bounds.

No missing input above blocks implementing measurement tooling, localhost UDP integration, bounded overload or local allocation/budget evidence. Mark only the affected deployment claims blocked. A shorter smoke run is useful preliminary evidence but must not be reported as the required 30-minute run; absent any required measured threshold leaves that measurement pending or failed, independently of software implementation completion.

## Trace concurrency handoff

P13 instrumentation is not implemented in this preparation. `t_device_done` must be published together with the completion result before the existing release/acquire ready transition, or use an equivalent independently race-safe atomic trace channel. A non-atomic timestamp write after ready publication would race result consumption and is invalid evidence. The other hooks use the actual host steady clock at their stated event, with paired command identity; none may reuse the injected protocol clock. Engine/AsyncResult changes require a separate ownership/gate after P12.

## Agreed instrumentation interface proposal (not implemented)

`transaction/trace.hpp` will expose `TraceStage{received,validated,dispatch,device_done,recorded}`, `TraceKey{association_generation,operation,sid,mid}`, `TraceEvent{key,stage,monotonic_ns,field}`, and a setup-owned `TraceBinding{owner,context,now_ns,record}`. Runtime's actual route-handoff callback supplies `received`; the same operation context identifies Engine admission. Engine emits `validated` only after complete validation/admission, `dispatch` immediately before backend begin, and whole-command `recorded` after terminal result incorporation/records. Field tags allow multiple field dispatches without pretending first-field completion is command completion. The qualification workload uses one-field commands to the independent four-field model.

ResultStorage owns one pinned TraceBinding; each result slot has a device completion timestamp. AsyncResult borrows pointers to these within its existing lifetime-pinned storage. Only the winning writer captures `now_ns` and writes the stamp before ready release. The acquiring Engine emits `device_done` from that stored timestamp on the serialized record callback; the backend thread never calls the trace sink. Late capabilities keep the clock callback owner alive. The actual ready-publication timing gap and trace overhead remain measurable and reported.

The native bounded peer sends unique MIDs at100 commands/s with64-command bursts to avoid the public Controller's intentionally retained256-record capacity. It does not relax duplicate retention. A generic inline virtual-register backend updates its bounded model and publishes completion through AsyncResult; Engine ticket consumption remains real, and no IQ boundary wait or Context EffectSink is introduced into that benchmark endpoint. These ownership-specific edits wait for P12 PASS.
