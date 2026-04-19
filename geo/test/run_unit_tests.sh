#!/bin/bash
# Ensure the directory is correct
cd "$(dirname "$0")"

# Build all tests using the Makefile (handles incremental work)
echo "Building unit tests..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "❌ Build failed"
    exit 1
fi

echo "Starting JotCAD Operator Unit Tests..."
echo "--------------------------------------"

TESTS=("hexagon" "box" "triangle" "offset" "outline" "group" "rotate" "pdf" "corners" "on" "cut" "nested_cut" "cut_hexagon" "accumulator" "schema" "offset_closure")
FAILED=0

for test in "${TESTS[@]}"; do
    echo "[Run] bin/${test}_test"
    ./bin/${test}_test
    if [ $? -ne 0 ]; then
        echo "❌ ${test}_test FAILED"
        FAILED=1
    fi
    echo "--------------------------------------"
done

if [ $FAILED -eq 0 ]; then
    echo "✨ ALL UNIT TESTS PASSED"
    exit 0
else
    echo "💥 SOME TESTS FAILED"
    exit 1
fi
