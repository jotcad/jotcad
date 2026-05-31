# Local AI Home Automation & Voice Assistant Node

This directory contains the JotCAD **Local AI and Home Automation (HA)** integrations. Designed from the ground up to operate completely offline, this module serves as a private, high-performance voice assistant node. 

In the future, this directory will be integrated as a formal node in the JotCAD **Virtual File System (VFS)** mesh network, allowing other components to subscribe to speech-to-text transcriptions, query local LLM models, and play generated text-to-speech audio via structured WebSocket events.

---

## 1. Core Responsibilities
- **Speech-to-Text (ST7789 Visual Echo)**: Receives raw audio feed, runs local fast Whisper transcription, and publishes the result.
- **Local LLM Orchestration**: Interfaces with **Ollama** to route user prompt commands to lightweight, high-performance local models (such as `gemma2:2b` or `phi3:mini`).
- **Text-to-Speech Output**: Generates natural neural speech using the optimized, ultra-low-latency **Piper** engine.
- **VFS Mesh Bridging**: Exposes asynchronous JSON-RPC style methods over standard WebSocket connections.

---

## 2. Directory Structure
*   **`bin/`**: Contains standalone precompiled local executables (e.g. `ollama`, `piper`) to avoid requiring global system-wide root installations.
*   **`models/`**: Stores local neural models:
    *   `models/piper/`: Voice ONNX models and configurations.
    *   `models/whisper/`: Optimized CTranslate2/Whisper model directories.
*   **`venv/`**: Local Python 3 virtual environment for running memory-efficient C++/Python inference routines (like `faster-whisper`).
*   **`setup.sh`**: One-click local installation and bootstrapping script that downloads binaries and models directly.
