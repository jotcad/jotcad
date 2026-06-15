#!/bin/bash
# Script to monitor the esp32c3_human_presence application

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

pio run -e esp32c3_human_presence -t monitor
