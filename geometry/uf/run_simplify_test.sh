#!/bin/bash
g++ -O3 -std=c++17 -fexceptions -fPIC \
    -I. \
    -I.. \
    -I../native/include/CGAL/include/ \
    -I../boost \
    -I../native/include \
    simplify_test.cc \
    -pthread -L../native/lib -lmpfr -lgmp \
    -o simplify_test && ./simplify_test
