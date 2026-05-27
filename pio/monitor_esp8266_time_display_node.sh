#!/bin/bash
# Script to monitor the esp8266_time_display_node application

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

# Parse suffix (defaulting to 01)
RAW_SUFFIX=${1:-1}
SUFFIX=$(printf "%02d" $RAW_SUFFIX)
NODE_ID="esp8266-time-display-$SUFFIX"

echo "[JotCAD] Starting Monitor for $NODE_ID..."

if DEVICE_NODE_ID="$NODE_ID" pio run -e esp8266_time_display -t monitor; then
    echo "[JotCAD] Monitor finished."
else
    echo "[JotCAD] Monitor FAILED."
    exit 1
fi
