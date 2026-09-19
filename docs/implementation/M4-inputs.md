# M4 qualification inputs

P12–P13 implementation is authorized. Local software testing uses this workspace host unless the user supplies a designated qualification configuration. A preference question about the host/peer was sent; no deployment configuration is inferred from silence.

## Available local configuration

- Apple M4 Max; 16 logical CPUs; 68,719,476,736 bytes physical RAM.
- macOS 27.0 build 26A428; Darwin 27.0.0 arm64.
- Apple Clang 21.0.0 (`clang-2100.3.34.2`), libc++; C++23.
- Existing isolated lab fixture configuration and external CPU buffer providers.
- Actual host monotonic clock for local elapsed-time measurements; injected protocol-clock fixtures remain explicitly synthetic.
- Local IPv4/IPv6 socket behavior passed independent P12 tests.

## Deployment inputs not supplied

An authorized production OUI, deployment peer provisioning and independent peer/capture, qualified GPS/PPS clock binding, designated NIC/firmware/affinity/socket configuration, and real hardware backend evidence have not been supplied. These block the associated deployment claims, not local software implementation or explicitly labeled local performance measurements.

P13 must retain exact command lines, compiler flags, pool/stack/native budget, packet sizes, offered load, duration, loss method and raw control timing observations. The 30-minute four-stream run and 60-second 120% overload were executed; the original normal run is a measured performance failure and the original overload has correlation loss. Remediation is verified; revised 60-second normal/overload captures are valid, but normal no-drop and a new sustained run remain open. See [results](M4-integration.md). Localhost cannot establish independent-peer interoperability, and a short smoke run cannot replace the sustained measurement.

## Revised receiver characterization scope

Production IQ generation runs on a separate machine. Candidate receiver hardware is an output of performance modeling and application sizing, rather than a prerequisite chosen solely to pass the old local generator benchmark. A receiver-only harness and local replay tests can be developed now; deployment-representative measurements need a separate sender, known offered traffic, receiver host/NIC configuration and specified consumer work. Clock synchronization is needed for cross-host one-way latency claims, but not for receiver-local service-time modeling. No production capability is inferred from missing inputs. The prior no-drop/sustained-rerun language above is historical; see [current direction](P13-receiver-performance-model.md).
