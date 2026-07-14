#!/bin/bash
set -e
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

if [ ! -d "zenoh-c" ]; then
  echo "[Geo] Cloning eclipse-zenoh/zenoh-c version 1.9.0..."
  git clone --depth 1 --branch 1.9.0 https://github.com/eclipse-zenoh/zenoh-c.git
  cd zenoh-c
  mkdir -p build
  cd build
  echo "[Geo] Building zenoh-c with CMake..."
  cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$SCRIPT_DIR/zenoh"
  make
  make install
  echo "[Geo] zenoh-c local installation complete."
else
  echo "[Geo] zenoh-c already exists."
fi

echo "[Geo] Checking if zenohd router daemon is installed..."
if ! command -v zenohd &> /dev/null && [ ! -f "$HOME/.cargo/bin/zenohd" ]; then
  echo "[Geo] Building/installing zenohd router daemon version 1.9.0 via Cargo..."
  cargo install --version 1.9.0 zenohd
else
  echo "[Geo] zenohd router daemon is already installed."
fi

echo "[Geo] Checking if zenoh-bridge-remote-api is installed..."
if ! command -v zenoh-bridge-remote-api &> /dev/null && [ ! -f "$HOME/.cargo/bin/zenoh-bridge-remote-api" ]; then
  echo "[Geo] Building/installing zenoh-bridge-remote-api version 1.9.0 via Cargo..."
  cargo install --version 1.9.0 zenoh-bridge-remote-api
else
  echo "[Geo] zenoh-bridge-remote-api is already installed."
fi
