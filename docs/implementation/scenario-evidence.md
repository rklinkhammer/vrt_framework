# Executable scenario evidence index

The JSON fixtures describe expected cases. Only independent tests of production APIs establish implementation evidence; the Python fixture checker does not close these rows. Update test names and reports as package gates pass.

| Scenario | Behavior | Package | Executable evidence |
|---|---|---|---|
| S1 | partial_execute | P06 | `p06_verify_scenarios` and `p06_loopback_transaction`; [PASS report](P06-verification.md) |
| S2 | dry_run | P06 | `p06_verify_scenarios` and `p06_loopback_transaction`; [PASS report](P06-verification.md) |
| S3 | cancel_before_commit | P07 | `p07_verify_manager` and leased `p07_loopback`; [PASS report](P07-verification.md) |
| S4 | cancel_after_commit | P07 | `p07_verify_manager` and leased `p07_loopback`; [PASS report](P07-verification.md) |
| S5 | clock_step_timeout | P08/P10 | `p10_verify_clock_scenarios`: mapping step, unarmed timed write revalidation, unchanged monotonic deadline; [PASS report](P10-verification.md) |
| S6 | context_gate_failure | P09 | `p09_verify_effects`; [PASS report](P09-verification.md) |
| S7 | unknown_register_state | P09 | `p09_verify_effects`; [PASS report](P09-verification.md) |
| S8 | old_packet_revision | P09 | `p09_verify_effects`; [PASS report](P09-verification.md) |
| S9 | two_effect_times | P09 | `p09_verify_effects`; [PASS report](P09-verification.md) |
| S10 | partial_cancellation | P07 | `p07_verify_manager` and leased `p07_loopback`; [PASS report](P07-verification.md) |
| S11 | untimed_control_during_clock_loss | P08/P10 | `p10_verify_clock_scenarios`: mode-0 known-state query and timeout continue after clock fault; Data start rejects; [PASS report](P10-verification.md) |
| S12 | completion_publication | P04 | `p04_verify_tickets`; [PASS report](P04-verification.md) |
| S13 | stale_duplicate_completion | P04 | `p04_verify_tickets`; [PASS report](P04-verification.md) |
| S14 | fresh_sid_recovery | P11 | `p11_verify_scenarios`, `p11_verify_recovery`, and bank/lifetime regressions; [PASS report](P11-verification.md) |
| S15 | same_sid_recovery_rejected | P11 | `p11_verify_scenarios`: actual unknown-effect association rejects same SID before mutation; [PASS report](P11-verification.md) |
| S16 | synthetic_failure_not_quiescence | P04 | `p04_verify_quiescence_budget`; [PASS report](P04-verification.md) |

Wire W1–W8 passed independent P02 literal-vector tests; see [P02 verification](P02-verification.md). Timing T1–T6 passed independent P08 tests; see [P08 verification](P08-verification.md). [M3 integration passed](M3-integration.md) with 108/108 combined checks, including the public transaction, clock, recovery and example paths. Linux, independent peer, real clock, device and performance qualification are tracked separately from deterministic local gates.
