#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <esp_bt.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

// --- Pins ---
const int BTN_STATE = 1;
const int LED_DATA  = 48;
const int PWR_LED   = 18;

// --- LEDs ---
const int NUM_LEDS = 19;
Adafruit_NeoPixel strip(NUM_LEDS, LED_DATA, NEO_GRB + NEO_KHZ800);

// --- WiFi / Webserver ---
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

const char* AP_SSID = "PomoDesk-Config";
const char* AP_PASS = "pomodesk";

IPAddress apIP(10, 10, 10, 1);
IPAddress gateway(10, 10, 10, 1);
IPAddress subnet(255, 255, 255, 0);

bool configMode = false;

// --- Defaults ---
const uint32_t WORK_MS_DEFAULT     = 45UL * 60UL * 1000UL;
const uint32_t BREAK_MS_DEFAULT    = 5UL  * 60UL * 1000UL;
const uint32_t IDLE_LED_MS_DEFAULT = 10UL * 60UL * 1000UL;

const uint32_t CONFIG_TIME_MS  = 1500;
const uint32_t RAINBOW_TIME_MS = 3000;
const uint32_t OFF_TIME_MS     = 6000;

const uint32_t DEFAULT_IDLE_RAW   = 0xFF5000;
const uint32_t DEFAULT_WORK_RAW   = 0x00FF00;
const uint32_t DEFAULT_BREAK_RAW  = 0xFF0000;
const uint32_t DEFAULT_CONFIG_RAW = 0x0078FF;

// --- Settings ---
uint32_t workMs;
uint32_t breakMs;
uint32_t idleLedMs;
uint8_t brightness = 150;

// --- Transitions ---
enum Transition {
  SET_RING,
  RUNNING_LIGHT,
  RUNNING_FILL,
  RUNNING_LIGHT_CLEAR,
  FADE
};

Transition transition = RUNNING_FILL;
uint16_t transitionDelayMs = 40;

// --- States ---
enum State { IDLE, WORK, BREAK };
State state = IDLE;

uint32_t stateStartMs = 0;
bool idleLedsOff = false;
bool rainbowMode = false;
uint8_t rainbowOffset = 0;
uint32_t lastRainbowStepMs = 0;

// --- Colors ---
uint32_t COLOR_IDLE;
uint32_t COLOR_WORK;
uint32_t COLOR_BREAK;
uint32_t COLOR_CONFIG;

uint32_t currentColor = 0;

// --- Forward declarations ---
void stopConfigServer();
void powerOffDevice();
void handleExitConfig();
void handleSleep();
void startRainbowMode();
void stopRainbowMode();

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

uint16_t transitionDelay() {
  return constrain(transitionDelayMs, 5, 500);
}

uint32_t wheel(uint8_t wheelPos) {
  wheelPos = 255 - wheelPos;

  if (wheelPos < 85) {
    return strip.Color(255 - wheelPos * 3, 0, wheelPos * 3);
  }

  if (wheelPos < 170) {
    wheelPos -= 85;
    return strip.Color(0, wheelPos * 3, 255 - wheelPos * 3);
  }

  wheelPos -= 170;
  return strip.Color(wheelPos * 3, 255 - wheelPos * 3, 0);
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
    strip.setPixelColor(i, color);
    strip.setPixelColor((i - 1 + NUM_LEDS) % NUM_LEDS, color);
    strip.setPixelColor((i - 2 + NUM_LEDS) % NUM_LEDS, color);
    strip.show();
    delay(transitionDelay());
  }

  setRing(color);
}

void runningFill(uint32_t color) {
  allLedsOff();

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, color);
    strip.show();
    delay(transitionDelay());
  }

  currentColor = color;
}

void runningFillClear(uint32_t color) {
  runningFill(color);
  delay(transitionDelay());

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, 0);
    strip.show();
    delay(transitionDelay());
  }

  currentColor = 0;
}

uint8_t blend8(uint8_t from, uint8_t to, uint8_t step, uint8_t steps) {
  return from + ((int16_t)(to - from) * step) / steps;
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
      strip.setPixelColor(i, c);
    }

    strip.show();
    delay(transitionDelay());
  }

  currentColor = targetColor;
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

void updateRainbowCycle() {
  const uint32_t now = millis();

  if (now - lastRainbowStepMs < 30) {
    return;
  }

  lastRainbowStepMs = now;
  strip.setBrightness(brightness);

  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t wheelPos = (i * 256 / NUM_LEDS + rainbowOffset) & 0xFF;
    strip.setPixelColor(i, wheel(wheelPos));
  }

  strip.show();
  rainbowOffset++;
}

// --- Settings ---
void loadSettings() {
  prefs.begin("pomodesk", true);

  workMs = prefs.getUInt("workMs", WORK_MS_DEFAULT);
  breakMs = prefs.getUInt("breakMs", BREAK_MS_DEFAULT);
  idleLedMs = prefs.getUInt("idleMs", IDLE_LED_MS_DEFAULT);

  brightness = prefs.getUChar("bright", 150);
  transitionDelayMs = prefs.getUShort("delay", 40);
  transition = (Transition)prefs.getUChar("trans", RUNNING_FILL);

  COLOR_IDLE   = prefs.getUInt("idleColor", rawToNeo(DEFAULT_IDLE_RAW));
  COLOR_WORK   = prefs.getUInt("workColor", rawToNeo(DEFAULT_WORK_RAW));
  COLOR_BREAK  = prefs.getUInt("breakColor", rawToNeo(DEFAULT_BREAK_RAW));
  COLOR_CONFIG = prefs.getUInt("configColor", rawToNeo(DEFAULT_CONFIG_RAW));

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

  prefs.putUInt("idleColor", COLOR_IDLE);
  prefs.putUInt("workColor", COLOR_WORK);
  prefs.putUInt("breakColor", COLOR_BREAK);
  prefs.putUInt("configColor", COLOR_CONFIG);

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

  saveSettings();
}

// --- State logic ---
void enterState(State s) {
  if (configMode) return;

  rainbowMode = false;

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
}

void nextState() {
  if (configMode) return;

  if (state == IDLE) {
    enterState(WORK);
  }
  else if (state == WORK) {
    enterState(BREAK);
  }
  else {
    enterState(IDLE);
  }
}

// --- Web UI ---
String htmlPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>PomoDesk Config</title>

<style>
body {
  margin: 0;
  font-family: Arial, sans-serif;
  background: radial-gradient(circle at top, #1b1b1b, #050505);
  color: #eee;
  letter-spacing: 2px;
}

.wrap {
  max-width: 760px;
  margin: auto;
  padding: 36px 24px;
}

h1 {
  text-align: center;
  color: #ff9b54;
  font-weight: 300;
  letter-spacing: 10px;
}

h2 {
  text-align: center;
  color: #aaa;
  font-weight: 300;
  font-size: 14px;
  margin-bottom: 40px;
}

.card {
  border: 1px solid #333;
  background: rgba(255,255,255,0.035);
  padding: 24px;
  margin-bottom: 22px;
  border-radius: 18px;
  box-shadow: 0 0 40px rgba(255,120,60,0.08);
}

label {
  color: #ff9b54;
  font-size: 13px;
}

input, select {
  padding: 12px;
  border-radius: 10px;
  border: 1px solid #333;
  background: #111;
  color: #eee;
  font-size: 15px;
  box-sizing: border-box;
}

select {
  width: 100%;
  margin-top: 8px;
}

input[type=number] {
  width: 62px;
  text-align: center;
}

input[type=range] {
  width: 100%;
}

button {
  border: 0;
  border-radius: 12px;
  background: #ff6b2b;
  color: #111;
  font-weight: bold;
  letter-spacing: 2px;
}

.color-row {
  display: grid;
  grid-template-columns: 1fr auto;
  align-items: center;
  gap: 10px;
  margin-top: 18px;
}

.color-actions {
  justify-self: end;
  display: flex;
  align-items: end;
  gap: 10px;
}

.swatch {
  width: 44px;
  height: 44px;
  border-radius: 10px;
  border: 1px solid #444;
  cursor: pointer;
}

.custom-wrap {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 4px;
}

.custom-label {
  font-size: 9px;
  color: #aaa;
  line-height: 1.1;
  text-align: center;
  letter-spacing: 1px;
}

.custom-color {
  width: 44px;
  height: 44px;
  padding: 2px;
  border-radius: 10px;
  border: 1px solid #444;
  background: #111;
}

.time-row {
  display: grid;
  grid-template-columns: 1fr 40px 62px 40px;
  align-items: center;
  gap: 8px;
  margin-top: 18px;
}

.time-row label {
  justify-self: start;
}

.step-btn {
  width: 40px;
  height: 40px;
  padding: 0;
  font-size: 20px;
  line-height: 40px;
}

.setting-row {
  margin-top: 18px;
}

.inline-control {
  display: grid;
  grid-template-columns: 1fr auto auto;
  align-items: center;
  gap: 10px;
  margin-top: 8px;
}

.inline-control input {
  width: 100%;
}

.preset-btn {
  height: 42px;
  padding: 0 14px;
  font-size: 11px;
}

.brightness-row {
  display: grid;
  grid-template-columns: 1fr;
  gap: 12px;
  margin-top: 8px;
}

.brightness-buttons {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 12px;
}

.action-row {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 12px;
  margin-top: 22px;
}

.save-btn,
.reset,
.exit-btn,
.sleep-btn {
  width: 100%;
  padding: 15px;
  margin: 0;
}

.reset,
.exit-btn {
  background: transparent;
  color: #ff6b2b;
  border: 1px solid #ff6b2b;
}

.sleep-btn {
  background: #222;
  color: #ff6b2b;
  border: 1px solid #333;
}

.hint {
  color: #777;
  font-size: 12px;
  line-height: 1.6;
  margin-top: 12px;
  letter-spacing: 1px;
}

.footer {
  text-align: center;
  color: #777;
  margin-top: 36px;
  font-size: 12px;
}
</style>
</head>

<body>
<div class="wrap">
<h1>POMODESK</h1>
<h2>CONFIGURATION WEBSERVER</h2>

<form id="configForm" action="/save" method="POST">

<div class="card">
  <div class="color-row">
    <label>Idle color</label>
    <div class="color-actions">
      <button class="swatch" type="button" style="background:#FF5000" onclick="setColor('idleColor', '#FF5000', 'idle')"></button>
      <div class="custom-wrap">
        <span class="custom-label">Custom</span>
        <input class="custom-color" type="color" id="idleColor" name="idleColor" value="%IDLE_COLOR%" onchange="customColorChosen('idleColor', 'idle')">
      </div>
    </div>
  </div>

  <div class="color-row">
    <label>Focus color</label>
    <div class="color-actions">
      <button class="swatch" type="button" style="background:#00FF00" onclick="setColor('workColor', '#00FF00', 'work')"></button>
      <div class="custom-wrap">
        <span class="custom-label">Custom</span>
        <input class="custom-color" type="color" id="workColor" name="workColor" value="%WORK_COLOR%" onchange="customColorChosen('workColor', 'work')">
      </div>
    </div>
  </div>

  <div class="color-row">
    <label>Break color</label>
    <div class="color-actions">
      <button class="swatch" type="button" style="background:#FF0000" onclick="setColor('breakColor', '#FF0000', 'break')"></button>
      <div class="custom-wrap">
        <span class="custom-label">Custom</span>
        <input class="custom-color" type="color" id="breakColor" name="breakColor" value="%BREAK_COLOR%" onchange="customColorChosen('breakColor', 'break')">
      </div>
    </div>
  </div>

  <div class="color-row">
    <label>Config mode color</label>
    <div class="color-actions">
      <button class="swatch" type="button" style="background:#0078FF" onclick="setColor('configColor', '#0078FF', 'config')"></button>
      <div class="custom-wrap">
        <span class="custom-label">Custom</span>
        <input class="custom-color" type="color" id="configColor" name="configColor" value="%CONFIG_COLOR%" onchange="customColorChosen('configColor', 'config')">
      </div>
    </div>
  </div>
</div>

<div class="card">
  <div class="time-row">
    <label>Focus time</label>
    <button class="step-btn" type="button" onclick="stepValue('workMin', -1)">-</button>
    <input id="workMin" type="number" name="workMin" min="1" max="99" value="%WORK_MIN%">
    <button class="step-btn" type="button" onclick="stepValue('workMin', 1)">+</button>
  </div>

  <div class="time-row">
    <label>Break time</label>
    <button class="step-btn" type="button" onclick="stepValue('breakMin', -1)">-</button>
    <input id="breakMin" type="number" name="breakMin" min="1" max="99" value="%BREAK_MIN%">
    <button class="step-btn" type="button" onclick="stepValue('breakMin', 1)">+</button>
  </div>

  <div class="time-row">
    <label>Idle LED timeout</label>
    <button class="step-btn" type="button" onclick="stepValue('idleMin', -1)">-</button>
    <input id="idleMin" type="number" name="idleMin" min="1" max="99" value="%IDLE_MIN%">
    <button class="step-btn" type="button" onclick="stepValue('idleMin', 1)">+</button>
  </div>
</div>

<div class="card">
  <div class="setting-row">
    <label>Transition</label>
    <select name="transition" onchange="previewTransition('config')">
      <option value="0" %T0%>Set ring</option>
      <option value="1" %T1%>Running light</option>
      <option value="2" %T2%>Running fill</option>
      <option value="3" %T3%>Running fill clear</option>
      <option value="4" %T4%>Fade</option>
    </select>
  </div>

  <div class="setting-row">
    <label>Transition speed, ms</label>
    <div class="inline-control">
      <input id="delayInput" type="number" name="delay" min="5" max="500" value="%DELAY%" onchange="previewTransition('config')">
      <button class="preset-btn" type="button" onclick="setDelay(40)">NORMAL</button>
      <button class="preset-btn" type="button" onclick="setDelay(80)">GENTLE</button>
    </div>
  </div>

  <div class="setting-row">
    <label>Brightness / ECO mode</label>
    <div class="brightness-row">
      <input id="brightnessInput" type="range" name="brightness" min="5" max="255" value="%BRIGHTNESS%" oninput="previewTransition('config')">
      <div class="brightness-buttons">
        <button class="preset-btn" type="button" onclick="setBrightnessNormal()">NORMAL</button>
        <button class="preset-btn" type="button" onclick="setEco()">ECO</button>
      </div>
    </div>
    <div class="hint">Lower brightness reduces current consumption.</div>
  </div>
</div>

<div class="action-row">
  <button class="reset" type="submit" formaction="/reset" formmethod="POST">RESET TO DEFAULT</button>
  <button class="save-btn" type="submit">SAVE SETTINGS</button>
</div>

<div class="action-row">
  <button class="exit-btn" type="button" onclick="location.href='/exit'">EXIT CONFIG</button>
  <button class="sleep-btn" type="button" onclick="location.href='/sleep'">GO TO SLEEP</button>
</div>

</form>

<div class="footer">
  PHYSICAL POMODORO TIMER<br>
  OPEN: pomodesk.local OR 10.10.10.1
</div>
</div>

<script>
function stepValue(id, step) {
  const input = document.getElementById(id);
  let value = parseInt(input.value || "1");
  value += step;
  value = Math.max(1, Math.min(99, value));
  input.value = value;
}

function setColor(id, value, state) {
  const input = document.getElementById(id);
  input.value = value;
  previewTransition(state);
}

function customColorChosen(id, state) {
  const input = document.getElementById(id);

  if (!input.dataset.customStarted) {
    input.dataset.customStarted = "true";
    input.value = "#808080";
  }

  previewTransition(state);
}

function setDelay(value) {
  document.getElementById("delayInput").value = value;
  previewTransition("config");
}

function setEco() {
  document.getElementById("brightnessInput").value = 60;
  previewTransition("config");
}

function setBrightnessNormal() {
  document.getElementById("brightnessInput").value = 150;
  previewTransition("config");
}

let previewTimeout = null;

function previewTransition(state = "config") {
  clearTimeout(previewTimeout);

  previewTimeout = setTimeout(() => {
    const form = document.getElementById("configForm");
    const data = new FormData(form);
    data.append("previewState", state);

    fetch("/preview", {
      method: "POST",
      body: data
    }).catch(() => {});
  }, 150);
}
</script>

</body>
</html>
)rawliteral";

  html.replace("%IDLE_COLOR%", colorToHex(COLOR_IDLE));
  html.replace("%WORK_COLOR%", colorToHex(COLOR_WORK));
  html.replace("%BREAK_COLOR%", colorToHex(COLOR_BREAK));
  html.replace("%CONFIG_COLOR%", colorToHex(COLOR_CONFIG));

  html.replace("%WORK_MIN%", String(constrain(workMs / 60000UL, 1UL, 99UL)));
  html.replace("%BREAK_MIN%", String(constrain(breakMs / 60000UL, 1UL, 99UL)));
  html.replace("%IDLE_MIN%", String(constrain(idleLedMs / 60000UL, 1UL, 99UL)));

  html.replace("%DELAY%", String(transitionDelayMs));
  html.replace("%BRIGHTNESS%", String(brightness));

  html.replace("%T0%", transition == SET_RING ? "selected" : "");
  html.replace("%T1%", transition == RUNNING_LIGHT ? "selected" : "");
  html.replace("%T2%", transition == RUNNING_FILL ? "selected" : "");
  html.replace("%T3%", transition == RUNNING_LIGHT_CLEAR ? "selected" : "");
  html.replace("%T4%", transition == FADE ? "selected" : "");

  return html;
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleSave() {
  COLOR_IDLE = hexToColor(server.arg("idleColor"));
  COLOR_WORK = hexToColor(server.arg("workColor"));
  COLOR_BREAK = hexToColor(server.arg("breakColor"));
  COLOR_CONFIG = hexToColor(server.arg("configColor"));

  workMs = constrain(server.arg("workMin").toInt(), 1, 99) * 60UL * 1000UL;
  breakMs = constrain(server.arg("breakMin").toInt(), 1, 99) * 60UL * 1000UL;
  idleLedMs = constrain(server.arg("idleMin").toInt(), 1, 99) * 60UL * 1000UL;

  transition = (Transition)server.arg("transition").toInt();
  transitionDelayMs = constrain(server.arg("delay").toInt(), 5, 500);
  brightness = constrain(server.arg("brightness").toInt(), 5, 255);

  saveSettings();
  applyTransition(COLOR_CONFIG);

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleReset() {
  resetSettings();
  applyTransition(COLOR_CONFIG);

  server.sendHeader("Location", "/");
  server.send(303);
}

void handlePreview() {
  if (server.hasArg("brightness")) {
    brightness = constrain(server.arg("brightness").toInt(), 5, 255);
  }

  if (server.hasArg("delay")) {
    transitionDelayMs = constrain(server.arg("delay").toInt(), 5, 500);
  }

  if (server.hasArg("transition")) {
    transition = (Transition)server.arg("transition").toInt();
  }

  if (server.hasArg("idleColor")) COLOR_IDLE = hexToColor(server.arg("idleColor"));
  if (server.hasArg("workColor")) COLOR_WORK = hexToColor(server.arg("workColor"));
  if (server.hasArg("breakColor")) COLOR_BREAK = hexToColor(server.arg("breakColor"));
  if (server.hasArg("configColor")) COLOR_CONFIG = hexToColor(server.arg("configColor"));

  String previewState = server.arg("previewState");

  if (previewState == "idle") {
    applyTransition(COLOR_IDLE);
  }
  else if (previewState == "work") {
    applyTransition(COLOR_WORK);
  }
  else if (previewState == "break") {
    applyTransition(COLOR_BREAK);
  }
  else {
    applyTransition(COLOR_CONFIG);
  }

  server.send(200, "text/plain", "OK");
}

void handleCaptivePortal() {
  server.sendHeader("Location", "http://10.10.10.1", true);
  server.send(302, "text/plain", "");
}

// --- Config server control ---
void startConfigServer() {
  rainbowMode = false;
  configMode = true;

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, gateway, subnet);
  WiFi.softAP(AP_SSID, AP_PASS);

  dnsServer.start(53, "*", apIP);
  MDNS.begin("pomodesk");

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/preview", HTTP_POST, handlePreview);
  server.on("/exit", HTTP_GET, handleExitConfig);
  server.on("/sleep", HTTP_GET, handleSleep);
  server.onNotFound(handleCaptivePortal);

  server.begin();

  applyTransition(COLOR_CONFIG);
}

void stopConfigServer() {
  server.stop();
  dnsServer.stop();
  MDNS.end();

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);

  configMode = false;
  enterState(IDLE);
}

void startRainbowMode() {
  if (configMode) {
    stopConfigServer();
  }

  rainbowMode = true;
  idleLedsOff = false;
  lastRainbowStepMs = 0;
}

void stopRainbowMode() {
  if (!rainbowMode) {
    return;
  }

  rainbowMode = false;
  rainbowOffset = 0;
  enterState(IDLE);
}

void powerOffDevice() {
  server.stop();
  dnsServer.stop();
  MDNS.end();

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();

  allLedsOff();

  while (digitalRead(BTN_STATE) == LOW) {
    delay(10);
  }

  delay(200);

  esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_STATE, 0);

  rtc_gpio_pullup_en((gpio_num_t)BTN_STATE);
  rtc_gpio_pulldown_dis((gpio_num_t)BTN_STATE);

  esp_deep_sleep_start();
}

void handleExitConfig() {
  server.send(
    200,
    "text/html",
    "<html><body style='background:#050505;color:#eee;font-family:Arial;text-align:center;padding-top:80px;'>"
    "<h1>PomoDesk</h1><p>Exiting configuration mode...</p>"
    "</body></html>"
  );

  delay(500);
  stopConfigServer();
}

void handleSleep() {
  server.send(
    200,
    "text/html",
    "<html><body style='background:#050505;color:#eee;font-family:Arial;text-align:center;padding-top:80px;'>"
    "<h1>PomoDesk</h1><p>Going to sleep...</p>"
    "</body></html>"
  );

  delay(500);
  powerOffDevice();
}

void toggleConfigServer() {
  if (configMode) {
    stopConfigServer();
  } else {
    startConfigServer();
  }
}

// --- Button ---
struct Button {
  int pin;

  bool lastRaw = HIGH;
  bool stable = HIGH;

  uint32_t lastChangeMs = 0;
  uint32_t pressStartMs = 0;

  static const uint32_t DEBOUNCE_MS = 30;

  explicit Button(int pinNumber) : pin(pinNumber) {}

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
      }

      else {
        uint32_t held = millis() - pressStartMs;

        if (held >= OFF_TIME_MS) {
          powerOffDevice();
        }
        else if (held >= RAINBOW_TIME_MS) {
          if (rainbowMode) {
            stopRainbowMode();
          }
          else {
            startRainbowMode();
          }
        }
        else if (held >= CONFIG_TIME_MS) {
          toggleConfigServer();
        }
        else if (rainbowMode) {
          stopRainbowMode();
        }
        else if (!configMode) {
          nextState();
        }
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
    dnsServer.processNextRequest();
    server.handleClient();
    return;
  }

  if (rainbowMode) {
    updateRainbowCycle();
    return;
  }

  uint32_t now = millis();

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