#!/bin/bash
set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

echo "[Geo] Building C++ Kernels..."
if [ "$1" = "clean" ]; then
  echo "[Geo] Cleaning build directories..."
  make -C "$DIR" clean
fi
make -C "$DIR" all

