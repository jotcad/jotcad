# Multi-Digit OCR Subproject Node

This subproject isolates the on-device multi-digit OCR detector firmware and its training utilities. It allows the ESP32-CAM to perform edge-level binarization, horizontal character segmentation, and sequential TensorFlow Lite Micro CNN inference to recognize multi-digit numbers from printed, handwritten, and 7-segment display sources.

## Core Responsibility
- **On-Device Vision Pipeline**: Grayscale image binarization and adaptive column-scanning horizontal segmentation.
- **Sequential TinyML Inference**: Process segmented characters using a fast Keras CNN quantized into a 9KB Flatbuffer.
- **Mesh Communication**: Publish results directly over the VFS mesh on the `sensor/vision/digits` topic.

## Component Files
- **[main.cpp](file:///home/brian/github/jotcad/pio/src/esp32/number_ocr_node/main.cpp)**: C++ source for the ESP32-CAM firmware, including camera configuration, CV segmentation, TFLite Micro runtime, and WebSocket VFS publisher.
- **[model_data.h](file:///home/brian/github/jotcad/pio/src/esp32/number_ocr_node/model_data.h)**: Standardized hexadecimal C++ array holding the quantized 8-bit universal digit classifier model.
- **[train_model.py](file:///home/brian/github/jotcad/pio/src/esp32/number_ocr_node/train_model.py)**: Python training script to generate `model_data.h` using a blended training dataset (MNIST + Font-rendered printed + 7-segment LED generators).
