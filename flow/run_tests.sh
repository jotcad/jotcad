#!/bin/bash
# Ensure the directory is correct
cd "$(dirname "$0")"

echo "=================================================="
echo "Compiling & Running CFD Simulation Regression Tests"
echo "=================================================="

# 1. Wave Propagation Test
echo "--- [1/4] Wave Propagation Test ---"
g++ -O3 -std=c++17 wave_test.cpp -o wave_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Wave test compilation failed"
    exit 1
fi
./wave_test
if [ $? -ne 0 ]; then
    echo "❌ Wave test execution failed"
    exit 1
fi

# 2. Bed Erosion Test
echo "--- [2/4] Bed Erosion Dam-Breach Test ---"
g++ -O3 -std=c++17 erosion_test.cpp -o erosion_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Erosion test compilation failed"
    exit 1
fi
./erosion_test
if [ $? -ne 0 ]; then
    echo "❌ Erosion test execution failed"
    exit 1
fi

# 3. Darcy Seepage Test
echo "--- [3/4] Surface-Subsurface Darcy Seepage Test ---"
g++ -O3 -std=c++17 seepage_test.cpp -o seepage_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Seepage test compilation failed"
    exit 1
fi
./seepage_test
if [ $? -ne 0 ]; then
    echo "❌ Seepage test execution failed"
    exit 1
fi

# 4. Piping Test
echo "--- [4/5] Coupled Seepage Piping Erosion Test ---"
g++ -O3 -std=c++17 piping_test.cpp -o piping_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Piping test compilation failed"
    exit 1
fi
./piping_test
if [ $? -ne 0 ]; then
    echo "❌ Piping test execution failed"
    exit 1
fi

# 5. Piping Time-Step Test
echo "--- [5/6] Coupled Piping Time-Step Test ---"
g++ -O3 -std=c++17 piping_dt_test.cpp -o piping_dt_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Piping time-step test compilation failed"
    exit 1
fi
./piping_dt_test
if [ $? -ne 0 ]; then
    echo "❌ Piping time-step test execution failed"
    exit 1
fi

# 6. Stream Formation Test
echo "--- [6/6] Stream Formation Test ---"
g++ -O3 -std=c++17 stream_test.cpp -o stream_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Stream test compilation failed"
    exit 1
fi
./stream_test
if [ $? -ne 0 ]; then
    echo "❌ Stream test execution failed"
    exit 1
fi

echo "=================================================="
echo "✨ ALL 6 CFD REGRESSION TESTS PASSED SUCCESSFULLY"
echo "=================================================="
exit 0
