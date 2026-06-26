#!/bin/bash
# Note: set -e removed to allow counting failures
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

echo "Building all tests..."
make -C "$DIR" all

tests=0
pass=0
fail=0

if [ -f "$DIR/bin/unit_tests" ]; then
    "$DIR/bin/unit_tests" all
    exit $?
else
    echo "Error: bin/unit_tests not found."
    exit 1
fi
