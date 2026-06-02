#!/bin/bash
# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

# Detect port using the smart python utility (supports direct override via $1)
PORT=$(python3 "$SCRIPT_DIR/detect_port.py" "$1" --hint ch91)

if [ $? -ne 0 ]; then
    echo "[T-DISPLAY-DEMO] Port selection failed or cancelled."
    exit 1
fi

echo "[T-DISPLAY-DEMO] Starting serial monitor on $PORT at 115200..."
pio device monitor -d "$SCRIPT_DIR" -b 115200 -p "$PORT"
