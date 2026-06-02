#!/bin/bash
# Script to monitor the esp32_dual_music_synth serial output

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

PORT=${1:-/dev/ttyUSB0}

echo "[JotCAD] Starting Serial Monitor for esp32_dual_music_synth on $PORT..."

if pio run -e esp32_dual_music_synth -t monitor --monitor-port "$PORT"; then
    echo "[JotCAD] Monitoring finished."
else
    echo "[JotCAD] Monitoring FAILED."
    exit 1
fi
