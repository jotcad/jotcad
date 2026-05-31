# Standalone T-Display LCD Demo

This directory contains the firmware for the **T-Display LCD Demo** (`esp32_tdisplay_demo`), a standalone demonstration designed specifically for the **Tenstar T-Display ESP32 board** (LilyGO T-Display clone).

This target demonstrates full control over the built-in **1.14" IPS LCD screen (driven by ST7789V)**, the two onboard physical side buttons, and scrolling USB-serial communications completely independently of any other tasks.

---

## 1. Hardware Pin Configurations

This board connects the ST7789 display over high-speed hardware SPI. Button inputs are wired with pull-up configurations:

*   **TFT MOSI (Data)**: **GPIO 19**
*   **TFT SCLK (Clock)**: **GPIO 18**
*   **TFT CS (Chip Select)**: **GPIO 5**
*   **TFT DC (Data/Command)**: **GPIO 16**
*   **TFT RST (Reset)**: **GPIO 23**
*   **TFT BL (Backlight Enable)**: **GPIO 4** (Needs to be written `HIGH` to enable display lighting)
*   **Button 1**: **GPIO 35** (Boot Button, Active `LOW` when pressed)
*   **Button 2**: **GPIO 0** (Active `LOW` when pressed)

---

## 2. Dynamic Features

This demo implements a high-refresh dark-mode visual interface:
1.  **System Diagnostics**: Displays live uptime count and free heap RAM (`ESP.getFreeHeap()`) in real-time.
2.  **Dual Button Telemetry**: Senses side buttons and visually highlights them on-screen in vibrant green boxes when pressed.
3.  **Real-Time Sine Wave Graph**: Draws an animating trigonometric sine wave at the bottom at a smooth 60 FPS.
4.  **Scrolling Serial Terminal**: Listens at 115200 baud. Anything sent from your computer's keyboard to the USB serial port will automatically scroll and print on the screen's console section.

---

## 3. How to Flash and Monitor

We provide smart helper scripts that automatically query PlatformIO's device list, identify the physical USB port of the T-Display board's **CH9102** bridge chip, and bypass motherboard legacy ports (`/dev/ttyS*`).

### Smart Compilation & Flashing
```bash
./pio/flash_esp32_tdisplay_demo.sh
```
*   **Auto-Detection**: If only one USB-serial board is connected, it locks onto it directly. If multiple boards are active, it prompts you with a clean numbered menu to choose the target.
*   **Manual Override**: Pass a direct port argument to skip detection completely:
    ```bash
    ./pio/flash_esp32_tdisplay_demo.sh /dev/ttyUSB1
    ```

### Smart Serial Monitoring
```bash
./pio/monitor_esp32_tdisplay_demo.sh
```
*   Integrates the exact same auto-detection and manual overrides at 115200 baud.
*   Type characters and press **Enter** to watch your text scroll on the T-Display LCD Screen in real-time!
