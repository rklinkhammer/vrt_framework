# P13 receiver characterization analyzer

Implementation candidate; independent verification is maintained separately. This tool characterizes the receiver harness and does not apply the former combined Control latency or zero-drop qualification targets. It does not validate a deployment hardware model.

Run `python3 bench/analyze_receiver.py RECEIVER_DIR [--sender-dir SENDER_DIR] [--output FILE]`. Default output is `receiver_analysis.json`; exit 0 means consistent characterization evidence, exit 2 means invalid/incomplete evidence or an analysis resource/I/O failure.

The analyzer consumes schema-1 receiver summary and the eleven-column `receiver.csv`. It checks generated/written/raw counts, the 3-by-4 ingress-phase/status matrix, per-stream consumer counters when supplied, capture overflow and I/O errors, explicit receiver memory category totals, both worker/writer stack declarations, and the declaration of C-allocation instrumentation coverage. A false coverage declaration is reported honestly; it does not establish absence of allocations. Missing explicit budget fields cannot silently qualify an older incomplete summary.

Delivered samples require known metadata, complete ordered receiver-local timestamps, the configured IQ16 payload extent, and an exact standard FNV1a64 checksum against sixteen literal canonical IQ16 pairs. Unsigned 64-bit checksums are validated in Python; they are not truncated into SQLite signed integers. The generation and epoch identify the capture association. Within that association, `(SID, ordinal, rx_ns)` is the event key: identical events invalidate capture, while repeated ordinals at distinct receive times remain observable duplicates. Coverage, overlap, reordering and interior gaps are reported without assigning their cause to the receiver.

Metadata expiry and pending overflow have no application stages; their payload extent may be zero because no consumer payload was inspected. Checked rejection before the consumer likewise has no fabricated application timestamps. A checked rejection after consumer entry retains ordered application/consumption evidence, known metadata and full payload extent; its possibly incorrect checksum is the rejection observation. Such processing has separate paired quantiles and is excluded from successful throughput. Every non-delivery remains counted, including measurement-ingress packets completed during drain.

For the successful measurement-ingress cohort, exact nearest-rank p50/p90/p99/max are computed from paired `rx→validated`, `validated→app`, `app→consumed`, and `rx→consumed` intervals. Validation outcomes across all statuses are also reported separately. Cohort throughput includes later drain completions. Consumption-window throughput includes only successful consumer completions inside the actual local measurement window, including earlier warmup-ingress packets completed there. `consumed` means checksum/consumption completed before callback return; final provider-lease reclamation is not timed by this seam.

Optional sender summaries remain separate evidence with their own offered/accepted/skipped/retry counts and window. Missing sender evidence does not invalidate receiver-local service measurements. No cross-host clock subtraction or residual-loss attribution is performed.

Analysis is streaming with a default 10-million-row hard limit (`--max-rows`, maximum 100 million), 128-character CSV fields, a 1 MiB summary limit, 32 bounded issue examples, and a 4 MiB SQLite cache. Keys and exact interval distributions occupy an indexed temporary SQLite database with a hard default 4096 MiB page cap (`--max-disk-mib`, 1–65536). Indexes exist before ingestion, avoiding unbounded final sort staging. Limits fail explicitly rather than truncate observations. Python/SQLite fixed runtime overhead and OS caching are outside the stated cache size; input-dependent rows are disk-backed. The temporary database is removed after analysis, including failure.

Developer validation: ten synthetic tests pass, covering exact paired ranks, drain/cohort separation, byte-zero expiry, staged consumer rejection, duplicates versus corrupt duplicate events, stage/checksum/phase corruption, budget and consumer counter mismatch, sender-clock independence, and both row and disk exhaustion. The actual `artifacts/P13/receiver-smoke/receiver` capture validates 2424 rows with the independently reported sender summary; this short smoke is integration evidence, not sustained performance qualification.

Frozen candidate SHA-256:

- `bench/analyze_receiver.py`: `ad8f412797791a6a36aa09c1ea3f027bdd3fd32cf56f6d27f6f2aa7b7aef1c12`
- `tests/unit/P13/receiver_analysis_test.py`: `4a91b07329c0fbfadca6a1ae12aa529cc05e7ff6105f49e20f786ac4530524f5`
