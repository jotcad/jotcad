#!/bin/bash
# Script to build, flash, and monitor the esp32dev_counter_node application

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

# Parse suffix (defaulting to 01)
RAW_SUFFIX=${1:-1}
SUFFIX=$(printf "%02d" $RAW_SUFFIX)
NODE_ID="esp32-counter-node-$SUFFIX"

echo "============================================================"
echo "  JotCAD Node: ESP32 dev Counter Node"
echo "============================================================"
echo ""
echo "[JotCAD] Starting Build, Upload, and Monitor for $NODE_ID..."

if DEVICE_NODE_ID="$NODE_ID" pio run -e esp32dev -t upload -t monitor; then
    echo "[JotCAD] Execution finished."
else
    echo "[JotCAD] Upload or Monitor FAILED."
    exit 1
fi
