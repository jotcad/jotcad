#!/bin/bash
set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

if [[ "$1" == "--install-deps" ]]; then
  echo "[Geo] Installing System Dependencies (requires sudo)..."
  sudo apt-get update
  sudo apt-get install -y libboost-all-dev libssl-dev build-essential libxi-dev libglu1-mesa-dev libglew-dev pkg-config xvfb
  shift
fi

echo "[Geo] Building C++ Kernels..."
make -C "$DIR" clean
make -C "$DIR" all
