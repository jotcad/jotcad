#!/bin/bash
set -e
echo "[Geo] Installing System Dependencies (requires sudo)..."
sudo apt-get update
sudo apt-get install -y libboost-all-dev libssl-dev build-essential libxi-dev libglu1-mesa-dev libglew-dev pkg-config xvfb
