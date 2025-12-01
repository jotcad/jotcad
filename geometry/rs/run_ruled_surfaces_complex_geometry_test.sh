#!/bin/bash
g++ -O3 -std=c++17 -fexceptions -fPIC \
    -I. \
    -I.. \
    -I../native/include/CGAL/include/ \
    -I../boost \
    -I../native/include \
    ruled_surfaces_complex_geometry_test.cc \
    -pthread -L../native/lib -lgmp \
    -o ruled_surfaces_complex_geometry_test && ./ruled_surfaces_complex_geometry_test
