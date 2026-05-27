#!/bin/bash
# Script to build and flash the esp8266_time_display_node application

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

# Parse suffix (defaulting to 01)
RAW_SUFFIX=${1:-1}
SUFFIX=$(printf "%02d" $RAW_SUFFIX)
NODE_ID="esp8266-time-display-$SUFFIX"

echo "[JotCAD] Starting Build and Upload for $NODE_ID..."

if DEVICE_NODE_ID="$NODE_ID" pio run -e esp8266_time_display -t upload; then
    echo "[JotCAD] Upload finished."
else
    echo "[JotCAD] Upload FAILED."
    exit 1
fi
