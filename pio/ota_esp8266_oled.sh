#!/bin/bash
# Script to build and upload via OTA the esp8266_oled application

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

# Parse suffix (defaulting to 01)
RAW_SUFFIX=${1:-1}
SUFFIX=$(printf "%02d" $RAW_SUFFIX)
NODE_ID="esp8266-oled-$SUFFIX"

# Target IP or Hostname (default to auto)
IP_OR_HOST=${2:-auto}

if [ "$IP_OR_HOST" = "auto" ]; then
    echo "[JotCAD] Querying VFS mesh to resolve IP for $NODE_ID..."
    TARGET_IP=$(node "$SCRIPT_DIR/discover_ota.js" "$NODE_ID")
    if [ $? -ne 0 ] || [ -z "$TARGET_IP" ]; then
        echo "[JotCAD] VFS resolution failed. Is the device online and connected to the VFS?"
        exit 1
    fi
    echo "[JotCAD] Resolved $NODE_ID to IP: $TARGET_IP"
    IP_OR_HOST="$TARGET_IP"
fi

echo "============================================================"
echo "  JotCAD Node: ESP8266 OLED Display Node (OTA)"
echo "============================================================"
echo ""
echo "[JotCAD] Starting OTA Build and Upload for $NODE_ID to $IP_OR_HOST..."

if DEVICE_NODE_ID="$NODE_ID" pio run -e esp8266_oled -t upload --upload-port "$IP_OR_HOST"; then
    echo "[JotCAD] OTA Upload SUCCESSFUL!"
else
    echo "[JotCAD] OTA Upload FAILED."
    exit 1
fi
