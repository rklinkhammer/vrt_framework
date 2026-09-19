# P00 implementation evidence

Date: 2026-09-18. Candidate: SHA-256 file manifest below (uncommitted workspace).

## Scope

Implemented CMake INTERFACE target `vita::core`, optional separate POSIX UDP target boundary (fails explicitly until P12 exists), C++23 language/library feature probe, plain CTest harness, Debug/ASan+UBSan/TSan presets, and `check-docs` target. Core public headers expose inline version constants and `capacity_policy<N>`; capacity variations are distinct explicit template types, without public-layout macros. The policy is a declaration of capacity, not an allocator or storage owner.

Default core target disables RTTI. Compile test executables additionally disable exceptions, exercise two translation units with different public capacity policy specializations, and include the mandatory feature probe. Test helper `vita_add_test(name sources...)` links the core and adds no dependency beyond the standard library. Verifier tests register from `tests/verification/P00/CMakeLists.txt` if present. No networking or adapter is linked into core-only tests.

## Environment and exact commands

Apple clang 21.0.0 (clang-2100.3.34.2), Apple libc++, arm64-apple-darwin27.0.0, CMake 4.4.3, Ninja, Python 3.14.7. Available Homebrew clang 23.1.0 was inspected but not used for these tests.

Commands executed successfully:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan
ctest --preset asan-ubsan --output-on-failure
cmake --preset tsan
cmake --build --preset tsan
ctest --preset tsan --output-on-failure
```

Each preset passed 3/3 developer/specification tests: `p00_core_multitu`, `p00_cxx23`, `architecture_fixtures` (190 specification checks). Logs: `build/{dev,asan-ubsan,tsan}/Testing/Temporary/LastTest.log`. These are local ignored logs, not retained CI artifacts. Verifier additions and their outcomes are tracked separately in P00-verification.md.

## Boundaries

No implementation allocates memory or performs concurrency yet. TSan smoke execution proves instrumentation starts and runs these programs; it does not prove any future concurrency correctness. No `concurrency` tests exist until actual concurrency APIs arrive. Linux GCC/libstdc++ and Clang/libc++ remain unqualified locally. POSIX UDP is deliberately unavailable until P12; requesting it fails explicitly. API behavior for semantic values, codecs, memory, and runtime belongs to later packages.

Linux qualification CI is configured for Ubuntu 24.04 with GCC 14/libstdc++ and Clang 19/libc++, across all three presets. These remote jobs have not been executed in this session; configuration is not qualification evidence. Core-only builds make Python optional; test-enabled builds require it for the specification and verifier tests.

## Candidate source hashes

| Path | SHA-256 |
|---|---|
| `CMakeLists.txt` | `9630aecd2633a2d635b8795c44b6bc5786415893f445cd27ad3c317a19ce55e4` |
| `CMakePresets.json` | `e2e705349919ce3574b3797ee2af737df11b1449aa42b22a101943e2fbcd3489` |
| `.gitignore` | `a847be1f4c2f0e6a86c0cda01efce1f5811428faeb03e7878408460ebb74795f` |
| `.github/workflows/build.yml` | `4fb4cdff07a6a11f25830b2fb19bb69b353b672078e057858e517f1e6377bdee` |
| `cmake/cxx23_probe.cpp` | `44713798e3207e4def280f92ea69665283fe84b076c2fde03cf67d29202e94d6` |
| `include/vita/core/version.hpp` | `7d0a8c6896e0eb4d76d233b021cdd34731282ced6882a580e907c58a09636ab9` |
| `include/vita/core/capacity_policy.hpp` | `c7090fadba63f24786de0de12c2cda91c0069124db1b7b934949ee88a5c530eb` |
| `tests/compile/core_a.cpp` | `058432859acbeceb07f2310f38ce20baefe4f494d68d6faf0de0b34e8a381c0e` |
| `tests/compile/core_b.cpp` | `b028534d4ddcc288378ad25da72a12b3508e732afeb3c0f1f3810799e4ab80e2` |
