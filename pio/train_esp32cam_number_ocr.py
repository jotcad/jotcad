#!/usr/bin/env python3
"""
Local TinyML Training Pipeline for Single-Digit 7-Segment LED + No-Digit ESP32-CAM OCR.
Trains a small, fast 2D CNN on a 48x48 input shape using:
 1. Programmatically synthesized 7-segment display digits (Classes 0-9)
 2. Synthesized empty / noise / non-digit frames (Class 10 - No-Digit)
Automatically exports it as an unquantized pure Float32 C++ header (model_data.h).
"""
import os
import sys

# Set paths to point into the correct source folder
OUTPUT_DIR = os.path.dirname(os.path.abspath(__file__))
HEADER_PATH = os.path.join(OUTPUT_DIR, "src", "esp32", "number_ocr_node", "model_data.h")

print("==========================================================================")
print("     JotCAD 11-CLASS TinyML 7-SEGMENT ONLY OCR TRAINING PIPELINE          ")
print("==========================================================================")

try:
    import numpy as np
    import tensorflow as tf
    from tensorflow.keras import layers, models
    from PIL import Image, ImageDraw
    
    # Enforce float32 globally
    tf.keras.mixed_precision.set_global_policy('float32')
except ImportError:
    print("[Error] TensorFlow, NumPy, and Pillow are required to run this training pipeline.")
    print("Please install them inside your Python environment:")
    print("  pip install tensorflow numpy pillow")
    sys.exit(1)

# Segment mapping for 7-segment LED display
SEGMENT_MAP = {
    0: ['a', 'b', 'c', 'd', 'e', 'f'],
    1: ['b', 'c'],
    2: ['a', 'b', 'g', 'e', 'd'],
    3: ['a', 'b', 'g', 'c', 'd'],
    4: ['f', 'g', 'b', 'c'],
    5: ['a', 'f', 'g', 'c', 'd'],
    6: ['a', 'f', 'g', 'e', 'c', 'd'],
    7: ['a', 'b', 'c'],
    8: ['a', 'b', 'c', 'd', 'e', 'f', 'g'],
    9: ['a', 'b', 'c', 'd', 'f', 'g']
}

def draw_segment(draw, seg, width, height, thick, gap, active):
    x_min, x_max = gap + thick, width - gap - thick
    x_mid = width // 2
    y_min, y_max = gap + thick, height - gap - thick
    y_mid = height // 2
    
    color = 255 if active else 0
    
    if seg == 'a':
         draw.line([(x_min + gap, y_min), (x_max - gap, y_min)], fill=color, width=thick)
    elif seg == 'd':
         draw.line([(x_min + gap, y_max), (x_max - gap, y_max)], fill=color, width=thick)
    elif seg == 'g':
         draw.line([(x_min + gap, y_mid), (x_max - gap, y_mid)], fill=color, width=thick)
    elif seg == 'f':
         draw.line([(x_min, y_min + gap), (x_min, y_mid - gap)], fill=color, width=thick)
    elif seg == 'e':
         draw.line([(x_min, y_mid + gap), (x_min, y_max - gap)], fill=color, width=thick)
    elif seg == 'b':
         draw.line([(x_max, y_min + gap), (x_max, y_mid - gap)], fill=color, width=thick)
    elif seg == 'c':
         draw.line([(x_max, y_mid + gap), (x_max, y_max - gap)], fill=color, width=thick)

def generate_7seg_digit(digit, slant=0.1, thickness=3, gap=2, width=32, height=40):
    img_temp = Image.new('L', (width, height), color=0)
    draw = ImageDraw.Draw(img_temp)
    
    active_segs = SEGMENT_MAP[digit]
    for seg in ['a', 'b', 'c', 'd', 'e', 'f', 'g']:
        draw_segment(draw, seg, width, height, thickness, gap, seg in active_segs)
        
    if slant != 0:
        img_temp = img_temp.transform((width, height), Image.AFFINE, (1, slant, 0, 0, 1, 0), Image.BILINEAR)
        
    # Center the temp digit on a 48x48 black canvas
    img = Image.new('L', (48, 48), color=0)
    offset_x = (48 - width) // 2
    offset_y = (48 - height) // 2
    img.paste(img_temp, (offset_x, offset_y))
    
    return np.array(img, dtype=np.float32) / 255.0

# 1. Synthesize 7-Segment LED display digits (Classes 0-9)
print("\n[1/5] Synthesizing 7-segment display digits...")
x_synth = []
y_synth = []

slants = [-0.15, -0.10, -0.05, 0.0, 0.05, 0.10, 0.15]
thicknesses = [2, 3, 4, 5]
gaps = [1, 2, 3]
widths = [20, 24, 28, 32, 36]
heights = [32, 36, 40, 44]

for digit in range(10):
    for slant in slants:
        for thick in thicknesses:
            for gap in gaps:
                for w in widths:
                    for h in heights:
                        img = generate_7seg_digit(digit, slant, thick, gap, w, h)
                        
                        # Generate background noise (lines/scratches)
                        img_noise = np.zeros_like(img)
                        if np.random.rand() > 0.4:
                            draw_img = Image.new('L', (48, 48), color=0)
                            draw = ImageDraw.Draw(draw_img)
                            for _ in range(np.random.randint(1, 3)):
                                draw.line([(np.random.randint(0, 48), np.random.randint(0, 48)), 
                                           (np.random.randint(0, 48), np.random.randint(0, 48))], 
                                          fill=np.random.randint(50, 180), width=np.random.randint(1, 3))
                            img_noise = np.array(draw_img, dtype=np.float32) / 255.0
                        
                        # Mask out digit to keep it clean (no overlap)
                        digit_mask = (img > 0.05).astype(np.float32)
                        clean_noise = img_noise * (1.0 - digit_mask)
                        
                        # Combine
                        img_combined = np.clip(img + clean_noise, 0.0, 1.0)
                        
                        # Add random translations (shift)
                        dx = np.random.randint(-3, 4)
                        dy = np.random.randint(-3, 4)
                        img_shift = np.zeros_like(img_combined)
                        if dx >= 0:
                            x_src_start, x_src_end = 0, 48 - dx
                            x_dst_start, x_dst_end = dx, 48
                        else:
                            x_src_start, x_src_end = -dx, 48
                            x_dst_start, x_dst_end = 0, 48 + dx
                        if dy >= 0:
                            y_src_start, y_src_end = 0, 48 - dy
                            y_dst_start, y_dst_end = dy, 48
                        else:
                            y_src_start, y_src_end = -dy, 48
                            y_dst_start, y_dst_end = 0, 48 + dy
                        
                        img_shift[y_dst_start:y_dst_end, x_dst_start:x_dst_end] = img_combined[y_src_start:y_src_end, x_src_start:x_src_end]
                        
                        # Add Gaussian noise
                        noise = np.random.normal(0, 0.04, img_shift.shape)
                        img_shift = np.clip(img_shift + noise, 0.0, 1.0)
                        
                        x_synth.append(img_shift.reshape(48, 48, 1))
                        y_synth.append(digit)

x_synth = np.array(x_synth, dtype=np.float32)
y_synth = np.array(y_synth, dtype=np.int32)

# 2. Synthesize Class 10 ("No-Digit / Background")
print("Synthesizing class 10 (No-Digit / Background) empty and noise-only frames...")
x_nodigit = []
y_nodigit = []
for duplicate in range(6000):
    canvas = np.zeros((48, 48, 1), dtype=np.float32)
    # Add background noise
    if np.random.rand() > 0.4:
        noise = np.random.normal(0, np.random.uniform(0.01, 0.08), canvas.shape)
        canvas = np.clip(canvas + noise, 0, 1)
    if np.random.rand() > 0.7:
        draw_img = Image.fromarray((canvas.reshape(48, 48) * 255).astype(np.uint8), mode='L')
        draw = ImageDraw.Draw(draw_img)
        # Draw random non-digit lines/scratches
        draw.line([(np.random.randint(0, 48), np.random.randint(0, 48)), 
                   (np.random.randint(0, 48), np.random.randint(0, 48))], fill=255, width=np.random.randint(1, 4))
        canvas = np.array(draw_img, dtype=np.float32).reshape(48, 48, 1) / 255.0
        
    x_nodigit.append(canvas)
    y_nodigit.append(10)

x_nodigit = np.array(x_nodigit, dtype=np.float32)
y_nodigit = np.array(y_nodigit, dtype=np.int32)

# Blend and shuffle
x_all = np.concatenate([x_synth, x_nodigit], axis=0)
y_all = np.concatenate([y_synth, y_nodigit], axis=0)

indices = np.arange(len(x_all))
np.random.shuffle(indices)
x_all = x_all[indices]
y_all = y_all[indices]

print(f"Dataset blended: {len(x_all)} images total ({len(x_synth)} digits, {len(x_nodigit)} empty/noise background frames).")

# 3. Build the 11-Class Sequential CNN
print("\n[2/5] Compiling 11-class CNN with strided Conv2D for 48x48 input...")
model = models.Sequential([
    layers.Input(shape=(48, 48, 1), name="image_input"),
    # Stride of (2,2) reduces spatial size to 22x22 immediately, saving activation memory
    layers.Conv2D(8, (5, 5), strides=(2, 2), activation='relu'), 
    layers.MaxPooling2D((2, 2)), # Output: 11x11x8
    layers.Conv2D(16, (3, 3), activation='relu'), # Output: 9x9x16
    layers.MaxPooling2D((2, 2)), # Output: 4x4x16
    layers.Flatten(), # Output: 256
    layers.Dense(24, activation='relu'),
    layers.Dense(11, activation='softmax', name='digit_output')
])

model.compile(
    optimizer=tf.keras.optimizers.Adam(learning_rate=0.001),
    loss='sparse_categorical_crossentropy',
    metrics=['accuracy']
)

model.summary()

# 4. Train the Model
print("\n[3/5] Training model on 11-class dataset...")
model.fit(
    x_all,
    y_all,
    epochs=10,
    batch_size=128,
    validation_split=0.1
)

# 5. Convert to TensorFlow Lite and Export to C++ Header
print("\n[4/5] Exporting TFLite float32 model...")
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()

print(f"TFLite Model Size: {len(tflite_model)} bytes")

# Write the model into model_data.h
print(f"\n[5/5] Generating C++ TinyML header: {HEADER_PATH}")
with open(HEADER_PATH, "w") as f:
    f.write("// JotCAD On-Device 7-Segment LED Single-Digit + No-Digit OCR Classifier Model Header.\n")
    f.write("// Generated automatically by train_esp32cam_number_ocr.py.\n\n")
    f.write("#ifndef MODEL_DATA_H\n")
    f.write("#define MODEL_DATA_H\n\n")
    f.write("alignas(8) const unsigned char model_data[] = {\n    ")
    
    for i, byte in enumerate(tflite_model):
        f.write(f"0x{byte:02x}, ")
        if (i + 1) % 12 == 0:
            f.write("\n    ")
            
    f.write("\n};\n\n")
    f.write(f"const unsigned int model_data_len = {len(tflite_model)};\n\n")
    f.write("#endif // MODEL_DATA_H\n")

print("==========================================================================")
print(f"{HEADER_PATH} successfully created!")
print("==========================================================================")
