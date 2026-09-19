# P13 local receiver replay verification

Status: independent readiness gate PASS; all ten local replay captures pass evidence-integrity audit. Remote-host and sustained qualification remain deferred.

The measured receiver composition is the actual POSIX UDP adapter, route registry, checked codec, per-stream ContextReceiver and a synchronous application checksum callback. It does not instantiate the VitaRuntime generator/transaction facade and must not be labeled a full-facade receive measurement. Packets contain256 IQ16 pairs. Consumer retention is not exercised or claimed.

Latency stages must use only the receiver's monotonic clock: complete authorized envelope handoff before semantic decoding, checked application entry, and callback completion. Protocol timestamps identify packets/metadata and must not be subtracted from receiver wall-clock values. Sender pacing/timestamps are not receiver latency clocks.

## Independent readiness gate

**PASS for isolated local replay characterization.** The12-file manifest `/tmp/p13-receiver-verifier-manifest.txt` has SHA-256 `3d579a0caf3859c1b97a2c89f2c3ff32c5ca7c3c62545ca3e8e8357311dd2661`. It includes the receiver/replay components, runner/analyzer and independent tests. No production repair was made by the verifier.

- `p13_verify_receiver_wire` sends literal Context and256-pair Data bytes through real native UDP into the framework adapter, checked parser and ContextReceiver. It verifies known metadata, the complete literal IQ16 payload, receiver-local timestamp ordering, the malformed-Context callback barrier, zero application retention, pool exhaustion/drop and subsequent lease reuse.
- `p13_verify_receiver_core` exercises the actual benchmark composition with Data arriving before Context. The eventual callback retains the original Data ingress and checked timestamps despite the newer Context ingress. It verifies checksum rejection as an explicit outcome;64 pending payloads,65th-packet overflow; and10ms metadata expiry without fabricated application timestamps.
- `p13_verify_receiver_analysis` independently verifies paired percentile arithmetic,12 corruption cases, honest preconsumer expiry/overflow, and a postconsumer checksum rejection. Nondelivery is not inserted as zero-latency delivery. Rejected consumer work is kept separate from successful throughput.
- `receiver_raw_oracle.py` independently parses real receiver rows, verifies canonical checksum, per-stream consumer counts and phase matrix, and recomputes all four paired intervals without importing the production analyzer.

The two C++ tests pass ASan/UBSan. A full configured ASan/UBSan run completed141 tests successfully; its sole failure was the independent Python fixture missing newly mandatory budget declarations. Adding the required explicit synthetic budget fields fixed that fixture, and its targeted rerun passed, yielding **142 configured tests passing across the full run and corrected-fixture rerun**. Logs: `/tmp/p13-receiver-asan-build.log`, `/tmp/p13-receiver-asan-tests.log`. Benchmark executables are not linked with allocator interposition in this sanitizer preset; optimized measurement allocation coverage is evaluated separately.

The receiver worker and CSV writer each explicitly request a1MiB pthread stack. The main thread reads their non-atomic result counters only after joining; trace transfer uses the previously verified SPSC ring. CPU time is measured separately with each thread's `CLOCK_THREAD_CPUTIME_ID`. The accounted component sum includes both stacks and loop state. The summary explicitly declares C-allocation instrumentation availability. The final small writer-CPU counter addition was inspected; it is written by the writer and read after join.

Closed readiness findings: the analyzer originally rejected legitimate byte0 expiry/overflow rows and real application timestamps on checksum-rejected rows. It now distinguishes those outcomes. Missing stack/C-coverage accounting was closed by the coordinator's explicit receiver worker and summary fields. No control-plane latency target is applied to these Data measurements.

The checked timestamp is taken after semantic wire/profile validation and before ContextReceiver metadata resolution; waiting for Context is therefore visible before application entry. The ingress hook occurs after the adapter's fixed envelope parsing, source/route authorization and receive copy. Thus the reported local interval excludes kernel queue residence, receive-copy work and that fixed framing work. `consumed_ns` marks checksum completion, not a separately instrumented final provider lease return or the very last instruction of the callback.

## Functional smoke audit

The real local two-process smoke contains2,424 delivered rows:392 warm-up,1,956 measurement-ingress,76 drain. All rows have known metadata and the independent canonical checksum; generated/written/raw counts and per-stream metrics agree. No malformed input, overlap, pending overflow, metadata expiry, capture error or critical C/C++ allocation occurred. The smoke accounts4,218,560 bytes with two explicit stacks; later layout changes must use their own recorded sum.

Independent measurement-cohort p99 intervals are333ns ingress-to-checked,125ns checked-to-application,1,708ns application-to-checksum and1,958ns ingress-to-checksum. All12 independently recomputed p50/p99/max values match the analyzer. Aggregate cohort and consumption-window rate is1,001,472 complex samples/s against four250kS/s configured streams; finite packet/window boundaries explain why a short interval need not equal the configured rate exactly. This0.5-second smoke is functional evidence, not capacity qualification.

## Final local replay sweep audit

**All ten captures pass independent evidence-integrity checks.** Two sequential trials at each250k,500k,1M,2M and4M complex samples/s **per stream** used four streams,256-pair IQ16 packets,1 second warm-up and a4-second receiver measurement window. Sender and receiver ran in separate local processes; all exited successfully. This is short-run characterization, not a receiver capacity limit, sustained qualification or deployment acceptance.

The265-entry frozen manifest, including both executables, matches every current file. Manifest SHA-256: `7f960620d596d59cb46bd64680eccbe4e444b17ffb4b9afe0ca74a5806e7c32a` at `artifacts/P13/receiver-validation/manifest.json`. Independent raw audits verified every delivered payload checksum, unique full packet key, local stage ordering, phase matrix, per-stream delivery/known/sample counters and generated/written/raw row counts. No metadata expiry, pending overflow, consumer rejection, malformed packet, unauthorized input, pool drop, overlap, capture error or critical C/C++ allocation was observed. Waiting high-water is zero in every measured stream; the independent unit tests cover waiting/expiry, but this sweep does not measure their performance.

| Configured rate per stream | Aggregate consumed samples/s, trials1 /2 | Ingress→checksum p99, trials1 /2 | Maximum interval, trials1 /2 |
|---|---:|---:|---:|
| 250,000 | 999,936 /999,936 | 1,917 /1,917ns | 11,834 /11,167ns |
| 500,000 | 1,999,616 /1,999,872 | 1,917 /1,875ns | 29,042 /20,916ns |
| 1,000,000 | 4,000,000 /3,999,744 | 1,875 /1,875ns | 14,500 /550,833ns |
| 2,000,000 | 7,997,184 /7,996,032 | 1,875 /1,917ns | 146,542 /45,250ns |
| 4,000,000 | 15,997,696 /15,997,440 | 1,875 /1,875ns | 49,500 /41,750ns |

The table uses **completion timestamps inside the receiver's actual measurement window**. The independent oracle also retains the measurement-ingress cohort and its possibly later completion; those populations are not silently interchanged. All quantiles use paired receiver-local timestamps. No sender clock or protocol sample timestamp is subtracted from a receiver host timestamp. The isolated550,833ns maximum remains visible despite the much smaller p99.

These results show that the tested local composition processed nearly16M complex samples/s aggregate at the highest configured point with valid known-metadata callbacks. They do not demonstrate a maximum capacity or zero loss on a physical network. Sender scheduling skipped samples in some trials: per-stream lifetime skips are0/0 at250k,256/0 at500k,0/256 at1M,2,816/4,096 at2M and2,560/2,560 at4M. Receiver-observed interior gaps are compatible with that sender evidence; at4M trial1 the observed cohort gap is2,304 rather than the sender's lifetime2,560. The differing windows prevent assigning the difference to a receiver loss.

The native sender intentionally runs5.1 seconds; the receiver records its1-second warm-up,4-second measurement and20ms drain after the first checked Data packet. Therefore total sender accepted packets exceed receiver captured packets even without a receiver drop: the sender continues beyond the receiver window. The artifacts provide no sender per-packet transmission ledger aligned to receiver ingress. Neither that lifetime count difference nor an ordinal gap alone is labeled network/kernel/receiver loss. Finite packet/window boundaries also explain small throughput differences from the configured rates.

The receiver worker uses about99.81–99.95% of one core across the sweep because it busy-polls; this is not evidence that checksum/decoding alone costs a full core or that the worker is at its processing limit. Writer CPU time is separately reported, approximately118–204ms over each trial's lifetime. Sender CPU is likewise separate. Trace formatting and writing remain part of the benchmark overhead; application work is a synchronous checksum, not a retained-buffer consumer or downstream DSP.

Every trial independently reconciles the same **4,218,560 accounted bytes**: caller pools and provider metadata, receiver object/auxiliary storage, bounded capture buffer, two explicit1MiB stacks and loop state. The receiver has256 Data RX blocks and64 each for Context/control and cancellation. Kernel socket buffers are separately reported, and the application/main-thread and sender process are not silently merged into this component accounting. C instrumentation availability is explicitly true on this Apple host; both receiver and sender critical C/C++ counters are zero. This does not claim allocation-free setup or writer execution.

Raw audit outputs are `/tmp/rate-<rate>-trial-<n>-independent.json`, produced by `receiver_raw_oracle.py`. No historical Control1ms/2ms latency gate was applied. Remote-host replay, physical clock qualification, consumer-retained operation, full VitaRuntime facade performance and sustained receiver qualification remain unmeasured.

| Receiver raw CSV | SHA-256 |
|---|---|
| rate-250000-trial-1 | `2a6e477ff294b6446aa0122ae746ac24faa72dbd1524dd89ec9a89f584d4c706` |
| rate-250000-trial-2 | `afbd4c71a81db8c01de39a6ce9015a651f36491f9507523533b9469b12c20b7b` |
| rate-500000-trial-1 | `4bf4276d25bc66f9d2bb30cd893c161c16a764af6fa577cc7c8fe79a8962fe5f` |
| rate-500000-trial-2 | `e65d353bb477123d8ccfe81ef9058500398ba2c420a1353f2b9dfc54a68df77a` |
| rate-1000000-trial-1 | `6889b6d29a1be118df55aeb0d93634e34eea5bac94896dac46720e79da243a60` |
| rate-1000000-trial-2 | `2affd8ce6b30e9626387ac07fa189d388b9974c882ae59dd06109629cf42cd19` |
| rate-2000000-trial-1 | `b6772af046bd6b27104e5ae7b7e71f98dabb36e678ee3ef83ec7e66e44beb007` |
| rate-2000000-trial-2 | `f49f383cc18f670e33f11996b80086f409b34e624f711e8752292863b25fbe1d` |
| rate-4000000-trial-1 | `d3b83f79a18ebc481a86846e3f52ff69d01132cc2583c3bd0315178c6c26dbba` |
| rate-4000000-trial-2 | `b0f0f5cda4e6477cf2f2defae47f2dceba562925abdb3341ae22ab36a57e2d0d` |

After the coordinator's analyzers completed, all120 paired p50/p99/max statistics (four intervals ×three statistics ×ten runs) and all ten consumption-window throughput values exactly matched the independent recomputation. Every analyzer also classified its capture as valid characterization.

The compact independent evidence is preserved in [artifacts/P13-receiver/index.json](artifacts/P13-receiver/index.json), including hashes, the readiness source manifest and all ten per-run independent oracle outputs. The earlier `/tmp` paths identify execution locations; these repository copies are the durable audit evidence. Raw packet captures and the complete frozen source/binary manifest remain in `artifacts/P13/receiver-validation`.
