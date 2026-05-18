#!/bin/bash
set -e

# Establish vendor directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

# Cleanup existing
rm -rf clipper

echo "[Vendor] Installing Clipper (Legacy v6)..."
git clone --depth 1 https://github.com/tamasmeszaros/libpolyclipping.git clipper
cd clipper

# Build static library
echo "[Vendor] Building Clipper..."
g++ -O1 -fPIC -c clipper.cpp -o clipper.o
ar rcs libclipper.a clipper.o

echo "[Vendor] Clipper installation complete."
