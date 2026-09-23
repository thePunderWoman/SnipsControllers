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
// Confirmed against the real hardware — printed on the display module's
// own silkscreen (also documented in PCB/GPIO_table.md's Accelerometer
// section, which shares the bus at a different address).
constexpr uint8_t kI2cAddress = 0x3C;

}  // namespace Oled

class OledDisplay {
 public:
  // Initializes I2C and the display. Returns false if the display wasn't
  // found/didn't initialize (mirrors Adafruit_SSD1306::begin()).
  bool begin();

  void render(const ScreenBuffer &content);

  // Draws a 1-bit bitmap (row-major, MSB-first-per-byte, e.g.
  // snips_logo_bitmap.h) centered on the display and pushes it immediately.
  // Used for the boot splash; not part of the ScreenBuffer content model
  // since nothing else on the device draws bitmaps.
  void renderBitmapCentered(const uint8_t *bitmap, int width, int height);

  // Low-power mode support (see power_management.h). Dimming/undimming
  // and powering the panel back on take effect immediately; they don't
  // need a render() call to show existing content again since the
  // panel's GDRAM is untouched while off.
  void setDimmed(bool dimmed);
  void setPowerOn(bool on);

 private:
  Adafruit_SSD1306 display_{Oled::kWidth, Oled::kHeight, &Wire, -1};
};
