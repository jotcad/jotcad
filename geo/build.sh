#!/bin/bash
mkdir -p bin

# Common files
VFS_CLIENT="../fs/cpp/src/vfs_client.cpp"

# Include paths
INCLUDES="-I. -I./impl -I../fs/cpp/include -I../fs/cpp/vendor -I../geometry/native/include -I../geometry/boost -I../geometry/native/include/CGAL/include"

# CGAL flags
DEFS="-DCGAL_DISABLE_GMP=1"

echo "[Geo] Building Ops (Hexagon, Box, Triangle, Offset, Outline)..."
g++ -O3 -std=c++17 \
    $VFS_CLIENT \
    ops.cc \
    $INCLUDES \
    $DEFS \
    -lpthread \
    -o bin/ops

echo "[Geo] Build complete."
