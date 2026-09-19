# P06 implementation candidate

Status: implementation frozen for independent verification. No conformance or deployment timing qualification claim.

Implemented typed whole-command validation and bounded dependency ordering; pre-state revalidation; P/W/Er eligibility; V/X/S responses with distinct diagnostic, selector and state semantics; isolated dry-run snapshots; asynchronous per-field virtual backend outcomes; partial failures and unknown-state faulting. The default IQ profile exposes only Sample Rate writes (1–100,000,000 Hz), preserves input Q20, and permits ties-even integer adjustment only through W. A separately selected generic virtual test profile supplies S1/S2 multivariable behaviors.

The engine serializes the entire executing effectful plan. Future scheduled plans do not hold execution ownership; due boundaries order selection, then admission sequence breaks ties. It revalidates clock qualification, mapping generation, uncommitted boundaries, uncertainty windows and future-boundary rational rate representability before effects. Backend actual effect evidence is separate from requested time and AckS observation time. Mode0 with no qualified clock does not fabricate timestamps or ordinals.

All logical credits and physical revision slots are reserved before effects. `RevisionReservation` is move-only and releases unused slots. An emitted `EffectiveEvent` includes the originating `FieldOutcome`; sink record owns one transferred revision/Context credit pair until revision retirement. Queries/dry runs do not reserve live revisions. External asynchronous results are written only after winning a generation-tagged completion claim; capabilities pin storage beyond engine frontend lifetime. Synthetic failure leaves potentially changed state unknown and blocks subsequent real writes pending P11 recovery. Ticket cleanup checks exact ownership generation and cannot drain another transaction's completion.

Controller observations retain timeout evidence and late responses separately. Local sends, validation, state observations and hypothetical results never establish actual execution success. Response encoding takes a separate outgoing packet count and explicit clock epoch; correlated diagnostics use original request CAM. Generic Ack timing7 may omit an unavailable actual timestamp, with no invented epoch. See `P06-outcomes-implementation.md` for the supporting codec correction and observation tests.

Developer verification:

- `cmake --preset dev`; built `p06_engine`, `p06_outcomes`, `p06_loopback_transaction`: pass.
- `ctest --preset dev -R '^p06_' --output-on-failure`: 13/13 pass (includes independent preparatory tests, not an independent verdict).
- `cmake --preset asan-ubsan`; built the three developer targets; `ctest --preset asan-ubsan -R '^p06_(engine|outcomes|loopback_transaction)$' --output-on-failure`: 3/3 pass.
- Integration transmits real leased S1 and S2 commands through Loopback checked routing, engine admission, virtual backend completion, encoded V/X/S through a second route, and ControllerObserver. Separate sender counters commit only accepted submits. Partial S1 changes only A; S2 changes no live state. Four transmitted leases return exactly once.

Measured Apple arm64 clang/libc++ object sizes: `Engine<8>` 20,920 B; `Engine<128>` 330,040 B; per-plan 1,168 B; transaction record 1,272 B; `ResultStorage<32>` 3,072 B; completion state 88 B; each completion slot 88 B; `EffectiveEvent` 264 B; `AsyncResult` 64 B; `VirtualBackend<16>` 3,024 B. Static assertions enforce plan<=8 KiB and record<=2 KiB. Engine<N> inline storage includes N physical plans, N records and their bundles. Setup heap allocations are one ResultStorage<4N> (384N B plus shared-owner implementation overhead), one completion state (88 B plus shared-owner overhead), and its 4N slot array (352N B plus allocator overhead). No standalone SlotArena is instantiated by Engine; do not charge that alternative physical plan arena again. Shared-owner allocator overhead remains implementation-dependent and must be included by the complete P10 configuration budget, not hidden inside these native sizes.

Scope boundary: P07 supplies canonical duplicate retention/cancellation and full controller correlation; P09 supplies actual revision storage/Context publication; P10 supplies the generator binding and example clock; P11 supplies recovery. P06's fixed duplicate credit placeholder is not the final globally shared cache byte budget. No external OUI is fabricated: integration intentionally uses generic class-omitting in-process fixtures. No Linux or device timing qualification was executed locally.

## Frozen file manifest

- `include/vita/runtime/state/contracts.hpp` `7772e2d134d9c844f2f0d7dc6497505fb7b6a114158772854c8cc41722da6ad0`
- `include/vita/runtime/transaction/backend.hpp` `a4795ce7367e01ccc9e8bbbe2932b4de495724b46dc9bc0bc9f152db51d0f0d5`
- `include/vita/runtime/transaction/cam.hpp` `4ec208ef16dccb2233a7ec0269eff73dc641d1b63927209ac432fccedf284dda`
- `include/vita/runtime/transaction/engine.hpp` `4b74ec5eb9fd96fd49bd24089c44379d084a71f269d3cc6f7fd9dccc502e75bb`
- `include/vita/runtime/transaction/outcomes.hpp` `19a7bc51e19cc95f7ed38cb8c0ae2acb6573ec04fd2ee925c894a0c8d152bdbc`
- `tests/integration/P06/CMakeLists.txt` `42c4cab3a1432934eb994e65e9e60a8a40ab9f735a95b7b1d42972166a151675`
- `tests/integration/P06/chain.cpp` `98e0b8b978eddbf834bab328bf5e7092a3a519d321253b63d6618900ea6980eb`
- `tests/unit/P06/CMakeLists.txt` `ca8438c0229199006718c82b2351a344508c8237f56ecd607c3fe19f310fbd55`
- `tests/unit/P06/engine.cpp` `abc9e6b5ac5506ac4a411ffa49ce755cf3796b28f1320a22ef291cfdb2693dfc`
- `tests/unit/P06/outcomes.cpp` `df2debb691ed3b2dc07e7cb8cde6a01e27813baff32290e654300781a12b5105`

Verifier review correction: a failed validation schedule now omits a nonexistent planned timestamp; timing diagnostics retain AckT7. Developer Debug and ASan three-target suites rerun successfully after correction.
