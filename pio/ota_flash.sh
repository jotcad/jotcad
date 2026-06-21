#!/bin/bash
# Interactive/CLI OTA Flashing Script for JotCAD ESP nodes

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

ENV=$1
TARGET_IP_OR_HOST=$2
NODE_SUFFIX=${3:-1}

# If arguments are empty, let's offer an interactive menu
if [ -z "$ENV" ] || [ -z "$TARGET_IP_OR_HOST" ]; then
    echo "============================================================"
    echo "         JotCAD OTA Firmware Update Tool"
    echo "============================================================"
    echo ""
    echo "Select an ESP Environment to compile & update:"
    echo "------------------------------------------------------------"
    
    # Define available environments
    OPTIONS=(
        "esp32_dual_music_synth"
        "esp32dev"
        "esp32cam"
        "esp32cam_number_ocr"
        "esp32_midi_reader"
        "esp32_tdisplay_demo"
        "esp32c3_human_presence"
        "esp32_human_presence"
        "esp32_dht11"
        "esp8266"
        "esp8266_time_display"
        "esp8266_oled"
        "esp8266_dht11"
        "esp8266_human_presence"
        "esp8266_sgp40"
    )
    
    for i in "${!OPTIONS[@]}"; do
        printf " [%2d] %s\n" "$((i+1))" "${OPTIONS[$i]}"
    done
    echo ""
    read -rp "Enter choice [1-${#OPTIONS[@]}]: " CHOICE_IDX
    
    # Validate choice
    if ! [[ "$CHOICE_IDX" =~ ^[0-9]+$ ]] || [ "$CHOICE_IDX" -lt 1 ] || [ "$CHOICE_IDX" -gt "${#OPTIONS[@]}" ]; then
        echo "[ERROR] Invalid choice. Exiting."
        exit 1
    fi
    ENV="${OPTIONS[$((CHOICE_IDX-1))]}"
    
    # Prompt for IP / Hostname
    read -rp "Enter Target Device IP (e.g. 192.168.1.150) or press Enter to resolve via VFS mesh: " TARGET_IP_OR_HOST
    TARGET_IP_OR_HOST=${TARGET_IP_OR_HOST:-auto}
    
    # Prompt for Node suffix
    read -rp "Enter Node Suffix Number (default 1): " NODE_SUFFIX
    NODE_SUFFIX=${NODE_SUFFIX:-1}
fi

# Determine node base name from environment or mapping
case "$ENV" in
    esp32_dual_music_synth)   BASE_ID="esp32-dual-music-synth" ;;
    esp32dev)                 BASE_ID="esp32-node" ;;
    esp32cam)                 BASE_ID="esp32-cam" ;;
    esp32cam_number_ocr)      BASE_ID="esp32-cam-number-ocr" ;;
    esp32_midi_reader)        BASE_ID="esp32-midi-reader" ;;
    esp32_tdisplay_demo)      BASE_ID="esp32-tdisplay-demo" ;;
    esp32c3_human_presence)  BASE_ID="esp32-human-presence" ;;
    esp32_human_presence)    BASE_ID="esp32-human-presence" ;;
    esp32_dht11)             BASE_ID="esp32-dht11" ;;
    esp8266)                  BASE_ID="esp8266-node" ;;
    esp8266_time_display)     BASE_ID="esp8266-node" ;;
    esp8266_oled)             BASE_ID="esp8266-oled" ;;
    esp8266_dht11)            BASE_ID="esp8266-dht11" ;;
    esp8266_human_presence)   BASE_ID="esp8266-human-presence" ;;
    esp8266_sgp40)            BASE_ID="esp8266-sgp40" ;;
    *)                        BASE_ID="esp-node" ;;
esac

SUFFIX=$(printf "%02d" "$NODE_SUFFIX")
NODE_ID="${BASE_ID}-${SUFFIX}"

# Resolve auto IP via VFS
if [ "$TARGET_IP_OR_HOST" = "auto" ]; then
    echo "[JotCAD] Querying VFS mesh to resolve IP for $NODE_ID..."
    TARGET_IP=$(node "$SCRIPT_DIR/discover_ota.js" "$NODE_ID")
    if [ $? -ne 0 ] || [ -z "$TARGET_IP" ]; then
        echo "[JotCAD] VFS resolution failed. Is the device online and connected to the VFS?"
        exit 1
    fi
    echo "[JotCAD] Resolved $NODE_ID to IP: $TARGET_IP"
    TARGET_IP_OR_HOST="$TARGET_IP"
fi

echo ""
echo "============================================================"
echo "  Starting OTA Build and Upload"
echo "============================================================"
echo " * PlatformIO Env:  $ENV"
echo " * Device IP/Host:  $TARGET_IP_OR_HOST"
echo " * Build Node ID:   $NODE_ID"
echo "============================================================"
echo ""

if DEVICE_NODE_ID="$NODE_ID" pio run -e "$ENV" -t upload --upload-port "$TARGET_IP_OR_HOST"; then
    echo ""
    echo "[JotCAD] OTA Upload SUCCESSFUL!"
else
    echo ""
    echo "[JotCAD] OTA Upload FAILED."
    echo "Please ensure the device is powered on, connected to WiFi, and reachable."
    exit 1
fi
