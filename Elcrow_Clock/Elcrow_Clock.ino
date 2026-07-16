/*
 * Morphing Typography Clock for Elecrow CrowPanel 7.0-inch ESP32-S3 HMI Display
 * Uses LovyanGFX with a PSRAM double-buffer Sprite for 60FPS flicker-free animations.
 */

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <driver/i2c.h>
#include <WiFi.h>
#include <time.h>
#include <math.h>

// ==========================================
// WIFI & TIMEZONE CONFIGURATION
// ==========================================
const char* ssid     = "YOUR_WIFI_SSID";      // Enter your Wi-Fi SSID
const char* password = "YOUR_WIFI_PASSWORD";  // Enter your Wi-Fi Password

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600;           // Timezone offset in seconds (e.g. -21600 for CST)
const int   daylightOffset_sec = 3600;        // Daylight saving offset in seconds (3600 if active, 0 if not)

// ==========================================
// LGFX DISPLAY DEVICE CLASS (ESP32-8048S070)
// ==========================================
class LGFX : public lgfx::LGFX_Device
{
public:
  lgfx::Bus_RGB     _bus_instance;
  lgfx::Panel_RGB   _panel_instance;
  lgfx::Light_PWM   _light_instance;
  lgfx::Touch_GT911 _touch_instance;

  LGFX(void)
  {
    {
      auto cfg = _panel_instance.config();
      cfg.memory_width  = 800;
      cfg.memory_height = 480;
      cfg.panel_width   = 800;
      cfg.panel_height  = 480;
      cfg.offset_x      = 0;
      cfg.offset_y      = 0;
      _panel_instance.config(cfg);
    }

    {
      auto cfg = _panel_instance.config_detail();
      cfg.use_psram = 1; // Enable PSRAM framebuffer support
      _panel_instance.config_detail(cfg);
    }

    {
      auto cfg = _bus_instance.config();
      cfg.panel = &_panel_instance;
      cfg.pin_d0  = GPIO_NUM_15;  // B0
      cfg.pin_d1  = GPIO_NUM_7;   // B1
      cfg.pin_d2  = GPIO_NUM_6;   // B2
      cfg.pin_d3  = GPIO_NUM_5;   // B3
      cfg.pin_d4  = GPIO_NUM_4;   // B4
      cfg.pin_d5  = GPIO_NUM_9;   // G0
      cfg.pin_d6  = GPIO_NUM_46;  // G1
      cfg.pin_d7  = GPIO_NUM_3;   // G2
      cfg.pin_d8  = GPIO_NUM_8;   // G3
      cfg.pin_d9  = GPIO_NUM_16;  // G4
      cfg.pin_d10 = GPIO_NUM_1;   // G5
      cfg.pin_d11 = GPIO_NUM_14;  // R0
      cfg.pin_d12 = GPIO_NUM_21;  // R1
      cfg.pin_d13 = GPIO_NUM_47;  // R2
      cfg.pin_d14 = GPIO_NUM_48;  // R3
      cfg.pin_d15 = GPIO_NUM_45;  // R4

      cfg.pin_henable = GPIO_NUM_41;
      cfg.pin_vsync   = GPIO_NUM_40;
      cfg.pin_hsync   = GPIO_NUM_39;
      cfg.pin_pclk    = GPIO_NUM_0;
      cfg.freq_write  = 12000000;

      cfg.hsync_polarity    = 0;
      cfg.hsync_front_porch = 40;
      cfg.hsync_pulse_width = 48;
      cfg.hsync_back_porch  = 40;
      cfg.vsync_polarity    = 0;
      cfg.vsync_front_porch = 1;
      cfg.vsync_pulse_width = 31;
      cfg.vsync_back_porch  = 13;
      cfg.pclk_active_neg   = 1;
      cfg.de_idle_high      = 0;
      cfg.pclk_idle_high    = 0;
      _bus_instance.config(cfg);
    }
    _panel_instance.setBus(&_bus_instance);

    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = GPIO_NUM_2;
      _light_instance.config(cfg);
    }
    _panel_instance.light(&_light_instance);

    {
      auto cfg = _touch_instance.config();
      cfg.x_min      = 0;
      cfg.x_max      = 800;
      cfg.y_min      = 0;
      cfg.y_max      = 480;
      cfg.pin_int    = GPIO_NUM_NC;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.i2c_port   = I2C_NUM_1;
      cfg.pin_sda    = GPIO_NUM_19;
      cfg.pin_scl    = GPIO_NUM_20;
      cfg.pin_rst    = GPIO_NUM_38;
      cfg.freq       = 400000;
      cfg.i2c_addr   = 0x14;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};

LGFX lcd;
LGFX_Sprite canvas(&lcd);

// ==========================================
// GEOMETRIC BEZIER CURVE DEFINITIONS (0-9)
// ==========================================
// Each digit consists of exactly 13 points:
// Point 0: Start point (M)
// Points 1-3: Curve 1 control points and endpoint (C)
// Points 4-6: Curve 2 control points and endpoint (C)
// Points 7-9: Curve 3 control points and endpoint (C)
// Points 10-12: Curve 4 control points and endpoint (C)
const float digitPaths[10][13][2] = {
  // 0: Clean geometric pill
  { {50, 20}, {15, 20}, {15, 60}, {15, 100}, {15, 140}, {15, 180}, {50, 180}, {85, 180}, {85, 140}, {85, 100}, {85, 60}, {85, 20}, {50, 20} },
  // 1: Sharp line with a tiny hook
  { {35, 40}, {50, 20}, {50, 20}, {50, 20}, {50, 75}, {50, 125}, {50, 180}, {50, 180}, {50, 180}, {50, 180}, {50, 180}, {50, 180}, {50, 180} },
  // 2: Round top bow, angled stem, straight base
  { {20, 60}, {20, 15}, {80, 15}, {80, 60}, {80, 105}, {50, 135}, {20, 180}, {45, 180}, {65, 180}, {85, 180}, {85, 180}, {85, 180}, {85, 180} },
  // 3: Twin proportional round bows
  { {25, 45}, {25, 15}, {75, 15}, {75, 50}, {75, 80}, {50, 95}, {45, 100}, {60, 105}, {85, 120}, {80, 150}, {75, 190}, {25, 185}, {25, 155} },
  // 4: Clean diagonal, sharp crossbar, straight stem
  { {70, 180}, {70, 120}, {70, 60}, {70, 20}, {40, 60}, {20, 90}, {20, 130}, {40, 130}, {70, 130}, {90, 130}, {90, 130}, {90, 130}, {90, 130} },
  // 5: Straight top, straight spine, circular bottom bowl
  { {75, 25}, {50, 25}, {25, 25}, {25, 25}, {25, 50}, {25, 85}, {25, 85}, {55, 70}, {85, 90}, {80, 140}, {75, 190}, {25, 180}, {25, 150} },
  // 6: Sweeping arc into a perfect circle
  { {75, 35}, {50, 15}, {20, 40}, {20, 100}, {20, 180}, {80, 180}, {80, 140}, {80, 100}, {20, 100}, {20, 140}, {20, 140}, {20, 140}, {20, 140} },
  // 7: Straight horizontal bar, smooth angled stem
  { {20, 25}, {45, 25}, {65, 25}, {85, 25}, {75, 75}, {55, 125}, {40, 180}, {40, 180}, {40, 180}, {40, 180}, {40, 180}, {40, 180}, {40, 180} },
  // 8: Twin perfectly balanced circular loops
  { {50, 100}, {85, 90}, {85, 20}, {50, 20}, {15, 20}, {15, 90}, {50, 100}, {85, 110}, {85, 180}, {50, 180}, {15, 180}, {15, 110}, {50, 100} },
  // 9: Perfect circular top loop, sweeping tail
  { {80, 100}, {80, 20}, {20, 20}, {20, 60}, {20, 100}, {80, 100}, {80, 60}, {80, 130}, {70, 180}, {30, 165}, {30, 165}, {30, 165}, {30, 165} }
};

// ==========================================
// LAYOUT & STATE TRACKING
// ==========================================
struct DigitTracker {
  int currentDigit = -1;
  int targetDigit = -1;
  uint32_t transitionStartTime = 0;
  bool isTransitioning = false;
  float interpolatedPoints[13][2];
};

DigitTracker digits[6]; // h1, h2, m1, m2, s1, s2
bool wifiConnected = false;

// Layout Scaling parameters (maps SVG viewBox to Screen)
const float scale = 1.15f;
const float offsetX = 10.0f;
const float offsetY = 5.0f;

// Color definitions matching the original CSS values
const uint16_t hourColor   = lcd.color565(251, 191, 36);  // #fbbf24 (Amber)
const uint16_t minColor    = lcd.color565(244, 63, 94);   // #f43f5e (Rose)
const uint16_t secColor    = lcd.color565(56, 189, 248);  // #38bdf8 (Sky Blue)
const uint16_t bgColor     = lcd.color565(0, 0, 0);      // #000000 (Pure Black)

// Glow colors (dimmer/darker versions of main colors)
const uint16_t hourGlow    = lcd.color565(251/4, 191/4, 36/4);
const uint16_t minGlow     = lcd.color565(244/4, 63/4, 94/4);
const uint16_t secGlow     = lcd.color565(56/4, 189/4, 248/4);

// ==========================================
// MATH HELPERS
// ==========================================
// Standard cubic ease-in-out curve
float ease(float t) {
  return t < 0.5f ? 4.0f * t * t * t : 1.0f - pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

// Transforms local SVG coordinates to final scaled screen coordinates
void getScreenPoint(float localX, float localY, float groupX, float groupY, float& screenX, float& screenY) {
  float globalX = localX + groupX;
  float globalY = localY + groupY;
  screenX = globalX * scale + offsetX;
  screenY = globalY * scale + offsetY;
}

// ==========================================
// CORE DRAWING ROUTINES
// ==========================================
// Draws a thick line using parallel native lines (much faster than software drawWideLine)
void drawThickLine(LGFX_Sprite& canvas, int32_t x0, int32_t y0, int32_t x1, int32_t y1, float thickness, uint16_t color) {
  int r = (int)(thickness / 2.0f);
  if (r < 1) r = 1;
  int dx = abs(x1 - x0);
  int dy = abs(y1 - y0);
  if (dx > dy) {
    for (int i = -r; i <= r; i++) {
      canvas.drawLine(x0, y0 + i, x1, y1 + i, color);
    }
  } else {
    for (int i = -r; i <= r; i++) {
      canvas.drawLine(x0 + i, y0, x1 + i, y1, color);
    }
  }
}

// Draws a cubic Bezier curve as a series of thick lines
void drawCubicBezier(LGFX_Sprite& canvas, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float radius, uint16_t color) {
  const int segments = 16;
  const float dt = 1.0f / segments;
  
  float prevX = x0;
  float prevY = y0;
  
  for (int i = 1; i <= segments; i++) {
    float t = i * dt;
    float t_1 = 1.0f - t;
    
    float c0 = t_1 * t_1 * t_1;
    float c1 = 3.0f * t_1 * t_1 * t;
    float c2 = 3.0f * t_1 * t * t;
    float c3 = t * t * t;
    
    float currX = c0 * x0 + c1 * x1 + c2 * x2 + c3 * x3;
    float currY = c0 * y0 + c1 * y1 + c2 * y2 + c3 * y3;
    
    drawThickLine(canvas, (int32_t)prevX, (int32_t)prevY, (int32_t)currX, (int32_t)currY, radius, color);
    
    prevX = currX;
    prevY = currY;
  }
}

// Draws a digit with neon glow effect (thick dark line under thin bright line)
void drawDigit(LGFX_Sprite& canvas, DigitTracker& tracker, float groupX, float groupY, uint16_t glowColor, uint16_t strokeColor) {
  // Pass 1: Draw neon shadow glow (radius = 11.0f)
  for (int seg = 0; seg < 4; seg++) {
    int i = seg * 3;
    float sx0, sy0, sx1, sy1, sx2, sy2, sx3, sy3;
    getScreenPoint(tracker.interpolatedPoints[i][0],   tracker.interpolatedPoints[i][1],   groupX, groupY, sx0, sy0);
    getScreenPoint(tracker.interpolatedPoints[i+1][0], tracker.interpolatedPoints[i+1][1], groupX, groupY, sx1, sy1);
    getScreenPoint(tracker.interpolatedPoints[i+2][0], tracker.interpolatedPoints[i+2][1], groupX, groupY, sx2, sy2);
    getScreenPoint(tracker.interpolatedPoints[i+3][0], tracker.interpolatedPoints[i+3][1], groupX, groupY, sx3, sy3);
    
    drawCubicBezier(canvas, sx0, sy0, sx1, sy1, sx2, sy2, sx3, sy3, 11.0f, glowColor);
  }
  
  // Pass 2: Draw bright inner core (radius = 6.0f)
  for (int seg = 0; seg < 4; seg++) {
    int i = seg * 3;
    float sx0, sy0, sx1, sy1, sx2, sy2, sx3, sy3;
    getScreenPoint(tracker.interpolatedPoints[i][0],   tracker.interpolatedPoints[i][1],   groupX, groupY, sx0, sy0);
    getScreenPoint(tracker.interpolatedPoints[i+1][0], tracker.interpolatedPoints[i+1][1], groupX, groupY, sx1, sy1);
    getScreenPoint(tracker.interpolatedPoints[i+2][0], tracker.interpolatedPoints[i+2][1], groupX, groupY, sx2, sy2);
    getScreenPoint(tracker.interpolatedPoints[i+3][0], tracker.interpolatedPoints[i+3][1], groupX, groupY, sx3, sy3);
    
    drawCubicBezier(canvas, sx0, sy0, sx1, sy1, sx2, sy2, sx3, sy3, 6.0f, strokeColor);
  }
}

// Triggers the state morph transition if digit changes
void updateTarget(DigitTracker& tracker, int nextVal) {
  if (tracker.targetDigit == -1) {
    // Force direct initialization on boot (start with 0s then morph to current time)
    tracker.currentDigit = 0;
    tracker.targetDigit = nextVal;
    tracker.transitionStartTime = millis();
    tracker.isTransitioning = true;
  } else if (tracker.targetDigit != nextVal) {
    // Initiate morph animation
    tracker.currentDigit = tracker.targetDigit;
    tracker.targetDigit = nextVal;
    tracker.transitionStartTime = millis();
    tracker.isTransitioning = true;
  }
}

// Computes the linear interpolation points based on easing
void updateInterpolation(DigitTracker& tracker) {
  if (tracker.isTransitioning) {
    float t = (float)(millis() - tracker.transitionStartTime) / 600.0f; // 0.6s duration
    if (t >= 1.0f) {
      t = 1.0f;
      tracker.isTransitioning = false;
      tracker.currentDigit = tracker.targetDigit;
    }
    float e = ease(t);
    for (int i = 0; i < 13; i++) {
      tracker.interpolatedPoints[i][0] = digitPaths[tracker.currentDigit][i][0] + 
        (digitPaths[tracker.targetDigit][i][0] - digitPaths[tracker.currentDigit][i][0]) * e;
      tracker.interpolatedPoints[i][1] = digitPaths[tracker.currentDigit][i][1] + 
        (digitPaths[tracker.targetDigit][i][1] - digitPaths[tracker.currentDigit][i][1]) * e;
    }
  } else {
    // Set static target coordinates
    for (int i = 0; i < 13; i++) {
      tracker.interpolatedPoints[i][0] = digitPaths[tracker.targetDigit][i][0];
      tracker.interpolatedPoints[i][1] = digitPaths[tracker.targetDigit][i][1];
    }
  }
}

// Helper to blend color values dynamically for pulsing effect
uint16_t getPulsedColor(uint16_t mainCol, float opacity) {
  uint8_t r = ((mainCol >> 11) & 0x1F) << 3;
  uint8_t g = ((mainCol >> 5) & 0x3F) << 2;
  uint8_t b = (mainCol & 0x1F) << 3;
  
  r = (uint8_t)(r * opacity);
  g = (uint8_t)(g * opacity);
  b = (uint8_t)(b * opacity);
  
  return lcd.color565(r, g, b);
}

// ==========================================
// SETUP & INITIALIZATION
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(2000); // Give serial monitor time to connect
  Serial.println("=========================================");
  Serial.printf("PSRAM size: %d bytes\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
  Serial.printf("Free Heap:  %d bytes\n", ESP.getFreeHeap());
  Serial.println("=========================================");

  // Initialize display
  lcd.init();
  lcd.setRotation(0); // Standard landscape orientation
  lcd.setBrightness(255); // Turn on screen backlight
  lcd.fillScreen(bgColor); // Clear physical LCD once
  
  // Create double buffer sprite in internal SRAM (8-bit color depth, 800x240)
  canvas.setPsram(false);
  canvas.setColorDepth(8);
  if (!canvas.createSprite(800, 240)) {
    Serial.println("SRAM Sprite Allocation Failed! Resetting...");
    ESP.restart();
  }
  
  // Show connection status screen
  canvas.fillSprite(bgColor);
  canvas.setTextColor(hourColor);
  canvas.setTextSize(2);
  canvas.setTextDatum(textdatum_t::middle_center);
  canvas.drawString("Connecting to Wi-Fi...", 400, 120);
  canvas.pushSprite(0, 120);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 60) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    wifiConnected = true;
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  } else {
    Serial.println("\nWiFi Connection Failed. Using offline clock mode.");
  }
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  int h1_val, h2_val, m1_val, m2_val, s1_val, s2_val;
  
  // Obtain current time values
  if (wifiConnected && timeinfo.tm_year > 70) {
    int hr = timeinfo.tm_hour % 12;
    hr = hr ? hr : 12; // Format 12-hour
    h1_val = hr / 10;
    h2_val = hr % 10;
    m1_val = timeinfo.tm_min / 10;
    m2_val = timeinfo.tm_min % 10;
    s1_val = timeinfo.tm_sec / 10;
    s2_val = timeinfo.tm_sec % 10;
  } else {
    // Local offline clock backup based on ESP system uptime
    unsigned long total_secs = millis() / 1000;
    int hr = (total_secs / 3600 + 12) % 12;
    hr = hr ? hr : 12;
    h1_val = hr / 10;
    h2_val = hr % 10;
    int minutes = (total_secs % 3600) / 60;
    m1_val = minutes / 10;
    m2_val = minutes % 10;
    int seconds = total_secs % 60;
    s1_val = seconds / 10;
    s2_val = seconds % 10;
  }

  // Update target digits
  updateTarget(digits[0], h1_val);
  updateTarget(digits[1], h2_val);
  updateTarget(digits[2], m1_val);
  updateTarget(digits[3], m2_val);
  updateTarget(digits[4], s1_val);
  updateTarget(digits[5], s2_val);

  // Compute interpolation paths
  for (int i = 0; i < 6; i++) {
    updateInterpolation(digits[i]);
  }

  // Clear frame buffer with deep background color
  canvas.fillSprite(bgColor);

  // 1. Draw Hour Digits (Amber)
  drawDigit(canvas, digits[0], 10,  20, hourGlow, hourColor);
  drawDigit(canvas, digits[1], 110, 20, hourGlow, hourColor);
 
  // 2. Draw Minute Digits (Rose)
  drawDigit(canvas, digits[2], 245, 20, minGlow, minColor);
  drawDigit(canvas, digits[3], 345, 20, minGlow, minColor);
 
  // 3. Draw Second Digits (Sky Blue)
  drawDigit(canvas, digits[4], 480, 20, secGlow, secColor);
  drawDigit(canvas, digits[5], 580, 20, secGlow, secColor);
 
  // 4. Draw Pulsing Colons
  struct timeval tv;
  gettimeofday(&tv, NULL);
  long ms = tv.tv_usec / 1000;
  
  float pulseOpacity = 0.2f + 0.8f * fabsf(sinf(((float)ms / 1000.0f) * M_PI));
  
  uint16_t pulsedHourColor = getPulsedColor(hourColor, pulseOpacity);
  uint16_t pulsedMinColor  = getPulsedColor(minColor,  pulseOpacity);
  
  // Transform colon coordinates (Radius = 9.0f)
  float cx, cy;
  // Colon 1 (between Hours and Minutes)
  getScreenPoint(10, 60, 215, 20, cx, cy);
  canvas.fillCircle(cx, cy, 9.0f, pulsedHourColor);
  getScreenPoint(10, 140, 215, 20, cx, cy);
  canvas.fillCircle(cx, cy, 9.0f, pulsedHourColor);
  
  // Colon 2 (between Minutes and Seconds)
  getScreenPoint(10, 60, 450, 20, cx, cy);
  canvas.fillCircle(cx, cy, 9.0f, pulsedMinColor);
  getScreenPoint(10, 140, 450, 20, cx, cy);
  canvas.fillCircle(cx, cy, 9.0f, pulsedMinColor);
 
  // Draw WiFi Connection Status Indicator (Tiny dot in bottom right corner of sprite)
  if (wifiConnected) {
    canvas.fillCircle(780, 220, 4, secColor);
  } else {
    canvas.fillCircle(780, 220, 4, minColor);
  }
 
  // Push new frame to the physical LCD (centered vertically)
  canvas.pushSprite(0, 120);
 
  // Small delay to yield processing time to system background tasks (WiFi, etc.)
  delay(10);
}
