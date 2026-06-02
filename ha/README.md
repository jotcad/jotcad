# Local AI Home Automation & Voice Assistant Node

This directory contains the JotCAD **Local AI and Home Automation (HA)** integrations. Designed from the ground up to operate completely offline, this module serves as a private, high-performance voice assistant node. 

In the future, this directory will be integrated as a formal node in the JotCAD **Virtual File System (VFS)** mesh network, allowing other components to subscribe to speech-to-text transcriptions, query local LLM models, and play generated text-to-speech audio via structured WebSocket events.

---

## 1. Core Responsibilities
- **Speech-to-Text (ST7789 Visual Echo)**: Receives raw audio feed, runs local fast Whisper transcription, and publishes the result.
- **Local LLM Orchestration**: Interfaces with **Ollama** to route user prompt commands to lightweight, high-performance local models (such as `gemma2:2b` or `qwen2.5:1.5b`).
- **Text-to-Speech Output**: Generates natural neural speech using the optimized, ultra-low-latency **Piper** engine.
- **VFS Mesh Bridging**: Exposes asynchronous JSON-RPC style methods over standard WebSocket connections.

---

## 2. Directory Structure
*   **`bin/`**: Contains standalone precompiled local executables (e.g. `ollama`, `piper`) to avoid requiring global system-wide root installations.
*   **`models/`**: Stores local neural models:
    *   `models/piper/`: Voice ONNX models and configurations.
    *   `models/whisper/`: Optimized CTranslate2/Whisper model directories.
*   **`venv/`**: Local Python 3 virtual environment for running memory-efficient C++/Python inference routines (like `faster-whisper`).
*   **`assistant.py`**: Local voice assistant coordinator that handles dynamic audio recording, transcribes voice using the `tiny.en` Whisper model, queries Ollama for LLM replies, and streams text responses into the Piper synthesizer. Includes custom terminal triggers for voice-activated builds and hardware scans.
*   **`setup.sh`**: One-click local installation and bootstrapping script that downloads binaries and models directly.
*   **`start.sh`**: Robust background task orchestrator that starts the Ollama server in the background, tracks its PID, and sets a trap to automatically terminate all background services cleanly upon exit (SIGINT/SIGTERM).

---

## 3. Terminal & Hardware Interaction

The voice assistant actively runs directly in your active terminal session and can execute physical system tasks:

### A. Terminal State Indicators (Aesthetics)
The assistant logs its exact pipeline state with colorful, highly descriptive ANSI terminal indicators:
*   `[🎤 Assistant listening... Speak now]`: Microphone is active, polling for speech.
*   `[⚡ Recording started]`: Actively streaming microphone data to buffer.
*   `[Speech completed, transcribing]`: Audio buffer closed, calling local Whisper engine.
*   `[🤔 Assistant thinking...]`: Querying the local Ollama LLM.
*   `[🗣️ Assistant speaking]`: Streaming the response out of your speakers via Piper.

### B. Voice-Activated Shell Commands
The assistant intercepts custom speech triggers and runs actual terminal commands in your workspace:
*   **"Run tests"** (or *"Verify build"*): Runs `pio run -d pio` to compile the active microcontrollers and verbally speaks the build status back to you.
*   **"Mesh status"** (or *"Scan hardware"*): Runs `pio device list --json-output`, filters for connected USB microcontrollers, prints their paths, and verbally announces how many active hardware nodes are plugged in.
*   **Conversational Fallback**: If no terminal trigger is matched, it defaults to the **Ollama LLM** for conversational assistance.
