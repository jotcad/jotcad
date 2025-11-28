#!/bin/bash
echo "Compiling ruled_surfaces_join_strategy_naive_test..."
g++ -O3 -std=c++17 -fexceptions -fPIC \
    -I. \
    -I.. \
    -I../native/include/CGAL/include/ \
    -I../boost \
    -I../native/include \
    ruled_surfaces_join_strategy_naive_test.cc \
    -pthread -L../native/lib -lgmp \
    -o ruled_surfaces_join_strategy_naive_test &&
./ruled_surfaces_join_strategy_naive_test
echo "Test finished."