#!/bin/bash
set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

echo "[Geo] Building C++ Kernels..."
make -C "$DIR" clean
make -C "$DIR" all
