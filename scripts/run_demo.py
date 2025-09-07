#!/usr/bin/env python3

import os
import subprocess
import sys
from pathlib import Path

# Get paths
script_path = Path(__file__).resolve()
if script_path.parent.name == "scripts":
    project_root = script_path.parent.parent
else:
    # Script is in root directory
    project_root = script_path.parent
build_dir = project_root / "build"

print("Tribune Demo Script")
print(f"Project root: {project_root}")

# Check if CMake configuration exists
cmake_cache = build_dir / "CMakeCache.txt"
makefile = build_dir / "Makefile"
if not build_dir.exists() or not cmake_cache.exists() or not makefile.exists():
    print("Configuring project with CMake...")
    build_dir.mkdir(exist_ok=True)
    subprocess.run(["cmake", "..", "-DCMAKE_BUILD_TYPE=Debug", "-DBUILD_EXAMPLES=ON"], 
                   cwd=build_dir, check=True)

# Build the project
lib_path = build_dir / "libtribune_lib.a"
if not lib_path.exists():
    print("Building Tribune library...")
    subprocess.run(["cmake", "--build", ".", "--parallel"], cwd=build_dir, check=True)

# Build test executable 
test_exe = build_dir / "tribune_test_build"
if not test_exe.exists():
    print("Building test executable...")
    # Reconfigure with examples enabled if needed
    subprocess.run(["cmake", "..", "-DCMAKE_BUILD_TYPE=Debug", "-DBUILD_EXAMPLES=ON"], 
                   cwd=build_dir, check=True)
    subprocess.run(["cmake", "--build", ".", "--target", "tribune_test_build"], 
                   cwd=build_dir, check=True)

# Run the test
print("Running Tribune test...")
result = subprocess.run([str(test_exe)], capture_output=True, text=True)
print(result.stdout)
if result.stderr:
    print(result.stderr, file=sys.stderr)

if result.returncode != 0:
    print(f"Test failed with return code {result.returncode}")
    sys.exit(1)

print("Demo completed successfully!")