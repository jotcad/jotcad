# Geometry Domain Tests (geo/test)

This directory contains the unit tests and verification suite for the JotCAD geometry engine.

## Responsibility

Verify the correctness of geometry primitives, operators, serialization, boolean operations, rendering, and layout algorithms.

## Key Files & Directories

- **`bin/`**: Destination folder for the compiled unit test binary.
- **`baselines/`**: Target hashes and regression test references.
- **`unit_tests.cpp`**: Master test runner file representing the single translation unit (Unity/Jumbo build) for all consolidated C++ tests.
- **`unit_tests_run.h`**: Dynamically generated test namespaces declaration and registration table header.
- **`test_base.h`**: Shared testing utilities, mock VFS, and validation helpers.
- **`*_test.cpp`**: Individual test suites for specific operators and primitives.
