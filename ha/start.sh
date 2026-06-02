#!/bin/bash
# Local AI Voice Assistant Orchestrator.
# Manages background services (Ollama) and ensures clean shutdown on exit.

# Get the directory where the script is located
HA_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$HA_DIR"

# Array to keep track of background PIDs to kill on exit
BACKGROUND_PIDS=()

# Cleanup function to kill all background processes on exit
cleanup() {
    echo -e "\n\033[1;31m[Orchestrator] Shutting down background tasks cleanly...\033[0m"
    for pid in "${BACKGROUND_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            echo "[Orchestrator] Terminating process $pid..."
            kill "$pid" 2>/dev/null || kill -9 "$pid" 2>/dev/null
        fi
    done
    echo -e "\033[1;32m[Orchestrator] All background processes terminated. Goodbye!\033[0m"
}

# Trap exit signals to guarantee cleanup runs
trap cleanup SIGINT SIGTERM EXIT

echo "=========================================================================="
echo "           JotCAD LOCAL PRIVATE VOICE ASSISTANT CO-PILOT                  "
echo "=========================================================================="

# 1. Start Ollama Server in the background if not already alive
if ! curl -s -f http://127.0.0.1:11434/api/tags &>/dev/null; then
    echo -e "\033[0;34m[Orchestrator] Starting Ollama API server in the background...\033[0m"
    if command -v ollama &> /dev/null; then
        ollama serve &
        OLLAMA_PID=$!
    else
        "$HA_DIR/bin/ollama" serve &
        OLLAMA_PID=$!
    fi
    BACKGROUND_PIDS+=("$OLLAMA_PID")
    
    # Wait for the HTTP server to initialize
    echo -n "Waiting for Ollama to wake up..."
    for i in {1..10}; do
        if curl -s -f http://127.0.0.1:11434/api/tags &>/dev/null; then
            echo " Connection OK!"
            break
        fi
        echo -n "."
        sleep 1
    done
    echo ""
else
    echo -e "\033[0;32m[Orchestrator] Ollama API server is already running.\033[0m"
fi

# 2. Activate Python Virtual Environment
echo "[Orchestrator] Loading Python Virtual Environment..."
source "$HA_DIR/venv/bin/activate"

# Ensure the required LLM model is downloaded
if ! curl -s http://127.0.0.1:11434/api/tags | grep -q "qwen2.5:1.5b"; then
    echo -e "\033[0;33m[Orchestrator] Model 'qwen2.5:1.5b' not found. Pulling it locally (900 MB)... This may take a minute.\033[0m"
    if command -v ollama &>/dev/null; then
        ollama pull qwen2.5:1.5b
    else
        "$HA_DIR/bin/ollama" pull qwen2.5:1.5b
    fi
fi

# 3. Start Voice Assistant Loop (in foreground, blocking)
echo -e "\033[0;32m[Orchestrator] Launching Voice Assistant Terminal interface...\033[0m"
python3 "$HA_DIR/assistant.py" "$@"
