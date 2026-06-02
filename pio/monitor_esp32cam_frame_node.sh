#!/bin/bash
# Script to monitor the esp32cam_frame_node application serial output

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

echo "[JotCAD] Starting Serial Monitor for esp32cam_frame_node..."

if pio run -e esp32cam -t monitor; then
    echo "[JotCAD] Monitoring finished."
else
    echo "[JotCAD] Monitoring FAILED."
    exit 1
fi
