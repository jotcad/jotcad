#!/bin/bash

# Compile the C++ unit test
echo "Compiling geometry/repair_soup_util_test.cc..."
g++ -O3 \
    -DCGAL_DISABLE_GMP=1 \
    -I./native/include/CGAL/include/ \
    -I. \
    -I./boost \
    -I./native/include \
    -fexceptions \
    repair_soup_util_test.cc \
    -o repair_soup_util_test_runner

# Check if compilation was successful
if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

# Run the compiled test executable
echo "Running repair_soup_util test..."
./repair_soup_util_test_runner

# Check the test result
if [ $? -ne 0 ]; then
    echo "repair_soup_util test failed!"
    exit 1
fi

echo "repair_soup_util test completed successfully."

