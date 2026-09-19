#!/usr/bin/env python3
"""Consume the core as a subproject with test tooling and adapters unavailable."""
import pathlib
import subprocess
import sys
import tempfile

source = pathlib.Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix="vita-p00-core-only-") as tmp:
    root = pathlib.Path(tmp)
    (root / "main.cpp").write_text(
        '#include <vita/core/version.hpp>\n'
        '#include <vita/core/capacity_policy.hpp>\n'
        'int main() { return vita::capacity_policy<17>::capacity == 17 ? 0 : 1; }\n')
    (root / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.25)\n'
        'project(independent_core_consumer LANGUAGES CXX)\n'
        'set(BUILD_TESTING OFF CACHE BOOL "" FORCE)\n'
        'set(CMAKE_DISABLE_FIND_PACKAGE_Python3 ON CACHE BOOL "" FORCE)\n'
        f'add_subdirectory("{source}" vita)\n'
        'add_executable(core_consumer main.cpp)\n'
        'target_link_libraries(core_consumer PRIVATE vita::core)\n'
        'target_compile_options(core_consumer PRIVATE -fno-exceptions -fno-rtti)\n')
    commands = [["cmake", "-S", str(root), "-B", str(root / "build"),
                 f"-DCMAKE_CXX_COMPILER={sys.argv[1]}",
                 f"-DCMAKE_CXX_FLAGS={sys.argv[2] if len(sys.argv) > 2 else chr(32)}"],
                ["cmake", "--build", str(root / "build")],
                [str(root / "build" / "core_consumer")]]
    for command in commands:
        subprocess.run(command, check=True)
    print("PASS: core-only downstream builds without Python, adapters, RTTI, or exceptions")
