# P00 independent verification

Date: 2026-09-18. Verifier: V-P00 (separate from I-P00).

**Verdict: PASS for the P00 local functional build gate.** Linux toolchain qualification remains unexecuted. This approval covers only the frozen source manifest below, not later library APIs.

Baseline Git revision: `8435ab71d8004e9014a37a63d0f4576ea533252b`. Candidate is the uncommitted manifest below, SHA-256 `b8ea13315c3e391e813a98bd2592202a5e02fd1ec60a59075b9bf7ddc0bf4d30` (hash of sorted `sha256  path\n` records).

## Requirements and independent evidence

Architecture §§1.1, 2, 2.1 and implementation plan P00/§8 are the oracle. No wire interpretations apply. Tests were derived from required standard-library facilities and public API/build constraints, without reusing the implementation probe.

| Requirement | Evidence | Result |
|---|---|---|
| C++23 language and library facilities | Independent constexpr expected/error, span, byteswap, bit_cast, endian, concepts, tuple test | Pass |
| Header-only ODR/core-only linkage | Three TUs share inline version address and use distinct public capacity_policy<7>/<4096> types | Pass for existing public surface |
| No RTTI/exception requirement | Test targets compile with -fno-rtti/-fno-exceptions; downstream consumer does likewise | Pass |
| No required adapter or test-tool dependency | Independent downstream add_subdirectory build with BUILD_TESTING=OFF, Python discovery disabled, only vita::core linked | Pass |
| Missing required library fails clearly | Shadow expected header makes configure fail; diagnostic names C++23 and expected | Pass |
| Debug and sanitizer build interface | All seven CTest tests pass under dev, asan-ubsan, tsan | Pass locally |
| Documentation target | check-docs executes existing fixture checker | 190 specification checks pass; not implementation evidence |
| Linux compiler targets | CI inspected: GCC14/libstdc++, Clang19/libc++, all presets | Configured, not executed |

`otool -L` on the independent core-only multi-TU executable lists only libc++ and libSystem. No socket, device, third-party runtime, or adapter dependency appears. Public headers contain no per-TU configuration macros. Runtime policy instantiations do not exist yet; their ODR coverage must be added with their packages. P00's capacity policy is only a compile-time capacity declaration.

## Commands and environment

macOS/Darwin 27 arm64; Apple clang 21.0.0 (clang-2100.3.34.2), Apple libc++; CMake 4.4.3; Ninja; CMake-selected Python 3.14.7 (shell python3 is 3.13.5).

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
cmake --build --preset dev --target check-docs
otool -L build/dev/tests/verification/P00/p00_verify_multi_tu
git diff --check
```

All commands succeeded. Each complete preset run passed 7/7 tests. After preserving CMAKE_CXX_FLAGS in independent subprocess configuration (to retain libc++ selection in future Linux CI), affected independent tests were rerun: dev 4/4, ASan/UBSan 4/4; final TSan full run already included this change. Raw local logs: `build/{dev,asan-ubsan,tsan}/Testing/Temporary/LastTest.log`; dev and ASan logs contain the latest four-test reruns. Logs are ignored local artifacts and are not claimed as retained CI evidence.

## Review findings and limits

- Coordinator identified unconditional required Python discovery for core-only builds. Implementer made Python optional with tests disabled; independent downstream regression passes with discovery prohibited.
- Initial developer multi-TU test used only test-local policies. Implementer supplied a public explicit capacity policy; independent test now exercises that public surface.
- Initial CI used Clang18 instead of architecture Clang19. Corrected before final approval; remote execution remains unavailable in this session.
- No memory allocation, concurrency, semantic codec, device timing, or protocol behavior exists in P00. TSan execution is instrumentation smoke evidence only; no concurrency test coverage or race-freedom claim is made. No normative PDF review is required for this build-only package.
- Core-only and negative-library subprocess builds check configuration isolation; they do not inherit sanitizer target flags and are not additional sanitized runtime evidence.

Integration: same shared-workspace source manifest, no commit created. Relevant subsequent changes require affected verification reruns. Production-platform qualification is pending CI execution; it does not prevent subsequent deterministic packages on this feature-qualified developer platform.

## Frozen source manifest

```text
4fb4cdff07a6a11f25830b2fb19bb69b353b672078e057858e517f1e6377bdee  .github/workflows/build.yml
a847be1f4c2f0e6a86c0cda01efce1f5811428faeb03e7878408460ebb74795f  .gitignore
9630aecd2633a2d635b8795c44b6bc5786415893f445cd27ad3c317a19ce55e4  CMakeLists.txt
e2e705349919ce3574b3797ee2af737df11b1449aa42b22a101943e2fbcd3489  CMakePresets.json
44713798e3207e4def280f92ea69665283fe84b076c2fde03cf67d29202e94d6  cmake/cxx23_probe.cpp
c7090fadba63f24786de0de12c2cda91c0069124db1b7b934949ee88a5c530eb  include/vita/core/capacity_policy.hpp
7d0a8c6896e0eb4d76d233b021cdd34731282ced6882a580e907c58a09636ab9  include/vita/core/version.hpp
058432859acbeceb07f2310f38ce20baefe4f494d68d6faf0de0b34e8a381c0e  tests/compile/core_a.cpp
b028534d4ddcc288378ad25da72a12b3508e732afeb3c0f1f3810799e4ab80e2  tests/compile/core_b.cpp
5c346af88445c4ba6bb76e0f361b74828fe9c7e7db11d09d189de0e46c976a74  tests/verification/P00/CMakeLists.txt
c6553f75f75588d7535493a42da429ca8b2bc5f3a70a40f14550cb617dafcc49  tests/verification/P00/check_bad_library.py
b94dc5853226b62290bd30b5c8a49407f3a3da1fb7601eba78fb6b9523dd3ba7  tests/verification/P00/check_core_only.py
2d3e5ab72f4f7ae5c70b1f465b29ddcf6931aea9f1ab2c4b2e156765d7ff224e  tests/verification/P00/features.cpp
cbb028e087f73732b67f1ec7f0875fbf710bf9958c8e798daef58bb3c2b33def  tests/verification/P00/main.cpp
c29df6d901f3c6ed03d0a9f3060a9bc5011d7f6ca0f1d6fc7c84b6ccc9e908fe  tests/verification/P00/policies.hpp
8baacbdf8a5c93992a07e08dda84b08ed8b800d8d965a954565b44459c1e441b  tests/verification/P00/policy_a.cpp
66da362d1b79cf0e92f86e11338ec66ac56f2979cdaaa47d7109f523c1307900  tests/verification/P00/policy_b.cpp
```
