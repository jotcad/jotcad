#!/bin/bash
# Script to build, flash, and monitor the esp32_dual_music_synth application

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

# Parse suffix (defaulting to 01)
RAW_SUFFIX=${1:-1}
SUFFIX=$(printf "%02d" $RAW_SUFFIX)
NODE_ID="esp32-dual-music-synth-$SUFFIX"

# Set default serial port for classic ESP32 boards (typically /dev/ttyUSB0)
PORT=${2:-/dev/ttyUSB0}

echo "=========================================================================="
echo "           JotCAD ESP32 DUAL MUSIC SYNTH HARDWARE REQUIREMENT             "
echo "=========================================================================="
echo " * MUST use: Classic Dual-Core ESP32 (e.g., ESP32-WROOM-32, ESP32-WROVER)"
echo " * NOT SUPPORTED: ESP32-S3, ESP32-S2, ESP32-C3, ESP32-C6 (no Classic BT)"
echo " * This node streams high-fidelity music via Bluetooth A2DP Source, which"
echo "   physically requires a Classic Bluetooth radio."
echo "=========================================================================="
echo ""
echo "[JotCAD] Starting Build, Upload, and Monitor for $NODE_ID on $PORT..."

if DEVICE_NODE_ID="$NODE_ID" pio run -e esp32_dual_music_synth -t upload --upload-port "$PORT" -t monitor --monitor-port "$PORT"; then
    echo "[JotCAD] Execution finished."
else
    echo "[JotCAD] Upload or Monitor FAILED."
    echo "Double check that your Classic ESP32 is plugged in and mapped to $PORT."
    exit 1
fi
