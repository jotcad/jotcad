#!/bin/bash
# Note: set -e removed to allow counting failures
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

echo "Building all tests..."
make -C "$DIR" -j4 all

# Find all executables in the bin directory
TEST_BINS=$(find "$DIR/bin" -maxdepth 1 -type f -executable | sort)

tests=0
pass=0
fail=0

for b in $TEST_BINS; do
    name=$(basename "$b")
    # Skip cid_consistency_test as it's run separately or has different output
    if [ "$name" == "cid_consistency_test" ]; then continue; fi

    tests=$((tests + 1))
    echo "[RUN] $name"
    if "$b"; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        echo "✖ $name"
    fi
done

echo ""
echo "ℹ tests $tests"
echo "ℹ pass $pass"
echo "ℹ fail $fail"

if [ $fail -gt 0 ]; then
    exit 1
fi
