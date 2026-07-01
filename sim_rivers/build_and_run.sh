#!/bin/bash
# Exit immediately if a command exits with a non-zero status
set -e

# Change directory to the location of the script
cd "$(dirname "$0")"

echo "=== Building River Simulator ==="
make clean
make

echo "=== Running Droplet Erosion Simulation (1000 steps) ==="
# Clean old browser timeline images
rm -f terrain*.png

# Run the simulator outputting locally
./sim_rivers --width 256 --height 256 --steps 1000 --out-image terrain.png --out-data layers.csv

echo "=== SUCCESS: River simulation build and run completed successfully ==="
