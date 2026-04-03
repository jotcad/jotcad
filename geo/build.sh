#!/bin/bash
mkdir -p bin

# Common files
VFS_CLIENT="../fs/cpp/src/vfs_client.cpp"
GEO_CORE="src/geometry.cpp"

# Include paths
INCLUDES="-I./include -I../fs/cpp/include -I../fs/cpp/vendor"

echo "[Geo] Building Box Agent..."
g++ -O3 -std=c++17 \
    $VFS_CLIENT \
    $GEO_CORE \
    src/box_agent.cpp \
    $INCLUDES \
    -lpthread \
    -o bin/box_agent

echo "[Geo] Building Triangle Agent..."
g++ -O3 -std=c++17 \
    $VFS_CLIENT \
    $GEO_CORE \
    src/triangle_agent.cpp \
    $INCLUDES \
    -lpthread \
    -o bin/triangle_agent

echo "[Geo] Build complete."
