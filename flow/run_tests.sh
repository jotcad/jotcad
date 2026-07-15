#!/bin/bash
# Ensure the directory is correct
cd "$(dirname "$0")"

echo "=================================================="
echo "Compiling & Running CFD Simulation Regression Tests"
echo "=================================================="

# 18. Hexagonal Grid Landscape Evolution Test
echo "--- [18/18] Hexagonal Grid Landscape Evolution Test ---"
g++ -O3 -std=c++17 hex_regression_test.cpp -o hex_regression_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Hexagonal regression test compilation failed"
    exit 1
fi
./hex_regression_test
if [ $? -ne 0 ]; then
    echo "❌ Hexagonal regression test failed"
    exit 1
fi

echo "=================================================="
echo "✨ ALL ACTIVE CFD REGRESSION TESTS PASSED SUCCESSFULLY"
echo "=================================================="
exit 0
