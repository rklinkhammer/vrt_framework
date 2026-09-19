# P13 receiver validation and local performance characterization

Status: receiver harness functional validation and ten repeated local characterization trials complete; independent raw audit is recorded in the companion verification report. User explicitly selected local replay for this phase. Production IQ generation remains on a separate machine; local replay does not establish cross-machine capacity or hardware suitability.

## Tested path and scope

The receiver benchmark composes the production POSIX UDP adapter, static route registry and checked codec, per-stream ContextReceiver, and an application callback. It does not run an IQ generator. A separate native-socket replay process sends precomputed256-pair IQ16 packets with dynamic sample timestamps and packet counts plus initial/refresh Context. The receiver's consumer checks the entire payload and computes a checksum synchronously.

This is the production receive-component path, not a measurement of the entire VitaRuntime facade, transaction scheduling, arbitrary application DSP, retained/slow consumers, conversion or1024-pair packets. Each omitted path requires a separate profile before extrapolating hardware needs. The standalone pool/component budget must be reported independently of the earlier transmitter Runtime's reference ledger.

Trace stages are receiver-local monotonic handoff before semantic decoding, checked Data validation, application entry after metadata association, and checksum-consumer completion. The handoff follows fixed envelope parsing, authorization and the adapter receive copy; those costs and kernel/NIC queueing are excluded. Final lease return is not separately timed. Remote protocol timestamps identify sample ordinals; they are not subtracted from local monotonic timestamps to claim one-way latency.

Sender accepted/offered/skipped traffic is retained separately. Receiver interior ordinal gaps are observations between measured deliveries; they do not identify where loss occurred or prove missing head/tail traffic. Kernel/NIC drop counters are labeled unavailable where not collected. No historical zero-drop orControl-latency threshold is applied as a software gate to this characterization.

## Measurement plan

After functional and independent gates, run separate local sender/receiver processes with four streams,1second warmup and4seconds of measured reception per trial. Repeat five per-stream rates twice:250k,500k,1M,2M,4M sample pairs/s. Use the same frozen executables, packet size, consumer and tracing settings. No build, sanitizer or analysis workload should run during the sweep.

Publish delivered packet/payload/sample rates, local stage distributions, observed gaps/overlap, explicit receiver rejects/waits/drops, capture validity, CPU use and memory. These short repeated measurements characterize this local process topology. At fixed packet size, bytes and packets are proportional, so a fitted model cannot distinguish per-byte from per-packet coefficients. Busy-poll process CPU also includes idle polling and tracing; do not label it pure decode CPU cost. Use measured curves/ranges and validate interpolation separately before making capacity predictions.

## Evidence

Optimized build and full CTest gate: **146/146 PASS**. Independent ASan/UBSan gate and literal-wire, metadata-pressure and analyzer checks are documented in [verification](P13-receiver-verification.md). The analyzer has ten synthetic unit cases plus an independent corruption/paired-percentile oracle. All ten measured process pairs exited zero; all ten complete captures validated, totaling **1,215,620 rows**. The 265-entry source/binary manifest was unchanged after the sweep.

Apple M4 Max/macOS arm64, Apple Clang 21 Release; IPv4 loopback; four streams; 256 IQ16 pairs (1,024 payload bytes, 1,052 Data wire bytes). Each trial used one second warmup and four seconds of measured ingress. Sender emission lasted 5.1 seconds and therefore has a different counting window.

| Offered pairs/s per stream | Aggregate delivered Mpairs/s, trials 1 / 2 | Handoff-to-consumption p99 µs, trials 1 / 2 | Interior gap pairs, trials 1 / 2 |
|---|---:|---:|---:|
| 250,000 | 0.999936 / 0.999936 | 1.917 / 1.917 | 0 / 0 |
| 500,000 | 1.999616 / 1.999872 | 1.917 / 1.875 | 1,024 / 0 |
| 1,000,000 | 4.000000 / 3.999744 | 1.875 / 1.875 | 0 / 1,024 |
| 2,000,000 | 7.997184 / 7.996032 | 1.875 / 1.917 | 11,264 / 16,384 |
| 4,000,000 | 15.997696 / 15.997440 | 1.875 / 1.875 | 9,216 / 10,240 |

Per-trial maximum handoff-to-consumption latency ranged from 11.167 to 550.833 µs; the largest outlier occurred in the second 1,000,000-pairs/s-per-stream trial. These tails remain visible in the results despite the small p99 values.

Delivered rates use the measurement-ingress cohort. Per-packet paired distributions and separately counted consumption-window rates are retained in each analysis. Sender-wide skipped pairs were respectively 0/0, 1,024/0, 0/1,024, 11,264/16,384 and 10,240/10,240. Similar totals do not identify individual missing packets or prove zero network loss. Receiver head/tail loss and kernel drop attribution remain unknown.

All measured deliveries had known metadata and the canonical checksum. Adapter malformed/truncated/unauthorized/pool/lane/MTU drops and socket errors were zero, as were metadata expiry, pending overflow and overlap. Capture overflow/IO errors and instrumented critical C/C++ allocations were zero. Metadata waiting high-water was zero with this sender's initial Context ordering; independent tests separately exercise waits, expiry and pool pressure.

The standalone accounted budget is **4,218,560 bytes**, including 794,816 raw pool bytes, 31,992 provider bytes, 371,320 receiver bytes, 202,088 auxiliary bytes, 721,152 capture bytes, two 1,048,576-byte worker stacks and 40 bytes of loop state. It is not the full VitaRuntime reference ledger or process RSS. Data/Control/cancellation receive pools hold 256/64/64 fixed blocks; socket buffers are OS resources reported separately. Process RSS and comprehensive pool/lease high-water sampling are not collected in these trials.

Receiver busy-poll CPU was 0.9981–0.9995 core over its complete loop lifetime, including warmup and drain. The trace writer used approximately 0.0235–0.0406 core over that same denominator; it is reported separately. These measurements do not isolate useful per-packet CPU cost. The separate sender also busy-polls. Build, sanitizer and analysis jobs were paused during timed trials; OS scheduling was not controlled or cores pinned.

## Initial model and remaining validation

This establishes a measured local operating range of approximately 3,906–62,491 delivered packets/s (1–16 Mpairs/s, roughly 4–64 MB/s payload). It does not locate receiver saturation. The p99 interval includes the synchronous checksum consumer and excludes earlier receive costs; it cannot be used alone to predict maximum packet throughput. No fitted capacity coefficients or hardware-selection prediction are claimed. Two short repetitions quantify only limited run-to-run variability.

Next model coverage: separate sender hardware; representative 1,024-pair support and MTU/buffer verification; stream/packet-size sweeps; retained/slow and conversion consumers; full resource high-water/RSS and kernel drops where available; trace-overhead comparison; longer trials and independent validation of any fit. These are characterization limits, not a renewed local zero-drop software gate. P14/P15 were not started.

## Reproduction and retained artifacts

```sh
python3 bench/run_receiver_replay.py --output-dir artifacts/P13/my-receiver-run --sample-rate 1000000 --warmup-seconds 1 --duration-seconds 4
python3 bench/analyze_receiver.py artifacts/P13/my-receiver-run/receiver --sender-dir artifacts/P13/my-receiver-run/sender --output artifacts/P13/my-receiver-run/analysis.json
```

Use a fresh output directory for every run. Compact [results](artifacts/P13-receiver/results.json), [host](artifacts/P13-receiver/host.json), [source/binary manifest](artifacts/P13-receiver/manifest.json), [unchanged-manifest check](artifacts/P13-receiver/manifest-recheck.json), and [Release test log](artifacts/P13-receiver/release-tests.log) are retained alongside the report. Large raw CSVs, exact per-run invocations, summaries and analysis remain under `artifacts/P13/receiver-validation/rate-*-trial-*` (Git-ignored local evidence).
