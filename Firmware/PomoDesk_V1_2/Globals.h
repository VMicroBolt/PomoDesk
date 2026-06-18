#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>

// --- Pins ---
extern const int BTN_STATE;
extern const int LED_DATA;
extern const int PWR_LED;

// --- LEDs ---
extern const int NUM_LEDS;
extern Adafruit_NeoPixel strip;

// --- Preferences / Config mode ---
extern Preferences prefs;
extern bool configMode;

// --- Defaults ---
extern const uint32_t WORK_MS_DEFAULT;
extern const uint32_t BREAK_MS_DEFAULT;
extern const uint32_t IDLE_LED_MS_DEFAULT;

extern const uint32_t BREATH_TIME_MS;
extern const uint32_t BREATH_PHASE_MS;
extern const uint32_t DEFAULT_BREATH_RAW;

extern const uint32_t CONFIG_TIME_MS;
extern const uint32_t OFF_TIME_MS;

extern const uint32_t DEFAULT_IDLE_RAW;
extern const uint32_t DEFAULT_WORK_RAW;
extern const uint32_t DEFAULT_BREAK_RAW;
extern const uint32_t DEFAULT_CONFIG_RAW;

// --- Settings ---
extern uint32_t workMs;
extern uint32_t breakMs;
extern uint32_t idleLedMs;
extern uint8_t brightness;

// --- Transitions ---
enum Transition {
  SET_RING,
  RUNNING_LIGHT,
  RUNNING_FILL,
  RUNNING_LIGHT_CLEAR,
  FADE
};

extern Transition transition;
extern uint16_t transitionDelayMs;

// --- States ---
enum State { IDLE, WORK, BREAK, BREATH };

extern State state;
extern uint32_t stateStartMs;
extern bool idleLedsOff;

// --- Colors ---
extern uint32_t COLOR_IDLE;
extern uint32_t COLOR_WORK;
extern uint32_t COLOR_BREAK;
extern uint32_t COLOR_CONFIG;
extern uint32_t COLOR_BREATH;

extern uint32_t currentColor;

// --- Shared helpers / behavior ---
uint32_t rawToNeo(uint32_t raw);
String colorToHex(uint32_t color);
uint32_t hexToColor(String hex);
uint16_t transitionDelay();

void setRing(uint32_t color);
void allLedsOff();
void runningLights(uint32_t color);
void runningFill(uint32_t color);
void runningFillClear(uint32_t color);
uint8_t blend8(uint8_t from, uint8_t to, uint8_t step, uint8_t steps);
void fadeToColor(uint32_t targetColor);
void applyTransition(uint32_t color);


// --- Breathing state ---
enum BreathingStyle {
  BREATH_LINEAR,
  BREATH_CENTER_SPLIT
};

extern BreathingStyle breathingStyle;

void showBreathLinear(float progress);
void showBreathCenterSplit(float progress);
void drawSmoothPath(const uint8_t* path, uint8_t pathLen, float progress);

uint32_t scaleColor(uint32_t color, uint8_t amount);
void showBreathBarSmooth(float progress);
void updateBreathing();
void enterBreathing();

// --- Settings / config ---
void loadSettings();
void saveSettings();
void resetSettings();

void enterState(State s);
void nextState();
