#!/bin/bash
# Script to build, flash, and monitor the esp32cam_frame_node application

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

# Parse suffix (defaulting to 01)
RAW_SUFFIX=${1:-1}
SUFFIX=$(printf "%02d" $RAW_SUFFIX)
NODE_ID="esp32cam-frame-node-$SUFFIX"

echo "============================================================"
echo "  JotCAD Node: ESP32-CAM Video Frame Streamer Node"
echo "============================================================"
echo "$NODE_ID MANUAL FLASHING INSTRUCTIONS"
echo "============================================================"
echo "1. Connect GPIO 0 to GND (use a jumper wire)."
echo "2. Connect 5V power (ESP32-CAM is power-hungry)."
echo "3. Press the RST button on the back of the board."
echo "4. Once this script starts 'Connecting...', press RST again."
echo "5. After upload: Remove GPIO 0 jumper and press RST to run."
echo "============================================================"
echo ""
read -p "Press Enter to start build and upload..."

if DEVICE_NODE_ID="$NODE_ID" pio run -e esp32cam -t upload -t monitor; then
    echo "[JotCAD] Execution finished."
else
    echo "[JotCAD] Upload or Monitor FAILED."
    exit 1
fi
