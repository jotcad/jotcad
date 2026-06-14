#!/bin/bash
set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

echo "[Geo] Building C++ Kernels..."

# Build the argument list for make
MAKE_ARGS=()

# Default to parallel builds using all cores if -j is not specified
if [[ ! "$*" == *"-j"* ]]; then
  MAKE_ARGS+=("-j$(nproc)")
fi

# Handle "clean" as a special first argument
if [ "$1" = "clean" ]; then
  echo "[Geo] Cleaning build directories..."
  shift
  # Run clean SEQUENTIALLY first to avoid race conditions
  make -C "$DIR" clean
fi

# Run the build with parallel flags
make -C "$DIR" "${MAKE_ARGS[@]}" all "$@"
