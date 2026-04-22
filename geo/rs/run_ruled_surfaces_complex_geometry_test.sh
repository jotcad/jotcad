#!/bin/bash
g++ -DCGAL_DISABLE_GMP=1 -O3 -std=c++17 -fexceptions -fPIC \
    -I. \
    -I.. \
    -I../cgal/cgal/include -I../cgal/cgal/include/CGAL/include \
    -I../cgal/boost \
    -I../cgal \
    ruled_surfaces_complex_geometry_test.cc \
    -pthread  \
    -o ruled_surfaces_complex_geometry_test && ./ruled_surfaces_complex_geometry_test
