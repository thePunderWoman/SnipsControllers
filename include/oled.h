#pragma once

#include <Adafruit_SSD1306.h>

#include "screen.h"

// Thin hardware adapter over Adafruit_SSD1306/GFX — owns the real display
// object and draws whatever a ScreenBuffer currently holds. No content
// decisions happen here; that's ScreenBuffer's job (screen.h), which stays
// hardware-agnostic and unit tested. This file touches real I2C hardware
// and is excluded from native build/coverage (see platformio.ini's
// [env:native] build_src_filter).
namespace Oled {

constexpr int kWidth = 128;
constexpr int kHeight = 64;
// Common default for SSD1306 breakout modules — PCB/README.md doesn't
// pin down the exact module's I2C address, so this is an assumption.
// Confirm against the real hardware during this PR's bring-up milestone
// (0x3D is the other common alternative if 0x3C comes back empty).
constexpr uint8_t kI2cAddress = 0x3C;

}  // namespace Oled

class OledDisplay {
 public:
  // Initializes I2C and the display. Returns false if the display wasn't
  // found/didn't initialize (mirrors Adafruit_SSD1306::begin()).
  bool begin();

  void render(const ScreenBuffer &content);

  // Low-power mode support (see power_management.h). Dimming/undimming
  // and powering the panel back on take effect immediately; they don't
  // need a render() call to show existing content again since the
  // panel's GDRAM is untouched while off.
  void setDimmed(bool dimmed);
  void setPowerOn(bool on);

 private:
  Adafruit_SSD1306 display_{Oled::kWidth, Oled::kHeight, &Wire, -1};
};
