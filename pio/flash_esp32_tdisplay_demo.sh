#!/bin/bash
# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

echo "============================================================"
echo "  JotCAD Node: ESP32 Tenstar T-Display Demo Node"
echo "============================================================"
echo ""

# Detect port using the smart python utility (supports direct override via $1)
PORT=$(python3 "$SCRIPT_DIR/detect_port.py" "$1" --hint ch91)

if [ $? -ne 0 ]; then
    echo "[T-DISPLAY-DEMO] Port selection failed or cancelled."
    exit 1
fi

echo "[T-DISPLAY-DEMO] Flashing firmware to $PORT..."
pio run -d "$SCRIPT_DIR" -e esp32_tdisplay_demo --target upload --upload-port "$PORT"
