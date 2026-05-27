#!/bin/bash
# Script to build, flash, and monitor the esp32dev_node application

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

echo "[JotCAD] Starting Build, Upload, and Monitor for esp32dev_node..."

if pio run -e esp32dev -t upload -t monitor; then
    echo "[JotCAD] Execution finished."
else
    echo "[JotCAD] Upload or Monitor FAILED."
    exit 1
fi
