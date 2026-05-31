#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>

// Instantiate TFT controller
TFT_eSPI tft = TFT_eSPI();

// Scrolling Console Terminal Settings
#define CONSOLE_LINES 5
#define MAX_LINE_LEN 28
char console[CONSOLE_LINES][MAX_LINE_LEN] = {0};
int console_count = 0;

// Bouncing Sine Wave Visualizer Settings
static int wave_offset = 0;
static int prev_y[240] = {0};

// Push a line into the scrolling console buffer
void add_console_line(const char* line) {
    if (console_count < CONSOLE_LINES) {
        strncpy(console[console_count], line, MAX_LINE_LEN - 1);
        console_count++;
    } else {
        // Scroll up
        for (int i = 0; i < CONSOLE_LINES - 1; i++) {
            strcpy(console[i], console[i + 1]);
        }
        strncpy(console[CONSOLE_LINES - 1], line, MAX_LINE_LEN - 1);
    }

    // Redraw console region
    tft.fillRect(10, 48, 220, 60, TFT_BLACK); // Clear console area
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setTextFont(2);
    
    for (int i = 0; i < console_count; i++) {
        tft.drawString(console[i], 12, 48 + (i * 12));
    }
}

// Initial Static UI Layout Draw
void draw_static_ui() {
    tft.fillScreen(TFT_BLACK);
    
    // Top Header Frame
    tft.fillRect(0, 0, 240, 20, tft.color565(30, 30, 40));
    tft.setTextColor(TFT_WHITE);
    tft.setTextFont(2);
    tft.drawString("JotCAD T-Display Demo", 5, 2);
    
    // Grid Lines / Bounding Boxes
    tft.drawRoundRect(5, 24, 230, 88, 4, TFT_DARKGREY); // Console box
    tft.setTextColor(TFT_GOLD, TFT_BLACK);
    tft.drawString("Console Feed (Type via Serial)", 10, 26);
    
    // Buttons status frames
    tft.drawRoundRect(5, 114, 105, 18, 2, TFT_DARKGREY);
    tft.drawRoundRect(130, 114, 105, 18, 2, TFT_DARKGREY);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("BTN1: GPIO35", 10, 115);
    tft.drawString("BTN2: GPIO0", 135, 115);
}

void setup() {
    // 1. Initialize Serial Communication at 115200 baud
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[T-DISPLAY-DEMO] Standalone ST7789 Visual Dashboard active!");
    Serial.println("[T-DISPLAY-DEMO] Type characters and hit Enter to print them on the screen!");

    // 2. Configure Buttons (Active LOW with internal Pull-up)
    pinMode(35, INPUT_PULLUP);
    pinMode(0, INPUT_PULLUP);

    // 3. Configure TFT LCD Backlight control pin
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH); // Enable TFT Screen Backlight

    // 4. Initialize LCD Screen ST7789
    tft.init();
    tft.setRotation(1); // Landscape view (240x135)
    
    draw_static_ui();
    
    // Initialize wave buffer
    for (int x = 0; x < 240; x++) {
        prev_y[x] = 125;
    }

    add_console_line("Dashboard online.");
    add_console_line("Waiting for serial input...");
}

void loop() {
    // 1. Live USB-Serial Terminal Receiver
    static char serial_in[MAX_LINE_LEN] = {0};
    static int serial_idx = 0;
    
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\r') continue; // Ignore carriage return
        if (c == '\n' || serial_idx >= MAX_LINE_LEN - 1) {
            serial_in[serial_idx] = '\0';
            if (strlen(serial_in) > 0) {
                add_console_line(serial_in);
                Serial.printf("[ECHO] Print on T-Display: '%s'\n", serial_in);
            }
            serial_idx = 0;
            memset(serial_in, 0, MAX_LINE_LEN);
        } else {
            serial_in[serial_idx++] = c;
        }
    }

    // 2. Real-Time Physical Button Status Sensing & Highlights
    static bool last_btn1 = true;
    static bool last_btn2 = true;
    bool btn1 = (digitalRead(35) == LOW);
    bool btn2 = (digitalRead(0) == LOW);

    if (btn1 != last_btn1) {
        last_btn1 = btn1;
        tft.drawRoundRect(5, 114, 105, 18, 2, btn1 ? TFT_GREEN : TFT_DARKGREY);
        tft.setTextColor(btn1 ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
        tft.drawString("BTN1: PRESS ", 10, 115);
        if (btn1) {
            add_console_line("Button 1 (GPIO 35) Pressed");
            Serial.println("[BTN] Button 1 Pressed!");
        }
    }

    if (btn2 != last_btn2) {
        last_btn2 = btn2;
        tft.drawRoundRect(130, 114, 105, 18, 2, btn2 ? TFT_GREEN : TFT_DARKGREY);
        tft.setTextColor(btn2 ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
        tft.drawString("BTN2: PRESS ", 135, 115);
        if (btn2) {
            add_console_line("Button 2 (GPIO 0) Pressed");
            Serial.println("[BTN] Button 2 Pressed!");
        }
    }

    // 3. Telemetry Monitor: Uptime & Heap
    static unsigned long last_telemetry_tick = 0;
    if (millis() - last_telemetry_tick > 1000) { // Every second
        last_telemetry_tick = millis();
        tft.setTextColor(TFT_WHITE, tft.color565(30, 30, 40));
        tft.setTextFont(2);
        
        char telemetry_str[30];
        snprintf(telemetry_str, sizeof(telemetry_str), "Up: %luy  RAM: %ukb", 
                 millis() / 1000, 
                 ESP.getFreeHeap() / 1024);
        
        // Print on top header
        tft.drawString(telemetry_str, 120, 2);
    }

    // 4. Animating Trigonometric Waveform (Bottom overlay)
    for (int x = 6; x < 234; x += 2) {
        // Erase old pixel
        tft.drawPixel(x, prev_y[x], TFT_BLACK);
        
        // Calculate new sine wave coordinate
        float angle = (x + wave_offset) * 0.08f;
        int y = 38 + (int)(sinf(angle) * 7.0f); // Centers vertically inside console feed
        
        // Draw pixel in a glowing green shade
        tft.drawPixel(x, y, tft.color565(0, 180, 50));
        prev_y[x] = y;
    }
    wave_offset += 2;

    delay(16); // Regulate frame rate smoothly to ~60 FPS
}
