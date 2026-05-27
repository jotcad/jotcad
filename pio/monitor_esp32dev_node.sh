#!/bin/bash
# Script to monitor the esp32dev_node application serial output

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

echo "[JotCAD] Starting Serial Monitor for esp32dev_node..."

if pio run -e esp32dev -t monitor; then
    echo "[JotCAD] Monitoring finished."
else
    echo "[JotCAD] Monitoring FAILED."
    exit 1
fi
