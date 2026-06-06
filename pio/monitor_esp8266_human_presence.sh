#!/bin/bash
# Script to monitor the esp8266_human_presence application

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

pio run -e esp8266_human_presence -t monitor
