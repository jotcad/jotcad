#!/bin/bash
# Script to build, flash, and monitor the ESP32 VFS node using integrated targets

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

echo "[JotCAD] Starting Build, Upload, and Monitor in $SCRIPT_DIR..."
# Using -t upload -t monitor is the standard PIO way to catch boot logs
if pio run -t upload -t monitor --upload-port /dev/ttyUSB0; then
    echo "[JotCAD] Execution finished."
else
    echo "[JotCAD] Upload or Monitor FAILED. Check if port /dev/ttyUSB0 is busy or if device is unplugged."
    exit 1
fi
