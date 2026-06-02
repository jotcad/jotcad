#!/usr/bin/env python3
"""
Local AI Assistant Coordinator.
Listens to the microphone, transcribes speech using Whisper,
intercepts voice commands for terminal/hardware interactions,
and speaks responses using Piper.
"""
import os
import sys
import time
import numpy as np
import sounddevice as sd
import subprocess
import requests
import json
import threading
import collections
from faster_whisper import WhisperModel

# Feature flag to switch between default VAD and experimental continuous streaming mode
EXPERIMENTAL_CONTINUOUS_STREAMING = False

if "--stream" in sys.argv:
    EXPERIMENTAL_CONTINUOUS_STREAMING = True

DEBUG_MODE = False
if "--debug" in sys.argv:
    DEBUG_MODE = True

# Audio capture configurations
SAMPLE_RATE = 16000  # Whisper model expects 16kHz
THRESHOLD = 0.03     # Microphone volume threshold (RMS energy)
SILENCE_LIMIT = 1.0  # Seconds of silence to trigger end of speech (snappier detection)

# Paths and URLs
HA_DIR = os.path.dirname(os.path.abspath(__file__))
PIPER_BIN = os.path.join(HA_DIR, "bin/piper/piper")
PIPER_MODEL = os.path.join(HA_DIR, "models/piper/en_US-lessac-medium.onnx")
OLLAMA_URL = "http://127.0.0.1:11434/api/generate"

# Colors for terminal indicators
YELLOW = "\033[1;33m"
GREEN = "\033[1;32m"
BLUE = "\033[1;34m"
CYAN = "\033[1;36m"
RED = "\033[1;31m"
NC = "\033[0m"

print(f"{BLUE}[Assistant]{NC} Loading local CTranslate2 Whisper base.en model...")
stt_model = WhisperModel("base.en", device="cpu", compute_type="int8")
print(f"{GREEN}[Assistant]{NC} Local Whisper engine loaded successfully!")

def speak(text):
    """Pipes text to the local Piper TTS engine and plays synthesized PCM audio directly."""
    print(f"\n{CYAN}[🗣️ Assistant speaking]{NC}: {text}")
    try:
        # Run Piper to produce raw 22050Hz 16-bit mono PCM to stdout
        piper_proc = subprocess.Popen(
            [PIPER_BIN, "--model", PIPER_MODEL, "--output_raw"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL
        )
        
        # Stream text into stdin and close to signal completion
        raw_audio, _ = piper_proc.communicate(input=text.encode('utf-8'))
        
        # Convert raw 16-bit PCM bytes to float32 normalized audio for sounddevice
        audio_data = np.frombuffer(raw_audio, dtype=np.int16).astype(np.float32) / 32768.0
        
        # Hardware Wakeup Protection: Prepend 1.2 seconds of absolute silence (26460 zero samples at 22050Hz)
        # This gives the audio controller (especially HDMI or DACs on mini-PCs) enough time to wake up 
        # from its idle/power-saving state so the first word is never cut off!
        silence_samples = int(22050 * 1.2)
        silence = np.zeros(silence_samples, dtype=np.float32)
        audio_data = np.concatenate([silence, audio_data])
        
        # Play the voice response at Piper's native sample rate (22050 Hz)
        sd.play(audio_data, samplerate=22050)
        sd.wait()
    except Exception as e:
        print(f"{RED}[TTS Error]{NC}: Failed to synthesize speech: {e}")

def query_llm(prompt):
    """Queries the local Ollama server and returns the friendly short text response."""
    print(f"{BLUE}[🤔 Assistant thinking]{NC}...")
    try:
        payload = {
            "model": "qwen2.5:1.5b",
            "prompt": prompt + " (Keep your response short and friendly under two sentences)",
            "stream": False
        }
        resp = requests.post(OLLAMA_URL, json=payload, timeout=30)
        if resp.status_code == 200:
            return resp.json().get("response", "").strip()
        else:
            return "Ollama returned an error."
    except Exception as e:
        return "Failed to connect to Ollama. Please check if the server is running."

def execute_terminal_command(voice_command):
    """Parses voice commands and executes actual hardware or build tasks in the terminal."""
    cmd_lower = voice_command.lower()
    
    # 1. Voice-Activated Compilation & Verification Check
    if any(phrase in cmd_lower for phrase in ["run tests", "verify build", "compile firmware"]):
        speak("Executing platform io compilation check. Please check the terminal output.")
        print(f"\n{YELLOW}[🛠️ Executing shell command: pio run -d pio]{NC}")
        try:
            result = subprocess.run(
                ["pio", "run", "-d", "pio"],
                capture_output=True,
                text=True,
                timeout=30
            )
            if result.returncode == 0:
                speak("Verification complete! The firmware compilation succeeded and all builds are clean!")
            else:
                speak("Verification failed. There was a compilation error in the platform io project.")
                print(result.stderr)
        except Exception as e:
            speak(f"Could not run build: {e}")
            
    # 2. Voice-Activated Mesh Hardware Scanning
    elif any(phrase in cmd_lower for phrase in ["mesh status", "list devices", "scan hardware"]):
        speak("Scanning your connected hardware nodes.")
        print(f"\n{YELLOW}[🔍 Executing shell command: pio device list --json-output]{NC}")
        try:
            result = subprocess.run(
                ["pio", "device", "list", "--json-output"],
                capture_output=True,
                text=True,
                timeout=10
            )
            devices = json.loads(result.stdout)
            # Filter for active USB-serial connections
            usb_devs = [d for d in devices if "usb" in d.get("port", "").lower() or "acm" in d.get("port", "").lower()]
            if usb_devs:
                speak(f"I found {len(usb_devs)} active microcontroller board connected to your system.")
                for d in usb_devs:
                    speak(f"Port {d['port']} has hardware signature: {d.get('description', 'Unknown device')}")
            else:
                speak("No active USB microcontrollers were found connected to your system.")
        except Exception as e:
            speak(f"Failed to scan devices: {e}")
            
    # 3. Standard local LLM conversation fallback
    else:
        reply = query_llm(voice_command)
        speak(reply)

def listen():
    """Records microphone audio dynamically, activating on speech and stopping on silence."""
    print(f"\n{YELLOW}[🎤 Assistant listening... Speak now]{NC}")
    audio_buffer = []
    pre_roll = []
    recording = False
    silence_start = None
    
    # Live transcription tracking
    transcribed_text = ""
    transcription_lock = threading.Lock()
    stop_event = threading.Event()
    
    def transcribe_worker():
        nonlocal transcribed_text
        last_len = 0
        while not stop_event.is_set():
            # Check in small sleep increments to respond quickly to stop_event
            for _ in range(5):
                if stop_event.is_set():
                    break
                time.sleep(0.1)
                
            with transcription_lock:
                if not audio_buffer or not recording:
                    continue
                current_len = len(audio_buffer)
                if current_len == last_len:
                    continue
                last_len = current_len
                snapshot = np.concatenate(audio_buffer, axis=0).flatten()
                
            try:
                # Transcribe accumulated audio so far
                segments, _ = stt_model.transcribe(snapshot, beam_size=1)
                text = "".join(seg.text for seg in segments).strip()
                if text:
                    with transcription_lock:
                        transcribed_text = text
                    # Print in-place using carriage return to show live speech feedback
                    sys.stdout.write(f"\r\033[K{GREEN}[You]{NC}: {text} ...")
                    sys.stdout.flush()
            except Exception:
                pass

    # Callback stream handler for sounddevice
    def audio_callback(indata, frames, time_info, status):
        nonlocal recording, silence_start
        energy = np.sqrt(np.mean(indata**2))
        
        if energy > THRESHOLD:
            if not recording:
                print(f"{GREEN}[⚡ Recording started]{NC}")
                recording = True
                with transcription_lock:
                    audio_buffer.extend(pre_roll)
                # Launch the parallel live transcription worker thread
                threading.Thread(target=transcribe_worker, daemon=True).start()
            silence_start = None
            with transcription_lock:
                audio_buffer.append(indata.copy())
        elif recording:
            if silence_start is None:
                silence_start = time.time()
            with transcription_lock:
                audio_buffer.append(indata.copy())
        else:
            # Keep a rolling pre-roll buffer of last 0.5s (8000 samples at 16kHz) to avoid speech clipping
            pre_roll.append(indata.copy())
            total_samples = sum(len(c) for c in pre_roll)
            while total_samples > 8000 and len(pre_roll) > 1:
                pre_roll.pop(0)
                total_samples = sum(len(c) for c in pre_roll)

    # Open microphone capture stream
    with sd.InputStream(samplerate=SAMPLE_RATE, channels=1, callback=audio_callback):
        while True:
            sd.sleep(100)
            if recording and silence_start and (time.time() - silence_start > SILENCE_LIMIT):
                sys.stdout.write(f"\r\033[K{BLUE}[🤔 Assistant thinking]{NC}...")
                sys.stdout.flush()
                stop_event.set()
                break
                
    if not audio_buffer:
        return None
        
    # Stop background thread and perform one final high-precision transcription pass
    stop_event.set()
    
    with transcription_lock:
        audio_data = np.concatenate(audio_buffer, axis=0).flatten()
        
    try:
        segments, _ = stt_model.transcribe(audio_data, beam_size=5, vad_filter=True)
        final_text = "".join(seg.text for seg in segments).strip()
    except Exception:
        final_text = transcribed_text
        
    return audio_data, final_text

def experimental_streaming_main():
    """Experimental continuous streaming assistant that processes audio sliding-window style."""
    print(f"\n{BLUE}[Assistant]{NC} Starting experimental continuous streaming mode...")
    print(f"{YELLOW}[Assistant]{NC} Speak commands naturally. No need to pause or wait.")
    
    speak("Experimental continuous streaming activated. I am listening.")
    
    # Bounded queue of raw PCM samples (float32, 16000Hz)
    audio_queue = collections.deque()
    queue_lock = threading.Lock()
    
    # Store locked-in context text to pass as initial_prompt
    locked_text_buffer = []
    
    # Callback stream handler that continuously feeds the queue
    def streaming_callback(indata, frames, time_info, status):
        with queue_lock:
            # Accumulate audio samples
            audio_queue.extend(indata.copy().flatten())
            # Hard limit buffer to max 10 seconds of raw audio (160000 samples)
            # to prevent unbounded memory growth if user says nothing or background noise is constant
            while len(audio_queue) > 160000:
                audio_queue.popleft()

    # Open continuous microphone input stream
    with sd.InputStream(samplerate=SAMPLE_RATE, channels=1, callback=streaming_callback):
        while True:
            try:
                time.sleep(0.4)  # Poll and process every 400ms
                
                with queue_lock:
                    audio_data = np.array(audio_queue, dtype=np.float32)
                    
                # Ignore if we have less than 0.6 seconds of audio
                if len(audio_data) < SAMPLE_RATE * 0.6:
                    continue
                    
                # Build context prompt from the last few locked words
                context_prompt = " ".join(locked_text_buffer[-15:])  # Limit context to last 15 words
                
                # Transcribe current queue with word timestamps
                segments, info = stt_model.transcribe(
                    audio_data, 
                    beam_size=1, 
                    word_timestamps=True,
                    initial_prompt=context_prompt
                )
                
                prune_up_to_seconds = 0.0
                newly_locked_words = []
                newly_locked_probs = []
                unconfirmed_words = []
                unconfirmed_probs = []
                collect_unconfirmed = False
                
                # A balanced threshold for high-quality semantic locking
                CONFIDENCE_THRESHOLD = 0.70
                
                for segment in segments:
                    if not segment.words:
                        continue
                    for word in segment.words:
                        w_text = word.word.strip()
                        if not w_text:
                            continue
                            
                        if DEBUG_MODE:
                            sys.stderr.write(f"[Debug] '{w_text}' | prob: {word.probability:.4f} | start: {word.start:.2f}s | end: {word.end:.2f}s | collect_unconfirmed: {collect_unconfirmed}\n")
                            sys.stderr.flush()
                            
                        if not collect_unconfirmed:
                            if word.probability >= CONFIDENCE_THRESHOLD:
                                newly_locked_words.append(w_text)
                                newly_locked_probs.append(word.probability)
                                prune_up_to_seconds = word.end
                            else:
                                # Stop locking immediately on the first low-confidence word.
                                # The queue is kept intact and not pruned before this point.
                                collect_unconfirmed = True
                                unconfirmed_words.append(w_text)
                                unconfirmed_probs.append(word.probability)
                        else:
                            unconfirmed_words.append(w_text)
                            unconfirmed_probs.append(word.probability)
                
                # 1. Print newly locked words permanently (ending in a newline) with confidence
                if newly_locked_words:
                    phrase = " ".join(newly_locked_words).strip()
                    if phrase:
                        avg_locked_conf = int(sum(newly_locked_probs) / len(newly_locked_probs) * 100)
                        # Clear active line and output permanently locked green line with confidence score
                        sys.stdout.write(f"\r\033[K{GREEN}[You] (Locked) [{avg_locked_conf}%]{NC}: {phrase}\n")
                        sys.stdout.flush()
                
                # 2. Print currently refining/unconfirmed words dynamically in-place with confidence
                full_unconfirmed = " ".join(unconfirmed_words).strip()
                if full_unconfirmed:
                    avg_refining_conf = int(sum(unconfirmed_probs) / len(unconfirmed_probs) * 100)
                    sys.stdout.write(f"\r\033[K{YELLOW}[You] (Refining) [{avg_refining_conf}%]{NC}: {full_unconfirmed}...")
                    sys.stdout.flush()
                else:
                    # Clear the refining line if there's no active refinement text
                    sys.stdout.write("\r\033[K")
                    sys.stdout.flush()
                
                if prune_up_to_seconds > 0:
                    # Crop the audio queue head up to the timestamp of the last locked word
                    samples_to_remove = int(prune_up_to_seconds * SAMPLE_RATE)
                    with queue_lock:
                        samples_to_remove = min(samples_to_remove, len(audio_queue))
                        for _ in range(samples_to_remove):
                            audio_queue.popleft()
                            
                    if newly_locked_words:
                        locked_text_buffer.extend(newly_locked_words)
                        
                        # Detect command matching on the newly updated locked buffer
                        # Gather the cumulative context text to see if a command is fully stated
                        cumulative_phrase = " ".join(locked_text_buffer[-10:]).lower()
                        
                        # 1. Voice-Activated Compilation & Verification Check
                        if any(trigger in cumulative_phrase for trigger in ["run tests", "verify build", "compile firmware"]):
                            locked_text_buffer.clear() # Clear context to prevent double triggers
                            with queue_lock:
                                audio_queue.clear()
                            execute_terminal_command("run tests")
                            
                        # 2. Voice-Activated Mesh Scanning
                        elif any(trigger in cumulative_phrase for trigger in ["mesh status", "list devices", "scan hardware"]):
                            locked_text_buffer.clear()
                            with queue_lock:
                                audio_queue.clear()
                            execute_terminal_command("mesh status")
                            
                        # 3. Exit command
                        elif any(trigger in cumulative_phrase for trigger in ["exit assistant", "goodbye assistant", "quit assistant"]):
                            speak("Goodbye!")
                            return
                                
            except KeyboardInterrupt:
                speak("Goodbye!")
                return
            except Exception as e:
                print(f"{RED}[Streaming Error]{NC}: {e}")
                time.sleep(1)

def main():
    if EXPERIMENTAL_CONTINUOUS_STREAMING:
        experimental_streaming_main()
    else:
        speak("Hello! I am your local private assistant. I am listening.")
        
        while True:
            try:
                result = listen()
                if result is None:
                    continue
                audio, text = result
                if len(audio) < 16000 * 0.5:  # Ignore clips under 0.5s
                    continue
                    
                if not text:
                    print(f"{RED}[Assistant] Could not understand speech.{NC}")
                    continue
                    
                sys.stdout.write(f"\r\033[K{GREEN}[You]{NC}: {text}\n")
                sys.stdout.flush()
                
                # Exit keyword detection
                if any(word in text.lower() for word in ["exit", "goodbye", "quit"]):
                    speak("Goodbye!")
                    break
                    
                # Intercept for terminal actions or conversational replies
                execute_terminal_command(text)
                
            except KeyboardInterrupt:
                speak("Goodbye!")
                break
            except Exception as e:
                print(f"{RED}[Error]{NC}: {e}")
                time.sleep(1)

if __name__ == "__main__":
    main()
