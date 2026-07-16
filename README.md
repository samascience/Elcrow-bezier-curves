# Morphing Typography Clock (Elecrow CrowPanel 7.0-inch)

A beautiful, fluid, hardware-accelerated **Morphing Typography Clock** designed for the **Elecrow CrowPanel 7.0" ESP32-S3 HMI Display** (800x480 resolution). 

This project utilizes the high-performance **LovyanGFX** graphics library to draw smooth, anti-aliased digits that transition using quadratic cubic Bezier curve interpolation. The rendering runs at a highly responsive **19 FPS** by executing a double-buffered layout inside the ESP32-S3's fast internal SRAM.

---

## Hardware Requirements
* **Device:** Elecrow CrowPanel 7.0-inch ESP32 Display (ESP32-S3-WROOM-1, 8MB AP_3v3 OPI PSRAM, 4MB Flash).
* **Connection Cable:** USB Type-C data cable.
* **Power Supply:** The 7-inch display draws up to 420mA. A stable USB port or power supply is required to prevent boot failures.

---

## Software Setup

### 1. Arduino IDE Configuration
1. Open the **Arduino IDE** (version 2.x or newer recommended).
2. Go to **Settings** > **Additional Board Manager URLs** and add the Espressif ESP32 package repository:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. Go to **Board Manager** and install the **`esp32`** board package by Espressif (version `3.0.x` or newer).
4. Go to **Library Manager** and search/install the **`LovyanGFX`** library (version `1.1.x` or newer).

### 2. Required Board Settings (Critical)
When flashing the ESP32-S3 module, configure the following settings under the **Tools** menu in your Arduino IDE to match the physical hardware constraints:
* **Board:** `ESP32S3 Dev Module`
* **PSRAM:** `OPI PSRAM` (The board has 8MB OPI PSRAM which must be activated so the display controller works).
* **Flash Size:** `4MB (32Mb)`  
  > [!IMPORTANT]
  > DO NOT select 16MB. Selecting 16MB writes a header check that crashes the bootloader instantly with: `Detected size(4096k) smaller than the size in the binary image header(16384k). Probe failed.`
* **Partition Scheme:** `Huge APP (3MB No OTA/1MB SPIFFS)` (This is necessary to fit the application).

---

## Configuration & Customization

Before compilation, open `Elcrow_Clock/Elcrow_Clock.ino` and configure the following parameters:

### 1. Wi-Fi Configuration
Enter your SSID and Password:
```cpp
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```
*Note: The clock will try to connect for 30 seconds on boot. If Wi-Fi is unavailable, it automatically falls back to an offline clock mode using the internal system uptime.*

### 2. Timezone Configuration
Configure the timezone offset to match your local timezone. For example, for **Houston, TX (CST/CDT)**:
```cpp
const long  gmtOffset_sec = -21600;      // -6 hours GMT offset for CST
const int   daylightOffset_sec = 3600;   // DST offset (3600 if active, 0 if not)
```

---

## Compilation and Flashing

### Using Arduino IDE
1. Select the serial port corresponding to your ESP32-S3 HMI board (e.g., `/dev/cu.usbserial-xxx` on macOS or `COMx` on Windows).
2. Click **Verify** to compile.
3. Click **Upload** to flash.

### Using Arduino CLI (Command Line)
If you prefer a command-line setup, run these commands inside your project root:

1. **Compile:**
   ```bash
   arduino-cli compile --fqbn esp32:esp32:esp32s3:PSRAM=opi,FlashSize=4M,PartitionScheme=huge_app Elcrow_Clock
   ```
2. **Upload:**
   ```bash
   arduino-cli upload -p /dev/cu.usbserial-11410 --fqbn esp32:esp32:esp32s3:PSRAM=opi,FlashSize=4M,PartitionScheme=huge_app,UploadSpeed=115200 Elcrow_Clock
   ```

---

## Troubleshooting Guide

### 1. The Screen Only Blinks (Crash Loop)
If the screen backlight flashes black-and-white every second:
* Open the serial monitor at **115200** baud.
* If you see `spi_flash: Detected size(4096k) smaller than...`: You compiled with `FlashSize=16M` instead of `4M`. Change the setting to 4MB and re-flash.
* If you see `SRAM Sprite Allocation Failed!`: The double-buffer canvas is too large to fit in internal memory. Check that you have enabled `OPI PSRAM` in your compile parameters and that `canvas.setPsram(true)` is set if allocating a full-screen 16-bit sprite.

### 2. Screen Backlight is On, but Display is Completely Black
* This means the ESP32-S3 is running successfully but the parallel RGB bus clock signals are unaligned.
* Verify that the pixel clock (PCLK) is mapped to **`GPIO 0`** (not GPIO 42), and that the horizontal/vertical porch values match the Elecrow specifications exactly as configured in the `LGFX` class.

### 3. Digit Morphing Animation Stutters (Low Framerate)
* Make sure you are not calling any blocking functions (like `getLocalTime()`) in the loop.
* Ensure the double-buffer sprite is allocated in internal SRAM at **8-bit color depth** (`canvas.setColorDepth(8)`) rather than 16-bit to bypass PSRAM latency.
