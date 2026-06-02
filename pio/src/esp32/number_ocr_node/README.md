# Single-Digit 7-Segment OCR Node

This subproject isolates the on-device single-digit OCR detector firmware and its training utilities. It is designed to recognize 7-segment LED display counter numbers (0-9) and empty displays (no-digit background / class 10) directly on the ESP32-CAM using a custom TensorFlow Lite Micro CNN model.

## Core Responsibility
- **On-Device Vision Pipeline**: Performs dynamic background estimation, border exclusion (to ignore bezel noise), active bounding-box isolation, and bilinear interpolation to resize the digit to a $48 \times 48$ normalized input.
- **TinyML 7-Segment CNN**: Runs local inference on the $48 \times 48$ input using a strided 2-layer CNN model (~35.7KB, pure Float32) trained on augmented 7-segment display variations (varying slants, aspect ratios, line thicknesses, and background noise).
- **VFS Mesh Integration**: Publishes raw diagnostic frames ($96 \times 96$ pixels) on `sensor/camera` and preprocessed frames ($48 \times 48$ pixels) on `sensor/camera/preprocessed` as binary streams, and digit classification output on `sensor/vision/digits` to mesh peers.

## Component Files
- **[main.cpp](file:///home/brian/github/jotcad/pio/src/esp32/number_ocr_node/main.cpp)**: C++ source for the ESP32-CAM firmware, including OV2640 configuration, dynamic bounding-box preprocessing, TFLite Micro interpreter, and WebSocket VFS networking.
- **[model_data.h](file:///home/brian/github/jotcad/pio/src/esp32/number_ocr_node/model_data.h)**: Hexadecimal C++ array containing the unquantized Float32 neural network model weights (~35.7KB).
- **[train_esp32cam_number_ocr.py](file:///home/brian/github/jotcad/pio/train_esp32cam_number_ocr.py)**: Python script to train the 11-class CNN on programmatically synthesized 7-segment digits and background noise.
