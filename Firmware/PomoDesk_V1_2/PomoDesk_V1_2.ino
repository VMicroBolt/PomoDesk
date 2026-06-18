/*
  PomoDesk Firmware
  Target board/module: ESP32-S3-WROOM-1-N16R8

  Recommended Arduino IDE board settings:
  - Board: ESP32S3 Dev Module
  - CPU Frequency: 240 MHz (board setting)
  - Runtime CPU: 80 MHz (set in firmware)
  - Flash Size: 16MB
  - PSRAM: Disabled
  - Partition: 16M Flash (3MB APP / 9.9MB FATFS)

  Notes:
  - This firmware disables WiFi and Bluetooth during normal timer operation
    to reduce power usage.
  - WiFi AP, DNS captive portal, mDNS, and webserver are only enabled while
    configuration mode is active.
*/

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_bt.h>

#include "Globals.h"
#include "WebServerHandler.h"


// --- Pins ---
extern const int BTN_STATE = 1;
extern const int LED_DATA  = 48;
extern const int PWR_LED   = 18;

// --- LEDs ---
extern const int NUM_LEDS = 19;
Adafruit_NeoPixel strip(NUM_LEDS, LED_DATA, NEO_GRB + NEO_KHZ800);

// --- Preferences / Config mode ---
Preferences prefs;
bool configMode = false;

// --- Defaults ---
extern const uint32_t WORK_MS_DEFAULT     = 45UL * 60UL * 1000UL;   // 45 mins
extern const uint32_t BREAK_MS_DEFAULT    = 5UL  * 60UL * 1000UL;   //  5 mins
extern const uint32_t IDLE_LED_MS_DEFAULT = 10UL * 60UL * 1000UL;   // 10 mins

extern const uint32_t BREATH_TIME_MS = 300;       // 0.3 secs
extern const uint32_t BREATH_PHASE_MS = 4000;     //   4 secs

extern const uint32_t CONFIG_TIME_MS = 1200;      // 1.2 secs
extern const uint32_t OFF_TIME_MS    = 3000;      //   3 secs

extern const uint32_t DEFAULT_IDLE_RAW   = 0xFF5000;
extern const uint32_t DEFAULT_WORK_RAW   = 0x00FF00;
extern const uint32_t DEFAULT_BREAK_RAW  = 0xFF0000;
extern const uint32_t DEFAULT_CONFIG_RAW = 0x0078FF;
extern const uint32_t DEFAULT_BREATH_RAW = 0x0000FF;

// --- Settings ---
uint32_t stateStartMs = 0;
uint32_t workMs;
uint32_t breakMs;
uint32_t idleLedMs;
uint8_t brightness = 150;
bool idleLedsOff = false;

// --- Transitions ---
//   SET_RING,
//   RUNNING_LIGHT,
//   RUNNING_FILL,
//   RUNNING_LIGHT_CLEAR,
//   FADE
Transition transition = RUNNING_FILL;
uint16_t transitionDelayMs = 40;

// --- States ---
//   BREATH_LINEAR,
//   BREATH_CENTER_SPLIT
BreathingStyle breathingStyle = BREATH_CENTER_SPLIT;
State state = IDLE;

// --- Colors ---
uint32_t COLOR_IDLE;
uint32_t COLOR_WORK;
uint32_t COLOR_BREAK;
uint32_t COLOR_CONFIG;
uint32_t COLOR_BREATH;

uint32_t currentColor = 0;

// --- Helpers ---
uint32_t rawToNeo(uint32_t raw) {
  uint8_t r = (raw >> 16) & 0xFF;
  uint8_t g = (raw >> 8) & 0xFF;
  uint8_t b = raw & 0xFF;
  return strip.Color(r, g, b);
}

String colorToHex(uint32_t color) {
  char hex[8];
  sprintf(hex, "#%02X%02X%02X",
          (uint8_t)(color >> 16),
          (uint8_t)(color >> 8),
          (uint8_t)color);
  return String(hex);
}

uint32_t hexToColor(String hex) {
  hex.replace("#", "");
  uint32_t value = strtoul(hex.c_str(), nullptr, 16);
  return rawToNeo(value);
}

uint8_t ledMap(uint8_t logicalIndex) {
  return (logicalIndex + 17) % NUM_LEDS;
}

uint16_t transitionDelay() {
  return constrain(transitionDelayMs, 5, 500);
}

// --- LED functions ---
void setRing(uint32_t color) {
  strip.setBrightness(brightness);

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, color);
  }

  strip.show();
  currentColor = color;
}

void allLedsOff() {
  strip.clear();
  strip.show();
  currentColor = 0;
}

void runningLights(uint32_t color) {
  allLedsOff();

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.clear();

    strip.setPixelColor(ledMap(i), color);
    strip.setPixelColor(ledMap((i - 1 + NUM_LEDS) % NUM_LEDS), color);
    strip.setPixelColor(ledMap((i - 2 + NUM_LEDS) % NUM_LEDS), color);

    strip.show();
    delay(transitionDelay());
  }

  setRing(color);
}

void runningFill(uint32_t color) {
  allLedsOff();

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(ledMap(i), color);
    strip.show();
    delay(transitionDelay());
  }

  currentColor = color;
}

void runningFillClear(uint32_t color) {
  runningFill(color);
  delay(transitionDelay());

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(ledMap(i), 0);
    strip.show();
    delay(transitionDelay());
  }

  currentColor = 0;
}

void fadeToColor(uint32_t targetColor) {
  uint8_t r1 = (currentColor >> 16) & 0xFF;
  uint8_t g1 = (currentColor >> 8) & 0xFF;
  uint8_t b1 = currentColor & 0xFF;

  uint8_t r2 = (targetColor >> 16) & 0xFF;
  uint8_t g2 = (targetColor >> 8) & 0xFF;
  uint8_t b2 = targetColor & 0xFF;

  const uint8_t fadeSteps = 40;

  for (uint8_t step = 0; step <= fadeSteps; step++) {
    uint32_t c = strip.Color(
      blend8(r1, r2, step, fadeSteps),
      blend8(g1, g2, step, fadeSteps),
      blend8(b1, b2, step, fadeSteps)
    );

    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(ledMap(i), c);
    }

    strip.show();
    delay(transitionDelay());
  }

  currentColor = targetColor;
}

uint8_t blend8(uint8_t from, uint8_t to, uint8_t step, uint8_t steps) {
  return from + ((int16_t)(to - from) * step) / steps;
}

uint32_t scaleColor(uint32_t color, uint8_t amount) {
  uint8_t r = ((color >> 16) & 0xFF) * amount / 255;
  uint8_t g = ((color >> 8) & 0xFF) * amount / 255;
  uint8_t b = (color & 0xFF) * amount / 255;

  return strip.Color(r, g, b);
}

void drawSmoothPath(const uint8_t* path, uint8_t pathLen, float progress) {
  float litAmount = constrain(progress, 0.0f, 1.0f) * pathLen;

  for (uint8_t i = 0; i < pathLen; i++) {
    float pixelFill = litAmount - i;

    if (pixelFill >= 1.0f) {
      strip.setPixelColor(path[i], COLOR_BREATH);
    }
    else if (pixelFill > 0.0f) {
      uint8_t fade = (uint8_t)(pixelFill * 255.0f);
      strip.setPixelColor(path[i], scaleColor(COLOR_BREATH, fade));
    }
  }
}

void showBreathCenterSplit(float progress) {
  strip.setBrightness(brightness);
  strip.clear();

  // Physical LEDs:
  // Left side: 18 → 17 → ... → 8
  // Right side: 19 → 1 → 2 → ... → 8

  const uint8_t leftPath[] = {
    16, 15, 14, 13, 12, 11, 10, 9, 8, 7
  };

  const uint8_t rightPath[] = {
    17, 18, 0, 1, 2, 3, 4, 5, 6, 7
  };

  drawSmoothPath(leftPath, sizeof(leftPath), progress);
  drawSmoothPath(rightPath, sizeof(rightPath), progress);

  strip.show();
}

void showBreathLinear(float progress) {
  strip.setBrightness(brightness);
  strip.clear();

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    float pixelFill = constrain(progress, 0.0f, 1.0f) * NUM_LEDS - i;

    if (pixelFill >= 1.0f) {
      strip.setPixelColor(i, COLOR_BREATH);
    }
    else if (pixelFill > 0.0f) {
      strip.setPixelColor(i, scaleColor(COLOR_BREATH, pixelFill * 255));
    }
  }

  strip.show();
}

void showBreathBarSmooth(float progress) {
  progress = constrain(progress, 0.0f, 1.0f);

  float litAmount = progress * NUM_LEDS;

  strip.setBrightness(brightness);
  strip.clear();

  for (int i = 0; i < NUM_LEDS; i++) {
    float pixelFill = litAmount - i;

    if (pixelFill >= 1.0f) {
      strip.setPixelColor(i, COLOR_BREATH);
    }
    else if (pixelFill > 0.0f) {
      uint8_t fade = (uint8_t)(pixelFill * 255.0f);
      strip.setPixelColor(i, scaleColor(COLOR_BREATH, fade));
    }
  }

  strip.show();

  currentColor = progress <= 0.0f ? 0 : COLOR_BREATH;
}

void enterBreathing() {
  if (configMode) return;

  state = BREATH;
  stateStartMs = millis();
  idleLedsOff = false;
  allLedsOff();
}

void updateBreathing() {
  uint32_t elapsed = (millis() - stateStartMs) % (BREATH_PHASE_MS * 4UL);
  float progress = 0.0f;

  if (elapsed < BREATH_PHASE_MS) {
    progress = (float)elapsed / BREATH_PHASE_MS;
  }
  else if (elapsed < BREATH_PHASE_MS * 2UL) {
    progress = 1.0f;
  }
  else if (elapsed < BREATH_PHASE_MS * 3UL) {
    uint32_t phaseElapsed = elapsed - BREATH_PHASE_MS * 2UL;
    progress = 1.0f - ((float)phaseElapsed / BREATH_PHASE_MS);
  }
  else {
    progress = 0.0f;
  }

  if (breathingStyle == BREATH_CENTER_SPLIT) {
    showBreathCenterSplit(progress);
  } else {
    showBreathLinear(progress);
  }

  currentColor = progress <= 0.0f ? 0 : COLOR_BREATH;
}

void applyTransition(uint32_t color) {
  strip.setBrightness(brightness);

  switch (transition) {
    case SET_RING:
      setRing(color);
      break;

    case RUNNING_LIGHT:
      runningLights(color);
      break;

    case RUNNING_FILL:
      runningFill(color);
      break;

    case RUNNING_LIGHT_CLEAR:
      runningFillClear(color);
      break;

    case FADE:
      fadeToColor(color);
      break;
  }
}

// --- Settings ---
void loadSettings() {
  prefs.begin("pomodesk", true);

  workMs = prefs.getUInt("workMs", WORK_MS_DEFAULT);
  breakMs = prefs.getUInt("breakMs", BREAK_MS_DEFAULT);
  idleLedMs = prefs.getUInt("idleMs", IDLE_LED_MS_DEFAULT);

  brightness = prefs.getUChar("bright", 150);
  transitionDelayMs = prefs.getUShort("delay", 40);

  bool transitionWasSaved = prefs.getBool("transSaved", false);

  if (transitionWasSaved) {
    transition = (Transition)prefs.getUChar("trans", transition);
  }

  COLOR_IDLE   = prefs.getUInt("idleColor", rawToNeo(DEFAULT_IDLE_RAW));
  COLOR_WORK   = prefs.getUInt("workColor", rawToNeo(DEFAULT_WORK_RAW));
  COLOR_BREAK  = prefs.getUInt("breakColor", rawToNeo(DEFAULT_BREAK_RAW));
  COLOR_CONFIG = prefs.getUInt("configColor", rawToNeo(DEFAULT_CONFIG_RAW));
  COLOR_BREATH = prefs.getUInt("breathColor", rawToNeo(DEFAULT_BREATH_RAW));

  prefs.end();
}

void saveSettings() {
  prefs.begin("pomodesk", false);

  prefs.putUInt("workMs", workMs);
  prefs.putUInt("breakMs", breakMs);
  prefs.putUInt("idleMs", idleLedMs);

  prefs.putUChar("bright", brightness);
  prefs.putUShort("delay", transitionDelayMs);

  prefs.putUChar("trans", transition);
  prefs.putBool("transSaved", true);

  prefs.putUInt("idleColor", COLOR_IDLE);
  prefs.putUInt("workColor", COLOR_WORK);
  prefs.putUInt("breakColor", COLOR_BREAK);
  prefs.putUInt("configColor", COLOR_CONFIG);
  prefs.putUInt("breathColor", COLOR_BREATH);

  prefs.end();
}

void resetSettings() {
  workMs = WORK_MS_DEFAULT;
  breakMs = BREAK_MS_DEFAULT;
  idleLedMs = IDLE_LED_MS_DEFAULT;

  brightness = 150;
  transitionDelayMs = 40;
  transition = RUNNING_FILL;

  COLOR_IDLE   = rawToNeo(DEFAULT_IDLE_RAW);
  COLOR_WORK   = rawToNeo(DEFAULT_WORK_RAW);
  COLOR_BREAK  = rawToNeo(DEFAULT_BREAK_RAW);
  COLOR_CONFIG = rawToNeo(DEFAULT_CONFIG_RAW);
  COLOR_BREATH = rawToNeo(DEFAULT_BREATH_RAW);

  saveSettings();
}

// --- State logic ---
void enterState(State s) {
  if (configMode) return;

  state = s;
  stateStartMs = millis();
  idleLedsOff = false;

  if (state == IDLE) {
    applyTransition(COLOR_IDLE);
  }
  else if (state == WORK) {
    applyTransition(COLOR_WORK);
  }
  else if (state == BREAK) {
    applyTransition(COLOR_BREAK);
  }
  else if (state == BREATH) {
    enterBreathing();
  }
}

void nextState() {
  if (configMode) return;

  if (state == BREATH) {
    enterState(IDLE);
  }
  else if (state == IDLE) {
    enterState(WORK);
  }
  else if (state == WORK) {
    enterState(BREAK);
  }
  else {
    enterState(IDLE);
  }
}

// --- Button ---
struct Button {
  int pin;

  bool lastRaw = HIGH;
  bool stable = HIGH;

  uint32_t lastChangeMs = 0;
  uint32_t pressStartMs = 0;

  bool long15Done = false;
  bool long3Done = false;

  static const uint32_t DEBOUNCE_MS = 30;

  void begin() {
    pinMode(pin, INPUT_PULLUP);
    lastRaw = digitalRead(pin);
    stable = lastRaw;
    lastChangeMs = millis();
  }

  void update() {
    bool raw = digitalRead(pin);

    if (raw != lastRaw) {
      lastRaw = raw;
      lastChangeMs = millis();
    }

    if (millis() - lastChangeMs <= DEBOUNCE_MS) {
      return;
    }

    if (stable != raw) {
      stable = raw;

      if (stable == LOW) {
        pressStartMs = millis();
        long15Done = false;
        long3Done = false;
      }

      if (stable == HIGH) {
        uint32_t held = millis() - pressStartMs;

        if (!configMode) {
          if (held >= BREATH_TIME_MS && held < CONFIG_TIME_MS) {
            enterBreathing();
          }
          else if (held < BREATH_TIME_MS) {
            nextState();
          }
        }
      }
    }

    if (stable == LOW) {
      uint32_t held = millis() - pressStartMs;

      if (held >= OFF_TIME_MS && !long3Done) {
        long3Done = true;
        powerOffDevice();
      }
      else if (held >= CONFIG_TIME_MS && !long15Done) {
        long15Done = true;
        toggleConfigServer();
      }
    }
  }
};

Button button { BTN_STATE };

// --- Setup / Loop ---
void setup() {
  pinMode(PWR_LED, OUTPUT);
  digitalWrite(PWR_LED, HIGH);

  WiFi.mode(WIFI_OFF);
  btStop();
  setCpuFrequencyMhz(80);

  button.begin();

  pinMode(LED_DATA, OUTPUT);
  digitalWrite(LED_DATA, LOW);

  delay(200);

  strip.begin();
  loadSettings();
  strip.setBrightness(brightness);
  allLedsOff();

  delay(200);

  enterState(IDLE);
}

void loop() {
  delay(5);

  button.update();

  if (configMode) {
    processConfigServer();
    return;
  }

  uint32_t now = millis();

  if (state == BREATH) {
    updateBreathing();
    return;
  }

  if (state == WORK && now - stateStartMs >= workMs) {
    enterState(BREAK);
  }
  else if (state == BREAK && now - stateStartMs >= breakMs) {
    enterState(IDLE);
  }
  else if (state == IDLE && !idleLedsOff && now - stateStartMs >= idleLedMs) {
    allLedsOff();
    idleLedsOff = true;
  }
}