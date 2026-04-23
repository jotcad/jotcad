#!/bin/bash
# Ensure the directory is correct
cd "$(dirname "$0")"

# Build and run all unit tests using the Makefile
echo "Executing C++ Operator Unit Tests..."
make -j$(nproc) unit_test

if [ $? -ne 0 ]; then
    echo "❌ Unit tests failed"
    exit 1
fi

exit 0
