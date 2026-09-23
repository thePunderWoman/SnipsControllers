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
// The two addresses essentially every SSD1306 breakout module uses —
// begin() tries kI2cAddress first and falls back to kI2cAddressAlt.
// Printed on this project's display module's silkscreen as 0x3C, but
// bring-up testing found nothing ACKing there, so this now actually
// probes both rather than assuming.
constexpr uint8_t kI2cAddress = 0x3C;
constexpr uint8_t kI2cAddressAlt = 0x3D;

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
  // Adafruit_SSD1306 only allocates its internal frame buffer inside its
  // own begin() — every other method (clearDisplay(), display(), etc.)
  // writes into that buffer unconditionally, with no null check, and
  // crashes (StoreProhibited) if begin() was never successfully called.
  // Since begin() can legitimately fail (no display present) and callers
  // throughout SnipsController.ino call render()/etc. unconditionally
  // every tick regardless, this class has to remember its own readiness
  // and no-op everything until begin() actually succeeds.
  bool ready_ = false;
};
