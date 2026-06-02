#!/bin/bash
# Setup and bootstrap script for the local AI Home Automation (HA) / Voice assistant node
# Installs Ollama, Piper, and Whisper completely locally without sudo/root access.

# Exit on any error
set -e

# Get the directory where the script is located
HA_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$HA_DIR"

echo "=========================================================================="
# Color formatting helper
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}[JotCAD HA] Bootstrapping Local AI Assistant Environment...${NC}"
echo "=========================================================================="

# 1. Create target directories
echo -e "${YELLOW}[1/5] Creating local directories...${NC}"
mkdir -p "$HA_DIR/bin"
mkdir -p "$HA_DIR/models/piper"
mkdir -p "$HA_DIR/models/whisper"
echo -e "${GREEN}Directories created successfully.${NC}"

# 2. Python Virtual Environment Setup
echo -e "\n${YELLOW}[2/5] Initializing Python Virtual Environment (venv)...${NC}"
if [ ! -d "$HA_DIR/venv" ]; then
    python3 -m venv "$HA_DIR/venv"
    echo -e "${GREEN}Virtual environment created at: $HA_DIR/venv${NC}"
else
    echo "Virtual environment already exists, skipping creation."
fi

# Activate virtual environment
source "$HA_DIR/venv/bin/activate"

# Check for PortAudio system-wide dependency
if ! ldconfig -p | grep -q "libportaudio" &>/dev/null; then
    echo -e "${YELLOW}[WARNING] libportaudio C library not found on your system.${NC}"
    echo "Python 'sounddevice' requires 'libportaudio2' to capture microphone input."
    echo -e "We recommend running: ${GREEN}sudo apt-get install -y libportaudio2${NC}\n"
fi

echo "Upgrading pip and installing Python local AI requirements..."
pip install --upgrade pip
pip install faster-whisper sounddevice numpy requests

# 3. Local Ollama Standalone Installation
echo -e "\n${YELLOW}[3/5] Checking for Ollama LLM Runner...${NC}"
if command -v ollama &> /dev/null; then
    SYSTEM_OLLAMA=$(command -v ollama)
    echo -e "${GREEN}Found system-wide Ollama installation at: $SYSTEM_OLLAMA${NC}"
    echo "We will use your system-wide installation. Skipping local download."
else
    if [ ! -f "$HA_DIR/bin/ollama" ]; then
        echo "No system-wide Ollama found. Downloading standalone Linux-amd64 archive..."
        curl -L https://ollama.com/download/ollama-linux-amd64.tar.zst -o "$HA_DIR/bin/ollama.tar.zst"
        
        echo "Decompressing and extracting Ollama package..."
        zstd -d "$HA_DIR/bin/ollama.tar.zst" --stdout | tar -xf - -C "$HA_DIR"
        rm "$HA_DIR/bin/ollama.tar.zst"
        
        chmod +x "$HA_DIR/bin/ollama"
        echo -e "${GREEN}Ollama standalone binary extracted to: $HA_DIR/bin/ollama${NC}"
    else
        echo -e "${GREEN}Ollama standalone binary already exists at: $HA_DIR/bin/ollama${NC}"
    fi
fi

# 4. Local Piper TTS Standalone Installation & Voice Models
echo -e "\n${YELLOW}[4/5] Checking for Piper Text-to-Speech...${NC}"
if [ ! -f "$HA_DIR/bin/piper/piper" ]; then
    echo "Downloading precompiled Piper standalone release..."
    curl -L https://github.com/rhasspy/piper/releases/download/v1.2.0/piper_amd64.tar.gz -o "$HA_DIR/bin/piper_amd64.tar.gz"
    
    echo "Extracting Piper package..."
    tar -xzf "$HA_DIR/bin/piper_amd64.tar.gz" -C "$HA_DIR/bin/"
    rm "$HA_DIR/bin/piper_amd64.tar.gz"
    echo -e "${GREEN}Piper extracted to: $HA_DIR/bin/piper/piper${NC}"
else
    echo -e "${GREEN}Piper standalone engine already exists at: $HA_DIR/bin/piper/piper${NC}"
fi

# Download a default natural voice model if not present (English Lessac Medium)
VOICE_ONNX="$HA_DIR/models/piper/en_US-lessac-medium.onnx"
VOICE_JSON="$HA_DIR/models/piper/en_US-lessac-medium.onnx.json"

if [ ! -f "$VOICE_ONNX" ]; then
    echo "Downloading English voice model (en_US-lessac-medium.onnx)..."
    curl -L "https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0/en/en_US/lessac/medium/en_US-lessac-medium.onnx" -o "$VOICE_ONNX"
    curl -L "https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0/en/en_US/lessac/medium/en_US-lessac-medium.onnx.json" -o "$VOICE_JSON"
    echo -e "${GREEN}Voice model downloaded to: $HA_DIR/models/piper/${NC}"
else
    echo -e "${GREEN}Voice model already exists: en_US-lessac-medium.onnx${NC}"
fi

# 5. Pre-caching Whisper STT Model
echo -e "\n${YELLOW}[5/5] Pre-caching local Whisper model (base.en)...${NC}"
python3 -c "
from faster_whisper import WhisperModel
print('Downloading and caching CTranslate2 Whisper base.en model...')
WhisperModel('base.en', device='cpu', compute_type='int8')
print('Whisper model cached successfully.')
"

echo -e "\n=========================================================================="
echo -e "${GREEN}[SUCCESS] Local AI Assistant stack is successfully bootstrapped!${NC}"
echo "=========================================================================="
echo " - Python venv loaded with: faster-whisper, sounddevice, numpy."
echo " - Piper Voice engine downloaded with natural English voice."
echo " - Standalone Whisper model cached for fast CPU transcription."
echo ""
echo "To test Ollama reasoning:"
if command -v ollama &> /dev/null; then
    echo "  ollama run qwen2.5:1.5b"
else
    echo "  $HA_DIR/bin/ollama serve &"
    echo "  $HA_DIR/bin/ollama run qwen2.5:1.5b"
fi
echo "=========================================================================="
