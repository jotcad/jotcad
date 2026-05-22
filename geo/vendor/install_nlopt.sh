#!/bin/bash
set -e

# Establish vendor directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

# Cleanup existing
rm -rf nlopt

echo "[Vendor] Installing NLopt..."
git clone --depth 1 https://github.com/stevengj/nlopt.git nlopt
cd nlopt
mkdir -p build
cd build

echo "[Vendor] Configuring NLopt..."
cmake .. \
    -DBUILD_SHARED_LIBS=OFF \
    -DNLOPT_PYTHON=OFF \
    -DNLOPT_OCTAVE=OFF \
    -DNLOPT_MATLAB=OFF \
    -DNLOPT_GUILE=OFF \
    -DNLOPT_SWIG=OFF \
    -DNLOPT_TESTS=OFF

echo "[Vendor] Building NLopt..."
make -j$(nproc 2>/dev/null || echo 4)

echo "[Vendor] NLopt installation complete."
