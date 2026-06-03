#!/bin/bash
# Note: set -e removed to allow counting failures
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

echo "Building all tests..."
make -C "$DIR" all

tests=0
pass=0
fail=0

# Query the combined test binary for the list of tests
if [ -f "$DIR/bin/unit_tests" ]; then
    TEST_NAMES=$("$DIR/bin/unit_tests" --list)
    for name in $TEST_NAMES; do
        tests=$((tests + 1))
        echo "[RUN] $name"
        if "$DIR/bin/unit_tests" "$name"; then
            pass=$((pass + 1))
        else
            fail=$((fail + 1))
            echo "✖ $name"
        fi
    done
else
    echo "Error: bin/unit_tests not found."
    exit 1
fi

echo ""
echo "ℹ tests $tests"
echo "ℹ pass $pass"
echo "ℹ fail $fail"

if [ $fail -gt 0 ]; then
    exit 1
fi
