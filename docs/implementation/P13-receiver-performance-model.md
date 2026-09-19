# P13 receiver performance model

## Accepted direction

The user clarified that production IQ generation runs on a separate machine. P13 characterizes performance, particularly the receiver, to guide hardware selection against application requirements. A zero-drop run by a co-located software generator is not a hard framework implementation gate. This direction supersedes the earlier interpretation of P13 as a mandatory 30-minute/no-drop benchmark on the development Mac.

Correctness, safe ownership/reclamation, bounded resource use, valid capture and honest loss accounting remain mandatory. The configured memory cap and no-critical-path-allocation contract remain unchanged. Application throughput, latency and permitted-loss requirements determine deployment hardware selection; they are not inferred from what this Mac happens to achieve.

## Existing evidence and limits

Existing measurements combine the framework IQ transmitter, Control processing and a native-socket peer on one host. Their timing stages measure Control operations. The native peer decodes/checks IQ packets and records coverage; these runs do not measure the complete framework Data/Context receiver through application consumption in isolation. Source-side obsolete-interval skips are not evidence of receiver packet drops. Generated, copied and prefilled payload trials remain useful component/combined-host characterization, with their original raw results and analyzer target failures preserved.

The legacy transmitter/Control analyzer still evaluates historical reference thresholds; the new receiver analyzer validates evidence without applying those thresholds. Its measured-target-failure result is not a new implementation blocker under this revised P13 scope. Capture corruption or unexplained accounting gaps remain evidence defects, not acceptable performance measurements.

## Receiver experiment scope

1. Add a receiver-only benchmark using the real UDP adapter, checked codec, Context association and application delivery path; no local IQ generator in its timed workload. The sender runs on a separate machine for deployment-representative measurements. An explicitly labeled local replay can prepare and verify the harness before an external sender is available.
2. Retain the sender's accepted/offered packet counts, actual pacing and sample-ordinal/timestamp evidence. Verify that the sender sustains the intended offered rate. Keep sender shortfall separate from receiver capacity.
3. Record receiver socket/kernel drops where available, adapter pool/admission drops, framing/decode rejects, Context waits/timeouts, application deliveries, retained-buffer pressure and application-consumption completion. Unknown loss location remains unknown; do not assign a residual automatically to the receiver. The four-bit VRT Packet Count cannot independently establish an exact loss count.
4. Measure receiver-local service stages with a monotonic clock: socket handoff to validation, Context-ready application delivery, and consumption/lease release. Kernel/NIC ingress timestamps may add queueing measurements where supported. Cross-machine one-way latency requires measured clock synchronization/uncertainty; do not subtract unrelated host clocks. Local service times remain useful without synchronized hosts.
5. Sweep offered load, stream count, payload size, format and consumer behavior. Record packet and byte rates as well as sample rate. Include decode-only, Context-aware delivery, conversion, and retained/slow-consumer cases as distinct paths rather than mixing their costs.
6. Publish capacity curves, loss/rejection causes, latency percentiles and maxima, CPU time/utilization, resident and framework/pool memory, queue/lease high-water marks, and measurement overhead. Repeat trials and report variability; validate any fitted model against additional workloads and retain nonlinear behavior near saturation.

The intended normal workload is 1024 IQ pairs per packet, as clarified by the user. Existing 256-pair results remain the small-packet comparison; 1024-pair support, buffer sizing and actual path MTU must be implemented/verified before claiming measurements at that size. No fragmentation policy or packet capacity changes are made by this document.

## Model for selecting hardware

At aggregate sample-pair rate R and N pairs per packet, packet rate is R/N; IQ16 payload byte rate is 4R. Account separately for wire/header overhead. Fit measured receiver CPU cost against packet rate, payload bytes, conversions, Context/Control traffic and application work; do not assume that increasing packet size or core count scales performance linearly. Report the measured range and validation error of any fit. Model memory using simultaneous receive, queued, Context-held and application-retained leases and the measured retention duration, with separate NIC/kernel buffer reporting.

Hardware is chosen after identifying the required sample/packet rates, stream count, format, application processing, latency/loss envelope and headroom. Report measured candidate-host operating envelopes and their limits, then qualify the selected deployment configuration against those application requirements. The older four-stream load, 1 ms/2 ms Control thresholds, 30-minute duration and 120% overload remain reference experiments, not universal P13 pass/fail requirements.

## Status

The receiver-only harness is now implemented and validated using explicitly authorized local replay. Ten repeated trials with four 256-pair streams produced valid captures across approximately 1–16 million aggregate pairs/s; the full optimized suite passed 146 tests. See [local validation and results](P13-receiver-validation.md) and [independent verification](P13-receiver-verification.md). P13 continues as non-blocking receiver characterization; external sender measurements, broader workload/resource coverage and a validated fitted model remain outstanding. Do not mark a receiver performance model complete based on the existing localhost transmitter trials. Missing deployment equipment limits representative measurement, but does not block independent implementation packages such as P14. This direction does not itself authorize starting P14/P15 or changing packet size.
