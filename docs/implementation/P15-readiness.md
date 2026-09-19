# P15 / M6 hardware adapter readiness

Current scope notice: the accepted operational M5 scope excludes Array-of-CIFs from production support and has passed the independent local software acceptance review. Its optional structural utility and missing peer semantic agreement do not block that scope. The P15/M6 hardware-input blocker below remains unchanged and independent of M5 acceptance. See [M5 operational scope](M5-operational-scope.md) and [operational verification](M5-operational-verification.md).

Status: BLOCKED on required adapter selection and device inputs. Execution through M6 was authorized, but no particular DMA/GPU/DPDK/RDMA adapter or hardware backend was selected. The plan explicitly scopes P15 to selected adapters only; it does not require implementing every listed option.

Available prerequisites: locally verified CPU ownership/completion contracts, virtual backend, loopback and POSIX UDP integration, and initial receiver characterization. These support interface preparation but are not device-specific evidence.

Required before implementation: selected adapter/backend and use case; target device/OS and accessible SDK/driver interfaces; CPU/device memory visibility and synchronization requirements; submission, cancellation/disarm and completion/quiescence behavior. Required for M6 qualification: access to the actual device and independent tests of fences, safe reclamation, timing/uncertainty, failure, cancellation cutoff and actual signal behavior where applicable.

No substitute adapter, invented SDK contract or simulated hardware qualification has been chosen. M6 remains optional and does not block ready P14 generic codec work. P13's remaining hardware-sizing evidence is non-blocking software characterization, separate from the missing device-specific inputs here.

Authority: implementation plan §5 P15, §9 release gates; architecture §§4 and13/M6. No production changes or hardware tests were performed for P15.
