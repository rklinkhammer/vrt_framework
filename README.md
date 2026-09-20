# VRT Framework

A C++23 framework for VITA 49.2 / VITA Radio Transport (VRT) packet processing and Controller/Controllee applications. It combines a header-only core with bounded runtime resources, typed command and state APIs, IQ source support, and an optional compiled POSIX UDP adapter.

The CMake project is named `vita49` (currently version `0.1.0`), and public headers live under `include/vita/`.

## Features

- Packet codecs, typed Context fields, and sample-format handling.
- Controller/Controllee transactions with validation, execution, state reporting, and cancellation.
- Runtime support for timing, stream routing, Context publication, and resource budgets.
- IQ source profiles and a virtual RF scene for frequency-scan demonstrations.
- Loopback transport and an optional POSIX UDP transport.
- Unit, verification, integration, packet-mutation, and performance qualification tooling.

Protocol coverage and qualification evidence are documented in the [protocol design](docs/vita49_protocol_design.md) and [implementation reports](docs/implementation/). The examples use isolated lab profiles and identities; they do not establish independent-vendor interoperability.

## Requirements

- CMake 3.25 or newer.
- A C++23 compiler **and standard library** providing `std::expected`, `std::span`, `std::bit_cast`, `std::endian`, `std::byteswap`, and concepts. CMake checks these during configuration.
- Ninja when using the supplied presets.
- Python 3 for tests and documentation checks.
- POSIX sockets when building the UDP adapter.

The Linux CI configuration uses GCC 14 with libstdc++ and Clang 19 with libc++.

## Build and test

Run these commands from the repository root:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

To select a compiler explicitly, add `-DCMAKE_CXX_COMPILER=g++-14` to the configure command, or use `-DCMAKE_CXX_COMPILER=clang++-19 -DCMAKE_CXX_FLAGS=-stdlib=libc++` for Clang with libc++.

The default development build includes tests, examples, and bounded packet-mutation replay. Available presets include:

| Preset | Configuration |
| --- | --- |
| `dev` | Debug core, tests, and examples |
| `asan-ubsan` | Address and undefined-behavior sanitizers |
| `tsan` | Thread sanitizer |
| `udp-dev` | Debug build with the POSIX UDP adapter |
| `udp-asan-ubsan` / `udp-tsan` | UDP builds with sanitizers |
| `udp-release` | Release UDP build with qualification tooling |

Each preset has matching configure, build, and test presets. Sanitizer configurations require a supported GCC or Clang toolchain and runtime.

Check the architecture fixtures separately with:

```sh
cmake --build build/dev --target check-docs
```

## Examples

After building the `dev` preset, run a minimal combined Controller/Controllee example or a deterministic frequency scan:

```sh
build/dev/examples/vita_combined_example
build/dev/examples/frequency_scan/vita_frequency_scan_combined --deterministic
```

The scan requests center-frequency changes, waits for matching execution and state observations, and then dwells at each frequency. Its default sweep covers 100.000–100.200 MHz in 25 kHz steps.

For separate Controller and Controllee processes over UDP:

```sh
cmake --preset udp-dev
cmake --build --preset udp-dev
python3 examples/frequency_scan/run_pair.py --controller build/udp-dev/examples/frequency_scan/vita_frequency_scan_controller --controllee build/udp-dev/examples/frequency_scan/vita_frequency_scan_controllee
```

See the [frequency-scan guide](examples/frequency_scan/README.md) for manual process startup, timing, RF-scene behavior, and command-line options. Smaller API examples are available in [controller.cpp](examples/controller.cpp), [controllee.cpp](examples/controllee.cpp), and [combined.cpp](examples/combined.cpp).

## Use in a CMake project

Place this repository at `external/vrt_framework` in your project and add it as a subdirectory:

```cmake
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(VITA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(VITA_BUILD_FUZZING OFF CACHE BOOL "" FORCE)
add_subdirectory(external/vrt_framework)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE vita::core)
```

`BUILD_TESTING` is the standard CTest option and affects the parent project too. The `vita::core` target supplies the include path, C++23 requirement, and RTTI-disabled compiler option. The public runtime entry point is `<vita/runtime/public/runtime.hpp>`.

For UDP, set `VITA_BUILD_POSIX_UDP` to `ON` before `add_subdirectory` and link `vita::posix_udp`. Enable `VITA_BUILD_BENCHMARKS` for qualification tooling; it requires the UDP adapter. The core itself does not require sockets.

## Repository layout and documentation

| Path | Contents |
| --- | --- |
| [`include/vita/`](include/vita/) | Public core, runtime, profile, and adapter headers |
| [`adapters/posix_udp/`](adapters/posix_udp/) | Compiled POSIX socket implementation |
| [`examples/`](examples/) | Controller, Controllee, combined, and frequency-scan examples |
| [`tests/`](tests/) | Compile, unit, verification, and integration tests |
| [`fuzz/`](fuzz/) | Packet-mutation replay and optional Clang libFuzzer target |
| [`bench/`](bench/) | Local performance and qualification tools |
| [`docs/`](docs/) | Architecture, protocol design, fixtures, and implementation evidence |

Start with the [framework architecture](docs/vita49_framework_architecture.md), [protocol design](docs/vita49_protocol_design.md), and [implementation plan](docs/vita49_implementation_plan.md). The [frequency-scan integration report](docs/implementation/P16-integration.md) records validation of the teaching example.

## License

This project is licensed under the [MIT License](LICENSE).

The explicitly configured [GraphX radio profile](docs/implementation/P17-graphx-profile.md)
adds atomic four-setting control, supported-limit queries, burst packetization and
bounded TCP framing. Its device and mutual-TLS transport adapters remain host-owned.
