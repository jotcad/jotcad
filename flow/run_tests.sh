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
echo "--- [6/7] Stream Formation Test ---"
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

# 7. Tectonic Faulting Test
echo "--- [7/8] Tectonic Faulting Test ---"
g++ -O3 -std=c++17 fault_test.cpp -o fault_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Tectonic fault test compilation failed"
    exit 1
fi
./fault_test
if [ $? -ne 0 ]; then
    echo "❌ Tectonic fault test execution failed"
    exit 1
fi

# 8. Volcanic Eruption Test
echo "--- [8/9] Volcanic Eruption Test ---"
g++ -O3 -std=c++17 volcano_test.cpp -o volcano_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Volcanic eruption test compilation failed"
    exit 1
fi
./volcano_test
if [ $? -ne 0 ]; then
    echo "❌ Volcanic eruption test execution failed"
    exit 1
fi

# 9. Strike-Slip Shearing Test
echo "--- [9/10] Strike-Slip Shearing Test ---"
g++ -O3 -std=c++17 strikeslip_test.cpp -o strikeslip_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Strike-slip test compilation failed"
    exit 1
fi
./strikeslip_test
if [ $? -ne 0 ]; then
    echo "❌ Strike-slip test execution failed"
    exit 1
fi

# 10. Transpressional Shear Test
echo "--- [10/11] Transpressional Shear Test ---"
g++ -O3 -std=c++17 transpression_test.cpp -o transpression_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Transpression test compilation failed"
    exit 1
fi
./transpression_test
if [ $? -ne 0 ]; then
    echo "❌ Transpression test execution failed"
    exit 1
fi

# 11. Overthrust Collision Test
echo "--- [11/12] Overthrust Collision Test ---"
g++ -O3 -std=c++17 overthrust_test.cpp -o overthrust_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Overthrust test compilation failed"
    exit 1
fi
./overthrust_test
if [ $? -ne 0 ]; then
    echo "❌ Overthrust test execution failed"
    exit 1
fi

# 12. Aeolian Dune Transport Test
echo "--- [12/13] Aeolian Dune Transport Test ---"
g++ -O3 -std=c++17 dune_test.cpp -o dune_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Dune transport test compilation failed"
    exit 1
fi
./dune_test
if [ $? -ne 0 ]; then
    echo "❌ Dune transport test execution failed"
    exit 1
fi

# 13. Bolide Impact Crater Test
echo "--- [13/14] Bolide Impact Crater Test ---"
g++ -O3 -std=c++17 crater_test.cpp -o crater_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Crater test compilation failed"
    exit 1
fi
./crater_test
if [ $? -ne 0 ]; then
    echo "❌ Crater test execution failed"
    exit 1
fi

# 14. Glacial Deposition Test
echo "--- [14/15] Glacial Deposition Test ---"
g++ -O3 -std=c++17 glacial_test.cpp -o glacial_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Glacial test compilation failed"
    exit 1
fi
./glacial_test
if [ $? -ne 0 ]; then
    echo "❌ Glacial test execution failed"
    exit 1
fi

# 15. Dune Grass Vegetation Test
echo "--- [15/16] Dune Grass Vegetation Test ---"
g++ -O3 -std=c++17 dune_grass_test.cpp -o dune_grass_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Dune grass test compilation failed"
    exit 1
fi
./dune_grass_test
if [ $? -ne 0 ]; then
    echo "❌ Dune grass test execution failed"
    exit 1
fi

# 16. Continental Shelf Sedimentation Test
echo "--- [16/16] Continental Shelf Sedimentation Test ---"
g++ -O3 -std=c++17 shelf_test.cpp -o shelf_test -lcrypto
if [ $? -ne 0 ]; then
    echo "❌ Continental shelf test compilation failed"
    exit 1
fi
./shelf_test
if [ $? -ne 0 ]; then
    echo "❌ Continental shelf test execution failed"
    exit 1
fi

echo "=================================================="
echo "✨ ALL 16 CFD REGRESSION TESTS PASSED SUCCESSFULLY"
echo "=================================================="
exit 0
