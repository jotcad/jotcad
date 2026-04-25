#!/bin/bash
set -e

# Build the main library and all tests in parallel
echo "Building all tests..."
make -j4 all

# Run all tests in parallel using the unit_test target
echo "Running all unit tests..."
make -j4 unit_test
