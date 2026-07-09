#pragma once

#include <Arduino.h>
#include <vector>

#include "esp_err.h"
#include "led_strip.h"

#ifndef NEO_GRB
#define NEO_GRB 0
#endif

#ifndef NEO_KHZ800
#define NEO_KHZ800 0
#endif

using neoPixelType = uint16_t;

class Adafruit_NeoPixel {
 public:
  Adafruit_NeoPixel(uint16_t n, int16_t pin, neoPixelType type = NEO_GRB + NEO_KHZ800)
      : numPixels_(n), pin_(pin), type_(type), pixels_(n, 0) {}

  void begin() {
    if (strip_ != nullptr) {
      return;
    }

    led_strip_config_t stripConfig = {
        .strip_gpio_num = pin_,
        .max_leds = numPixels_,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {.invert_out = false},
    };

    led_strip_rmt_config_t rmtConfig = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 0,
        .flags = {.with_dma = 0},
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&stripConfig, &rmtConfig, &strip_));
    ESP_ERROR_CHECK(led_strip_clear(strip_));
  }

  static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
  }

  void setBrightness(uint8_t brightness) {
    brightness_ = brightness;
  }

  void setPixelColor(uint16_t n, uint32_t color) {
    if (n >= numPixels_) {
      return;
    }

    pixels_[n] = color;
  }

  void clear() {
    for (uint16_t i = 0; i < numPixels_; ++i) {
      pixels_[i] = 0;
    }
  }

  void show() {
    if (strip_ == nullptr) {
      return;
    }

    for (uint16_t i = 0; i < numPixels_; ++i) {
      const uint32_t color = pixels_[i];
      const uint8_t red = scale((color >> 16) & 0xFF);
      const uint8_t green = scale((color >> 8) & 0xFF);
      const uint8_t blue = scale(color & 0xFF);
      ESP_ERROR_CHECK(led_strip_set_pixel(strip_, i, red, green, blue));
    }

    ESP_ERROR_CHECK(led_strip_refresh(strip_));
  }

 private:
  uint8_t scale(uint8_t value) const {
    return static_cast<uint8_t>((static_cast<uint16_t>(value) * brightness_) / 255);
  }

  uint16_t numPixels_;
  int16_t pin_;
  neoPixelType type_;
  uint8_t brightness_ = 255;
  led_strip_handle_t strip_ = nullptr;
  std::vector<uint32_t> pixels_;
};