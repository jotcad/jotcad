#!/bin/bash
# Note: set -e removed to allow counting failures
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

echo "Building all tests..."
if ! make -C "$DIR" all; then
    echo "❌ Build failed. Aborting test execution."
    exit 1
fi

tests=0
pass=0
fail=0

if [ -f "$DIR/bin/unit_tests" ]; then
    TARGET="${1:-all}"
    "$DIR/bin/unit_tests" "$TARGET"
    exit $?
else
    echo "Error: bin/unit_tests not found."
    exit 1
fi
